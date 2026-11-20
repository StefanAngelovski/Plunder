#include "include/HexromFilterModal.h"
#include "../../utils/include/Theme.h"

HexromFilterModal::HexromFilterModal() {
    filterDropdownOptions = {"Search", "Region", "Sort", "Order", "Genre"};
}

namespace {
// Small downward chevron drawn from horizontal strokes.
void drawChevron(SDL_Renderer* r, int cx, int cy, SDL_Color c) {
    for (int i = 0; i < 5; ++i)
        UiUtils::FillRoundedRect(r, SDL_Rect{cx - 5 + i, cy - 2 + i, 11 - 2 * i, 2}, 0, c);
}
}  // namespace

void HexromFilterModal::render(SDL_Renderer* renderer, TTF_Font* font, int x, int y, int width, int height) {
    (void)font;
    using namespace UiUtils;
    const ThemeColors& t = Theme::getInstance().colors();
    SDL_Rect modal{x, y, width, height};
    DrawPanel(renderer, modal, 24);
    DrawText(renderer, GetFont(24, true), "Search & filter", x + 32, y + 26, t.textPrimary);

    TTF_Font* labelFont = GetFont(17);
    TTF_Font* valueFont = GetFont(18);
    const int fieldX = x + 170, fieldW = width - 170 - 32, fieldH = 48, rowStep = 58;
    const int rowsTop = y + 84;
    const char* labels[5] = {"Search", "Region", "Sort by", "Order", "Genre"};
    const std::string values[5] = {filterSearchText, regionOptions[filterRegionIndex], sortOptions[filterSortIndex],
                                   orderOptions[filterOrderIndex], genreOptions[filterGenreIndex]};
    for (int i = 0; i < 5; ++i) {
        SDL_Rect field{fieldX, rowsTop + i * rowStep, fieldW, fieldH};
        bool focused = (i == filterDropdownFocus && !filterDropdownPulldown);
        DrawText(renderer, labelFont, labels[i], x + 32, field.y + (fieldH - TextHeight(labelFont)) / 2, focused ? t.textPrimary : t.textSecondary);
        FillRoundedRect(renderer, field, 12, focused ? Lighten(t.backgroundDark, 0.06f) : WithAlpha(t.backgroundDark, 170));
        if (focused) DrawFocusRing(renderer, field, 12, t.accentPrimary, 2, 2);
        int textY = field.y + (fieldH - TextHeight(valueFont)) / 2;
        if (i == 0) {
            bool empty = values[0].empty();
            std::string shown = empty ? "Type to search…" : values[0];
            int w = DrawText(renderer, valueFont, Ellipsize(valueFont, shown, fieldW - 40), field.x + 16, textY, empty ? WithAlpha(t.textSecondary, 150) : t.textPrimary);
            if (focused && (SDL_GetTicks() / 500) % 2 == 0)
                FillRoundedRect(renderer, SDL_Rect{field.x + 16 + (empty ? 0 : w) + 2, field.y + 12, 2, fieldH - 24}, 0, t.accentPrimary);
        } else {
            DrawText(renderer, valueFont, Ellipsize(valueFont, values[i], fieldW - 56), field.x + 16, textY, t.textPrimary);
            drawChevron(renderer, field.x + field.w - 24, field.y + fieldH / 2 - 2, t.textSecondary);
        }
    }

    // Apply button
    SDL_Rect btn{x + width - 32 - 200, rowsTop + 5 * rowStep + 10, 200, 50};
    bool btnFocused = (filterDropdownFocus == 5 && !filterDropdownPulldown);
    FillRoundedRect(renderer, btn, 14, btnFocused ? t.accentPrimary : WithAlpha(t.accentPrimary, 50));
    if (btnFocused) DrawFocusRing(renderer, btn, 14, t.accentPrimary, 3, 3);
    RenderTextCenteredInBox(renderer, GetFont(18, true), "Apply filters", btn, Color(btnFocused ? t.backgroundDark : t.textPrimary));

    // Dropdown list
    if (filterDropdownPulldown) {
        const int maxVisible = 6, itemH = 42;
        int total = (int)filterDropdownOptions.size();
        int first = std::min(std::max(0, filterDropdownPulldownIndex - maxVisible / 2), std::max(0, total - maxVisible));
        int visibleCount = std::min(maxVisible, total);
        SDL_Rect list{fieldX, rowsTop + filterDropdownFocus * rowStep + fieldH + 6, fieldW, visibleCount * itemH + 12};
        if (list.y + list.h > y + height - 12) list.y = rowsTop + filterDropdownFocus * rowStep - list.h - 6;
        DrawShadow(renderer, list, 14, 24, 150, 8);
        FillRoundedRect(renderer, list, 14, Lighten(t.cardBackground, 0.06f));
        StrokeRoundedRect(renderer, list, 14, 1, WithAlpha(t.textPrimary, 26));
        for (int i = 0; i < visibleCount; ++i) {
            int optIdx = first + i;
            SDL_Rect row{list.x + 6, list.y + 6 + i * itemH, list.w - 12, itemH};
            bool sel = (optIdx == filterDropdownPulldownIndex);
            if (sel) FillRoundedRect(renderer, row, 10, WithAlpha(t.accentPrimary, 60));
            DrawText(renderer, valueFont, Ellipsize(valueFont, filterDropdownOptions[optIdx], row.w - 24), row.x + 14,
                     row.y + (itemH - TextHeight(valueFont)) / 2, sel ? t.textPrimary : t.textSecondary);
        }
    }

    if (showOnscreenKeyboard && filterDropdownFocus == 0 && !filterDropdownPulldown) {
        onscreenKeyboard.render(renderer, GetFont(20), x + 44, y + 150, width - 88, height - 160);
        DrawHints(renderer, {{"DPAD", "Move"}, {"A", "Type"}, {"X", "Delete"}, {"B", "Done"}});
    } else if (filterDropdownPulldown) {
        DrawHints(renderer, {{"DPAD", "Choose"}, {"A", "Select"}, {"B", "Cancel"}});
    } else {
        DrawHints(renderer, {{"DPAD", "Move"}, {"A", "Edit"}, {"B", "Close"}});
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