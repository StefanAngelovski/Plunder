#pragma once

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>

// Other imports
#include "../../Screen.h"
#include "CarouselMenuTypes.h"
#include "../../menuApp/include/MenuSystem.h"
#include "../../../utils/include/UiUtils.h"
#include "../../ControllerButtons.h"

// SDL
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_image.h>

class MenuSystem;

class CarouselMenuScreen : public Screen {

private:
    int selectedIndex = 0;

    // Animation state
    float animSelectedIndex = 0.0f; // interpolated position
    float animVelocity = 0.0f;      // for spring effect
    Uint32 lastTick = 0;            // timing
    float springK = 70.0f;          // higher stiffness for faster convergence
    float damping = 18.0f;          // damping tuned to prevent excessive overshoot

    std::vector<CarouselMenuItem> items;
    Mix_Chunk* moveSound = nullptr;
    
    // Image caching
    std::map<std::string, SDL_Texture*> imageCache;
    SDL_Renderer* lastRenderer = nullptr;
    void clearImageCache();

protected:
    virtual void renderItem(SDL_Renderer* renderer, TTF_Font* font, int i, int x, int y, int w, int h, bool focused);

public:
    CarouselMenuScreen(const std::string& title);
    ~CarouselMenuScreen();
    
    void addItem(const std::string& label, const std::string& imagePath, std::function<void()> action);
    void clearItems();
    
    // Screen interface
    void render(SDL_Renderer* renderer, TTF_Font* font) override;
    void handleInput(const SDL_Event& e, MenuSystem& menuSystem) override;
    bool update();

    // Getter for items
    const std::vector<CarouselMenuItem>& getItems() const { return items; }
};