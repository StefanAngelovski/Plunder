#pragma once
#include <string>
#include <SDL2/SDL.h>

// Theme color definitions
struct ThemeColors {
    // Background colors
    SDL_Color background;           // Main background (carousel, settings, etc.)
    SDL_Color backgroundAlt;        // Alternative background (loading screen, etc.)
    SDL_Color backgroundDark;       // Dark background (no internet screen)
    SDL_Color cardBackground;       // Card/tile background in lists
    
    // Text colors
    SDL_Color textPrimary;          // Primary text color
    SDL_Color textSecondary;        // Secondary/hint text
    SDL_Color textHighlight;        // Highlighted text (update notification, etc.)
    
    // UI element colors
    SDL_Color borderFocused;        // Focused item border
    SDL_Color borderUnfocused;      // Unfocused item border
    SDL_Color accentPrimary;        // Primary accent color (buttons, highlights)
    SDL_Color accentSecondary;      // Secondary accent
    SDL_Color activeItem;           // Active/selected item background
    SDL_Color toggleOn;             // Toggle enabled state
    SDL_Color toggleOff;            // Toggle disabled state
};

class Theme {
public:
    // Get the singleton instance
    static Theme& getInstance();
    
    // Load theme from config (called on startup and when theme changes)
    void loadTheme(const std::string& themeId);
    
    // Get current theme ID
    std::string getCurrentThemeId() const { return currentThemeId; }
    
    // Get current theme colors
    const ThemeColors& colors() const { return currentColors; }
    
    // Convenience color getters
    SDL_Color background() const { return currentColors.background; }
    SDL_Color backgroundAlt() const { return currentColors.backgroundAlt; }
    SDL_Color backgroundDark() const { return currentColors.backgroundDark; }
    SDL_Color cardBackground() const { return currentColors.cardBackground; }
    SDL_Color textPrimary() const { return currentColors.textPrimary; }
    SDL_Color textSecondary() const { return currentColors.textSecondary; }
    SDL_Color textHighlight() const { return currentColors.textHighlight; }
    SDL_Color borderFocused() const { return currentColors.borderFocused; }
    SDL_Color borderUnfocused() const { return currentColors.borderUnfocused; }
    SDL_Color accentPrimary() const { return currentColors.accentPrimary; }
    SDL_Color accentSecondary() const { return currentColors.accentSecondary; }
    SDL_Color activeItem() const { return currentColors.activeItem; }
    SDL_Color toggleOn() const { return currentColors.toggleOn; }
    SDL_Color toggleOff() const { return currentColors.toggleOff; }
    
    // Apply background color to renderer
    void applyBackground(SDL_Renderer* renderer) const;
    void applyBackgroundAlt(SDL_Renderer* renderer) const;
    void applyBackgroundDark(SDL_Renderer* renderer) const;

private:
    Theme();
    ~Theme() = default;
    Theme(const Theme&) = delete;
    Theme& operator=(const Theme&) = delete;
    
    std::string currentThemeId;
    ThemeColors currentColors;
    
    // Theme presets
    ThemeColors getDefaultTheme() const;
    ThemeColors getDarkTheme() const;
    ThemeColors getWaspTheme() const;
    ThemeColors getOceanTheme() const;
    ThemeColors getSunsetTheme() const;
};
