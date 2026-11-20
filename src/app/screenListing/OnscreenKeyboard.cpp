#include "include/OnscreenKeyboard.h"
#include "../../utils/include/Theme.h"

OnscreenKeyboard::OnscreenKeyboard()
    : mode(Mode::Letters), caps(false), row(0), col(0) {
    rowsLetters = {
        "qwertyuiop",
        "asdfghjkl",
        std::string(1, '\x18') + "zxcvbnm" + std::string(1, '\x08'),
        std::string(1, '\x19') + "/     ." + std::string(1, '\x1a')
    };
    rowsSpecial = {
        "1234567890",
        "!@#$%^&*()",
        std::string(1, '\x18') + "-=+[]{}" + std::string(1, '\x08'),
        std::string(1, '\x19') + "_     /" + std::string(1, '\x1a')
    };
    updateRows();
}

void OnscreenKeyboard::setCaps(bool c) {
    caps = c;
    updateRows();
}

void OnscreenKeyboard::setMode(Mode m) {
    mode = m;
    updateRows();
}

OnscreenKeyboard::Mode OnscreenKeyboard::getMode() const {
    return mode;
}

void OnscreenKeyboard::resetPosition() {
    row = 0; col = 0;
}

void OnscreenKeyboard::setRows(const std::vector<std::string>& rows) {
    currentRows = rows;
}

const std::vector<std::string>& OnscreenKeyboard::getRows() const {
    return currentRows;
}

int OnscreenKeyboard::getRow() const { return row; }
int OnscreenKeyboard::getCol() const { return col; }

void OnscreenKeyboard::updateRows() {
    if (mode == Mode::Letters) {
        if (caps) {
            currentRows = {
                "QWERTYUIOP",
                "ASDFGHJKL",
                std::string(1, '\x18') + "ZXCVBNM" + std::string(1, '\x08'),
                std::string(1, '\x19') + "/     ." + std::string(1, '\x1a')
            };
        } else {
            currentRows = rowsLetters;
        }
    } else {
        currentRows = rowsSpecial;
    }
}

void OnscreenKeyboard::render(SDL_Renderer* renderer, TTF_Font* font, int x, int y, int w, int h) {
    (void)font; (void)w; (void)h;
    using namespace UiUtils;
    const ThemeColors& t = Theme::getInstance().colors();
    const int keyW = 64, keyH = 50, spacing = 8;
    const int kbRows = (int)currentRows.size();
    const int kbCols = 10;
    const int kbW = kbCols * keyW + (kbCols - 1) * spacing;
    const int kbH = kbRows * keyH + (kbRows - 1) * spacing;
    SDL_Rect panel{x - 16, y - 16, kbW + 32, kbH + 32};
    DrawShadow(renderer, panel, 20, 28, 160, 10);
    FillRoundedRect(renderer, panel, 20, Darken(t.cardBackground, 0.25f));
    StrokeRoundedRect(renderer, panel, 20, 1, WithAlpha(t.textPrimary, 22));
    TTF_Font* keyFont = GetFont(20, true);
    TTF_Font* specialFont = GetFont(14, true);
    for (int r = 0; r < kbRows; ++r) {
        const std::string& keys = currentRows[r];
        int numKeys = (int)keys.size();
        int rowWidth = numKeys * keyW + (numKeys - 1) * spacing;
        int rowStartX = x + (kbW - rowWidth) / 2;
        int rowY = y + r * (keyH + spacing);
        for (int c = 0; c < numKeys; ++c) {
            SDL_Rect key{rowStartX + c * (keyW + spacing), rowY, keyW, keyH};
            bool sel = (r == row && c == col);
            unsigned char code = keys[c];
            bool special = code == 0x08 || code == 0x1a || code == 0x18 || code == 0x19;
            std::string label;
            if (code == 0x08) label = "Delete";
            else if (code == 0x1a) label = "Done";
            else if (code == 0x18) label = (mode == Mode::Letters ? "Shift" : "ABC");
            else if (code == 0x19) label = (mode == Mode::Letters ? "?123" : "ABC");
            else label = std::string(1, keys[c]);
            SDL_Color fill = sel ? t.accentPrimary : (special ? Lighten(t.cardBackground, 0.02f) : Lighten(t.cardBackground, 0.10f));
            if (sel) DrawShadow(renderer, key, 10, 12, 140, 4);
            FillRoundedRect(renderer, key, 10, fill);
            SDL_Color fg = sel ? t.backgroundDark : (special ? t.textSecondary : t.textPrimary);
            if (code == ' ') {
                // space bar glyph
                SDL_Color g = sel ? t.backgroundDark : t.textSecondary;
                FillRoundedRect(renderer, SDL_Rect{key.x + keyW / 2 - 12, key.y + keyH / 2 + 4, 24, 3}, 1, g);
                FillRoundedRect(renderer, SDL_Rect{key.x + keyW / 2 - 12, key.y + keyH / 2 - 3, 3, 8}, 1, g);
                FillRoundedRect(renderer, SDL_Rect{key.x + keyW / 2 + 9, key.y + keyH / 2 - 3, 3, 8}, 1, g);
            } else {
                RenderTextCentered(renderer, special ? specialFont : keyFont, label, key.x + keyW / 2, key.y + keyH / 2, Color(fg));
            }
        }
    }
}

bool OnscreenKeyboard::handleInput(const SDL_Event& e, std::string& text, bool& showOnscreenKeyboard) {
    const auto& oskRows = currentRows;
    if (e.type == SDL_CONTROLLERBUTTONDOWN) {
        // Close keyboard
        if (e.cbutton.button == BUTTON_B) {
            showOnscreenKeyboard = false;
            resetPosition();
            return true;
        }
        if (e.cbutton.button == BUTTON_DPAD_DOWN) {
            row = std::min(row + 1, (int)oskRows.size() - 1);
            int newNum = oskRows[row].size();
            if (col >= newNum) col = newNum - 1;
            return true;
        }
        if (e.cbutton.button == BUTTON_DPAD_UP) {
            row = std::max(row - 1, 0);
            int newNum = oskRows[row].size();
            if (col >= newNum) col = newNum - 1;
            return true;
        }
        if (e.cbutton.button == BUTTON_DPAD_LEFT) {
            col = std::max(col - 1, 0);
            return true;
        }
        if (e.cbutton.button == BUTTON_DPAD_RIGHT) {
            col = std::min(col + 1, (int)oskRows[row].size() - 1);
            return true;
        }
        if (e.cbutton.button == BUTTON_X) {
            if (!text.empty()) {
                text.pop_back();
            }
            return true;
        }
        // Open keyboard
        if (e.cbutton.button == BUTTON_A) {
            std::string key(1, oskRows[row][col]);
            if (key.size() == 1) {
                unsigned char code = key[0];
                if (code == 0x08) {
                    if (!text.empty()) {
                        text.pop_back();
                    }
                    return true;
                } // Backspace
                if (code == 0x1a) {
                    // Enter pressed: close keyboard, but allow reopening
                    showOnscreenKeyboard = false;
                    resetPosition();
                    return true;
                }
                if (code == 0x18) {
                    if (mode == Mode::Letters) {
                        setCaps(!caps);
                    } else {
                        setMode(Mode::Letters);
                    }
                    resetPosition();
                    return true;
                }
                if (code == 0x19) {
                    setMode(mode == Mode::Letters ? Mode::Special : Mode::Letters);
                    resetPosition();
                    return true;
                }
            }
            text += key;
            return true;
        }
    }
    return false;
}