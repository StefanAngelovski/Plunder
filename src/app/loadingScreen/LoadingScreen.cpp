#include "include/LoadingScreen.h"

LoadingScreen::LoadingScreen(const std::string& msg) : message(msg) {}
void LoadingScreen::render(SDL_Renderer* renderer, TTF_Font* font) {
    // Animate loading circle
    frameCounter++;
    if (frameCounter > 2) { frameCounter = 0; animationFrame = (animationFrame + 1) % 60; }
    // Teal background (main menu color)
    SDL_SetRenderDrawColor(renderer, 32, 170, 180, 255);
    SDL_RenderClear(renderer);
    int centerX = 1280 / 2;
    int centerY = 720 / 2;
    // Draw animated loading circle
    int radius = 48;
    int dotRadius = 10;
    int numDots = 12;
    float angleStep = 2.0f * 3.1415926f / numDots;
    for (int i = 0; i < numDots; ++i) {
        float angle = i * angleStep - 3.1415926f / 2;
        float progress = (animationFrame % 60) / 60.0f;
        float fade = 0.3f + 0.7f * (float)(((i + animationFrame/5) % numDots) == 0);
        int x = centerX + (int)(radius * cos(angle));
        int y = centerY + (int)(radius * sin(angle));
        int alpha = (int)(180 + 75 * fade);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, alpha);
        for (int dx = -dotRadius; dx <= dotRadius; ++dx) {
            for (int dy = -dotRadius; dy <= dotRadius; ++dy) {
                if (dx*dx + dy*dy <= dotRadius*dotRadius) {
                    SDL_RenderDrawPoint(renderer, x + dx, y + dy);
                }
            }
        }
    }
    // Draw loading message below the circle
    UiUtils::RenderTextCentered(renderer, font, message, centerX, centerY + radius + 48, UiUtils::Color(255, 255, 255));
}
void LoadingScreen::handleInput(const SDL_Event& e, MenuSystem& menuSystem) { /* ignore input */ }