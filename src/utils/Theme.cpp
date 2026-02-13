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
    
    if (themeId == "dark") {
        currentColors = getDarkTheme();
    } else if (themeId == "wasp") {
        currentColors = getWaspTheme();
    } else if (themeId == "ocean") {
        currentColors = getOceanTheme();
    } else if (themeId == "sunset") {
        currentColors = getSunsetTheme();
    } else {
        currentColors = getDefaultTheme();
    }
    
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

ThemeColors Theme::getDefaultTheme() const {
    return ThemeColors{
        // Background colors (teal theme - original)
        {24, 153, 165, 255},     // background
        {32, 170, 180, 255},     // backgroundAlt
        {20, 20, 40, 255},       // backgroundDark
        {32, 120, 140, 255},     // cardBackground
        
        // Text colors
        {255, 255, 255, 255},    // textPrimary
        {200, 200, 200, 255},    // textSecondary
        {50, 255, 150, 255},     // textHighlight (green)
        
        // UI element colors
        {255, 255, 255, 255},    // borderFocused
        {180, 180, 180, 255},    // borderUnfocused
        {80, 200, 255, 255},     // accentPrimary (cyan)
        {120, 200, 255, 255},    // accentSecondary
        {0, 180, 120, 255},      // activeItem (green)
        {0, 220, 180, 255},      // toggleOn
        {0, 120, 140, 255}       // toggleOff
    };
}

ThemeColors Theme::getDarkTheme() const {
    return ThemeColors{
        // Background colors (dark/charcoal)
        {30, 30, 35, 255},       // background
        {40, 40, 48, 255},       // backgroundAlt
        {15, 15, 20, 255},       // backgroundDark
        {50, 50, 60, 255},       // cardBackground
        
        // Text colors
        {240, 240, 245, 255},    // textPrimary
        {160, 160, 170, 255},    // textSecondary
        {100, 200, 255, 255},    // textHighlight (blue)
        
        // UI element colors
        {200, 200, 210, 255},    // borderFocused
        {100, 100, 110, 255},    // borderUnfocused
        {100, 150, 255, 255},    // accentPrimary (blue)
        {80, 130, 220, 255},     // accentSecondary
        {60, 100, 180, 255},     // activeItem
        {80, 180, 120, 255},     // toggleOn
        {60, 60, 70, 255}        // toggleOff
    };
}

ThemeColors Theme::getWaspTheme() const {
    return ThemeColors{
        // Background colors (yellow/black wasp)
        {40, 35, 10, 255},       // background (dark golden)
        {50, 45, 15, 255},       // backgroundAlt
        {20, 18, 5, 255},        // backgroundDark
        {70, 60, 20, 255},       // cardBackground
        
        // Text colors
        {255, 230, 150, 255},    // textPrimary (warm yellow)
        {180, 160, 100, 255},    // textSecondary
        {255, 220, 0, 255},      // textHighlight (bright yellow)
        
        // UI element colors
        {255, 220, 80, 255},     // borderFocused (yellow)
        {140, 120, 60, 255},     // borderUnfocused
        {255, 200, 0, 255},      // accentPrimary (golden yellow)
        {255, 180, 50, 255},     // accentSecondary
        {180, 150, 30, 255},     // activeItem
        {255, 200, 0, 255},      // toggleOn (bright yellow)
        {60, 50, 20, 255}        // toggleOff
    };
}

ThemeColors Theme::getOceanTheme() const {
    return ThemeColors{
        // Background colors (deep blue ocean)
        {20, 50, 90, 255},       // background
        {25, 60, 100, 255},      // backgroundAlt
        {10, 25, 45, 255},       // backgroundDark
        {30, 70, 110, 255},      // cardBackground
        
        // Text colors
        {230, 240, 255, 255},    // textPrimary
        {150, 180, 210, 255},    // textSecondary
        {100, 255, 200, 255},    // textHighlight (aqua)
        
        // UI element colors
        {200, 220, 255, 255},    // borderFocused
        {100, 130, 170, 255},    // borderUnfocused
        {60, 180, 220, 255},     // accentPrimary (cyan)
        {80, 160, 200, 255},     // accentSecondary
        {40, 140, 180, 255},     // activeItem
        {60, 200, 180, 255},     // toggleOn
        {40, 80, 120, 255}       // toggleOff
    };
}

ThemeColors Theme::getSunsetTheme() const {
    return ThemeColors{
        // Background colors (warm sunset)
        {80, 40, 60, 255},       // background
        {100, 50, 70, 255},      // backgroundAlt
        {40, 20, 30, 255},       // backgroundDark
        {120, 60, 80, 255},      // cardBackground
        
        // Text colors
        {255, 240, 230, 255},    // textPrimary
        {200, 180, 170, 255},    // textSecondary
        {255, 200, 100, 255},    // textHighlight (gold)
        
        // UI element colors
        {255, 220, 200, 255},    // borderFocused
        {180, 140, 130, 255},    // borderUnfocused
        {255, 150, 100, 255},    // accentPrimary (orange)
        {255, 180, 120, 255},    // accentSecondary
        {200, 100, 80, 255},     // activeItem
        {255, 180, 100, 255},    // toggleOn
        {100, 60, 70, 255}       // toggleOff
    };
}
