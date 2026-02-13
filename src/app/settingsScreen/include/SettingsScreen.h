#pragma once
#include <functional>
#include <chrono>
#include <json-c/json.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <iostream>
#include <errno.h>
#include <thread>
#include <atomic>
#include <mutex>

#include "../../carousel/include/CarouselMenuScreen.h"
#include "../../../utils/include/UpdateChecker.h"

class SettingsScreen : public CarouselMenuScreen {
public:
    SettingsScreen(std::function<void()> onBack = nullptr, std::function<void()> onThemes = nullptr);

    // Persistent settings
    static bool isIntroDisabled();
    static void setIntroDisabled(bool disabled);
    static void clearCache();

    // UI update
    void updateCheckboxLabel();
    void render(SDL_Renderer* renderer, TTF_Font* font) override;

    // Update checking
    void checkForUpdates();
    void downloadAndInstallUpdate();

protected:
    // Custom rendering for each carousel item
    void renderItem(SDL_Renderer* renderer, TTF_Font* font, int i, int x, int y, int w, int h, bool focused) override;

private:
    // State
    bool introDisabled;
    std::function<void()> onBackCallback;
    std::function<void()> onThemesCallback;

    // Animation state for UI effects
    bool disableIntroGlow = false;
    float glowPhase = 0.0f;
    int disableIntroIndex = -1;
    int clearCacheIndex = -1;
    bool cacheClearedFlash = false;
    std::chrono::steady_clock::time_point cacheClearedFlashStart;
    static constexpr int cacheClearedFlashDurationMs = 1000; // 1 second

    // Update state
    enum class UpdateState {
        Idle,
        Checking,
        UpdateAvailable,
        Downloading,
        ReadyToInstall,
        Error
    };
    
    std::atomic<UpdateState> updateState{UpdateState::Idle};
    UpdateInfo updateInfo;
    std::mutex updateMutex;
    std::string updateStatusText;
    std::atomic<long> downloadProgress{0};
    std::atomic<long> downloadTotal{0};
    
    // Helper to get status text for current update state
    std::string getUpdateStatusText();
};