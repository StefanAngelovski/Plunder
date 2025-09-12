#include "include/SettingsScreen.h"

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

// Custom rendering for each carousel item
void SettingsScreen::renderItem(SDL_Renderer* renderer, TTF_Font* font, int i, int x, int y, int w, int h, bool focused) {
    const auto& items = getItems();
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