#include "include/PatchNotesScreen.h"

PatchNotesScreen::PatchNotesScreen() {}

void PatchNotesScreen::setNotes(const std::vector<std::string>& n) {
    notes = n;
    wrappedLines.clear();
    scrollOffset = 0;
    const int maxWidth = 1100;
    // Use the font already loaded by the app (assume InterVariable.ttf, size 20)
    TTF_Font* font = TTF_OpenFont("res/InterVariable.ttf", 20);
    if (!font) return;
    for (const auto& para : notes) {
        if (para.empty()) {
            wrappedLines.push_back("");
            continue;
        }
        // Use SDL_ttf to wrap the paragraph
        SDL_Surface* surface = TTF_RenderText_Blended_Wrapped(font, para.c_str(), {255,255,255,255}, maxWidth);
        if (!surface) {
            wrappedLines.push_back(para);
            continue;
        }
        // Extract lines from the surface (simulate line splitting)
        // We'll split manually on '\n' for now
        size_t start = 0, end = 0;
        while ((end = para.find('\n', start)) != std::string::npos) {
            std::string line = para.substr(start, end - start);
            if (!line.empty()) wrappedLines.push_back(line);
            start = end + 1;
        }
        if (start < para.size()) wrappedLines.push_back(para.substr(start));
        SDL_FreeSurface(surface);
    }
    TTF_CloseFont(font);
}

void PatchNotesScreen::render(SDL_Renderer* renderer, TTF_Font* font) {
    // Teal background for consistency with main menu and carousel
    SDL_SetRenderDrawColor(renderer, 32, 170, 180, 255);
    SDL_RenderClear(renderer);
    int x = 80, y = 60;
    UiUtils::RenderText(renderer, font, "Patch Notes", x, y, UiUtils::Color(255,255,80), 1.5f);
    y += 50;
    int lineHeight = 32;
    int totalLines = wrappedLines.size();
    int visibleEnd = std::min(totalLines, scrollOffset + maxVisibleLines);
    for (int i = scrollOffset; i < visibleEnd; ++i) {
        std::string& line = wrappedLines[i];
        // Separator for boxes (--- marker)
        if (line == "---") {
            y += 10;
            SDL_SetRenderDrawColor(renderer, 120, 120, 120, 180);
            SDL_Rect sepRect = {x, y, 1100, 2};
            SDL_RenderFillRect(renderer, &sepRect);
            y += 18;
            continue;
        }
        // Heading detection: [H1]..[/H], [H2]..[/H], etc.
        if (line.rfind("[H", 0) == 0 && line.find("]") != std::string::npos) {
            size_t close = line.find("]");
            int level = 1;
            try { level = std::stoi(line.substr(2, close-2)); } catch (...) {}
            std::string headingText = line.substr(close+1);
            // Find closing [/H] in this or next lines
            while (headingText.find("[/H]") == std::string::npos && i+1 < totalLines) {
                headingText += "\n" + wrappedLines[++i];
            }
            size_t endTag = headingText.find("[/H]");
            if (endTag != std::string::npos) headingText = headingText.substr(0, endTag);
            float scale = 1.08f + 0.08f * (6 - std::min(level, 6)); // H1 biggest, but less large
            int boldLineHeight = int(lineHeight * scale) + 6;
            // Add space above heading if previous line is not empty or a separator
            if (i == scrollOffset || (i > 0 && !wrappedLines[i-1].empty() && wrappedLines[i-1] != "---")) {
                y += 8;
            }
            UiUtils::RenderText(renderer, font, headingText, x, y, UiUtils::Color(255,255,255), scale);
            y += boldLineHeight + 8; // extra space after heading
            continue;
        }
        // List item: starts with "- "
        if (line.rfind("- ", 0) == 0) {
            UiUtils::RenderTextWrapped(renderer, font, line, x+16, y, 1080, UiUtils::Color(220,220,220));
            y += lineHeight; // tighter spacing for lists
            continue;
        }
        // Normal paragraph
        UiUtils::RenderTextWrapped(renderer, font, line, x, y, 1100, UiUtils::Color(220,220,220));
        y += lineHeight + 2;
    }
    // Scrollbar
    if (totalLines > maxVisibleLines) {
        int barX = 1200, barY = 110;
        int barW = 10, barH = maxVisibleLines * lineHeight;
        float thumbH = (float)maxVisibleLines / totalLines * barH;
        float thumbY = barY + ((float)scrollOffset / (totalLines - maxVisibleLines)) * (barH - thumbH);
        SDL_SetRenderDrawColor(renderer, 80, 80, 100, 100);
        SDL_Rect bgRect = {barX, barY, barW, barH};
        SDL_RenderFillRect(renderer, &bgRect);
        SDL_SetRenderDrawColor(renderer, 220, 220, 80, 160);
        SDL_Rect thumbRect = {barX, (int)thumbY, barW, (int)thumbH};
        SDL_RenderFillRect(renderer, &thumbRect);
    }
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