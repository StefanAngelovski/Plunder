#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <cmath>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>


// Draw a rounded rectangle with the given color and corner radius
void DrawRoundedRect(SDL_Renderer* renderer, const SDL_Rect& rect, int radius, SDL_Color color);


namespace UiUtils {
    struct Color {
        uint8_t r, g, b, a;
        Color(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0, uint8_t a = 255);
        SDL_Color toSDLColor() const;
    };

    void RenderTextCentered(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y, const Color& color);
    void RenderText(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y, const Color& color, float scale = 1.0f);
    void RenderTextWrapped(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y, int maxWidth, const Color& color);
    void RenderTextCenteredInBox(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, const SDL_Rect& box, const Color& color);
    std::string ClampTextToLines(const std::string& text, TTF_Font* font, int maxWidth, int maxLines);
    void DrawRoundedRect(SDL_Renderer* renderer, const SDL_Rect& rect, int radius, SDL_Color color);
}