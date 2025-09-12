#pragma once
#include <string>
#include <vector>
#include <algorithm>

// SDL
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

// Other imports
#include "../../../utils/include/UiUtils.h"
#include "../../ControllerButtons.h"


// OnscreenKeyboard handles state, rendering, and input for the virtual keyboard
class OnscreenKeyboard {
public:
    enum class Mode { Letters, Special };

    OnscreenKeyboard();
    void render(SDL_Renderer* renderer, TTF_Font* font, int x, int y, int w, int h);
    bool handleInput(const SDL_Event& e, std::string& text, bool& showOnscreenKeyboard);
    void setCaps(bool caps);
    void setMode(Mode mode);
    Mode getMode() const;
    void resetPosition();
    void setRows(const std::vector<std::string>& rows);
    const std::vector<std::string>& getRows() const;
    int getRow() const;
    int getCol() const;

private:
    Mode mode;
    bool caps;
    int row, col;
    std::vector<std::string> rowsLetters;
    std::vector<std::string> rowsSpecial;
    std::vector<std::string> currentRows;
    void updateRows();
};