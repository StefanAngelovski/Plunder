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
#include "../../../utils/include/UpdateChecker.h"
#include "../../ControllerButtons.h"

// SDL
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_image.h>

class MenuSystem;

// Optional per-item decoration supplied by subclasses (Settings, Themes).
struct CarouselItemDecor {
    std::string sublabel;               // small line under the label
    std::string badge;                  // pill in the card's top-right corner
    bool badgeOn = false;               // accent-coloured badge when true
    double iconAngle = 0.0;             // icon rotation in degrees
    float flash = -1.0f;                // 0..1 sweep highlight progress, <0 = none
    std::vector<SDL_Color> swatches;    // theme preview: bg, card, accent, text
};

class CarouselMenuScreen : public Screen {

private:
    int selectedIndex = 0;

    // Animation state
    float animSelectedIndex = 0.0f; // interpolated position
    float animVelocity = 0.0f;      // for spring effect
    Uint32 lastTick = 0;            // timing
    float springK = 280.0f;         // stiffness
    float damping = 36.0f;          // damping tuned to prevent excessive overshoot

    std::vector<CarouselMenuItem> items;
    Mix_Chunk* moveSound = nullptr;

    std::string title;
    std::string subtitle;
    bool showBackHint = true;

    // Image caching
    std::map<std::string, SDL_Texture*> imageCache;
    void clearImageCache();
    SDL_Texture* getImage(SDL_Renderer* renderer, const std::string& path);

    void renderCard(SDL_Renderer* renderer, int i, const SDL_Rect& rect, float focusT, Uint8 alpha);
    void renderSwatches(SDL_Renderer* renderer, const SDL_Rect& area, const std::vector<SDL_Color>& sw, Uint8 alpha);

protected:
    virtual CarouselItemDecor decorate(int i) { (void)i; return CarouselItemDecor{}; }

public:
    CarouselMenuScreen(const std::string& title);
    ~CarouselMenuScreen();

    void addItem(const std::string& label, const std::string& imagePath, std::function<void()> action);
    void clearItems();
    void setSelectedIndex(int index);
    int getSelectedIndex() const { return selectedIndex; }

    void setTitle(const std::string& t) { title = t; }
    void setSubtitle(const std::string& s) { subtitle = s; }
    void setShowBackHint(bool show) { showBackHint = show; }

    // Screen interface
    void render(SDL_Renderer* renderer, TTF_Font* font) override;
    void handleInput(const SDL_Event& e, MenuSystem& menuSystem) override;
    bool update();

    // Getter for items
    const std::vector<CarouselMenuItem>& getItems() const { return items; }
};
