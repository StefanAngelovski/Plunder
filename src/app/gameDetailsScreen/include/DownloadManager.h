#pragma once

#include <atomic>
#include <string>
#include <thread>
#include <vector>
#include <cstdio>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <csignal>
#include <regex>
#include <fstream>
#include <algorithm>
#include <dirent.h>

#include <SDL2/SDL.h>

#include "../../../model/GameDetails.h"
#include "../../../utils/include/HttpUtils.h"
#include "../../../utils/include/StringUtils.h"
#include "../../consolePolicies/ConsoleZipPolicy.h"

struct DownloadOption {
    std::string label;
    std::string url;
};

class DownloadManager {
public:
    DownloadManager();
    ~DownloadManager();
    
    // Main download operations
    void startDownload(const std::string& url, const std::string& outPath, const GameDetails& details);
    void cancelDownload();
    
    // State queries
    bool isDownloading() const { return downloadInProgress; }
    bool isCancelRequested() const { return downloadCancelRequested; }
    
    // Progress getters for UI
    long getCurrentBytes() const { return downloadCurrentBytes; }
    long getTotalBytes() const { return downloadTotalBytes; }
    std::string getProgressText() const { return downloadProgressText; }
    std::string getOutputPath() const { return downloadOutPath; }
    pid_t getDownloadPid() const { return downloadPid; }
    
    // Focus management for UI
    int getButtonFocus() const { return downloadButtonFocus; }
    void setButtonFocus(int focus) { downloadButtonFocus = focus; }
    
    // Static utility methods for scraping
    static std::vector<DownloadOption> scrapeDownloadOptions(const std::string& pageUrl);
    static std::string scrapeFinalDownloadLink(const std::string& versionUrl);
    static long getRemoteFileSize(const std::string& url);
    
private:
    // Worker thread function
    void downloadWorker(const std::string& url, const std::string& outPath, const GameDetails& details);
    
    // Utility methods
    static std::string humanReadableSize(long bytes);
    static void downloadFileToDisk(const std::string& url, const std::string& outPath);
    
    // State variables
    std::atomic<bool> downloadInProgress{false};
    std::atomic<bool> downloadCancelRequested{false};
    std::atomic<long> downloadCurrentBytes{0};
    std::atomic<long> downloadTotalBytes{0};
    std::string downloadProgressText;
    pid_t downloadPid = -1;
    std::string downloadOutPath;
    int downloadButtonFocus = 0; // 0 = progress bar, 1 = cancel button
};