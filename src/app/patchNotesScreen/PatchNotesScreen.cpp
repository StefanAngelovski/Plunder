#include "include/PatchNotesScreen.h"
#include "../../utils/include/Theme.h"

PatchNotesScreen::PatchNotesScreen() {}

// Drop emoji (U+1F000 and up) and variation selectors: the UI font has no glyphs
// for them, so they would render as empty boxes. Symbols like the warning sign stay.
static std::string stripEmoji(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size();) {
        unsigned char c = (unsigned char)in[i];
        size_t len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 1;
        if (i + len > in.size()) len = 1;
        bool drop = false;
        if (len == 4) drop = true;  // supplementary planes: emoji
        if (len == 3 && c == 0xEF && (unsigned char)in[i + 1] == 0xB8 &&
            ((unsigned char)in[i + 2] & 0xF0) == 0x80) drop = true;  // U+FE00..FE0F variation selectors
        if (len == 3 && c == 0xE2 && (unsigned char)in[i + 1] == 0x80 && (unsigned char)in[i + 2] == 0x8D) drop = true;  // ZWJ
        if (!drop) out.append(in, i, len);
        i += len;
    }
    // collapse the space left behind at the start, e.g. "[H2]<emoji> Plunder" -> "[H2]Plunder"
    size_t tag = out.rfind("[H", 0) == 0 ? out.find(']') : std::string::npos;
    size_t start = tag == std::string::npos ? 0 : tag + 1;
    size_t firstText = out.find_first_not_of(' ', start);
    if (firstText != std::string::npos && firstText > start) out.erase(start, firstText - start);
    return out;
}

void PatchNotesScreen::setNotes(const std::vector<std::string>& n) {
    notes.clear();
    for (const auto& line : n) notes.push_back(stripEmoji(line));
    wrappedLines.clear();
    rowKinds.clear();
    rowLevels.clear();
    layoutWidth = -1;  // re-laid out on next render with the real fonts
    scrollOffset = 0;
}

static TTF_Font* headingFont(int level) {
    return UiUtils::GetFont(level <= 1 ? 26 : (level == 2 ? 23 : 20), true);
}

void PatchNotesScreen::layout(int width) {
    using namespace UiUtils;
    wrappedLines.clear();
    rowKinds.clear();
    rowLevels.clear();
    TTF_Font* body = GetFont(18);
    auto push = [this](const std::string& text, char kind, int level = 0) {
        wrappedLines.push_back(text); rowKinds.push_back(kind); rowLevels.push_back(level);
    };
    for (size_t i = 0; i < notes.size(); ++i) {
        std::string line = notes[i];
        if (line == "---") { push("", '-'); continue; }
        if (line.rfind("[H", 0) == 0 && line.find(']') != std::string::npos) {
            size_t close = line.find(']');
            int level = 1;
            try { level = std::stoi(line.substr(2, close - 2)); } catch (...) {}
            std::string text = line.substr(close + 1);
            while (text.find("[/H]") == std::string::npos && i + 1 < notes.size()) text += " " + notes[++i];
            size_t endTag = text.find("[/H]");
            if (endTag != std::string::npos) text = text.substr(0, endTag);
            if (!rowKinds.empty() && rowKinds.back() != 'e' && rowKinds.back() != '-') push("", 'e');
            for (auto& l : WrapText(headingFont(level), text, width)) push(l, 'H', level);
            continue;
        }
        if (line.empty()) {
            if (rowKinds.empty() || rowKinds.back() != 'e') push("", 'e');
            continue;
        }
        if (line.rfind("- ", 0) == 0 || line.rfind("* ", 0) == 0) {
            auto lines = WrapText(body, line.substr(2), width - 26);
            for (size_t k = 0; k < lines.size(); ++k) push(lines[k], k == 0 ? 'b' : 'c');
            continue;
        }
        for (auto& l : WrapText(body, line, width)) push(l, 'p');
    }
    while (!rowKinds.empty() && rowKinds.back() == 'e') { wrappedLines.pop_back(); rowKinds.pop_back(); rowLevels.pop_back(); }
    layoutWidth = width;
}

static int rowHeight(char kind, int level) {
    switch (kind) {
        case 'H': return level <= 1 ? 38 : 33;
        case '-': return 26;
        case 'e': return 12;
        default: return 28;
    }
}

