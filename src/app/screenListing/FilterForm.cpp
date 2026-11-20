#include "include/FilterForm.h"

#include "../ControllerButtons.h"
#include "../../utils/include/Theme.h"
#include "../../utils/include/UiUtils.h"

using namespace UiUtils;

namespace {
constexpr int kWidth = 800, kRowStep = 58, kFieldH = 48, kRowsTop = 84;

void drawChevron(SDL_Renderer* r, int cx, int cy, SDL_Color c) {
    for (int i = 0; i < 5; ++i) FillRoundedRect(r, SDL_Rect{cx - 5 + i, cy - 2 + i, 11 - 2 * i, 2}, 0, c);
}
}  // namespace

FilterForm::FilterForm(std::vector<FilterField> fields, ApplyFn onApply)
    : fields_(std::move(fields)), onApply_(std::move(onApply)), choices_(fields_.size(), 0) {}

void FilterForm::show(bool show) {
    visible_ = show;
    if (!show) { pulldown_ = false; keyboardOpen_ = false; }
}

void FilterForm::handleInput(const SDL_Event& e) {
    if (keyboardOpen_) {
        keyboard_.handleInput(e, search_, keyboardOpen_);
        return;
    }
    if (e.type != SDL_CONTROLLERBUTTONDOWN) return;
    const Uint8 b = e.cbutton.button;

    if (pulldown_) {
        const auto& options = fields_[focus_ - 1].options;
        int n = (int)options.size();
        if (b == BUTTON_DPAD_DOWN) pullIndex_ = (pullIndex_ + 1) % n;
        else if (b == BUTTON_DPAD_UP) pullIndex_ = (pullIndex_ + n - 1) % n;
        else if (b == BUTTON_A) { choices_[focus_ - 1] = pullIndex_; pulldown_ = false; }
        else if (b == BUTTON_B) pulldown_ = false;
        return;
    }
    const int rows = applyIndex() + 1;
    if (b == BUTTON_DPAD_DOWN) focus_ = (focus_ + 1) % rows;
    else if (b == BUTTON_DPAD_UP) focus_ = (focus_ + rows - 1) % rows;
    else if (b == BUTTON_B) show(false);
    else if (b == BUTTON_A) {
        if (focus_ == 0) keyboardOpen_ = true;
        else if (focus_ == applyIndex()) { show(false); if (onApply_) onApply_(search_, choices_); }
        else { pulldown_ = true; pullIndex_ = choices_[focus_ - 1]; }
    }
}

