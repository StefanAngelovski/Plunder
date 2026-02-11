#include "include/SettingsScreen.h"
#include "../../Version.h"

#define CONFIG_PATH "/mnt/SDCARD/Apps/Plunder/config.json"
#define CACHE_PATH "/mnt/SDCARD/Apps/Plunder/cache/"

// SettingsScreen constructor implementation
SettingsScreen::SettingsScreen(std::function<void()> onBack)
    : CarouselMenuScreen("Settings"), onBackCallback(onBack) {
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
    // Add Check for Updates button (uses back.png as placeholder until update.png is added)
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
void SettingsScreen::renderItem(SDL_Renderer* renderer, TTF_Font* font, int i, int x, int y, int w, int h, bool focused) {
    const auto& items = getItems();
    
    // Special rendering for the Update button - rotate icon 90 degrees and show version
    if (items[i].label.find("Check for Updates") != std::string::npos || 
        items[i].label.find("Update:") != std::string::npos ||
        items[i].label.find("Checking") != std::string::npos ||
        items[i].label.find("Downloading") != std::string::npos ||
        items[i].label.find("Restart to Update") != std::string::npos ||
        items[i].label.find("Error") != std::string::npos) {
        
        // Draw border
        SDL_Color borderColor = focused ? SDL_Color{255, 255, 255, 255} : SDL_Color{180, 180, 180, 255};
        int borderThickness = focused ? 3 : 2;
        SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
        for (int t = 0; t < borderThickness; ++t) {
            SDL_Rect borderRect = {x - t, y - t, w + 2*t, h + 2*t};
            SDL_RenderDrawRect(renderer, &borderRect);
        }
        
        // Load and draw icon rotated -90 degrees (pointing down)
        if (!items[i].imagePath.empty()) {
            SDL_Texture* tex = IMG_LoadTexture(renderer, items[i].imagePath.c_str());
            if (tex) {
                int maxSize = w * 0.6;
                int texW = 0, texH = 0;
                SDL_QueryTexture(tex, NULL, NULL, &texW, &texH);
                float aspect = texW > 0 && texH > 0 ? (float)texW / texH : 1.0f;
                int drawW = maxSize, drawH = maxSize;
                if (aspect > 1.0f) drawH = (int)(maxSize / aspect);
                else drawW = (int)(maxSize * aspect);
                
                SDL_Rect imgRect = {x + (w-drawW)/2, y + (h-drawH)/2, drawW, drawH};
                // Rotate -90 degrees (270 clockwise) to point down
                SDL_RenderCopyEx(renderer, tex, NULL, &imgRect, -90.0, NULL, SDL_FLIP_NONE);
                SDL_DestroyTexture(tex);
            }
        }
        
        // Draw label
        UiUtils::Color labelColor(255,255,255);
        UiUtils::RenderTextCentered(renderer, font, items[i].label, x + w/2, y + h + 32, labelColor);
        
        // Draw version below the label
        UiUtils::Color versionColor(180, 180, 180);
        std::string versionText = "v" + std::string(PLUNDER_VERSION);
        UiUtils::RenderTextCentered(renderer, font, versionText, x + w/2, y + h + 55, versionColor);
        
        return;
    }
    
    // Custom background for Disable Intro BEFORE base so border/icon render atop
    if (items[i].label == "Disable Intro") {
        if (introDisabled) {
            SDL_SetRenderDrawColor(renderer, 0, 220, 180, focused ? 255 : 220);
        } else {
            SDL_SetRenderDrawColor(renderer, 0, 120, 140, focused ? 200 : 160);
        }
        SDL_Rect bgRect = {x + 2, y + 2, w - 4, h - 4};
        SDL_RenderFillRect(renderer, &bgRect);
    }
    // First draw standard item (border, icon, label)
    CarouselMenuScreen::renderItem(renderer, font, i, x, y, w, h, focused);
    // Overlay shine AFTER base so it is visible over icon
    if (items[i].label == "Clear Cache" && cacheClearedFlash) {
        int elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - cacheClearedFlashStart).count();
        if (elapsed < cacheClearedFlashDurationMs) {
            float t = (float)elapsed / cacheClearedFlashDurationMs;
            int shineCenter = y + (int)(h * t);
            int shineHeight = 40;
            SDL_BlendMode oldMode; // not retrievable directly; just set additive and then revert to blend
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
            for (int yy = 0; yy < h; ++yy) {
                int absY = y + yy;
                float dist = std::abs(absY - shineCenter) / (shineHeight / 2.0f);
                float alphaF = std::max(0.0f, 1.0f - dist);
                int alpha = (int)(alphaF * 180); // brighter overlay
                if (alpha <= 0) continue;
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, alpha);
                SDL_RenderDrawLine(renderer, x + 4, absY, x + w - 4, absY);
            }
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        } else {
            cacheClearedFlash = false;
        }
    }
}

// Read the "disable_intro" setting from config file
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