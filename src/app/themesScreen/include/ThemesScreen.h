#pragma once
#include <functional>
#include <vector>
#include <string>

#include "../../carousel/include/CarouselMenuScreen.h"

// Represents a theme option
struct ThemeOption {
    std::string id;       // Internal identifier
    std::string name;     // Display name
    std::string imagePath; // Path to theme preview image
};

class ThemesScreen : public CarouselMenuScreen {
public:
    // Construct the themes screen with a callback for the Back button
    ThemesScreen(std::function<void()> onBack = nullptr);
    static std::string getCurrentTheme();
    static void setCurrentTheme(const std::string& themeId);
    // UI rendering
    void render(SDL_Renderer* renderer, TTF_Font* font) override;

protected:
    // Custom rendering for each carousel item
    void renderItem(SDL_Renderer* renderer, TTF_Font* font, int i, int x, int y, int w, int h, bool focused) override;

private:
    std::function<void()> onBackCallback;
    std::vector<ThemeOption> availableThemes;
    std::string currentThemeId;
    bool needsRefresh = false;
    int pendingSelectedIndex = -1;
    
    // Setup theme options
    void initializeThemes();
    void updateMenuItems();
};
