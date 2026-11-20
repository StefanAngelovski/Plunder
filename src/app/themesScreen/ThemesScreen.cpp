#include "include/ThemesScreen.h"
#include "../../utils/include/Theme.h"
#include <json-c/json.h>
#include <sys/stat.h>
#include <iostream>

#define CONFIG_PATH "/mnt/SDCARD/Apps/Plunder/config.json"

ThemesScreen::ThemesScreen(std::function<void()> onBack)
    : CarouselMenuScreen("Themes"), onBackCallback(onBack) {
    setSubtitle("Settings");
    currentThemeId = getCurrentTheme();
    initializeThemes();
    updateMenuItems();
}

// Initialize available themes
void ThemesScreen::initializeThemes() {
    availableThemes.clear();
    
    // Default theme
    availableThemes.push_back({
        "default",
        "Plunder",
        "images/settingsmenu/themes.png"
    });
    
    // Dark theme
    availableThemes.push_back({
        "dark",
        "Dark",
        "images/settingsmenu/themes.png"
    });
    
    // Wasp theme
    availableThemes.push_back({
        "wasp",
        "Wasp",
        "images/settingsmenu/themes.png"
    });
    
    // Ocean theme
    availableThemes.push_back({
        "ocean",
        "Ocean",
        "images/settingsmenu/themes.png"
    });
    
    // Sunset theme
    availableThemes.push_back({
        "sunset",
        "Sunset",
        "images/settingsmenu/themes.png"
    });
}

// Update the carousel menu items
void ThemesScreen::updateMenuItems() {
    int prevIndex = getSelectedIndex(); // Preserve current selection
    clearItems();
    
    // Add theme items
    for (const auto& theme : availableThemes) {
        std::string label = theme.name;
        
        addItem(label, theme.imagePath, [this, themeId = theme.id]() {
            pendingSelectedIndex = getSelectedIndex(); // Save position for deferred refresh
            currentThemeId = themeId;
            setCurrentTheme(themeId);
            Theme::getInstance().loadTheme(themeId); // Apply theme immediately
            needsRefresh = true; // Defer menu rebuild to next render
            std::cerr << "[ThemesScreen] Theme changed to: " << themeId << std::endl;
        });
    }
    
    // Add Back button
    addItem("Back", "images/settingsmenu/back.png", [this]() {
        if (onBackCallback) onBackCallback();
    });
    
    // Restore selection if valid
    if (prevIndex > 0 && prevIndex < (int)getItems().size()) {
        setSelectedIndex(prevIndex);
    }
}

// Render the themes screen
void ThemesScreen::render(SDL_Renderer* renderer, TTF_Font* font) {
    // Handle deferred refresh (to avoid destroying lambda while executing)
    if (needsRefresh) {
        needsRefresh = false;
        int savedIndex = pendingSelectedIndex;
        updateMenuItems();
        if (savedIndex >= 0 && savedIndex < (int)getItems().size()) {
            setSelectedIndex(savedIndex);
        }
        pendingSelectedIndex = -1;
    }
    CarouselMenuScreen::render(renderer, font);
}

// Custom rendering for theme items
CarouselItemDecor ThemesScreen::decorate(int i) {
    CarouselItemDecor d;
    if (i < 0 || i >= (int)availableThemes.size()) return d;  // Back button
    ThemeColors c = Theme::preset(availableThemes[i].id);
    d.swatches = {c.background, c.cardBackground, c.accentPrimary, c.textPrimary};
    if (availableThemes[i].id == currentThemeId) {
        d.badge = "Active";
        d.badgeOn = true;
    }
    return d;
}

// Read the current theme from config file
std::string ThemesScreen::getCurrentTheme() {
    struct stat st;
    if (stat(CONFIG_PATH, &st) != 0) return "default";
    
    FILE* f = fopen(CONFIG_PATH, "r");
    if (!f) return "default";
    
    struct json_object* jobj = json_object_from_file(CONFIG_PATH);
    if (!jobj) { 
        fclose(f); 
        return "default"; 
    }
    
    struct json_object* val = nullptr;
    std::string result = "default";
    if (json_object_object_get_ex(jobj, "theme", &val)) {
        const char* themeStr = json_object_get_string(val);
        if (themeStr) result = themeStr;
    }
    
    json_object_put(jobj);
    fclose(f);
    return result;
}

// Write the current theme to config file
void ThemesScreen::setCurrentTheme(const std::string& themeId) {
    struct json_object* jobj = nullptr;
    struct stat st;
    
    if (stat(CONFIG_PATH, &st) == 0) {
        jobj = json_object_from_file(CONFIG_PATH);
    }
    if (!jobj) jobj = json_object_new_object();
    
    struct json_object* val = json_object_new_string(themeId.c_str());
    json_object_object_add(jobj, "theme", val);
    json_object_to_file(CONFIG_PATH, jobj);
    json_object_put(jobj);
}
