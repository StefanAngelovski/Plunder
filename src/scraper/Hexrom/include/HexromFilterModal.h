

#pragma once
#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "../../../model/ListItem.h"
#include "../../../model/PaginationInfo.h"
#include "../../../app/screenListing/include/OnscreenKeyboard.h"
#include "../../../utils/include/UiUtils.h"
#include "../../../utils/include/StringUtils.h" // for splitUtf8ToCodepoints
#include "../../../app/ControllerButtons.h"

class HexromFilterModal {
public:
    HexromFilterModal();
    void render(SDL_Renderer* renderer, TTF_Font* font, int x, int y, int width, int height);
    void handleInput(const SDL_Event& e);
    void setOnFilter(std::function<void(const std::string& search, int region, int sort, int order, int genre)> cb);
    bool isVisible() const;
    void showModal(bool show);

    // Modal state
    bool show = false;
    int filterDropdownFocus = 0;
    bool filterDropdownPulldown = false;
    int filterDropdownPulldownIndex = 0;
    std::vector<std::string> filterDropdownOptions;
    std::string filterSearchText;
    int filterRegionIndex = 0;
    int filterSortIndex = 0;
    int filterOrderIndex = 0;
    int filterGenreIndex = 0;
    // Option lists
    const std::vector<std::string> regionOptions = {"Select Region", "USA", "English(USA)", "English", "Europe", "USA Europe", "Germany", "Italy", "France", "Japan", "Korea", "USA & Japan", "Global", "English (USA)"};
    const std::vector<std::string> sortOptions = {"Date", "Title", "Most Popular"};
    const std::vector<std::string> orderOptions = {"Ascending", "Descending"};
    const std::vector<std::string> genreOptions = {
        "Select Genre",
        "Role Playing",
        "Adventure, Role Playing",
        "Fighting",
        "Action",
        "Adventure",
        "Action, Simulation",
        "Adventure, Platform",
        "Hack, Role Playing",
        "Misc",
        "Platform",
        "Action, Racing",
        "Card Game",
        "Action, Adventure",
        "Action, Fighting",
        "Strategy, Turn Based Tactics",
        "Sports",
        "Role Playing, Shooter",
        "Beat Em Up",
        "Board Game",
        "Action, Beat Em Up",
        "Racing",
        "Role Playing, Strategy",
        "Turn Based Tactics",
        "Shooter",
        "Action, Shooter",
        "Role Playing, Simulation",
        "Action, Role Playing",
        "Action, Platform",
        "Compilation",
        "Action, Adventure, Platform",
        "Action, Pinball",
        "Puzzle",
        "Action, Fighting, Role Playing",
        "Adventure, Fighting",
        "Platform, Shooter",
        "Strategy",
        "Card Game, Compilation",
        "Simulation",
        "Compilation, Misc",
        "Sports, Simulation",
        "Racing, Simulation",
        "Action, Puzzle",
        "Board Game, Card Game",
        "Adventure, Beat Em Up",
        "Action, Puzzle, Simulation, Strategy",
        "Music, Puzzle, Simulation, Strategy",
        "Card Game, Simulation",
        "Racing, Sports",
        "Card Game, Puzzle",
        "Sports, Strategy",
        "Puzzle, Simulation, Strategy",
        "Puzzle, Shooter",
        "Action, Music",
        "Simulation, Strategy",
        "Music",
        "Board Game, Card Game, Misc",
        "Educational",
        "Puzzle, Sports",
        "Board Game, Misc",
        "Role-Playing",
        "RPG"
    };
    // On-screen keyboard
    bool showOnscreenKeyboard = false;
    OnscreenKeyboard onscreenKeyboard;

private:
    bool visible = false;
    std::function<void(const std::string&, int, int, int, int)> onFilter;
};
