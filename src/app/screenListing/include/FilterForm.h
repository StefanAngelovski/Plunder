#pragma once
#include <functional>
#include <string>
#include <vector>

#include <SDL2/SDL.h>

#include "OnscreenKeyboard.h"
#include "../../../scraper/engine/SiteDefinition.h"

// The "Search & filter" dialog on a game list (Y). Built from a site's
// FilterRules: a search box (on-screen keyboard) plus one dropdown per field.
class FilterForm {
public:
    using ApplyFn = std::function<void(const std::string& search, const std::vector<int>& choices)>;

    FilterForm(std::vector<FilterField> fields, ApplyFn onApply);

    bool visible() const { return visible_; }
    void show(bool show);
    void handleInput(const SDL_Event& e);
    void render(SDL_Renderer* renderer);

private:
    int rowCount() const { return 1 + (int)fields_.size(); }  // search + dropdowns
    int applyIndex() const { return rowCount(); }

    std::vector<FilterField> fields_;
    ApplyFn onApply_;
    bool visible_ = false;

    std::string search_;
    std::vector<int> choices_;
    int focus_ = 0;               // 0 search, 1..n dropdowns, n+1 apply
    bool pulldown_ = false;
    int pullIndex_ = 0;
    bool keyboardOpen_ = false;
    OnscreenKeyboard keyboard_;
};
