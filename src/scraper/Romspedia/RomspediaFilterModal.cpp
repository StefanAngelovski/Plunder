#include "include/RomspediaFilterModal.h"
#include "../../utils/include/Theme.h"

RomspediaFilterModal::RomspediaFilterModal() {}
RomspediaFilterModal::~RomspediaFilterModal() {}

void RomspediaFilterModal::render(SDL_Renderer* renderer, TTF_Font* font, int x, int y, int width, int height) {
    (void)font; (void)height;
    using namespace UiUtils;
    const ThemeColors& t = Theme::getInstance().colors();
    // Only two controls: use a compact panel unless the keyboard needs room.
    int h = showOnscreenKeyboard ? height : 260;
    int py = showOnscreenKeyboard ? y : (SCREEN_H - h) / 2;
    SDL_Rect modal{x, py, width, h};
    DrawPanel(renderer, modal, 24);
    DrawText(renderer, GetFont(24, true), "Search", x + 32, py + 26, t.textPrimary);

    TTF_Font* valueFont = GetFont(18);
    SDL_Rect field{x + 32, py + 80, width - 64, 52};
    bool fieldFocused = (focusIndex == 0);
    FillRoundedRect(renderer, field, 12, fieldFocused ? Lighten(t.backgroundDark, 0.06f) : WithAlpha(t.backgroundDark, 170));
    if (fieldFocused) DrawFocusRing(renderer, field, 12, t.accentPrimary, 2, 2);
    bool empty = search.empty();
    int textY = field.y + (field.h - TextHeight(valueFont)) / 2;
    int w = DrawText(renderer, valueFont, Ellipsize(valueFont, empty ? "Type a game name…" : search, field.w - 40), field.x + 16, textY,
                     empty ? WithAlpha(t.textSecondary, 150) : t.textPrimary);
    if (fieldFocused && (SDL_GetTicks() / 500) % 2 == 0)
        FillRoundedRect(renderer, SDL_Rect{field.x + 16 + (empty ? 0 : w) + 2, field.y + 13, 2, field.h - 26}, 0, t.accentPrimary);

    if (!showOnscreenKeyboard) {
        SDL_Rect btn{x + width - 32 - 200, py + h - 32 - 50, 200, 50};
        bool btnFocused = (focusIndex == 1);
        FillRoundedRect(renderer, btn, 14, btnFocused ? t.accentPrimary : WithAlpha(t.accentPrimary, 50));
        if (btnFocused) DrawFocusRing(renderer, btn, 14, t.accentPrimary, 3, 3);
        RenderTextCenteredInBox(renderer, GetFont(18, true), "Search", btn, Color(btnFocused ? t.backgroundDark : t.textPrimary));
        DrawHints(renderer, {{"DPAD", "Move"}, {"A", "Select"}, {"B", "Close"}});
    } else if (focusIndex == 0) {
        onscreenKeyboard.render(renderer, GetFont(20), x + 44, py + 156, width - 88, h - 170);
        DrawHints(renderer, {{"DPAD", "Move"}, {"A", "Type"}, {"X", "Delete"}, {"B", "Done"}});
    }
}

void RomspediaFilterModal::handleInput(const SDL_Event& e) {
    if (showOnscreenKeyboard) {
        if (focusIndex == 0) {
            onscreenKeyboard.handleInput(e, search, showOnscreenKeyboard);
        }
        return;
    }
    if (e.type == SDL_CONTROLLERBUTTONDOWN) {
        bool isA = (e.cbutton.button == BUTTON_A);
        bool isB = (e.cbutton.button == BUTTON_B);
        if (e.cbutton.button == BUTTON_DPAD_DOWN) {
            focusIndex = (focusIndex + 1) % 2;
            return;
        } else if (e.cbutton.button == BUTTON_DPAD_UP) {
            focusIndex = (focusIndex + 1) % 2;
            return;
        }
        if (focusIndex == 0 && isA) {
            showOnscreenKeyboard = true;
            return;
        } else if (focusIndex == 1 && isA) {
            if (onFilter) onFilter(search, 0, 0, 0, 0);
            showModal(false);
            return;
        } else if (isB) {
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
                if (focusIndex == 1) {
                    if (onFilter) onFilter(search, 0, 0, 0, 0);
                    showModal(false);
                }
                break;
            case SDLK_ESCAPE:
            case SDLK_b:
                showModal(false);
                break;
            case SDLK_BACKSPACE:
                if (focusIndex == 0 && !search.empty()) search.pop_back();
                break;
            default:
                break;
        }
    } else if (e.type == SDL_TEXTINPUT) {
        if (focusIndex == 0) search += e.text.text;
    }
}

void RomspediaFilterModal::setOnFilter(std::function<void(const std::string& search, int region, int sort, int order, int genre)> cb) {
    onFilter = cb;
}

bool RomspediaFilterModal::isVisible() const {
    return visible;
}

void RomspediaFilterModal::showModal(bool show) {
    visible = show;
    if (!show) showOnscreenKeyboard = false;
}

std::string RomspediaFilterModal::getSearch() const {
    return search;
}