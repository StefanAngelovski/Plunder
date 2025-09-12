#include "include/GamulatorFilterModal.h"

GamulatorFilterModal::GamulatorFilterModal() {}

void GamulatorFilterModal::render(SDL_Renderer* renderer, TTF_Font* font, int x, int y, int width, int height) {
    // --- Filter Modal UI (Hexrom style, no dropdowns) ---
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

    // Title (use UiUtils for consistency)
    UiUtils::RenderTextCentered(renderer, font, "Filter/Search ROMs", 1280/2, modalY + 40, UiUtils::Color(255,255,255));

    int yPos = modalY + 100;
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
    // Highlight if focused
    if (focusIndex == 0 && !showOnscreenKeyboard) {
        SDL_Rect focusRect = {modalX + 40, rowY, 700, 44};
        SDL_SetRenderDrawColor(renderer, 60, 120, 220, 180);
        SDL_RenderDrawRect(renderer, &focusRect);
    }
    SDL_SetRenderDrawColor(renderer, 60, 60, 80, 220);
    SDL_RenderFillRect(renderer, &searchBox);
    // Draw search text
    UiUtils::RenderText(renderer, font, searchText, searchBox.x + 8, searchBox.y + (searchBox.h - 24) / 2, UiUtils::Color(255,255,255));
    // Draw blinking cursor if focused
    if (focusIndex == 0 && !showOnscreenKeyboard) {
        int textW = 0, textH = 0;
        TTF_SizeText(font, searchText.c_str(), &textW, &textH);
        int cursorX = searchBox.x + 8 + textW + 2;
        int cursorY1 = searchBox.y + (searchBox.h - 24) / 2;
        int cursorY2 = cursorY1 + 24;
        if ((SDL_GetTicks() / 500) % 2 == 0) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 80, 255);
            SDL_RenderDrawLine(renderer, cursorX, cursorY1, cursorX, cursorY2);
            SDL_RenderDrawLine(renderer, cursorX+1, cursorY1, cursorX+1, cursorY2);
        }
    }
    // Draw filter button (match Hexrom: 200x48, centered, focus highlight)
    int filterBtnW = 200, filterBtnH = 48;
    int filterBtnX = modalX + 200;
    int filterBtnY = searchBoxY + 80;
    SDL_Rect filterBtn = {filterBtnX, filterBtnY, filterBtnW, filterBtnH};
    if (focusIndex == 1 && !showOnscreenKeyboard) {
        SDL_Rect btnFocus = {filterBtnX - 8, filterBtnY - 8, filterBtnW + 16, filterBtnH + 16};
        SDL_SetRenderDrawColor(renderer, 60, 120, 220, 180);
        SDL_RenderDrawRect(renderer, &btnFocus);
    }
    SDL_SetRenderDrawColor(renderer, 60, 120, 220, 160);
    SDL_RenderFillRect(renderer, &filterBtn);
    UiUtils::RenderTextCenteredInBox(renderer, font, "Filter", filterBtn, UiUtils::Color(255,255,255));

    // --- On-screen keyboard rendering ---
    if (showOnscreenKeyboard && focusIndex == 0) {
        onscreenKeyboard.render(renderer, font, modalX, modalY + 160, modalW, modalH - 160);
    }
}

void GamulatorFilterModal::handleInput(const SDL_Event& e) {
    // --- BLOCK ALL INPUT IF ONSCREEN KEYBOARD IS OPEN ---
    if (showOnscreenKeyboard) {
        if (focusIndex == 0) {
            onscreenKeyboard.handleInput(e, searchText, showOnscreenKeyboard);
        }
        return;
    }
    // --- Modal input handling (Hexrom style, no dropdowns) ---
    if (e.type == SDL_CONTROLLERBUTTONDOWN) {
        bool isA = (e.cbutton.button == BUTTON_A);
        bool isB = (e.cbutton.button == BUTTON_B);
        // D-pad up/down to move focus
        if (e.cbutton.button == BUTTON_DPAD_DOWN) {
            focusIndex = (focusIndex + 1) % 2;
            return;
        } else if (e.cbutton.button == BUTTON_DPAD_UP) {
            focusIndex = (focusIndex + 1) % 2;
            return;
        }
        // A/B: open keyboard or close modal
        if (focusIndex == 0 && isA) { // Search: open keyboard
            showOnscreenKeyboard = true;
            return;
        } else if (focusIndex == 1 && isA) {
            if (onFilter) onFilter(searchText, 0, 0, 0, 0);
            showModal(false);
            return;
        } else if (isB) {
            // If onscreen keyboard is not open, close modal. If it was open, it will have been closed above.
            showModal(false);
            return;
        }
    } else if (e.type == SDL_KEYDOWN) {
        switch (e.key.keysym.sym) {
            case SDLK_TAB:
            case SDLK_DOWN:
            case SDLK_s:
                focusIndex = (focusIndex + 1) % 2;
                break;
            case SDLK_UP:
            case SDLK_w:
                focusIndex = (focusIndex + 1) % 2;
                break;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
            case SDLK_SPACE:
                if (focusIndex == 1) { // Filter button
                    if (onFilter) onFilter(searchText, 0, 0, 0, 0);
                    showModal(false);
                }
                break;
            case SDLK_ESCAPE:
            case SDLK_b:
                showModal(false);
                break;
            case SDLK_BACKSPACE:
                if (focusIndex == 0 && !searchText.empty()) searchText.pop_back();
                break;
            default:
                break;
        }
    } else if (e.type == SDL_TEXTINPUT) {
        if (focusIndex == 0) searchText += e.text.text;
    }
}

void GamulatorFilterModal::setOnFilter(std::function<void(const std::string& search, int region, int sort, int order, int genre)> cb) {
    onFilter = cb;
}

bool GamulatorFilterModal::isVisible() const {
    return visible;
}

void GamulatorFilterModal::showModal(bool show) {
    visible = show;
    if (!show) showOnscreenKeyboard = false;
}