#pragma once
#include <string>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include "../../../utils/include/UiUtils.h"
#include "../../../app/ControllerButtons.h"
#include <functional>
#include "../../../model/ListItem.h"
#include "../../../model/PaginationInfo.h"
#include "../../../app/screenListing/include/OnscreenKeyboard.h"


class GamulatorFilterModal {
public:
    GamulatorFilterModal();
    void render(SDL_Renderer* renderer, TTF_Font* font, int x, int y, int width, int height);
    void handleInput(const SDL_Event& e);
    void setOnFilter(std::function<void(const std::string& search, int region, int sort, int order, int genre)> cb);
    bool isVisible() const;
    void showModal(bool show);

    // Modal state
    bool visible = false;
    int focusIndex = 0; // 0 = search box, 1 = filter button
    std::function<void(const std::string&, int, int, int, int)> onFilter;
    std::string searchText;
    bool showOnscreenKeyboard = false;
    OnscreenKeyboard onscreenKeyboard;
};
