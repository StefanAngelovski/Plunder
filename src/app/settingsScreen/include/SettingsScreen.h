#pragma once
#include <functional>
#include <chrono>
#include <json-c/json.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <iostream>
#include <errno.h>

#include "../../carousel/include/CarouselMenuScreen.h"

class SettingsScreen : public CarouselMenuScreen {
public:
    // Construct the settings screen, optionally with a callback for the Back button
    SettingsScreen(std::function<void()> onBack = nullptr);

    // Persistent settings
    static bool isIntroDisabled();
    static void setIntroDisabled(bool disabled);
    static void clearCache();

    // UI update
    void updateCheckboxLabel();
    void render(SDL_Renderer* renderer, TTF_Font* font) override;

protected:
    // Custom rendering for each carousel item
    void renderItem(SDL_Renderer* renderer, TTF_Font* font, int i, int x, int y, int w, int h, bool focused) override;

private:
    // State
    bool introDisabled;
    std::function<void()> onBackCallback;

    // Animation state for UI effects
    bool disableIntroGlow = false;
    float glowPhase = 0.0f;
    int disableIntroIndex = -1;
    int clearCacheIndex = -1;
    bool cacheClearedFlash = false;
    std::chrono::steady_clock::time_point cacheClearedFlashStart;
    static constexpr int cacheClearedFlashDurationMs = 1000; // 1 second
};