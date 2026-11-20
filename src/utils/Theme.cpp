#include "include/Theme.h"
#include <json-c/json.h>
#include <sys/stat.h>
#include <iostream>

#define CONFIG_PATH "/mnt/SDCARD/Apps/Plunder/config.json"

Theme& Theme::getInstance() {
    static Theme instance;
    return instance;
}

Theme::Theme() {
    // Load theme from config on startup
    std::string themeId = "default";
    
    struct stat st;
    if (stat(CONFIG_PATH, &st) == 0) {
        struct json_object* jobj = json_object_from_file(CONFIG_PATH);
        if (jobj) {
            struct json_object* val = nullptr;
            if (json_object_object_get_ex(jobj, "theme", &val)) {
                const char* themeStr = json_object_get_string(val);
                if (themeStr) themeId = themeStr;
            }
            json_object_put(jobj);
        }
    }
    
    loadTheme(themeId);
}

void Theme::loadTheme(const std::string& themeId) {
    currentThemeId = themeId;
    currentColors = preset(themeId);
    std::cerr << "[Theme] Loaded theme: " << themeId << std::endl;
}

void Theme::applyBackground(SDL_Renderer* renderer) const {
    SDL_SetRenderDrawColor(renderer, 
        currentColors.background.r, 
        currentColors.background.g, 
        currentColors.background.b, 
        currentColors.background.a);
}

void Theme::applyBackgroundAlt(SDL_Renderer* renderer) const {
    SDL_SetRenderDrawColor(renderer, 
        currentColors.backgroundAlt.r, 
        currentColors.backgroundAlt.g, 
        currentColors.backgroundAlt.b, 
        currentColors.backgroundAlt.a);
}

void Theme::applyBackgroundDark(SDL_Renderer* renderer) const {
    SDL_SetRenderDrawColor(renderer, 
        currentColors.backgroundDark.r, 
        currentColors.backgroundDark.g, 
        currentColors.backgroundDark.b, 
        currentColors.backgroundDark.a);
}

// ==================== Theme Presets ====================
// Modern dark palettes: tinted near-black backgrounds, slightly lifted card
// surfaces, one vivid accent. Field order follows ThemeColors.

ThemeColors Theme::preset(const std::string& themeId) {
    if (themeId == "dark") {
        return ThemeColors{
            {20, 20, 25, 255},      // background
            {27, 27, 33, 255},      // backgroundAlt
            {11, 11, 14, 255},      // backgroundDark
            {36, 36, 44, 255},      // cardBackground
            {244, 244, 247, 255},   // textPrimary
            {150, 150, 166, 255},   // textSecondary
            {165, 180, 252, 255},   // textHighlight
            {129, 140, 248, 255},   // borderFocused
            {56, 56, 68, 255},      // borderUnfocused
            {129, 140, 248, 255},   // accentPrimary (indigo)
            {56, 189, 248, 255},    // accentSecondary
            {67, 56, 202, 255},     // activeItem
            {74, 222, 128, 255},    // toggleOn
            {63, 63, 74, 255}       // toggleOff
        };
    }
    if (themeId == "wasp") {
        return ThemeColors{
            {24, 21, 11, 255},
            {32, 28, 14, 255},
            {13, 12, 6, 255},
            {45, 40, 19, 255},
            {255, 246, 220, 255},
            {196, 178, 128, 255},
            {253, 224, 71, 255},
            {250, 204, 21, 255},
            {76, 67, 32, 255},
            {250, 204, 21, 255},    // accentPrimary (yellow)
            {251, 146, 60, 255},
            {161, 128, 8, 255},
            {250, 204, 21, 255},
            {76, 67, 32, 255}
        };
    }
    if (themeId == "ocean") {
        return ThemeColors{
            {11, 27, 51, 255},
            {15, 35, 64, 255},
            {5, 14, 30, 255},
            {22, 46, 80, 255},
            {234, 244, 255, 255},
            {143, 172, 208, 255},
            {125, 211, 252, 255},
            {56, 189, 248, 255},
            {42, 70, 108, 255},
            {56, 189, 248, 255},    // accentPrimary (sky)
            {45, 212, 191, 255},
            {3, 105, 161, 255},
            {45, 212, 191, 255},
            {42, 70, 108, 255}
        };
    }
    if (themeId == "sunset") {
        return ThemeColors{
            {38, 17, 32, 255},
            {48, 22, 40, 255},
            {20, 8, 17, 255},
            {64, 30, 53, 255},
            {255, 241, 236, 255},
            {212, 168, 175, 255},
            {253, 186, 116, 255},
            {251, 146, 60, 255},
            {96, 52, 74, 255},
            {251, 146, 60, 255},    // accentPrimary (orange)
            {244, 114, 182, 255},
            {157, 58, 72, 255},
            {251, 146, 60, 255},
            {96, 52, 74, 255}
        };
    }
    // "default": Plunder teal
    return ThemeColors{
        {10, 34, 40, 255},          // background
        {14, 42, 49, 255},          // backgroundAlt
        {4, 17, 21, 255},           // backgroundDark
        {21, 56, 64, 255},          // cardBackground
        {236, 248, 247, 255},       // textPrimary
        {140, 178, 180, 255},       // textSecondary
        {94, 234, 212, 255},        // textHighlight
        {45, 212, 191, 255},        // borderFocused
        {38, 78, 86, 255},          // borderUnfocused
        {45, 212, 191, 255},        // accentPrimary (teal)
        {56, 189, 248, 255},        // accentSecondary
        {17, 110, 100, 255},        // activeItem
        {45, 212, 191, 255},        // toggleOn
        {38, 78, 86, 255}           // toggleOff
    };
}
