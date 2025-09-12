#include "include/MenuScreen.h"

MenuScreen::MenuScreen(const std::string& title) : title(title) {}
void MenuScreen::addItem(const std::string& label, std::function<void()> action) { items.emplace_back(label, action); }
void MenuScreen::clearItems() { items.clear(); }
void MenuScreen::render(SDL_Renderer* renderer, TTF_Font* font) {
    // Time delta
    Uint32 now = SDL_GetTicks();
    if (lastTick == 0) lastTick = now;
    float dt = (now - lastTick) / 1000.0f;
    lastTick = now;

    // Smoothly approach target index
    float target = static_cast<float>(selectedIndex);
    float diff = target - animSelectedIndex;
    float step = animSpeed * dt;
    if (fabs(diff) <= step) {
        animSelectedIndex = target;
    } else {
        animSelectedIndex += (diff > 0 ? step : -step);
    }

    SDL_SetRenderDrawColor(renderer, 40, 40, 60, 120);
    int centerX = 1280 / 2;
    float lineHeight = 40.0f;
    float listHeight = items.size() * lineHeight;
    float baseY = 720 / 2 - listHeight / 2.0f;

    UiUtils::RenderTextCentered(renderer, font, title, centerX, 100, UiUtils::Color(255, 255, 255));

    for (size_t i = 0; i < items.size(); i++) {
        // Visual vertical offset for smooth movement
        float visualY = baseY + (static_cast<float>(i) - animSelectedIndex + selectedIndex) * lineHeight;
        // Slight scale/pulse for selected item
        bool isSelected = (i == static_cast<size_t>(selectedIndex));
        UiUtils::Color itemColor = isSelected ? UiUtils::Color(255, 255, 0) : UiUtils::Color(200, 200, 200);
        UiUtils::RenderTextCentered(renderer, font, items[i].first, centerX, static_cast<int>(visualY), itemColor);
    }
    UiUtils::RenderTextCentered(renderer, font, "Press A to select, B to go back", centerX, 720 - 40, UiUtils::Color(200, 200, 200));
}
void MenuScreen::handleInput(const SDL_Event& e, MenuSystem& menuSystem) {
    if (e.type == SDL_CONTROLLERBUTTONDOWN) {
        switch (e.cbutton.button) {
            case BUTTON_DPAD_UP:
                if (selectedIndex > 0) selectedIndex--;
                break;
            case BUTTON_DPAD_DOWN:
                if (selectedIndex < (int)items.size() - 1) selectedIndex++;
                break;
            case BUTTON_A: // A - select
                if (!items.empty()) items[selectedIndex].second();
                break;
            case BUTTON_B: // B - back
                menuSystem.popScreen();
                break;
        }
    }
}