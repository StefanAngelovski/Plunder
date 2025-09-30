#include "include/DownloadManager.h"

DownloadManager::DownloadManager() {
    // Initialize state
    downloadInProgress = false;
    downloadCancelRequested = false;
    downloadCurrentBytes = 0;
    downloadTotalBytes = 0;
    downloadPid = -1;
    downloadButtonFocus = 0;
}

DownloadManager::~DownloadManager() {
    if (downloadInProgress) {
        cancelDownload();
    }
}

void DownloadManager::startDownload(const std::string& url, const std::string& outPath, const GameDetails& details) {
    if (downloadInProgress) return; // Already downloading
    
    downloadOutPath = outPath;
    downloadProgressText = "Starting...";
    downloadInProgress = true;
    downloadCancelRequested = false;
    downloadCurrentBytes = 0;
    downloadTotalBytes = 0;
    
    // Start download in background thread
    std::thread(&DownloadManager::downloadWorker, this, url, outPath, details).detach();
}

void DownloadManager::cancelDownload() {
    downloadCancelRequested = true;
    downloadInProgress = false;
    if (downloadPid > 0) {
        kill(downloadPid, SIGTERM);
        downloadPid = -1;
    }
    downloadProgressText = "Download canceled.";
    downloadTotalBytes = 0;
    downloadCurrentBytes = 0;
    downloadButtonFocus = 0;
    
    // Ensure partial file is deleted immediately
    if (!downloadOutPath.empty()) {
        std::remove(downloadOutPath.c_str());
    }
}

