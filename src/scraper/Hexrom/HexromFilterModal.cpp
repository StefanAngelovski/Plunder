#include "include/HexromFilterModal.h"

HexromFilterModal::HexromFilterModal() {
    filterDropdownOptions = {"Search", "Region", "Sort", "Order", "Genre"};
}

void HexromFilterModal::render(SDL_Renderer* renderer, TTF_Font* font, int x, int y, int width, int height) {
    // --- Filter Modal UI (migrated from ListScreen) ---
    int modalW = width, modalH = height;
    int modalX = x, modalY = y;
    // Teal blue style
    SDL_Color modalBg = {36, 120, 160, 240};      // Teal blue
    SDL_Color modalBorder = {80, 200, 255, 255};  // Bright teal
    SDL_Color modalShadow = {0, 0, 0, 100};       // Shadow

    // Draw drop shadow
    SDL_Rect shadowRect = { modalX + 8, modalY + 8, modalW, modalH };
    SDL_SetRenderDrawColor(renderer, modalShadow.r, modalShadow.g, modalShadow.b, modalShadow.a);
    SDL_RenderFillRect(renderer, &shadowRect);

    // Draw modal background
    SDL_Rect modalRect = {modalX, modalY, modalW, modalH};
    SDL_SetRenderDrawColor(renderer, modalBg.r, modalBg.g, modalBg.b, modalBg.a);
    SDL_RenderFillRect(renderer, &modalRect);

    // Draw modal border
    SDL_SetRenderDrawColor(renderer, modalBorder.r, modalBorder.g, modalBorder.b, modalBorder.a);
    SDL_RenderDrawRect(renderer, &modalRect);

    UiUtils::RenderTextCentered(renderer, font, "Filter/Search ROMs", 1280/2, modalY + 40, UiUtils::Color(255,255,255));
    int yPos = modalY + 100;
    // Dropdown focus highlight
    int dropdownCount = 5; // Search, Region, Sort, Order, Genre
    for (int i = 0; i < dropdownCount; ++i) {
        if (i == filterDropdownFocus && !filterDropdownPulldown) {
            SDL_Rect focusRect = {modalX + 40, yPos-8 + i*60, 700, 44};
            SDL_SetRenderDrawColor(renderer, 60, 120, 220, 180);
            SDL_RenderDrawRect(renderer, &focusRect);
        }
    }
    // --- Centered search textbox background (vertically and horizontally) ---
    int searchBoxW = 400, searchBoxH = 36;
    int labelW = 0, labelH = 0;
    TTF_SizeText(font, "Search:", &labelW, &labelH);
    int rowHeight = std::max(searchBoxH, labelH);
    int rowY = yPos - 8 + (44 - rowHeight) / 2; // 44 is the focusRect height
    int searchBoxX = modalX + (modalW - searchBoxW) / 2;
    int searchBoxY = rowY + (rowHeight - searchBoxH) / 2;
    int labelX = modalX + 60; // Align with other labels
    int labelY = rowY + (rowHeight - labelH) / 2;
    UiUtils::RenderText(renderer, font, "Search:", labelX, labelY, UiUtils::Color(220,220,220));
    SDL_Rect searchBox = {searchBoxX, searchBoxY, searchBoxW, searchBoxH};
    SDL_SetRenderDrawColor(renderer, 60, 60, 80, 220);
    SDL_RenderFillRect(renderer, &searchBox);
    // --- Draw the search text using natural width for each character ---
    int textX = searchBox.x + 8;
    int textY = searchBox.y + (searchBox.h - 24) / 2; // 24 is an estimated char height
    auto codepoints = splitUtf8ToCodepoints(filterSearchText);
    int cursorX = textX;
    const int charSpacing = 4; // Extra space between each character
    for (int i = 0; i < (int)codepoints.size(); ++i) {
        int w = 0, h = 0;
        TTF_SizeText(font, codepoints[i].c_str(), &w, &h);
        UiUtils::RenderText(renderer, font, codepoints[i], cursorX, textY, UiUtils::Color(255,255,255));
        cursorX += w + charSpacing;
    }
    // --- Draw blinking cursor/line at the next slot ---
    if (filterDropdownFocus == 0 && !filterDropdownPulldown) {
        int cursorY1 = textY;
        int cursorY2 = textY + 24; // Match estimated char height
        if ((SDL_GetTicks() / 500) % 2 == 0) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 80, 255);
            SDL_RenderDrawLine(renderer, cursorX, cursorY1, cursorX, cursorY2);
            SDL_RenderDrawLine(renderer, cursorX+1, cursorY1, cursorX+1, cursorY2); // 2px wide
        }
    }
    yPos += 60;
    // Region dropdown
    UiUtils::RenderText(renderer, font, "Region:", modalX + 60, yPos, UiUtils::Color(220,220,220));
    UiUtils::RenderText(renderer, font, regionOptions[filterRegionIndex], modalX + 200, yPos, UiUtils::Color(255,255,180));
    yPos += 60;
    // Sort by dropdown
    UiUtils::RenderText(renderer, font, "Sort by:", modalX + 60, yPos, UiUtils::Color(220,220,220));
    UiUtils::RenderText(renderer, font, sortOptions[filterSortIndex], modalX + 200, yPos, UiUtils::Color(255,255,180));
    yPos += 60;
    // Order dropdown
    UiUtils::RenderText(renderer, font, "Order:", modalX + 60, yPos, UiUtils::Color(220,220,220));
    UiUtils::RenderText(renderer, font, orderOptions[filterOrderIndex], modalX + 200, yPos, UiUtils::Color(255,255,180));
    yPos += 60;
    // Genre dropdown
    UiUtils::RenderText(renderer, font, "Genre:", modalX + 60, yPos, UiUtils::Color(220,220,220));
    UiUtils::RenderText(renderer, font, genreOptions[filterGenreIndex], modalX + 200, yPos, UiUtils::Color(255,255,180));
    yPos += 80;
    // Filter button focus
    if (filterDropdownFocus == 5 && !filterDropdownPulldown) {
        SDL_Rect btnFocus = {modalX + 200 - 8, yPos - 8, 200 + 16, 48 + 16};
        SDL_SetRenderDrawColor(renderer, 60, 120, 220, 180);
        SDL_RenderDrawRect(renderer, &btnFocus);
    }
    // Filter button
    SDL_Rect filterBtn = {modalX + 200, yPos, 200, 48};
    SDL_SetRenderDrawColor(renderer, 60, 120, 220, 160);
    SDL_RenderFillRect(renderer, &filterBtn);
    UiUtils::RenderTextCenteredInBox(renderer, font, "Filter", filterBtn, UiUtils::Color(255,255,255));
    // Pulldown for dropdowns
    if (filterDropdownPulldown) {
        int maxVisible = 6;
        int total = filterDropdownOptions.size();
        int first = std::min(std::max(0, filterDropdownPulldownIndex - maxVisible/2), std::max(0, total - maxVisible));
        int visibleCount = std::min(maxVisible, total);
        int pulldownY = modalY + 100 + filterDropdownFocus*60 + 44;
        int pulldownX = modalX + 200;
        int pulldownW = 400;
        int pulldownH = 44 * visibleCount;
        SDL_Color dropdownBg = {60, 170, 220, 245}; // Lighter blue
        SDL_Color dropdownBorder = {120, 220, 255, 255}; // Bright border
        SDL_Rect pdRect = {pulldownX, pulldownY, pulldownW, pulldownH};
        SDL_SetRenderDrawColor(renderer, dropdownBg.r, dropdownBg.g, dropdownBg.b, dropdownBg.a);
        SDL_RenderFillRect(renderer, &pdRect);
        SDL_SetRenderDrawColor(renderer, dropdownBorder.r, dropdownBorder.g, dropdownBorder.b, dropdownBorder.a);
        SDL_RenderDrawRect(renderer, &pdRect);
        for (int i = 0; i < visibleCount; ++i) {
            int optIdx = first + i;
            UiUtils::Color c = (optIdx == filterDropdownPulldownIndex) ? UiUtils::Color(255,255,80) : UiUtils::Color(220,220,220);
            UiUtils::RenderText(renderer, font, filterDropdownOptions[optIdx], pulldownX + 12, pulldownY + i*44 + 8, c);
        }
    }
    // --- On-screen keyboard rendering ---
    if (showOnscreenKeyboard && filterDropdownFocus == 0 && !filterDropdownPulldown) {
        onscreenKeyboard.render(renderer, font, modalX, modalY + 160, modalW, modalH - 160);
    }
}

