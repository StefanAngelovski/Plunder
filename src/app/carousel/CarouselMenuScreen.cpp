#include "include/CarouselMenuScreen.h"
#include "../../utils/include/Theme.h"

using namespace UiUtils;

CarouselMenuScreen::CarouselMenuScreen(const std::string& title) : title(title) {
    moveSound = Mix_LoadWAV("sounds/move.wav");
    if (moveSound) Mix_VolumeChunk(moveSound, MIX_MAX_VOLUME);
}

CarouselMenuScreen::~CarouselMenuScreen() {
    clearImageCache();
    if (moveSound) {
        Mix_FreeChunk(moveSound);
        moveSound = nullptr;
    }
}

void CarouselMenuScreen::clearImageCache() {
    for (auto& pair : imageCache) {
        if (pair.second) SDL_DestroyTexture(pair.second);
    }
    imageCache.clear();
}

SDL_Texture* CarouselMenuScreen::getImage(SDL_Renderer* renderer, const std::string& path) {
    if (path.empty()) return nullptr;
    auto it = imageCache.find(path);
    if (it != imageCache.end()) return it->second;
    SDL_Texture* tex = IMG_LoadTexture(renderer, path.c_str());
    if (tex) SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    imageCache[path] = tex;  // cache misses too, so a missing file isn't retried every frame
    return tex;
}

void CarouselMenuScreen::clearItems() {
    items.clear();
    selectedIndex = 0;
}

void CarouselMenuScreen::setSelectedIndex(int index) {
    if (index >= 0 && index < (int)items.size()) {
        selectedIndex = index;
        animSelectedIndex = (float)index;
        animVelocity = 0.0f;
    }
}

void CarouselMenuScreen::addItem(const std::string& label, const std::string& imagePath, std::function<void()> action) {
    std::string img = imagePath;
    if (img.empty()) {
        if (label == "Settings") img = "images/mainmenu/settings.png";
        else if (label == "Exit") img = "images/mainmenu/exit.png";
        else if (label == "Patch Notes") img = "images/mainmenu/patchnotes.png";
        else if (label == "Rom Sites") img = "images/mainmenu/romsites.png";
        else if (label == "Hexrom") img = "images/consoles/hexrom.png";
        else if (label == "Romspedia") img = "images/consoles/romspedia.png";
    }
    items.push_back({label, img, action});
}

void CarouselMenuScreen::handleInput(const SDL_Event& e, MenuSystem& menuSystem) {
    if (e.type != SDL_CONTROLLERBUTTONDOWN) return;
    if (e.cbutton.button == BUTTON_DPAD_LEFT) {
        if (selectedIndex > 0) {
            selectedIndex--;
            animVelocity -= 6.0f;
            if (moveSound) Mix_PlayChannel(-1, moveSound, 0);
        }
    } else if (e.cbutton.button == BUTTON_DPAD_RIGHT) {
        if (selectedIndex < (int)items.size() - 1) {
            selectedIndex++;
            animVelocity += 6.0f;
            if (moveSound) Mix_PlayChannel(-1, moveSound, 0);
        }
    } else if (e.cbutton.button == BUTTON_B) {
        menuSystem.popScreen();
    } else if (e.cbutton.button == BUTTON_A) {
        if (!items.empty()) items[selectedIndex].action();
    }
}

bool CarouselMenuScreen::update() {
    Uint32 now = SDL_GetTicks();
    if (lastTick == 0) { lastTick = now; animSelectedIndex = (float)selectedIndex; }
    float dt = (now - lastTick) / 1000.0f;
    if (dt > 0.05f) dt = 0.05f;
    lastTick = now;

    float target = (float)selectedIndex;
    float force = (target - animSelectedIndex) * springK - animVelocity * damping;
    animVelocity += force * dt;
    animSelectedIndex += animVelocity * dt;
    if (std::fabs(target - animSelectedIndex) < 0.001f && std::fabs(animVelocity) < 0.001f) {
        animSelectedIndex = target;
        animVelocity = 0.0f;
    }
    return true;
}

