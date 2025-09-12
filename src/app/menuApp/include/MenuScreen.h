#pragma once
// Project headers
#include "../../Screen.h"
#include "MenuSystem.h"
#include "../../../utils/include/UiUtils.h"
#include "../../ControllerButtons.h"

// SDL
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

// STL
#include <vector>
#include <string>
#include <functional>
#include <cmath>

class MenuScreen : public Screen {
private:
    std::string title;
    std::vector<std::pair<std::string, std::function<void()>>> items;
    int selectedIndex = 0;
    // For smooth animation between selections
    float animSelectedIndex = 0.0f;
    Uint32 lastTick = 0;
    float animSpeed = 15.0f; // higher = snappier
public:
    MenuScreen(const std::string& title);
    void addItem(const std::string& label, std::function<void()> action);
    void clearItems();
    void render(SDL_Renderer* renderer, TTF_Font* font) override;
    void handleInput(const SDL_Event& e, MenuSystem& menuSystem) override;
};