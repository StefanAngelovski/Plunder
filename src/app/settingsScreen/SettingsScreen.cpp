#include "include/SettingsScreen.h"
#include "../../Version.h"
#include "../../utils/include/Theme.h"

#define CONFIG_PATH "/mnt/SDCARD/Apps/Plunder/config.json"
#define CACHE_PATH "/mnt/SDCARD/Apps/Plunder/cache/"

SettingsScreen::SettingsScreen(std::function<void()> onBack, std::function<void()> onThemes)
    : CarouselMenuScreen("Settings"), onBackCallback(onBack), onThemesCallback(onThemes) {
    setSubtitle("Plunder");
    introDisabled = isIntroDisabled();
    updateCheckboxLabel();
}

// Render the entire settings screen (delegates to base carousel)
void SettingsScreen::render(SDL_Renderer* renderer, TTF_Font* font) {
    CarouselMenuScreen::render(renderer, font);
}

// Update the carousel items for the settings screen
void SettingsScreen::updateCheckboxLabel() {
    clearItems();
    // Add Check for Updates button 
    addItem(getUpdateStatusText(), "images/settingsmenu/back.png", [this]() {
        UpdateState state = updateState.load();
        if (state == UpdateState::Idle || state == UpdateState::Error) {
            checkForUpdates();
        } else if (state == UpdateState::UpdateAvailable) {
            downloadAndInstallUpdate();
        } else if (state == UpdateState::ReadyToInstall) {
            // Exit app - launch.sh will handle the update
            std::cerr << "[SettingsScreen] Update ready, exiting to apply..." << std::endl;
            exit(0);
        }
    });
    // Add Disable Intro toggle
    addItem("Disable Intro", "images/settingsmenu/disableintro.png", [this]() {
        introDisabled = !introDisabled;
        setIntroDisabled(introDisabled);
    });
    // Add Clear Cache button
    addItem("Clear Cache", "images/settingsmenu/clearcache.png", [this]() {
        std::cerr << "[DEBUG] Clear Cache pressed!" << std::endl;
        clearCache();
        cacheClearedFlash = true;
        cacheClearedFlashStart = std::chrono::steady_clock::now();
    });
    // Add Themes button
    addItem("Themes", "images/settingsmenu/themes.png", [this]() {
        if (onThemesCallback) onThemesCallback();
    });
    // Add Back button
    addItem("Back", "images/settingsmenu/back.png", [this]() {
        if (onBackCallback) onBackCallback();
    });
}

// Get the status text for the update button
std::string SettingsScreen::getUpdateStatusText() {
    UpdateState state = updateState.load();
    switch (state) {
        case UpdateState::Idle:
            return "Check for Updates";
        case UpdateState::Checking:
            return "Checking...";
        case UpdateState::UpdateAvailable: {
            std::lock_guard<std::mutex> lock(updateMutex);
            return "Update: " + updateInfo.latestVersion;
        }
        case UpdateState::Downloading: {
            long prog = downloadProgress.load();
            long total = downloadTotal.load();
            if (total > 0) {
                int pct = (int)((prog * 100) / total);
                return "Downloading... " + std::to_string(pct) + "%";
            }
            return "Downloading...";
        }
        case UpdateState::ReadyToInstall:
            return "Restart to Update";
        case UpdateState::Error: {
            std::lock_guard<std::mutex> lock(updateMutex);
            return "Error (tap to retry)";
        }
        default:
            return "Check for Updates";
    }
}

// Check for updates asynchronously
void SettingsScreen::checkForUpdates() {
    updateState = UpdateState::Checking;
    updateCheckboxLabel();
    
    std::thread([this]() {
        UpdateInfo info = UpdateChecker::checkForUpdates();
        
        {
            std::lock_guard<std::mutex> lock(updateMutex);
            updateInfo = info;
        }
        
        if (!info.errorMessage.empty()) {
            updateState = UpdateState::Error;
            std::cerr << "[SettingsScreen] Update check error: " << info.errorMessage << std::endl;
        } else if (info.updateAvailable) {
            updateState = UpdateState::UpdateAvailable;
            std::cerr << "[SettingsScreen] Update available: " << info.latestVersion << std::endl;
        } else {
            updateState = UpdateState::Idle;
            std::cerr << "[SettingsScreen] No update available, current version is latest" << std::endl;
        }
        
        updateCheckboxLabel();
    }).detach();
}

// Download and install the update
void SettingsScreen::downloadAndInstallUpdate() {
    std::string downloadUrl;
    std::string version;
    std::string assetName;
    long assetSize;
    
    {
        std::lock_guard<std::mutex> lock(updateMutex);
        downloadUrl = updateInfo.downloadUrl;
        version = updateInfo.latestVersion;
        assetName = updateInfo.assetName;
        assetSize = updateInfo.assetSize;
    }
    
    if (downloadUrl.empty()) {
        updateState = UpdateState::Error;
        updateCheckboxLabel();
        return;
    }
    
    updateState = UpdateState::Downloading;
    downloadProgress = 0;
    downloadTotal = assetSize;
    updateCheckboxLabel();
    
    std::thread([this, downloadUrl, version, assetName]() {
        std::string stagingDir = UpdateChecker::getUpdateStagingDir();
        std::string outputPath = stagingDir + "/" + assetName;
        
        bool success = UpdateChecker::downloadUpdate(downloadUrl, outputPath, 
            [this](long current, long total) {
                downloadProgress = current;
                if (total > 0) downloadTotal = total;
            });
        
        if (success) {
            if (UpdateChecker::writeUpdateMarker(outputPath, version)) {
                updateState = UpdateState::ReadyToInstall;
                std::cerr << "[SettingsScreen] Update downloaded and ready to install" << std::endl;
            } else {
                updateState = UpdateState::Error;
                std::cerr << "[SettingsScreen] Failed to write update marker" << std::endl;
            }
        } else {
            updateState = UpdateState::Error;
            std::cerr << "[SettingsScreen] Download failed" << std::endl;
        }
        
        updateCheckboxLabel();
    }).detach();
}

