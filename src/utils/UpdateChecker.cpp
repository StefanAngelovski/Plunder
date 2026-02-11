#include "include/UpdateChecker.h"
#include "include/HttpUtils.h"
#include "../Version.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <thread>

// Simple JSON parsing helpers (avoiding external dependency)
static std::string extractJsonString(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return "";
    
    size_t colonPos = json.find(':', keyPos + searchKey.length());
    if (colonPos == std::string::npos) return "";
    
    size_t startQuote = json.find('"', colonPos + 1);
    if (startQuote == std::string::npos) return "";
    
    size_t endQuote = json.find('"', startQuote + 1);
    // Handle escaped quotes
    while (endQuote != std::string::npos && json[endQuote - 1] == '\\') {
        endQuote = json.find('"', endQuote + 1);
    }
    if (endQuote == std::string::npos) return "";
    
    return json.substr(startQuote + 1, endQuote - startQuote - 1);
}

static long extractJsonNumber(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return 0;
    
    size_t colonPos = json.find(':', keyPos + searchKey.length());
    if (colonPos == std::string::npos) return 0;
    
    // Skip whitespace
    size_t numStart = colonPos + 1;
    while (numStart < json.length() && (json[numStart] == ' ' || json[numStart] == '\t')) {
        numStart++;
    }
    
    std::string numStr;
    while (numStart < json.length() && (isdigit(json[numStart]) || json[numStart] == '-')) {
        numStr += json[numStart++];
    }
    
    return numStr.empty() ? 0 : std::stol(numStr);
}

// Find the first .zip asset in the assets array
static bool findZipAsset(const std::string& json, std::string& outUrl, std::string& outName, long& outSize) {
    size_t assetsPos = json.find("\"assets\"");
    if (assetsPos == std::string::npos) return false;
    
    size_t arrayStart = json.find('[', assetsPos);
    if (arrayStart == std::string::npos) return false;
    
    // Find each asset object - need to track both [] and {} depth
    int arrayDepth = 0;
    int objDepth = 0;
    size_t objStart = std::string::npos;
    
    for (size_t i = arrayStart; i < json.length(); ++i) {
        char c = json[i];
        
        if (c == '[') {
            arrayDepth++;
        } else if (c == ']') {
            arrayDepth--;
            if (arrayDepth == 0) break; // End of assets array
        } else if (c == '{') {
            if (arrayDepth == 1 && objDepth == 0) {
                objStart = i; // Start of a top-level asset object
            }
            objDepth++;
        } else if (c == '}') {
            objDepth--;
            if (arrayDepth == 1 && objDepth == 0 && objStart != std::string::npos) {
                // End of a top-level asset object
                std::string assetObj = json.substr(objStart, i - objStart + 1);
                std::string name = extractJsonString(assetObj, "name");
                
                // Look for .zip file
                if (name.length() > 4 && name.substr(name.length() - 4) == ".zip") {
                    outUrl = extractJsonString(assetObj, "browser_download_url");
                    outName = name;
                    outSize = extractJsonNumber(assetObj, "size");
                    return true;
                }
                objStart = std::string::npos;
            }
        }
    }
    
    return false;
}

std::string UpdateChecker::getUpdateStagingDir() {
    return "/mnt/SDCARD/Apps/Plunder/.update_staging";
}

std::string UpdateChecker::getUpdateMarkerPath() {
    return "/mnt/SDCARD/Apps/Plunder/.update_pending";
}

std::string UpdateChecker::getAppInstallDir() {
    return "/mnt/SDCARD/Apps/Plunder";
}

