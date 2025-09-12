#pragma once

#include <string>

// SDL
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

// Other imports
#include "../../Screen.h"
#include "../../../utils/include/UiUtils.h"

class MenuSystem;

class LoadingScreen : public Screen {
private:
    std::string message;
    int frameCounter = 0;
    int animationFrame = 0;

public:
    LoadingScreen(const std::string& message);
    
    void render(SDL_Renderer* renderer, TTF_Font* font) override;
    void handleInput(const SDL_Event& e, MenuSystem& menuSystem) override;
};