// Mini window mock-up in a theme's colours.
void CarouselMenuScreen::renderSwatches(SDL_Renderer* renderer, const SDL_Rect& a, const std::vector<SDL_Color>& sw, Uint8 alpha) {
    if (sw.size() < 4) return;
    auto A = [alpha](SDL_Color c) { return WithAlpha(c, (Uint8)(c.a * alpha / 255)); };
    int r = std::max(6, a.w / 12);
    FillRoundedRect(renderer, a, r, A(sw[0]));
    int pad = a.w / 10;
    int barH = std::max(4, a.h / 14);
    // title bar line + accent dot
    FillRoundedRect(renderer, SDL_Rect{a.x + pad, a.y + pad, a.w / 2, barH}, barH / 2, A(sw[3]));
    FillCircle(renderer, a.x + a.w - pad - barH, a.y + pad + barH / 2, barH, A(sw[2]));
    // three cards, the middle one focused
    int gap = std::max(4, a.w / 22);
    int cw = (a.w - 2 * pad - 2 * gap) / 3;
    int ch = a.h - 3 * pad - barH;
    for (int k = 0; k < 3; ++k) {
        SDL_Rect c{a.x + pad + k * (cw + gap), a.y + 2 * pad + barH, cw, ch};
        FillRoundedRect(renderer, c, std::max(3, r / 2), A(sw[1]));
        if (k == 1) StrokeRoundedRect(renderer, c, std::max(3, r / 2), 2, A(sw[2]));
    }
}

void CarouselMenuScreen::renderCard(SDL_Renderer* renderer, int i, const SDL_Rect& rect, float focusT, Uint8 alpha) {
    const ThemeColors& t = Theme::getInstance().colors();
    CarouselItemDecor decor = decorate(i);
    int radius = rect.w / 8;
    auto A = [alpha](SDL_Color c) { return WithAlpha(c, (Uint8)(c.a * alpha / 255)); };

    DrawShadow(renderer, rect, radius, 26 + (int)(14 * focusT), (Uint8)((70 + 70 * focusT) * alpha / 255), 10 + (int)(6 * focusT));
    SDL_Color fill = Mix(t.cardBackground, t.accentPrimary, 0.12f * focusT);
    FillRoundedRect(renderer, rect, radius, A(fill));
    StrokeRoundedRect(renderer, rect, radius, 1, A(WithAlpha(t.textPrimary, 22)));
    if (focusT > 0.01f) DrawFocusRing(renderer, rect, radius, WithAlpha(t.accentPrimary, (Uint8)(255 * focusT * alpha / 255)), 3, 4);

    if (!decor.swatches.empty()) {
        int sw = (int)(rect.w * 0.66f), sh = (int)(rect.h * 0.5f);
        renderSwatches(renderer, SDL_Rect{rect.x + (rect.w - sw) / 2, rect.y + (rect.h - sh) / 2, sw, sh}, decor.swatches, alpha);
    } else if (SDL_Texture* tex = getImage(renderer, items[i].imagePath)) {
        int texW = 0, texH = 0;
        SDL_QueryTexture(tex, nullptr, nullptr, &texW, &texH);
        float aspect = (texW > 0 && texH > 0) ? (float)texW / texH : 1.0f;
        int maxSize = (int)(rect.w * (aspect > 1.6f ? 0.72f : 0.44f));  // wide logos (e.g. wordmarks) need room
        int dw = maxSize, dh = maxSize;
        if (aspect > 1.0f) dh = (int)(maxSize / aspect); else dw = (int)(maxSize * aspect);
        SDL_Rect img{rect.x + (rect.w - dw) / 2, rect.y + (rect.h - dh) / 2, dw, dh};
        SDL_SetTextureAlphaMod(tex, alpha);
        SDL_RenderCopyEx(renderer, tex, nullptr, &img, decor.iconAngle, nullptr, SDL_FLIP_NONE);
    }

    if (!decor.badge.empty()) {
        TTF_Font* bf = GetFont(14, true);
        int bw = PillWidth(bf, decor.badge, 10);
        SDL_Color bg = decor.badgeOn ? t.accentPrimary : Lighten(t.cardBackground, 0.14f);
        SDL_Color fg = decor.badgeOn ? t.backgroundDark : t.textSecondary;
        DrawPill(renderer, bf, decor.badge, rect.x + rect.w - bw - 12, rect.y + 12, A(bg), A(fg), 10, 24);
    }

    if (decor.flash >= 0.0f && decor.flash <= 1.0f) {
        // Diagonal-ish light sweep across the card.
        int bandH = rect.h / 3;
        int cy = rect.y - bandH + (int)((rect.h + 2 * bandH) * decor.flash);
        for (int k = -bandH / 2; k < bandH / 2; k += 2) {
            int y = cy + k;
            if (y < rect.y + 4 || y > rect.y + rect.h - 6) continue;
            float f = 1.0f - std::fabs((float)k) / (bandH / 2.0f);
            FillRoundedRect(renderer, SDL_Rect{rect.x + 6, y, rect.w - 12, 2}, 0, WithAlpha(SDL_Color{255, 255, 255, 255}, (Uint8)(110 * f)));
        }
    }

    // Labels under the card
    int labelY = rect.y + rect.h + 24 + (int)(10 * focusT);
    TTF_Font* lf = focusT > 0.5f ? GetFont(22, true) : GetFont(19);
    SDL_Color lc = Mix(t.textSecondary, t.textPrimary, focusT);
    RenderTextCentered(renderer, lf, Ellipsize(lf, items[i].label, 300), rect.x + rect.w / 2, labelY, Color(A(lc)));
    if (!decor.sublabel.empty()) {
        TTF_Font* sf = GetFont(16);
        RenderTextCentered(renderer, sf, decor.sublabel, rect.x + rect.w / 2, labelY + 28, Color(A(WithAlpha(t.textSecondary, 220))));
    }
}