void PatchNotesScreen::render(SDL_Renderer* renderer, TTF_Font* font) {
    (void)font;
    using namespace UiUtils;
    const ThemeColors& t = Theme::getInstance().colors();
    DrawBackground(renderer);
    DrawHeader(renderer, "Patch Notes", "Plunder", "What's new");

    SDL_Rect panel{MARGIN_X, HEADER_H + 8, SCREEN_W - 2 * MARGIN_X, SCREEN_H - FOOTER_H - HEADER_H - 16};
    DrawPanel(renderer, panel, 22, false);
    const int padX = 36, padY = 24;
    const int textX = panel.x + padX;
    const int textW = panel.w - 2 * padX - 16;

    if (notes.empty()) {
        DrawSpinner(renderer, SCREEN_W / 2, panel.y + panel.h / 2 - 18, 18, t.accentPrimary);
        RenderTextCentered(renderer, GetFont(18), "Fetching release notes…", SCREEN_W / 2, panel.y + panel.h / 2 + 24, Color(t.textSecondary));
        DrawHints(renderer, {{"B", "Back"}});
        return;
    }
    if (layoutWidth != textW) layout(textW);

    TTF_Font* body = GetFont(18);
    const int totalRows = (int)wrappedLines.size();
    int y = panel.y + padY;
    const int limit = panel.y + panel.h - padY;
    int shown = 0;
    for (int i = scrollOffset; i < totalRows; ++i) {
        int hgt = rowHeight(rowKinds[i], rowLevels[i]);
        if (y + hgt > limit) break;
        const std::string& text = wrappedLines[i];
        switch (rowKinds[i]) {
            case 'H': {
                TTF_Font* hf = headingFont(rowLevels[i]);
                DrawText(renderer, hf, text, textX, y + (hgt - TextHeight(hf)) / 2, rowLevels[i] <= 1 ? t.textPrimary : Mix(t.textPrimary, t.accentPrimary, 0.35f));
                break;
            }
            case '-':
                FillRoundedRect(renderer, SDL_Rect{textX, y + hgt / 2, textW, 1}, 0, WithAlpha(t.textPrimary, 30));
                break;
            case 'b':
                FillCircle(renderer, textX + 8, y + hgt / 2, 3, t.accentPrimary);
                DrawText(renderer, body, text, textX + 26, y + (hgt - TextHeight(body)) / 2, Mix(t.textSecondary, t.textPrimary, 0.55f));
                break;
            case 'c':
                DrawText(renderer, body, text, textX + 26, y + (hgt - TextHeight(body)) / 2, Mix(t.textSecondary, t.textPrimary, 0.55f));
                break;
            case 'p':
                DrawText(renderer, body, text, textX, y + (hgt - TextHeight(body)) / 2, Mix(t.textSecondary, t.textPrimary, 0.55f));
                break;
            default: break;
        }
        y += hgt;
        ++shown;
    }
    maxVisibleLines = std::max(1, shown);
    if (totalRows > maxVisibleLines) {
        int maxOffset = std::max(1, totalRows - maxVisibleLines);
        DrawScrollbar(renderer, panel.x + panel.w - 22, panel.y + padY, panel.h - 2 * padY,
                      (float)maxVisibleLines / totalRows, (float)scrollOffset / maxOffset);
    }
    DrawHints(renderer, {{"DPAD", "Scroll"}, {"B", "Back"}});
}

void PatchNotesScreen::handleInput(const SDL_Event& e, MenuSystem& menuSystem) {
    int totalLines = wrappedLines.size();
    // If controller button pressed
    if (e.type == SDL_CONTROLLERBUTTONDOWN) {
        // Go back to previous screen
        if (e.cbutton.button == BUTTON_B) {
            menuSystem.popScreen();
        }
        if (e.cbutton.button == BUTTON_DPAD_DOWN) {
            if (scrollOffset + maxVisibleLines < totalLines) scrollOffset++;
        }
        if (e.cbutton.button == BUTTON_DPAD_UP) {
            if (scrollOffset > 0) scrollOffset--;
        }
    }
}

void PatchNotesScreen::update() {
    // Poll the left stick Y axis for continuous scrolling
    int totalLines = wrappedLines.size();
    SDL_GameController* controller = SDL_GameControllerFromInstanceID(0); // 0 = first controller
    int value = 0;
    if (controller) {
        value = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
    }
    Uint32 now = SDL_GetTicks();
    const int deadzone = 8000;
    const Uint32 initialDelay = 180; // ms before repeat starts
    const Uint32 repeatDelay = 40;   // ms between repeats
    const Uint32 accelDelay = 600;   // ms after which to accelerate
    int scrollAmount = 1;
    if (abs(value) > deadzone) {
        if (axisHeldStart == 0) {
            axisHeldStart = now;
            lastAxisTime = now - initialDelay; // allow immediate scroll
        }
        if (now - axisHeldStart > accelDelay) {
            scrollAmount = 3;
        }
        if (now - lastAxisTime >= ((axisHeldStart == lastAxisTime) ? initialDelay : repeatDelay)) {
            for (int i = 0; i < scrollAmount; ++i) {
                if (value > 0 && scrollOffset + maxVisibleLines < totalLines) {
                    scrollOffset++;
                } else if (value < 0 && scrollOffset > 0) {
                    scrollOffset--;
                }
            }
            lastAxisTime = now;
        }
        lastAxisValue = value;
    } else {
        lastAxisValue = 0;
        axisHeldStart = 0;
    }
}