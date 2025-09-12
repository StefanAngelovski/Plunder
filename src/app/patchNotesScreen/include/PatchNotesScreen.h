#pragma once
#include <string>
#include <vector>
#include <algorithm>

// SDL
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

// Other imports
#include "../../Screen.h"
#include "../../menuApp/include/MenuSystem.h"
#include "../../../utils/include/UiUtils.h"
#include <SDL2/SDL_gamecontroller.h>
#include "../../ControllerButtons.h"

class MenuSystem;

class PatchNotesScreen : public Screen {
private:
    std::vector<std::string> notes;
    std::vector<std::string> wrappedLines;
    int scrollOffset = 0;
    int maxVisibleLines = 18; // Maximum visible lines on screen (approximate for 720p)
    
    // Analog stick handling for continuous scrolling
    int lastAxisValue = 0;
    Uint32 axisHeldStart = 0;
    Uint32 lastAxisTime = 0;

public:
    PatchNotesScreen();
    
    void setNotes(const std::vector<std::string>& notes);
    void render(SDL_Renderer* renderer, TTF_Font* font) override;
    void handleInput(const SDL_Event& e, MenuSystem& menuSystem) override;
    void update();
};