void DownloadManager::downloadWorker(const std::string& url, const std::string& outPath, const GameDetails& details) {
    // Try to discover size (HEAD)
    long size = getRemoteFileSize(url);
    if (size > 0) downloadTotalBytes = size;
    
    std::string referer = url.find("gamulator.com") != std::string::npos ? "https://www.gamulator.com/" : "https://hexrom.com/";
    
    // Fork & exec curl so we can poll file size
    pid_t pid = fork();
    if (pid == 0) {
        // Child
        execlp("curl", "curl",
               "--globoff", "-L", "--retry", "2",
               "--cacert", "/etc/ssl/certs/ca-certificates.crt",
               "-A", "Mozilla/5.0 (X11; Linux x86_64)",
               "-e", referer.c_str(),
               "-o", outPath.c_str(),
               url.c_str(),
               (char*)nullptr);
        _exit(127);
    } else if (pid < 0) {
        downloadProgressText = "Failed to fork.";
        downloadInProgress = false;
        return;
    }
    
    downloadPid = pid;
    
    // Parent: poll
    while (true) {
        if (downloadCancelRequested) {
            kill(pid, SIGTERM);
        }
        
        int status = 0;
        pid_t r = waitpid(pid, &status, WNOHANG);
        
        struct stat st{};
        if (stat(outPath.c_str(), &st) == 0) {
            downloadCurrentBytes = st.st_size;
            if (downloadTotalBytes > 0) {
                double pct = downloadTotalBytes > 0 ? (double)downloadCurrentBytes / (double)downloadTotalBytes * 100.0 : 0.0;
                downloadProgressText = humanReadableSize(downloadCurrentBytes) + " / " + humanReadableSize(downloadTotalBytes) + " (" + std::to_string((int)std::round(pct)) + "%)";
            } else {
                downloadProgressText = humanReadableSize(downloadCurrentBytes);
            }
        }
        
        if (r == pid) break; // finished
        if (downloadCancelRequested) {
            downloadProgressText = "Download canceled.";
            break;
        }
        
        SDL_Delay(200); // sleep a bit
    }
    
    // Finalize
    if (downloadCancelRequested) {
        std::remove(outPath.c_str());
        if (downloadPid > 0) { 
            kill(downloadPid, SIGTERM); 
        }
        downloadInProgress = false;
        return;
    }
    
    struct stat stFinal{};
    if (stat(outPath.c_str(), &stFinal) == 0 && stFinal.st_size > 0) {
        downloadCurrentBytes = stFinal.st_size;
        if (downloadTotalBytes == 0) downloadTotalBytes = stFinal.st_size;
        
        // Attempt relocate using persisted mappedFolder (if any)
        if (!details.mappedFolder.empty()) {
            std::vector<std::string> roots = {"/mnt/SDCARD/Roms","/mnt/SDCARD/ROMS","Roms","ROMS","../Roms","../ROMS"};
            std::string baseDir;
            struct stat stDir{};
            for (auto &r : roots) {
                std::string candidate = r + "/" + details.mappedFolder;
                if (stat(candidate.c_str(), &stDir) == 0 && S_ISDIR(stDir.st_mode)) { 
                    baseDir = candidate; 
                    break; 
                }
            }
            
            if (!baseDir.empty()) {
                std::string finalFileName = outPath.substr(outPath.find_last_of('/')+1);
                std::string newPath = baseDir + "/" + finalFileName;
                bool isZip = false;
                if (finalFileName.size() > 4) {
                    std::string ext = finalFileName.substr(finalFileName.size()-4);
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".zip" || ext == ".rar") isZip = true;
                }
                
                bool unzip = isZip && shouldUnzipForFolder(details.mappedFolder);
                printf("[Download] Post-processing: file=%s isZip=%d unzip=%d folder=%s\n", finalFileName.c_str(), (int)isZip, (int)unzip, details.mappedFolder.c_str());
                
                if (unzip) {
                    // Create temp extraction dir
                    std::string tmpDir = baseDir + "/.__extract_tmp";
                    mkdir(tmpDir.c_str(), 0755);
                    
                    // Use busybox/unzip if available; try unzip then 7z
                    auto cmdExists = [](const char* c){ return system((std::string("command -v ") + c + " >/dev/null 2>&1").c_str()) == 0; };
                    bool haveUnzip = cmdExists("unzip");
                    bool have7z = cmdExists("7z");
                    int ret = -1;
                    
                    if (haveUnzip) {
                        // Show unzip message in UI
                        downloadProgressText = "Unzipping...";
                        ret = system((std::string("unzip -o \"") + outPath + "\" -d \"" + tmpDir + "\" >/dev/null 2>&1").c_str());
                    }
                    if (ret != 0 && have7z) {
                        ret = system((std::string("7z x -y \"") + outPath + "\" -o\"" + tmpDir + "\" >/dev/null 2>&1").c_str());
                    }
                    
                    if (ret != 0) {
                        printf("[Download] Extraction skipped/failed: unzip=%d 7z=%d ret=%d (keeping zip)\n", haveUnzip?1:0, have7z?1:0, ret);
                    }
                    
                    if (ret == 0) {
                        // Move extracted files into baseDir
                        DIR* d = opendir(tmpDir.c_str());
                        if (d) {
                            struct dirent* de;
                            while ((de = readdir(d))) {
                                if (std::string(de->d_name) == "." || std::string(de->d_name) == "..") continue;
                                std::string from = tmpDir + "/" + de->d_name;
                                std::string to = baseDir + "/" + de->d_name;
                                rename(from.c_str(), to.c_str());
                            }
                            closedir(d);
                        }
                        // Delete original zip
                        std::remove(outPath.c_str());
                        // Remove temp dir
                        rmdir(tmpDir.c_str());
                        downloadProgressText = std::string("Extracted to ") + baseDir;
                        printf("[Download] Extracted contents to %s (removed zip)\n", baseDir.c_str());
                    } else {
                        // Extraction failed -> attempt plain move of zip
                        if (rename(outPath.c_str(), newPath.c_str()) == 0) {
                            downloadOutPath = newPath;
                            downloadProgressText = std::string("Saved to ") + newPath + " (zip kept)";
                            printf("[Download] Extraction failed; moved zip to %s\n", newPath.c_str());
                        } else {
                            if (std::remove(outPath.c_str()) == 0) printf("[Download] Extraction+move failed; zip deleted %s\n", outPath.c_str());
                            downloadProgressText = "Download failed (extract+move).";
                        }
                    }
                } else {
                    // Just move the zip or non-zip file
                    if (rename(outPath.c_str(), newPath.c_str()) == 0) {
                        downloadOutPath = newPath;
                        downloadProgressText = std::string("Saved to ") + newPath;
                        printf("[Download] Moved file to %s\n", newPath.c_str());
                    } else {
                        if (std::remove(outPath.c_str()) == 0) printf("[Download] Move failed, file deleted: %s\n", outPath.c_str());
                        downloadProgressText = "Download failed (move error).";
                    }
                }
            } else {
                if (std::remove(outPath.c_str()) == 0) printf("[Download] Console folder missing, file deleted: %s (mappedFolder=%s)\n", outPath.c_str(), details.mappedFolder.c_str());
                downloadProgressText = "Download failed (console folder missing).";
            }
        } else {
            // No mapping persisted -> delete per earlier rule
            if (std::remove(outPath.c_str()) == 0) printf("[Download] No mapping (persisted empty), file deleted: %s\n", outPath.c_str());
            downloadProgressText = "Download failed (no console mapping).";
        }
    } else {
        downloadProgressText = "Download failed.";
    }
    
    downloadInProgress = false;
}