int UpdateChecker::compareVersions(const std::string& v1, const std::string& v2) {
    // Remove 'v' prefix if present
    std::string ver1 = v1, ver2 = v2;
    if (!ver1.empty() && (ver1[0] == 'v' || ver1[0] == 'V')) ver1 = ver1.substr(1);
    if (!ver2.empty() && (ver2[0] == 'v' || ver2[0] == 'V')) ver2 = ver2.substr(1);
    
    // Split by dots and hyphens for comparison
    // Format: MAJOR.MINOR.PATCH[-tag]
    struct VersionParts {
        int major, minor, patch;
        std::string tag;
    };
    
    auto parseVersion = [](const std::string& v) -> VersionParts {
        VersionParts parts = {0, 0, 0, ""};
        
        size_t hyphenPos = v.find('-');
        std::string numPart = (hyphenPos != std::string::npos) ? v.substr(0, hyphenPos) : v;
        if (hyphenPos != std::string::npos) parts.tag = v.substr(hyphenPos + 1);
        
        std::istringstream ss(numPart);
        char dot;
        ss >> parts.major;
        if (ss.peek() == '.') { ss >> dot >> parts.minor; }
        if (ss.peek() == '.') { ss >> dot >> parts.patch; }
        
        return parts;
    };
    
    VersionParts p1 = parseVersion(ver1);
    VersionParts p2 = parseVersion(ver2);
    
    // Compare numeric parts first
    if (p1.major != p2.major) return (p1.major < p2.major) ? -1 : 1;
    if (p1.minor != p2.minor) return (p1.minor < p2.minor) ? -1 : 1;
    if (p1.patch != p2.patch) return (p1.patch < p2.patch) ? -1 : 1;
    
    // If numeric parts equal, compare tags
    // No tag > any tag (e.g., 1.0.0 > 1.0.0-beta)
    if (p1.tag.empty() && !p2.tag.empty()) return 1;
    if (!p1.tag.empty() && p2.tag.empty()) return -1;
    if (p1.tag == p2.tag) return 0;
    
    // Compare tags alphabetically (alpha < beta < rc)
    return (p1.tag < p2.tag) ? -1 : 1;
}

UpdateInfo UpdateChecker::checkForUpdates() {
    UpdateInfo info;
    info.currentVersion = PLUNDER_VERSION;
    
    std::cerr << "[UpdateChecker] Checking for updates..." << std::endl;
    std::cerr << "[UpdateChecker] Current version: " << info.currentVersion << std::endl;
    
    // Fetch latest release info from GitHub API
    std::string apiUrl = PLUNDER_GITHUB_API_URL;
    
    // Use curl with Accept header for GitHub API
    std::string command = "curl --globoff --cacert /etc/ssl/certs/ca-certificates.crt "
                          "-H \"Accept: application/vnd.github.v3+json\" "
                          "-H \"User-Agent: Plunder-Updater\" "
                          "--max-time 15 -fsSL \"" + apiUrl + "\"";
    
    std::string json;
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe) {
        char buffer[256];
        while (!feof(pipe)) {
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                json += buffer;
            }
        }
        int status = pclose(pipe);
        if (status != 0) {
            info.errorMessage = "Failed to connect to GitHub API";
            std::cerr << "[UpdateChecker] API request failed with status: " << status << std::endl;
            return info;
        }
    } else {
        info.errorMessage = "Failed to execute curl";
        return info;
    }
    
    if (json.empty()) {
        info.errorMessage = "Empty response from GitHub API";
        return info;
    }
    
    std::cerr << "[UpdateChecker] Received " << json.length() << " bytes from API" << std::endl;
    
    // The /releases endpoint returns an array, extract the first release object
    std::string releaseJson = json;
    if (!json.empty() && json[0] == '[') {
        // Find the first object in the array
        size_t objStart = json.find('{');
        if (objStart == std::string::npos) {
            info.errorMessage = "No releases found";
            return info;
        }
        // Find matching closing brace (handle nested objects)
        int depth = 0;
        size_t objEnd = objStart;
        for (size_t i = objStart; i < json.length(); ++i) {
            if (json[i] == '{') depth++;
            else if (json[i] == '}') {
                depth--;
                if (depth == 0) {
                    objEnd = i;
                    break;
                }
            }
        }
        releaseJson = json.substr(objStart, objEnd - objStart + 1);
        std::cerr << "[UpdateChecker] Extracted first release object (" << releaseJson.length() << " bytes)" << std::endl;
    }
    
    // Parse the response
    std::string tagName = extractJsonString(releaseJson, "tag_name");
    if (tagName.empty()) {
        info.errorMessage = "Could not parse release version";
        return info;
    }
    
    info.latestVersion = tagName;
    std::cerr << "[UpdateChecker] Latest version: " << info.latestVersion << std::endl;
    
    // Compare versions
    int cmp = compareVersions(info.currentVersion, info.latestVersion);
    info.updateAvailable = (cmp < 0);
    
    std::cerr << "[UpdateChecker] Update available: " << (info.updateAvailable ? "yes" : "no") << std::endl;
    
    if (info.updateAvailable) {
        // Get release notes
        info.releaseNotes = extractJsonString(releaseJson, "body");
        
        // Find downloadable asset
        if (!findZipAsset(releaseJson, info.downloadUrl, info.assetName, info.assetSize)) {
            info.errorMessage = "No downloadable .zip asset found";
            info.updateAvailable = false;
            return info;
        }
        
        std::cerr << "[UpdateChecker] Download URL: " << info.downloadUrl << std::endl;
        std::cerr << "[UpdateChecker] Asset: " << info.assetName << " (" << info.assetSize << " bytes)" << std::endl;
    }
    
    return info;
}

