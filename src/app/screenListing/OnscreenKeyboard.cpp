#include "include/OnscreenKeyboard.h"

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
    // Keyboard rendering logic
    int keyW = 64, keyH = 48, spacing = 8;
    int kbRows = currentRows.size();
    int kbCols = 10; // Longest row
    int kbW = kbCols * keyW + (kbCols - 1) * spacing;
    int kbH = kbRows * (keyH + spacing);
    int kbX = x;
    int kbY = y;
    // Draw keyboard background
    SDL_Color kbShadow = {0, 0, 0, 120};
    SDL_Color kbBg = {54, 20, 80, 235}; // Deep purple/navy
    SDL_Color kbBorder = {0, 255, 200, 255}; // Bright aqua border
    SDL_Rect shadowRect = {kbX - 20, kbY - 20, kbW + 40, kbH + 40};
    SDL_SetRenderDrawColor(renderer, kbShadow.r, kbShadow.g, kbShadow.b, kbShadow.a);
    SDL_RenderFillRect(renderer, &shadowRect);
    SDL_Rect bgRect = {kbX - 16, kbY - 16, kbW + 32, kbH + 32};
    SDL_SetRenderDrawColor(renderer, kbBg.r, kbBg.g, kbBg.b, kbBg.a);
    SDL_RenderFillRect(renderer, &bgRect);
    SDL_SetRenderDrawColor(renderer, kbBorder.r, kbBorder.g, kbBorder.b, kbBorder.a);
    SDL_RenderDrawRect(renderer, &bgRect);
    for (int r = 0; r < kbRows; ++r) {
        int rowY = kbY + r * (keyH + spacing);
        auto keys = currentRows[r];
        int numKeys = (int)keys.size();
        int rowWidth = numKeys * keyW + (numKeys - 1) * spacing;
        int rowStartX = kbX + (kbW - rowWidth) / 2;
        for (int c = 0; c < numKeys; ++c) {
            int keyX = rowStartX + c * (keyW + spacing);
            SDL_Rect keyRect = {keyX, rowY, keyW, keyH};
            // Highlight selected key
            if (r == row && c == col) {
                SDL_SetRenderDrawColor(renderer, 120, 200, 255, 255);
                SDL_RenderFillRect(renderer, &keyRect);
            } else {
                SDL_SetRenderDrawColor(renderer, 60, 120, 180, 180);
                SDL_RenderFillRect(renderer, &keyRect);
            }
            SDL_SetRenderDrawColor(renderer, 80, 200, 255, 255);
            SDL_RenderDrawRect(renderer, &keyRect);
            // Draw key label (special keys as text)
            std::string label;
            unsigned char code = keys[c];
            if (code == 0x08) label = "<="; // Backspace
            else if (code == 0x1a) label = "->"; // Enter
            else if (code == 0x18) label = (mode == Mode::Letters ? "Shift" : "ABC");
            else if (code == 0x19) label = (mode == Mode::Letters ? "?123" : "ABC");
            else label = std::string(1, keys[c]);
            UiUtils::RenderTextCentered(renderer, font, label, keyX + keyW / 2, rowY + keyH / 2, UiUtils::Color(255,255,255));
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