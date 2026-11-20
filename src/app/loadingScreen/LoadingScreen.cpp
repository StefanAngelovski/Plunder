#include "include/LoadingScreen.h"
#include "../../utils/include/Theme.h"

LoadingScreen::LoadingScreen(const std::string& msg) : message(msg) {}
void LoadingScreen::render(SDL_Renderer* renderer, TTF_Font* font) {
    (void)font;
    using namespace UiUtils;
    const ThemeColors& t = Theme::getInstance().colors();
    DrawBackground(renderer);
    const int cx = SCREEN_W / 2, cy = SCREEN_H / 2 - 24;
    FillCircle(renderer, cx, cy, 46, WithAlpha(t.cardBackground, 200));
    DrawSpinner(renderer, cx, cy, 28, t.accentPrimary);
    RenderTextCentered(renderer, GetFont(22, true), message, cx, cy + 86, Color(t.textPrimary));
    DrawProgressBar(renderer, SDL_Rect{cx - 80, cy + 118, 160, 4}, -1.0f);
}
void LoadingScreen::handleInput(const SDL_Event& e, MenuSystem& menuSystem) { /* ignore input */ }