void HexromFilterModal::handleInput(const SDL_Event& e) {
    // --- BLOCK ALL INPUT IF ONSCREEN KEYBOARD IS OPEN ---
    if (showOnscreenKeyboard) {
        if (filterDropdownFocus == 0 && !filterDropdownPulldown) {
            onscreenKeyboard.handleInput(e, filterSearchText, showOnscreenKeyboard);
        }
        return;
    }
    // --- Filter Modal input handling ---
    // Block all background navigation when modal is open
    if (e.type == SDL_CONTROLLERBUTTONDOWN) {
        bool isA = (e.cbutton.button == BUTTON_A);
        bool isB = (e.cbutton.button == BUTTON_B);
        // D-pad up/down to move focus (only handle once)
        if (!filterDropdownPulldown) {
            if (e.cbutton.button == BUTTON_DPAD_DOWN) {
                filterDropdownFocus = (filterDropdownFocus + 1) % 6;
                return;
            } else if (e.cbutton.button == BUTTON_DPAD_UP) {
                filterDropdownFocus = (filterDropdownFocus + 5) % 6;
                return;
            }
        }
        // A: open dropdowns/keyboard or filter
        if (!filterDropdownPulldown && isA) {
            if (filterDropdownFocus == 0) { // Search: open keyboard
                showOnscreenKeyboard = true;
                return;
            } else if (filterDropdownFocus >= 1 && filterDropdownFocus <= 4) { // Dropdowns
                if (filterDropdownFocus == 1) { filterDropdownOptions = regionOptions; filterDropdownPulldown = true; filterDropdownPulldownIndex = filterRegionIndex; }
                else if (filterDropdownFocus == 2) { filterDropdownOptions = sortOptions; filterDropdownPulldown = true; filterDropdownPulldownIndex = filterSortIndex; }
                else if (filterDropdownFocus == 3) { filterDropdownOptions = orderOptions; filterDropdownPulldown = true; filterDropdownPulldownIndex = filterOrderIndex; }
                else if (filterDropdownFocus == 4) { filterDropdownOptions = genreOptions; filterDropdownPulldown = true; filterDropdownPulldownIndex = filterGenreIndex; }
                return;
            } else if (filterDropdownFocus == 5) {
                // Build the URL and print it
                if (onFilter) onFilter(filterSearchText, filterRegionIndex, filterSortIndex, filterOrderIndex, filterGenreIndex);
                showModal(false);
                return;
            }
        }
        // B: always close modal if not in dropdown or keyboard
        if (!filterDropdownPulldown && isB) {
            showModal(false);
            return;
        }
        // Pulldown navigation
        bool isPulldownA = (e.cbutton.button == BUTTON_A);
        bool isPulldownB = (e.cbutton.button == BUTTON_B);
        if (filterDropdownPulldown && e.cbutton.button == BUTTON_DPAD_DOWN) {
            filterDropdownPulldownIndex = (filterDropdownPulldownIndex + 1) % filterDropdownOptions.size();
            return;
        }
        if (filterDropdownPulldown && e.cbutton.button == BUTTON_DPAD_UP) {
            filterDropdownPulldownIndex = (filterDropdownPulldownIndex + filterDropdownOptions.size() - 1) % filterDropdownOptions.size();
            return;
        }
        if (filterDropdownPulldown && isPulldownA) {
            // Select option
            if (filterDropdownFocus == 1) filterRegionIndex = filterDropdownPulldownIndex;
            else if (filterDropdownFocus == 2) filterSortIndex = filterDropdownPulldownIndex;
            else if (filterDropdownFocus == 3) filterOrderIndex = filterDropdownPulldownIndex;
            else if (filterDropdownFocus == 4) filterGenreIndex = filterDropdownPulldownIndex;
            filterDropdownPulldown = false;
            return;
        }
        if (filterDropdownPulldown && isPulldownB) {
            filterDropdownPulldown = false;
            // Only close dropdown, not modal
            return;
        }
    }
}

// --- Implementation for required methods ---
void HexromFilterModal::setOnFilter(std::function<void(const std::string& search, int region, int sort, int order, int genre)> cb) {
    onFilter = cb;
}

bool HexromFilterModal::isVisible() const {
    return visible;
}

void HexromFilterModal::showModal(bool show_) {
    visible = show_;
    show = show_;
}