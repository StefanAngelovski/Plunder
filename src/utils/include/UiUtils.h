#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <cmath>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

// ---------------------------------------------------------------------------
// UiUtils: drawing toolkit shared by every screen.
//
// - Text is anti-aliased (UTF-8, blended) and cached as textures, so drawing
//   the same label every frame is a single texture copy.
// - Fonts are opened once per (size, weight) via GetFont(); headings use real
//   larger font sizes instead of stretching a 20px texture.
// - Shapes (rounded fills, outlines, soft shadows, corner masks) are built
//   from small cached anti-aliased textures drawn as 9-slices, so they are
//   cheap on the device GPU and have smooth edges.
// - Components (background, header, button-hint footer, scrollbar, progress
//   bar, spinner, pills) give all screens one consistent look, coloured by the
//   active Theme.
// ---------------------------------------------------------------------------

namespace UiUtils {
    struct Color {
        uint8_t r, g, b, a;
        Color(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0, uint8_t a = 255);
        Color(const SDL_Color& c);
        SDL_Color toSDLColor() const;
    };

    // Screen geometry shared by all layouts.
    constexpr int SCREEN_W = 1280;
    constexpr int SCREEN_H = 720;
    constexpr int MARGIN_X = 48;      // left/right page margin
    constexpr int HEADER_H = 104;     // content starts below this
    constexpr int FOOTER_H = 60;      // content ends above SCREEN_H - FOOTER_H

    // --- Fonts ---------------------------------------------------------------
    TTF_Font* GetFont(int size, bool bold = false);
    void ShutdownFonts();
    int TextWidth(TTF_Font* font, const std::string& text);
    int TextHeight(TTF_Font* font);

    // --- Text (legacy API, now anti-aliased + cached) ------------------------
    void RenderTextCentered(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y, const Color& color);
    void RenderText(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y, const Color& color, float scale = 1.0f);
    void RenderTextRight(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y, const Color& color);
    void RenderTextWrapped(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y, int maxWidth, const Color& color);
    void RenderTextCenteredInBox(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, const SDL_Rect& box, const Color& color);
    std::string ClampTextToLines(const std::string& text, TTF_Font* font, int maxWidth, int maxLines);

    // Text helpers
    // Draws text with its top-left at (x, y); returns drawn width.
    int DrawText(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y, SDL_Color color);
    // Truncates with an ellipsis so the text fits in maxWidth pixels.
    std::string Ellipsize(TTF_Font* font, const std::string& text, int maxWidth);
    // Greedy word wrap into lines no wider than maxWidth (keeps explicit newlines).
    std::vector<std::string> WrapText(TTF_Font* font, const std::string& text, int maxWidth);
    // "Atari 2600 (A2600) (1432)" -> name "Atari 2600 (A2600)", count "1432" (count empty if none).
    void SplitTrailingCount(const std::string& label, std::string& name, std::string& count);
    std::string StripTrailingCount(const std::string& label);

    // --- Colour helpers ------------------------------------------------------
    SDL_Color Mix(SDL_Color a, SDL_Color b, float t);
    SDL_Color WithAlpha(SDL_Color c, Uint8 a);
    SDL_Color Lighten(SDL_Color c, float t);   // toward white
    SDL_Color Darken(SDL_Color c, float t);    // toward black

    // --- Shapes ----------------------------------------------------------------
    void FillRoundedRect(SDL_Renderer* renderer, const SDL_Rect& rect, int radius, SDL_Color color);
    void StrokeRoundedRect(SDL_Renderer* renderer, const SDL_Rect& rect, int radius, int thickness, SDL_Color color);
    void DrawShadow(SDL_Renderer* renderer, const SDL_Rect& rect, int radius, int blur, Uint8 alpha, int offsetY = 6);
    // Paints `color` outside the rounded corners of rect: rounds the corners of
    // an image that sits on a solid surface of that colour.
    void MaskCorners(SDL_Renderer* renderer, const SDL_Rect& rect, int radius, SDL_Color color);
    void FillCircle(SDL_Renderer* renderer, int cx, int cy, int radius, SDL_Color color);
    void FillVerticalGradient(SDL_Renderer* renderer, const SDL_Rect& rect, SDL_Color top, SDL_Color bottom);
    // Legacy name kept for existing callers; now truly rounded and anti-aliased.
    void DrawRoundedRect(SDL_Renderer* renderer, const SDL_Rect& rect, int radius, SDL_Color color);

    // --- Components ------------------------------------------------------------
    // Themed full-screen background (gradient + soft accent glow).
    void DrawBackground(SDL_Renderer* renderer);
    // Page header: large title, optional subtitle below it, optional pill on the right.
    void DrawHeader(SDL_Renderer* renderer, const std::string& title, const std::string& subtitle = "", const std::string& rightPill = "");

    // Controller hint chips shown bottom-right ("A Select", "B Back", ...).
    // button: "A", "B", "X", "Y", "L", "R", "LR", or "DPAD".
    struct Hint { std::string button; std::string label; };
    void DrawHints(SDL_Renderer* renderer, const std::vector<Hint>& hints);
    // Small status text bottom-left of the footer (page info, loading, ...).
    void DrawFooterStatus(SDL_Renderer* renderer, const std::string& text, bool spinner = false);

    // Vertical scrollbar: viewFrac = visible/total, posFrac = 0..1 scroll position.
    void DrawScrollbar(SDL_Renderer* renderer, int x, int y, int h, float viewFrac, float posFrac);
    // progress < 0 draws an indeterminate (animated) bar.
    void DrawProgressBar(SDL_Renderer* renderer, const SDL_Rect& rect, float progress);
    void DrawSpinner(SDL_Renderer* renderer, int cx, int cy, int radius, SDL_Color color);
    // Rounded pill with text; returns its width. (x, y) is the top-left.
    int DrawPill(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y, SDL_Color bg, SDL_Color fg, int padX = 12, int height = 30);
    int PillWidth(TTF_Font* font, const std::string& text, int padX = 12);
    // Fade scrolled content into the background at the top/bottom edge of a list.
    void DrawEdgeFade(SDL_Renderer* renderer, int y, int h, bool fadeDown);
    // Dim the whole screen behind a modal.
    void DrawScrim(SDL_Renderer* renderer, Uint8 alpha = 170);
    // Elevated panel used for modals and content cards.
    void DrawPanel(SDL_Renderer* renderer, const SDL_Rect& rect, int radius = 20, bool elevated = true);
    // Focus ring drawn just outside rect.
    void DrawFocusRing(SDL_Renderer* renderer, const SDL_Rect& rect, int radius, SDL_Color color, int thickness = 3, int gap = 3);
}
