#include "include/UiUtils.h"


namespace UiUtils {
    // Color struct implementation
    Color::Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) : r(r), g(g), b(b), a(a) {}
    SDL_Color Color::toSDLColor() const { return {r, g, b, a}; }

    // --- Text Rendering Functions ---

    // Draws a single line of text centered at (x, y) in screen coordinates. The text baseline is centered both horizontally and vertically. Used for menu labels, buttons, etc.
    void RenderTextCentered(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y, const Color& color) {
        if (!font) return;
        int prevStyle = TTF_GetFontStyle(font);
        TTF_SetFontStyle(font, prevStyle | TTF_STYLE_BOLD);
        SDL_Surface* surface = TTF_RenderText_Solid(font, text.c_str(), color.toSDLColor());
        if (!surface) { TTF_SetFontStyle(font, prevStyle); return; }
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (!texture) { SDL_FreeSurface(surface); TTF_SetFontStyle(font, prevStyle); return; }
        SDL_Rect destRect = {x - surface->w / 2, y - surface->h / 2, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, NULL, &destRect);
        SDL_DestroyTexture(texture);
        SDL_FreeSurface(surface);
        TTF_SetFontStyle(font, prevStyle);
    }

    // Draws a single line of text at (x, y) in screen coordinates, with optional scaling. The top-left of the text is placed at (x, y). Used for labels, tooltips, etc.
    void RenderText(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y, const Color& color, float scale) {
        if (!font) return;
        int prevStyle = TTF_GetFontStyle(font);
        TTF_SetFontStyle(font, prevStyle | TTF_STYLE_BOLD);
        SDL_Surface* surface = TTF_RenderText_Solid(font, text.c_str(), color.toSDLColor());
        if (!surface) { TTF_SetFontStyle(font, prevStyle); return; }
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (!texture) { SDL_FreeSurface(surface); TTF_SetFontStyle(font, prevStyle); return; }
        SDL_Rect destRect = {x, y, int(surface->w * scale), int(surface->h * scale)};
        SDL_RenderCopy(renderer, texture, NULL, &destRect);
        SDL_DestroyTexture(texture);
        SDL_FreeSurface(surface);
        TTF_SetFontStyle(font, prevStyle);
    }

    // Draws multi-line text at (x, y), automatically wrapping lines to fit within maxWidth pixels. The top-left of the first line is placed at (x, y). Used for paragraphs, descriptions, etc.
    void RenderTextWrapped(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y, int maxWidth, const Color& color) {
        if (!font) return;
        int prevStyle = TTF_GetFontStyle(font);
        TTF_SetFontStyle(font, prevStyle | TTF_STYLE_BOLD);
        SDL_Surface* surface = TTF_RenderText_Blended_Wrapped(font, text.c_str(), color.toSDLColor(), maxWidth);
        if (!surface) { TTF_SetFontStyle(font, prevStyle); return; }
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (!texture) { SDL_FreeSurface(surface); TTF_SetFontStyle(font, prevStyle); return; }
        SDL_Rect destRect = {x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, NULL, &destRect);
        SDL_DestroyTexture(texture);
        SDL_FreeSurface(surface);
        TTF_SetFontStyle(font, prevStyle);
    }

    // Draws up to two lines of text, word-wrapped and centered within the given SDL_Rect box. If the text is too long, it is truncated with an ellipsis. Used for compact UI elements like cards or tiles.
    void RenderTextCenteredInBox(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, const SDL_Rect& box, const Color& color) {
        if (!font) return;
        int prevStyle = TTF_GetFontStyle(font);
        TTF_SetFontStyle(font, prevStyle | TTF_STYLE_BOLD);
        // Word wrap: split into lines that fit box.w, up to 2 lines
        std::vector<std::string> lines;
        std::istringstream iss(text);
        std::string word, current;
        while (iss >> word) {
            std::string test = current.empty() ? word : current + " " + word;
            int w = 0, h = 0;
            TTF_SizeText(font, test.c_str(), &w, &h);
            if (w > box.w && !current.empty()) {
                lines.push_back(current);
                current = word;
                if (lines.size() == 2) break;
            } else {
                current = test;
            }
        }
        if (!current.empty() && lines.size() < 2) lines.push_back(current);
        // If too many lines, truncate and add ellipsis
        if (lines.size() > 2) lines.resize(2);
        if (lines.size() == 2) {
            int w = 0, h = 0;
            TTF_SizeText(font, lines[1].c_str(), &w, &h);
            if (w > box.w) {
                // Truncate with ellipsis
                std::string& s = lines[1];
                while (!s.empty() && w > box.w) {
                    s.pop_back();
                    TTF_SizeText(font, (s + "...").c_str(), &w, &h);
                }
                s += "...";
            }
        }
        int totalH = 0, lineH = 0;
        for (const auto& l : lines) {
            int w = 0, h = 0;
            TTF_SizeText(font, l.c_str(), &w, &h);
            totalH += h;
            lineH = h;
        }
        int y = box.y + (box.h - totalH) / 2;
        for (const auto& l : lines) {
            int w = 0, h = 0;
            TTF_SizeText(font, l.c_str(), &w, &h);
            int x = box.x + (box.w - w) / 2;
            SDL_Surface* surface = TTF_RenderText_Solid(font, l.c_str(), color.toSDLColor());
            if (!surface) continue;
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (!texture) { SDL_FreeSurface(surface); continue; }
            SDL_Rect destRect = {x, y, surface->w, surface->h};
            SDL_RenderCopy(renderer, texture, NULL, &destRect);
            SDL_DestroyTexture(texture);
            SDL_FreeSurface(surface);
            y += h;
        }
        TTF_SetFontStyle(font, prevStyle);
    }

    // --- Utility Functions ---

    // Returns a string truncated to fit within maxWidth pixels and maxLines lines, adding an ellipsis if the text overflows. Used for previewing long text in limited space.
    std::string ClampTextToLines(const std::string& text, TTF_Font* font, int maxWidth, int maxLines) {
        if (!font || maxLines <= 0) return text;
        std::vector<std::string> lines;
        std::istringstream iss(text);
        std::string word, current;
        while (iss >> word && lines.size() < maxLines) {
            std::string test = current.empty() ? word : current + " " + word;
            int w = 0, h = 0;
            TTF_SizeText(font, test.c_str(), &w, &h);
            if (w > maxWidth && !current.empty()) {
                lines.push_back(current);
                current = word;
            } else {
                current = test;
            }
        }
        if (!current.empty() && lines.size() < maxLines) {
            lines.push_back(current);
        }
        // If we have the maximum number of lines and there's still text, add ellipsis
        if (lines.size() == maxLines && iss >> word) {
            std::string& lastLine = lines.back();
            int w = 0, h = 0;
            TTF_SizeText(font, (lastLine + "...").c_str(), &w, &h);
            while (w > maxWidth && !lastLine.empty()) {
                lastLine.pop_back();
                TTF_SizeText(font, (lastLine + "...").c_str(), &w, &h);
            }
            lastLine += "...";
        }
        std::string result;
        for (size_t i = 0; i < lines.size(); ++i) {
            if (i > 0) result += " ";
            result += lines[i];
        }
        return result;
    }

    // --- Shape Rendering Functions ---

    // Draws a filled rectangle with optional rounded corners (if radius > 0) using the given SDL_Color. The rectangle is filled, not stroked. Used for backgrounds, buttons, and UI panels.
    void DrawRoundedRect(SDL_Renderer* renderer, const SDL_Rect& rect, int radius, SDL_Color color) {
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        // If radius is 0 or too small, just draw a regular rectangle
        if (radius <= 0 || radius > rect.w / 2 || radius > rect.h / 2) {
            SDL_RenderFillRect(renderer, &rect);
            return;
        }
        // Draw the main rectangle (middle section)
        SDL_Rect mainRect = {rect.x, rect.y + radius, rect.w, rect.h - 2 * radius};
        SDL_RenderFillRect(renderer, &mainRect);
        // Draw top and bottom rectangles
        SDL_Rect topRect = {rect.x + radius, rect.y, rect.w - 2 * radius, radius};
        SDL_Rect bottomRect = {rect.x + radius, rect.y + rect.h - radius, rect.w - 2 * radius, radius};
        SDL_RenderFillRect(renderer, &topRect);
        SDL_RenderFillRect(renderer, &bottomRect);
    }

} 