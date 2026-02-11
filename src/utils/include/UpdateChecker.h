#pragma once

#include <string>
#include <functional>
#include <atomic>
#include <mutex>

struct UpdateInfo {
    bool updateAvailable = false;
    std::string latestVersion;
    std::string currentVersion;
    std::string downloadUrl;
    std::string releaseNotes;
    std::string assetName;
    long assetSize = 0;
    std::string errorMessage;
};

class UpdateChecker {
public:
    // Check for updates synchronously (call from background thread)
    static UpdateInfo checkForUpdates();
    
    // Compare version strings (returns: -1 if v1 < v2, 0 if equal, 1 if v1 > v2)
    static int compareVersions(const std::string& v1, const std::string& v2);
    
    // Download update to staging location
    // progressCallback receives (bytesDownloaded, totalBytes)
    static bool downloadUpdate(const std::string& url, 
                               const std::string& outputPath,
                               std::function<void(long, long)> progressCallback = nullptr);
    
    // Write update marker file for launch.sh to process
    static bool writeUpdateMarker(const std::string& zipPath, const std::string& version);
    
    // Check if there's a pending update marker
    static bool hasPendingUpdate();
    
    // Get paths
    static std::string getUpdateStagingDir();
    static std::string getUpdateMarkerPath();
    static std::string getAppInstallDir();
    
    // Cached update info for app-wide access
    static void checkForUpdatesAsync();  // Start background check
    static bool hasCheckedForUpdates();  // Has the check completed?
    static bool isUpdateAvailable();     // Is an update available?
    static std::string getLatestVersion(); // Get cached latest version
    
private:
    static std::atomic<bool> s_checkCompleted;
    static std::atomic<bool> s_updateAvailable;
    static std::mutex s_cacheMutex;
    static UpdateInfo s_cachedInfo;
};