void FilterForm::render(SDL_Renderer* r) {
    if (!visible_) return;
    const ThemeColors& t = Theme::getInstance().colors();
    DrawScrim(r);

    // Height fits the rows; with the keyboard open, enough room for it.
    int height = kRowsTop + rowCount() * kRowStep + 10 + 50 + 32;
    if (keyboardOpen_) height = std::max(height, 440);
    const int x = (SCREEN_W - kWidth) / 2, y = (SCREEN_H - height) / 2;
    DrawPanel(r, SDL_Rect{x, y, kWidth, height}, 24);
    DrawText(r, GetFont(24, true), fields_.empty() ? "Search" : "Search & filter", x + 32, y + 26, t.textPrimary);

    TTF_Font* labelFont = GetFont(17);
    TTF_Font* valueFont = GetFont(18);
    const int fieldX = x + 170, fieldW = kWidth - 170 - 32, rowsTop = y + kRowsTop;
    for (int i = 0; i < rowCount(); ++i) {
        SDL_Rect field{fieldX, rowsTop + i * kRowStep, fieldW, kFieldH};
        bool focused = (i == focus_ && !pulldown_ && !keyboardOpen_);
        DrawText(r, labelFont, i == 0 ? "Search" : fields_[i - 1].label, x + 32, field.y + (kFieldH - TextHeight(labelFont)) / 2,
                 focused ? t.textPrimary : t.textSecondary);
        FillRoundedRect(r, field, 12, focused || (i == 0 && keyboardOpen_) ? Lighten(t.backgroundDark, 0.06f) : WithAlpha(t.backgroundDark, 170));
        if (focused || (i == 0 && keyboardOpen_)) DrawFocusRing(r, field, 12, t.accentPrimary, 2, 2);
        int textY = field.y + (kFieldH - TextHeight(valueFont)) / 2;
        if (i == 0) {
            bool empty = search_.empty();
            int w = DrawText(r, valueFont, Ellipsize(valueFont, empty ? "Type to search…" : search_, fieldW - 40), field.x + 16, textY,
                             empty ? WithAlpha(t.textSecondary, 150) : t.textPrimary);
            if ((focused || keyboardOpen_) && (SDL_GetTicks() / 500) % 2 == 0)
                FillRoundedRect(r, SDL_Rect{field.x + 16 + (empty ? 0 : w) + 2, field.y + 12, 2, kFieldH - 24}, 0, t.accentPrimary);
        } else {
            const FilterField& f = fields_[i - 1];
            DrawText(r, valueFont, Ellipsize(valueFont, f.options[choices_[i - 1]], fieldW - 56), field.x + 16, textY, t.textPrimary);
            drawChevron(r, field.x + field.w - 24, field.y + kFieldH / 2 - 2, t.textSecondary);
        }
    }

    SDL_Rect apply{x + kWidth - 32 - 200, rowsTop + rowCount() * kRowStep + 10, 200, 50};
    if (!keyboardOpen_) {
        bool focused = focus_ == applyIndex() && !pulldown_;
        FillRoundedRect(r, apply, 14, focused ? t.accentPrimary : WithAlpha(t.accentPrimary, 50));
        if (focused) DrawFocusRing(r, apply, 14, t.accentPrimary, 3, 3);
        RenderTextCenteredInBox(r, GetFont(18, true), fields_.empty() ? "Search" : "Apply filters", apply, Color(focused ? t.backgroundDark : t.textPrimary));
    }

    if (pulldown_) {
        const auto& options = fields_[focus_ - 1].options;
        const int maxVisible = 6, itemH = 42;
        int total = (int)options.size(), visible = std::min(maxVisible, total);
        int first = std::min(std::max(0, pullIndex_ - maxVisible / 2), std::max(0, total - maxVisible));
        SDL_Rect list{fieldX, rowsTop + focus_ * kRowStep + kFieldH + 6, fieldW, visible * itemH + 12};
        if (list.y + list.h > y + height - 12) list.y = rowsTop + focus_ * kRowStep - list.h - 6;
        DrawShadow(r, list, 14, 24, 150, 8);
        FillRoundedRect(r, list, 14, Lighten(t.cardBackground, 0.06f));
        StrokeRoundedRect(r, list, 14, 1, WithAlpha(t.textPrimary, 26));
        for (int i = 0; i < visible; ++i) {
            int idx = first + i;
            SDL_Rect row{list.x + 6, list.y + 6 + i * itemH, list.w - 12, itemH};
            bool sel = idx == pullIndex_;
            if (sel) FillRoundedRect(r, row, 10, WithAlpha(t.accentPrimary, 60));
            DrawText(r, valueFont, Ellipsize(valueFont, options[idx], row.w - 24), row.x + 14, row.y + (itemH - TextHeight(valueFont)) / 2,
                     sel ? t.textPrimary : t.textSecondary);
        }
        DrawHints(r, {{"DPAD", "Choose"}, {"A", "Select"}, {"B", "Cancel"}});
    } else if (keyboardOpen_) {
        keyboard_.render(r, GetFont(20), x + 44, y + 150, kWidth - 88, height - 160);
        DrawHints(r, {{"DPAD", "Move"}, {"A", "Type"}, {"X", "Delete"}, {"B", "Done"}});
    } else {
        DrawHints(r, {{"DPAD", "Move"}, {"A", "Edit"}, {"B", "Close"}});
    }
}