std::string DownloadManager::humanReadableSize(long bytes) {
    const char* units[] = {"B","KB","MB","GB","TB"};
    double v = (double)bytes;
    int unit = 0;
    while (v >= 1024.0 && unit < 4) { 
        v /= 1024.0; 
        ++unit; 
    }
    char buf[64];
    if (v < 10.0) snprintf(buf, sizeof(buf), "%.2f %s", v, units[unit]);
    else if (v < 100.0) snprintf(buf, sizeof(buf), "%.1f %s", v, units[unit]);
    else snprintf(buf, sizeof(buf), "%.0f %s", v, units[unit]);
    return std::string(buf);
}

// Static utility methods for scraping
std::vector<DownloadOption> DownloadManager::scrapeDownloadOptions(const std::string& pageUrl) {
    std::vector<DownloadOption> result;
    std::string html = HttpUtils::fetchWebContent(pageUrl);
    if (html.empty()) return result;
    
    // Find the main table with download links
    std::smatch tableMatch;
    std::regex tableRe(R"(<table[^>]*>([\s\S]*?)</table>)", std::regex::icase);
    if (std::regex_search(html, tableMatch, tableRe)) {
        std::string table = tableMatch[1].str();
        std::regex rowRe(R"(<tr>([\s\S]*?)</tr>)", std::regex::icase);
        auto rowsBegin = std::sregex_iterator(table.begin(), table.end(), rowRe);
        auto rowsEnd = std::sregex_iterator();
        for (auto it = rowsBegin; it != rowsEnd; ++it) {
            std::string row = (*it)[1].str();
            std::smatch linkMatch;
            std::regex linkRe(R"(<a[^>]+href=["']([^"']+/download/[^"']+)["'][^>]*>([\s\S]*?)</a>)", std::regex::icase);
            if (std::regex_search(row, linkMatch, linkRe)) {
                std::string url = linkMatch[1].str();
                // Only prepend romsfun.com for Romsfun, not Hexrom
                if (url.find("http") != 0 && url.find("/download/") != std::string::npos) {
                    url = "https://hexrom.com" + url;
                }
                std::string label = StringUtils::cleanHtmlText(linkMatch[2].str());
                result.push_back({label, url});
            }
        }
    }
    return result;
}

std::string DownloadManager::scrapeFinalDownloadLink(const std::string& versionUrl) {
    std::string html = HttpUtils::fetchWebContent(versionUrl);
    if (html.empty()) return "";
    std::smatch linkMatch;
    std::regex linkRe(R"(<a[^>]+id=["']download["'][^>]+href=["']([^"']+)["'])", std::regex::icase);
    if (std::regex_search(html, linkMatch, linkRe)) {
        return linkMatch[1].str();
    }
    return "";
}

void DownloadManager::downloadFileToDisk(const std::string& url, const std::string& outPath) {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        // Could log cwd if needed
    }
    std::string data = HttpUtils::fetchWebContent(url);
    if (!data.empty()) {
        std::ofstream out(outPath, std::ios::binary);
        out.write(data.data(), data.size());
        out.close();
    }
}

long DownloadManager::getRemoteFileSize(const std::string& url) {
    std::string command = "curl --cacert /etc/ssl/certs/ca-certificates.crt -sI \"" + url + "\"";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return -1;
    
    char buffer[256];
    long size = -1;
    while (fgets(buffer, sizeof(buffer), pipe)) {
        std::string line(buffer);
        // Make case-insensitive and trim whitespace
        std::string lowerLine = line;
        std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);
        size_t pos = lowerLine.find("content-length:");
        if (pos != std::string::npos) {
            std::string val = line.substr(pos + 15);
            val.erase(0, val.find_first_not_of(" \t\r\n"));
            val.erase(val.find_last_not_of(" \t\r\n") + 1);
            try {
                size = std::stol(val);
            } catch (...) {
                size = -1;
            }
            break;
        }
    }
    pclose(pipe);
    return size;
}