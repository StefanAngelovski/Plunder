#include "include/CarouselMenuScreen.h"

void CarouselMenuScreen::renderItem(SDL_Renderer* renderer, TTF_Font* font, int i, int x, int y, int w, int h, bool focused) {
    // Transparent background with square borders
    SDL_Color borderColor = focused ? SDL_Color{255, 255, 255, 255} : SDL_Color{180, 180, 180, 255};
    int borderThickness = focused ? 3 : 2;
    
    // Draw square border (no fill, no rounded corners)
    SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
    for (int t = 0; t < borderThickness; ++t) {
        SDL_Rect borderRect = {x - t, y - t, w + 2*t, h + 2*t};
        SDL_RenderDrawRect(renderer, &borderRect);
    }
    
    // Icon
    if (!items[i].imagePath.empty()) {
        SDL_Texture* tex = nullptr;
        auto it = imageCache.find(items[i].imagePath);
        if (it != imageCache.end()) tex = it->second;
        else {
            SDL_Surface* imgSurf = IMG_Load(items[i].imagePath.c_str());
            if (imgSurf) {
                tex = SDL_CreateTextureFromSurface(renderer, imgSurf);
                SDL_FreeSurface(imgSurf);
                if (tex) imageCache[items[i].imagePath] = tex;
            }
        }
        // Maintain aspect ratio
        if (tex) {
            int maxSize = w * 0.6;
            int texW = 0, texH = 0;
            SDL_QueryTexture(tex, NULL, NULL, &texW, &texH);
            float aspect = texW > 0 && texH > 0 ? (float)texW / texH : 1.0f;
            int drawW = maxSize, drawH = maxSize;
            if (aspect > 1.0f) drawH = (int)(maxSize / aspect);
            else drawW = (int)(maxSize * aspect);
            SDL_Rect imgRect = {x + (w-drawW)/2, y + (h-drawH)/2, drawW, drawH};
            SDL_RenderCopy(renderer, tex, NULL, &imgRect);
        }
    }
    // Label
    UiUtils::Color labelColor(255,255,255);
    UiUtils::RenderTextCentered(renderer, font, items[i].label, x + w/2, y + h + 32, labelColor);
}
void CarouselMenuScreen::clearItems() {
    items.clear();
    selectedIndex = 0;
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

CarouselMenuScreen::CarouselMenuScreen(const std::string& title) {
    moveSound = Mix_LoadWAV("sounds/move.wav");
    if (moveSound) Mix_VolumeChunk(moveSound, MIX_MAX_VOLUME);
}

void CarouselMenuScreen::addItem(const std::string& label, const std::string& imagePath, std::function<void()> action) {
    std::string img = imagePath;
    if (label == "Settings" && imagePath.empty()) {
        img = "images/mainmenu/settings.png";
    }
    if (label == "Exit" && imagePath.empty()) {
        img = "images/mainmenu/exit.png";
    }
    if (label == "Patch Notes" && imagePath.empty()) {
        img = "images/mainmenu/patchnotes.png";
    }
    if (label == "Rom Sites" && imagePath.empty()) {
        img = "images/mainmenu/romsites.png";
    }
    if (label == "Gamulator" && imagePath.empty()) {
        img = "images/consoles/gamulator.png";
    }
    if (label == "Hexrom" && imagePath.empty()) {
        img = "images/consoles/hexrom.png";
    }
    if (label == "Romspedia" && imagePath.empty()) {
        img = "images/consoles/romspedia.png";
    }
    items.push_back({label, img, action});
}

void CarouselMenuScreen::handleInput(const SDL_Event& e, MenuSystem& menuSystem) {
    if (e.type == SDL_CONTROLLERBUTTONDOWN) {
        if (e.cbutton.button == BUTTON_DPAD_LEFT) {
            if (selectedIndex > 0) {
                selectedIndex--;
                // Add a velocity impulse for snappier start
                animVelocity -= 6.0f;
                if (moveSound) { Mix_PlayChannel(-1, moveSound, 0); }
            }
        } else if (e.cbutton.button == BUTTON_DPAD_RIGHT) {
            if (selectedIndex < (int)items.size() - 1) {
                selectedIndex++;
                animVelocity += 6.0f;
                if (moveSound) { Mix_PlayChannel(-1, moveSound, 0); }
            }
        } else if (e.cbutton.button == BUTTON_B) { // Go back
            menuSystem.popScreen();
        } else if (e.cbutton.button == BUTTON_A) { // Select
            if (!items.empty()) items[selectedIndex].action();
        }
    }
}

bool CarouselMenuScreen::update() {
    Uint32 now = SDL_GetTicks();
    if (lastTick == 0) { lastTick = now; animSelectedIndex = (float)selectedIndex; }
    float dt = (now - lastTick) / 1000.0f;
    if (dt > 0.05f) dt = 0.05f; // clamp for stability
    lastTick = now;

    // Spring toward selectedIndex
    float target = (float)selectedIndex;
    float x = animSelectedIndex;
    float v = animVelocity;
    float force = (target - x) * springK - v * damping; // F = kx - cv
    v += force * dt;
    x += v * dt;
    animVelocity = v;
    animSelectedIndex = x;

    // Snap if close enough
    if (fabs(target - animSelectedIndex) < 0.001f && fabs(animVelocity) < 0.001f) {
        animSelectedIndex = target;
        animVelocity = 0.0f;
    }
    return true; // we updated
}

void CarouselMenuScreen::render(SDL_Renderer* renderer, TTF_Font* font) {
    // Teal background
    SDL_SetRenderDrawColor(renderer, 24, 153, 165, 255);
    SDL_RenderClear(renderer);

    // --- Animation step ---
    Uint32 now = SDL_GetTicks();
    if (lastTick == 0) { lastTick = now; animSelectedIndex = (float)selectedIndex; }
    float dt = (now - lastTick) / 1000.0f;
    if (dt > 0.05f) dt = 0.05f; // clamp
    lastTick = now;
    // Spring integration
    float target = (float)selectedIndex;
    float x = animSelectedIndex;
    float v = animVelocity;
    float force = (target - x) * springK - v * damping;
    v += force * dt;
    x += v * dt;
    animVelocity = v;
    animSelectedIndex = x;
    if (fabs(target - animSelectedIndex) < 0.001f && fabs(animVelocity) < 0.001f) { animSelectedIndex = target; animVelocity = 0.0f; }
    // ---------------------------------------------------------------

    int centerX = 1280 / 2;
    int centerY = 720 / 2 - 20; // Center vertically, slight upward bias for label/hints
    int boxW = 180, boxH = 180;
    int focusBoxW = 250, focusBoxH = 250; // slightly larger focus for stronger emphasis
    int spacing = 70; // tighter spacing so movement feels quicker

    int n = items.size();
    for (int i = 0; i < n; ++i) {
        float offset = i - animSelectedIndex;
        float dist = fabs(offset);

    float focusT = std::max(0.0f, 1.0f - dist);
    focusT = focusT * focusT; 
        int w = (int)(boxW + (focusBoxW - boxW) * focusT);
        int h = (int)(boxH + (focusBoxH - boxH) * focusT);
        int x = centerX + int(offset * (focusBoxW + spacing)) - w/2;
        int y = centerY - h/2;
        bool focused = (dist < 0.5f); // highlight when near center
        renderItem(renderer, font, i, x, y, w, h, focused);
    }

    // Draw button hints (subtle gray)
    UiUtils::RenderTextCentered(renderer, font, "A: Back   B: Select   D-pad: Move", centerX, 700, UiUtils::Color(200,200,200));
}