bool UpdateChecker::downloadUpdate(const std::string& url, 
                                    const std::string& outputPath,
                                    std::function<void(long, long)> progressCallback) {
    std::cerr << "[UpdateChecker] Downloading update from: " << url << std::endl;
    std::cerr << "[UpdateChecker] Output path: " << outputPath << std::endl;
    
    // Create staging directory if needed
    std::string stagingDir = getUpdateStagingDir();
    mkdir(stagingDir.c_str(), 0755);
    
    // Fork and exec curl for download with progress tracking
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        execlp("curl", "curl",
               "--globoff", "-L",
               "--cacert", "/etc/ssl/certs/ca-certificates.crt",
               "-H", "Accept: application/octet-stream",
               "-H", "User-Agent: Plunder-Updater",
               "-o", outputPath.c_str(),
               url.c_str(),
               (char*)nullptr);
        _exit(127);
    } else if (pid < 0) {
        std::cerr << "[UpdateChecker] Failed to fork for download" << std::endl;
        return false;
    }
    
    // Parent: poll file size for progress
    long lastSize = 0;
    while (true) {
        int status = 0;
        pid_t r = waitpid(pid, &status, WNOHANG);
        
        struct stat st{};
        if (stat(outputPath.c_str(), &st) == 0) {
            if (progressCallback && st.st_size != lastSize) {
                progressCallback(st.st_size, 0); // Total unknown during download
                lastSize = st.st_size;
            }
        }
        
        if (r == pid) {
            // Process finished
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                std::cerr << "[UpdateChecker] Download completed successfully" << std::endl;
                return true;
            } else {
                std::cerr << "[UpdateChecker] Download failed with status: " << status << std::endl;
                return false;
            }
        }
        
        usleep(100000); // 100ms
    }
    
    return false;
}

bool UpdateChecker::writeUpdateMarker(const std::string& zipPath, const std::string& version) {
    std::string markerPath = getUpdateMarkerPath();
    std::ofstream marker(markerPath);
    if (!marker.is_open()) {
        std::cerr << "[UpdateChecker] Failed to write update marker" << std::endl;
        return false;
    }
    
    marker << "ZIP_PATH=" << zipPath << std::endl;
    marker << "VERSION=" << version << std::endl;
    marker.close();
    
    std::cerr << "[UpdateChecker] Update marker written: " << markerPath << std::endl;
    return true;
}

bool UpdateChecker::hasPendingUpdate() {
    struct stat st;
    return (stat(getUpdateMarkerPath().c_str(), &st) == 0);
}

// Static member definitions
std::atomic<bool> UpdateChecker::s_checkCompleted{false};
std::atomic<bool> UpdateChecker::s_updateAvailable{false};
std::mutex UpdateChecker::s_cacheMutex;
UpdateInfo UpdateChecker::s_cachedInfo;

void UpdateChecker::checkForUpdatesAsync() {
    std::thread([]() {
        UpdateInfo info = checkForUpdates();
        {
            std::lock_guard<std::mutex> lock(s_cacheMutex);
            s_cachedInfo = info;
        }
        s_updateAvailable = info.updateAvailable;
        s_checkCompleted = true;
        std::cerr << "[UpdateChecker] Background check completed. Update available: " 
                  << (info.updateAvailable ? "yes" : "no") << std::endl;
    }).detach();
}

bool UpdateChecker::hasCheckedForUpdates() {
    return s_checkCompleted.load();
}

bool UpdateChecker::isUpdateAvailable() {
    return s_updateAvailable.load();
}

std::string UpdateChecker::getLatestVersion() {
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    return s_cachedInfo.latestVersion;
}
