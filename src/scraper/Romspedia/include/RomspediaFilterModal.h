#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <functional>
#include <string>
#include <algorithm>
#include "../../../app/screenListing/include/OnscreenKeyboard.h"
#include "../../../utils/include/UiUtils.h"
#include "../../../app/ControllerButtons.h"

class RomspediaFilterModal {
public:
    RomspediaFilterModal();
    ~RomspediaFilterModal();

    void render(SDL_Renderer*, TTF_Font*, int, int, int, int);
    void setOnFilter(std::function<void(const std::string&, int, int, int, int)>);
    bool isVisible() const;
    void showModal(bool);
    void handleInput(const SDL_Event&);

    // Romspedia only uses search field
    std::string getSearch() const;
private:
    bool visible = false;
    std::string search;
    std::function<void(const std::string&, int, int, int, int)> onFilter;
    int focusIndex = 0;
    bool showOnscreenKeyboard = false;
    OnscreenKeyboard onscreenKeyboard;
};
