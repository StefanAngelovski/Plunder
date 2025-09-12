#pragma once
#include <vector>
#include <memory>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "../../Screen.h"

class MenuSystem {
private:
    std::vector<std::shared_ptr<Screen>> screenStack;
    SDL_Renderer* renderer;
    TTF_Font* font;
public:
    MenuSystem(SDL_Renderer* r, TTF_Font* f);
    void pushScreen(std::shared_ptr<Screen> screen);
    void popScreen();
    void replaceScreen(std::shared_ptr<Screen> screen);
    void render();
    void handleInput(const SDL_Event& e);
    bool isRunning() const;
    SDL_Renderer* getRenderer() const;
    std::shared_ptr<Screen> getCurrentScreen() const {
        if (!screenStack.empty()) return screenStack.back();
        return nullptr;
    }
};