void CarouselMenuScreen::render(SDL_Renderer* renderer, TTF_Font* font) {
    (void)font;
    const ThemeColors& t = Theme::getInstance().colors();
    DrawBackground(renderer);
    DrawHeader(renderer, title, subtitle);

    const int centerX = SCREEN_W / 2;
    const int centerY = 352;
    const int baseSize = 196, focusSize = 252;
    const int step = focusSize + 52;

    int n = (int)items.size();
    // Draw far items first so the focused card ends up on top.
    std::vector<int> order(n);
    for (int i = 0; i < n; ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [this](int a, int b) {
        return std::fabs(a - animSelectedIndex) > std::fabs(b - animSelectedIndex);
    });
    for (int i : order) {
        float offset = i - animSelectedIndex;
        float dist = std::fabs(offset);
        float focusT = std::max(0.0f, 1.0f - dist);
        focusT = focusT * focusT * (3.0f - 2.0f * focusT);
        int size = (int)(baseSize + (focusSize - baseSize) * focusT);
        int x = centerX + (int)(offset * step) - size / 2;
        if (x > SCREEN_W + 40 || x + size < -40) continue;
        Uint8 alpha = (Uint8)(255 * std::max(0.35f, 1.0f - 0.28f * std::max(0.0f, dist - 0.5f)));
        renderCard(renderer, i, SDL_Rect{x, centerY - size / 2, size, size}, focusT, alpha);
    }

    // Position dots
    if (n > 1) {
        int dotY = 590, gap = 10, dotW = 8, activeW = 26;
        int total = (n - 1) * (dotW + gap) + activeW;
        int x = centerX - total / 2;
        for (int i = 0; i < n; ++i) {
            bool active = (i == selectedIndex);
            int w = active ? activeW : dotW;
            FillRoundedRect(renderer, SDL_Rect{x, dotY, w, dotW}, dotW / 2, active ? t.accentPrimary : WithAlpha(t.textSecondary, 70));
            x += w + gap;
        }
    }

    std::vector<Hint> hints;
    if (n > 1) hints.push_back({"DPAD", "Navigate"});
    hints.push_back({"A", "Select"});
    if (showBackHint) hints.push_back({"B", "Back"});
    DrawHints(renderer, hints);

    if (UpdateChecker::hasCheckedForUpdates() && UpdateChecker::isUpdateAvailable()) {
        TTF_Font* f = GetFont(16, true);
        std::string text = "Update available: " + UpdateChecker::getLatestVersion();
        DrawPill(renderer, f, text, MARGIN_X, SCREEN_H - FOOTER_H / 2 - 15, WithAlpha(t.accentPrimary, 40), t.textHighlight, 14, 30);
    }
}