// Custom rendering for each carousel item
CarouselItemDecor SettingsScreen::decorate(int i) {
    CarouselItemDecor d;
    const auto& items = getItems();
    if (i < 0 || i >= (int)items.size()) return d;
    const std::string& label = items[i].label;

    if (i == 0) {  // update status card (label changes with the state)
        d.iconAngle = -90.0;  // back arrow rotated to point up
        d.sublabel = "Installed v" + std::string(PLUNDER_VERSION);
        switch (updateState.load()) {
            case UpdateState::UpdateAvailable: d.badge = "New"; d.badgeOn = true; break;
            case UpdateState::ReadyToInstall:  d.badge = "Ready"; d.badgeOn = true; break;
            case UpdateState::Error:           d.badge = "Error"; break;
            default: break;
        }
    } else if (label == "Disable Intro") {
        d.badge = introDisabled ? "On" : "Off";
        d.badgeOn = introDisabled;
        d.sublabel = introDisabled ? "Intro video is skipped" : "Intro video plays on launch";
    } else if (label == "Clear Cache") {
        d.sublabel = "Remove cached cover images";
        if (cacheClearedFlash) {
            int elapsed = (int)std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - cacheClearedFlashStart).count();
            if (elapsed < cacheClearedFlashDurationMs) {
                d.flash = (float)elapsed / cacheClearedFlashDurationMs;
                d.sublabel = "Cache cleared";
            } else {
                cacheClearedFlash = false;
            }
        }
    } else if (label == "Themes") {
        std::string id = Theme::getInstance().getCurrentThemeId();
        if (!id.empty()) id[0] = (char)std::toupper((unsigned char)id[0]);
        d.sublabel = "Current: " + id;
    }
    return d;
}

bool SettingsScreen::isIntroDisabled() {
    struct stat st;
    if (stat(CONFIG_PATH, &st) != 0) return false;
    FILE* f = fopen(CONFIG_PATH, "r");
    if (!f) return false;
    struct json_object* jobj = json_object_from_file(CONFIG_PATH);
    if (!jobj) { fclose(f); return false; }
    struct json_object* val = nullptr;
    bool result = false;
    if (json_object_object_get_ex(jobj, "disable_intro", &val)) {
        result = json_object_get_boolean(val);
    }
    json_object_put(jobj);
    fclose(f);
    return result;
}

// Write the "disable_intro" setting to config file
void SettingsScreen::setIntroDisabled(bool disabled) {
    struct json_object* jobj = nullptr;
    struct stat st;
    if (stat(CONFIG_PATH, &st) == 0) {
        jobj = json_object_from_file(CONFIG_PATH);
    }
    if (!jobj) jobj = json_object_new_object();
    struct json_object* val = json_object_new_boolean(disabled);
    json_object_object_add(jobj, "disable_intro", val);
    json_object_to_file(CONFIG_PATH, jobj);
    json_object_put(jobj);
}

// Helper: Recursively remove a directory and its contents (for clearing cache)
static void removeDirRecursive(const char* path) {
    DIR* dir = opendir(path);
    if (!dir) { std::cerr << "[DEBUG] Failed to open dir: " << path << " errno=" << errno << std::endl; return; }
    struct dirent* entry;
    char filepath[512];
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        snprintf(filepath, sizeof(filepath), "%s/%s", path, entry->d_name);
        struct stat st;
        if (stat(filepath, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                std::cerr << "[DEBUG] Recursing into dir: " << filepath << std::endl;
                removeDirRecursive(filepath);
            } else {
                std::cerr << "[DEBUG] Removing file: " << filepath << std::endl;
                if (remove(filepath) != 0) std::cerr << "[DEBUG] Failed to remove file: " << filepath << " errno=" << errno << std::endl;
            }
        } else {
            std::cerr << "[DEBUG] stat failed: " << filepath << " errno=" << errno << std::endl;
        }
    }
    closedir(dir);
    std::cerr << "[DEBUG] Removing dir: " << path << std::endl;
    if (rmdir(path) != 0) std::cerr << "[DEBUG] Failed to remove dir: " << path << " errno=" << errno << std::endl;
}

// Remove all files in the cache directory and recreate it
void SettingsScreen::clearCache() {
    struct stat st;
    if (stat(CACHE_PATH, &st) != 0) {
        std::cerr << "[DEBUG] Cache dir does not exist, creating: " << CACHE_PATH << std::endl;
        mkdir(CACHE_PATH, 0755); // Create cache dir if missing
        return;
    }
    std::cerr << "[DEBUG] Removing cache dir recursively: " << CACHE_PATH << std::endl;
    removeDirRecursive(CACHE_PATH);
    mkdir(CACHE_PATH, 0755); // Recreate empty cache dir
}