#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

class MenuSystem;

class Screen {
public:
    virtual ~Screen() {}
    virtual void render(SDL_Renderer* renderer, TTF_Font* font) = 0;
    virtual void handleInput(const SDL_Event& e, MenuSystem& menuSystem) = 0;
};
