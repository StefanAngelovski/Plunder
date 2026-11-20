#include "include/UiUtils.h"
#include "include/Theme.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
#include <tuple>
#include <unordered_map>

namespace UiUtils {

// ============================================================================
// Colour
// ============================================================================
Color::Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) : r(r), g(g), b(b), a(a) {}
Color::Color(const SDL_Color& c) : r(c.r), g(c.g), b(c.b), a(c.a) {}
SDL_Color Color::toSDLColor() const { return {r, g, b, a}; }

static inline float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }

SDL_Color Mix(SDL_Color a, SDL_Color b, float t) {
    t = clamp01(t);
    return SDL_Color{
        (Uint8)(a.r + (b.r - a.r) * t), (Uint8)(a.g + (b.g - a.g) * t),
        (Uint8)(a.b + (b.b - a.b) * t), (Uint8)(a.a + (b.a - a.a) * t)};
}
SDL_Color WithAlpha(SDL_Color c, Uint8 a) { c.a = a; return c; }
SDL_Color Lighten(SDL_Color c, float t) { return Mix(c, SDL_Color{255, 255, 255, c.a}, t); }
SDL_Color Darken(SDL_Color c, float t) { return Mix(c, SDL_Color{0, 0, 0, c.a}, t); }

static const ThemeColors& T() { return Theme::getInstance().colors(); }

// ============================================================================
// Fonts
// ============================================================================
namespace {
struct FontEntry { int size; bool bold; TTF_Font* font; };
std::vector<FontEntry> g_fonts;
const char* kFontPath = "res/InterVariable.ttf";
const char* kFallbackFontPath = "res/arial.ttf";

const FontEntry* fontInfo(TTF_Font* f) {
    for (auto& e : g_fonts) if (e.font == f) return &e;
    return nullptr;
}
}  // namespace

TTF_Font* GetFont(int size, bool bold) {
    if (size < 6) size = 6;
    for (auto& e : g_fonts) if (e.size == size && e.bold == bold) return e.font;
    TTF_Font* f = TTF_OpenFont(kFontPath, size);
    if (!f) f = TTF_OpenFont(kFallbackFontPath, size);
    if (!f) return nullptr;
    if (bold) TTF_SetFontStyle(f, TTF_STYLE_BOLD);
    TTF_SetFontHinting(f, TTF_HINTING_LIGHT);
    g_fonts.push_back({size, bold, f});
    return f;
}

void ShutdownFonts() {
    for (auto& e : g_fonts) TTF_CloseFont(e.font);
    g_fonts.clear();
}

int TextWidth(TTF_Font* font, const std::string& text) {
    if (!font || text.empty()) return 0;
    int w = 0, h = 0;
    TTF_SizeUTF8(font, text.c_str(), &w, &h);
    return w;
}

int TextHeight(TTF_Font* font) { return font ? TTF_FontHeight(font) : 0; }

// ============================================================================
// Text texture cache
// ============================================================================
namespace {
struct TextTex { SDL_Texture* tex; int w; int h; Uint32 lastUsed; };
std::unordered_map<std::string, TextTex> g_text;
constexpr size_t kTextCacheSoftLimit = 700;

void evictText(Uint32 now) {
    for (auto it = g_text.begin(); it != g_text.end();) {
        if (now - it->second.lastUsed > 2000) {
            SDL_DestroyTexture(it->second.tex);
            it = g_text.erase(it);
        } else {
            ++it;
        }
    }
    if (g_text.size() > kTextCacheSoftLimit) {
        for (auto& kv : g_text) SDL_DestroyTexture(kv.second.tex);
        g_text.clear();
    }
}

const TextTex* getText(SDL_Renderer* r, TTF_Font* f, const std::string& s, SDL_Color c, int wrap = 0) {
    if (!r || !f || s.empty()) return nullptr;
    char prefix[64];
    snprintf(prefix, sizeof(prefix), "%p|%02x%02x%02x|%d|", (void*)f, c.r, c.g, c.b, wrap);
    std::string key = prefix + s;
    Uint32 now = SDL_GetTicks();
    auto it = g_text.find(key);
    if (it != g_text.end()) { it->second.lastUsed = now; return &it->second; }
    if (g_text.size() >= kTextCacheSoftLimit) evictText(now);

    SDL_Color opaque{c.r, c.g, c.b, 255};
    SDL_Surface* surf = wrap > 0 ? TTF_RenderUTF8_Blended_Wrapped(f, s.c_str(), opaque, wrap)
                                 : TTF_RenderUTF8_Blended(f, s.c_str(), opaque);
    if (!surf) return nullptr;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    TextTex entry{tex, surf->w, surf->h, now};
    SDL_FreeSurface(surf);
    if (!tex) return nullptr;
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    auto res = g_text.emplace(std::move(key), entry);
    return &res.first->second;
}

void blitText(SDL_Renderer* r, const TextTex* t, int x, int y, Uint8 alpha, int w = -1, int h = -1) {
    if (!t) return;
    SDL_SetTextureAlphaMod(t->tex, alpha);
    SDL_Rect dst{x, y, w < 0 ? t->w : w, h < 0 ? t->h : h};
    SDL_RenderCopy(r, t->tex, nullptr, &dst);
}
}  // namespace

int DrawText(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y, SDL_Color color) {
    const TextTex* t = getText(renderer, font, text, color);
    if (!t) return 0;
    blitText(renderer, t, x, y, color.a);
    return t->w;
}

std::string Ellipsize(TTF_Font* font, const std::string& text, int maxWidth) {
    if (!font || TextWidth(font, text) <= maxWidth) return text;
    std::string s = text;
    while (!s.empty()) {
        // drop one UTF-8 code point
        size_t cut = s.size() - 1;
        while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80) --cut;
        s.erase(cut);
        while (!s.empty() && s.back() == ' ') s.pop_back();
        if (TextWidth(font, s + "…") <= maxWidth) return s + "…";
    }
    return "…";
}

std::vector<std::string> WrapText(TTF_Font* font, const std::string& text, int maxWidth) {
    std::vector<std::string> lines;
    std::istringstream paragraphs(text);
    std::string paragraph;
    while (std::getline(paragraphs, paragraph)) {
        std::istringstream words(paragraph);
        std::string word, line;
        bool any = false;
        while (words >> word) {
            any = true;
            std::string test = line.empty() ? word : line + " " + word;
            if (!line.empty() && TextWidth(font, test) > maxWidth) {
                lines.push_back(line);
                line = word;
            } else {
                line = test;
            }
        }
        if (any) lines.push_back(line);
        else lines.push_back("");
    }
    return lines;
}

void SplitTrailingCount(const std::string& label, std::string& name, std::string& count) {
    name = label; count.clear();
    size_t end = label.find_last_not_of(' ');
    if (end == std::string::npos || label[end] != ')') return;
    size_t open = label.rfind('(', end);
    if (open == std::string::npos || open + 1 >= end) return;
    for (size_t i = open + 1; i < end; ++i) if (!std::isdigit((unsigned char)label[i])) return;
    count = label.substr(open + 1, end - open - 1);
    name = label.substr(0, open);
    while (!name.empty() && name.back() == ' ') name.pop_back();
}

std::string StripTrailingCount(const std::string& label) {
    std::string name, count;
    SplitTrailingCount(label, name, count);
    return name;
}

// ---- legacy text API ---------------------------------------------------------
void RenderTextCentered(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y, const Color& color) {
    const TextTex* t = getText(renderer, font, text, color.toSDLColor());
    if (t) blitText(renderer, t, x - t->w / 2, y - t->h / 2, color.a);
}

void RenderText(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y, const Color& color, float scale) {
    if (std::fabs(scale - 1.0f) > 0.01f) {
        // Render at a real larger size instead of stretching a small texture.
        if (const FontEntry* info = fontInfo(font)) {
            TTF_Font* scaled = GetFont((int)std::lround(info->size * scale), info->bold);
            if (scaled) font = scaled, scale = 1.0f;
        }
    }
    const TextTex* t = getText(renderer, font, text, color.toSDLColor());
    if (!t) return;
    if (scale == 1.0f) blitText(renderer, t, x, y, color.a);
    else blitText(renderer, t, x, y, color.a, (int)(t->w * scale), (int)(t->h * scale));
}

void RenderTextRight(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y, const Color& color) {
    const TextTex* t = getText(renderer, font, text, color.toSDLColor());
    if (t) blitText(renderer, t, x - t->w, y - t->h / 2, color.a);
}

void RenderTextWrapped(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y, int maxWidth, const Color& color) {
    const TextTex* t = getText(renderer, font, text, color.toSDLColor(), std::max(1, maxWidth));
    if (t) blitText(renderer, t, x, y, color.a);
}

void RenderTextCenteredInBox(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, const SDL_Rect& box, const Color& color) {
    if (!font) return;
    std::vector<std::string> lines = WrapText(font, text, box.w);
    if (lines.size() > 2) {
        std::string rest = lines[1];
        for (size_t i = 2; i < lines.size(); ++i) rest += " " + lines[i];
        lines.resize(2);
        lines[1] = Ellipsize(font, rest, box.w);
    }
    for (auto& l : lines) l = Ellipsize(font, l, box.w);
    int lineH = TextHeight(font);
    int y = box.y + (box.h - lineH * (int)lines.size()) / 2;
    for (const auto& l : lines) {
        const TextTex* t = getText(renderer, font, l, color.toSDLColor());
        if (t) blitText(renderer, t, box.x + (box.w - t->w) / 2, y, color.a);
        y += lineH;
    }
}

std::string ClampTextToLines(const std::string& text, TTF_Font* font, int maxWidth, int maxLines) {
    if (!font || maxLines <= 0) return text;
    std::vector<std::string> lines = WrapText(font, text, maxWidth);
    if ((int)lines.size() <= maxLines) {
        std::string out;
        for (size_t i = 0; i < lines.size(); ++i) out += (i ? " " : "") + lines[i];
        return out;
    }
    std::string out;
    for (int i = 0; i < maxLines - 1; ++i) out += (i ? " " : "") + lines[i];
    std::string rest;
    for (size_t i = maxLines - 1; i < lines.size(); ++i) rest += (rest.empty() ? "" : " ") + lines[i];
    out += (out.empty() ? "" : " ") + Ellipsize(font, rest, maxWidth);
    return out;
}

// ============================================================================
// Shape textures (anti-aliased, generated once, drawn as 9-slices)
// ============================================================================
namespace {
enum ShapeKind { SK_FILL = 0, SK_RING = 1, SK_SHADOW = 2, SK_INVERSE = 3 };
struct ShapeTex { SDL_Texture* tex; int corner; };
std::map<std::tuple<int, int, int>, ShapeTex> g_shapes;

const ShapeTex* getShape(SDL_Renderer* ren, int kind, int r, int p) {
    auto key = std::make_tuple(kind, r, p);
    auto it = g_shapes.find(key);
    if (it != g_shapes.end()) return &it->second;

    const int pad = (kind == SK_SHADOW) ? p : 0;
    const int K = r + pad;          // corner size in texture pixels
    const int S = 2 * K + 2;        // 2px stretchable middle
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, S, S, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!s) return nullptr;
    const float c = S / 2.0f;
    const float half = (S - 2 * pad) / 2.0f;
    Uint32* px = static_cast<Uint32*>(s->pixels);
    for (int y = 0; y < S; ++y) {
        for (int x = 0; x < S; ++x) {
            float qx = std::fabs(x + 0.5f - c) - (half - r);
            float qy = std::fabs(y + 0.5f - c) - (half - r);
            float ox = std::max(qx, 0.f), oy = std::max(qy, 0.f);
            float d = std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.f) - r;
            float a = 0.f;
            switch (kind) {
                case SK_FILL: a = clamp01(0.5f - d); break;
                case SK_RING: a = clamp01(0.5f - d) * clamp01(0.5f + d + p); break;
                case SK_SHADOW: {
                    float t = clamp01((d + p * 0.35f) / (p * 1.35f));
                    a = 1.f - t * t * (3.f - 2.f * t);
                    break;
                }
                case SK_INVERSE: a = 1.f - clamp01(0.5f - d); break;
            }
            px[y * (s->pitch / 4) + x] = ((Uint32)(a * 255.f + 0.5f) << 24) | 0x00FFFFFFu;
        }
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, s);
    SDL_FreeSurface(s);
    if (!tex) return nullptr;
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(tex, kind == SK_SHADOW ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
    auto res = g_shapes.emplace(key, ShapeTex{tex, K});
    return &res.first->second;
}

enum NineParts { NP_CORNERS = 1, NP_EDGES = 2, NP_CENTER = 4, NP_ALL = 7 };

void drawNine(SDL_Renderer* ren, const ShapeTex* st, const SDL_Rect& d, SDL_Color c, int parts = NP_ALL) {
    if (!st || d.w <= 0 || d.h <= 0) return;
    const int K = st->corner;
    const int k = std::max(0, std::min(K, std::min(d.w / 2, d.h / 2)));
    SDL_SetTextureColorMod(st->tex, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(st->tex, c.a);
    const int sx[3] = {0, K, K + 2}, sw[3] = {K, 2, K};
    const int dx[3] = {d.x, d.x + k, d.x + d.w - k}, dw[3] = {k, d.w - 2 * k, k};
    const int dy[3] = {d.y, d.y + k, d.y + d.h - k}, dh[3] = {k, d.h - 2 * k, k};
    for (int j = 0; j < 3; ++j) {
        for (int i = 0; i < 3; ++i) {
            bool corner = (i != 1 && j != 1), center = (i == 1 && j == 1);
            int part = corner ? NP_CORNERS : (center ? NP_CENTER : NP_EDGES);
            if (!(parts & part) || dw[i] <= 0 || dh[j] <= 0) continue;
            SDL_Rect src{sx[i], sx[j], sw[i], sw[j]};
            SDL_Rect dst{dx[i], dy[j], dw[i], dh[j]};
            SDL_RenderCopy(ren, st->tex, &src, &dst);
        }
    }
}

void fillRectBlend(SDL_Renderer* r, const SDL_Rect& rect, SDL_Color c) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(r, &rect);
}
}  // namespace

void FillRoundedRect(SDL_Renderer* renderer, const SDL_Rect& rect, int radius, SDL_Color color) {
    if (rect.w <= 0 || rect.h <= 0) return;
    radius = std::min(radius, std::min(rect.w, rect.h) / 2);
    if (radius <= 0) { fillRectBlend(renderer, rect, color); return; }
    drawNine(renderer, getShape(renderer, SK_FILL, radius, 0), rect, color);
}

void StrokeRoundedRect(SDL_Renderer* renderer, const SDL_Rect& rect, int radius, int thickness, SDL_Color color) {
    if (rect.w <= 0 || rect.h <= 0 || thickness <= 0) return;
    radius = std::min(radius, std::min(rect.w, rect.h) / 2);
    if (radius < thickness) {
        SDL_Rect t{rect.x, rect.y, rect.w, thickness}, b{rect.x, rect.y + rect.h - thickness, rect.w, thickness};
        SDL_Rect l{rect.x, rect.y + thickness, thickness, rect.h - 2 * thickness}, r{rect.x + rect.w - thickness, rect.y + thickness, thickness, rect.h - 2 * thickness};
        fillRectBlend(renderer, t, color); fillRectBlend(renderer, b, color);
        fillRectBlend(renderer, l, color); fillRectBlend(renderer, r, color);
        return;
    }
    drawNine(renderer, getShape(renderer, SK_RING, radius, thickness), rect, color, NP_CORNERS | NP_EDGES);
}

void DrawShadow(SDL_Renderer* renderer, const SDL_Rect& rect, int radius, int blur, Uint8 alpha, int offsetY) {
    if (blur <= 0 || alpha == 0) return;
    radius = std::max(0, std::min(radius, std::min(rect.w, rect.h) / 2));
    SDL_Rect d{rect.x - blur, rect.y - blur + offsetY, rect.w + 2 * blur, rect.h + 2 * blur};
    drawNine(renderer, getShape(renderer, SK_SHADOW, radius, blur), d, SDL_Color{0, 0, 0, alpha});
}

void MaskCorners(SDL_Renderer* renderer, const SDL_Rect& rect, int radius, SDL_Color color) {
    radius = std::min(radius, std::min(rect.w, rect.h) / 2);
    if (radius <= 0) return;
    drawNine(renderer, getShape(renderer, SK_INVERSE, radius, 0), rect, color, NP_CORNERS);
}

void FillCircle(SDL_Renderer* renderer, int cx, int cy, int radius, SDL_Color color) {
    FillRoundedRect(renderer, SDL_Rect{cx - radius, cy - radius, radius * 2, radius * 2}, radius, color);
}

void DrawRoundedRect(SDL_Renderer* renderer, const SDL_Rect& rect, int radius, SDL_Color color) {
    FillRoundedRect(renderer, rect, radius, color);
}

namespace {
std::map<std::pair<Uint32, Uint32>, SDL_Texture*> g_gradients;
Uint32 packColor(SDL_Color c) { return (Uint32)c.r << 24 | (Uint32)c.g << 16 | (Uint32)c.b << 8 | c.a; }
}  // namespace

void FillVerticalGradient(SDL_Renderer* renderer, const SDL_Rect& rect, SDL_Color top, SDL_Color bottom) {
    auto key = std::make_pair(packColor(top), packColor(bottom));
    SDL_Texture* tex = nullptr;
    auto it = g_gradients.find(key);
    if (it != g_gradients.end()) {
        tex = it->second;
    } else {
        const int N = 256;
        SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, 1, N, 32, SDL_PIXELFORMAT_ARGB8888);
        if (!s) return;
        for (int y = 0; y < N; ++y) {
            SDL_Color c = Mix(top, bottom, y / float(N - 1));
            static_cast<Uint32*>(s->pixels)[y * (s->pitch / 4)] = (Uint32)c.a << 24 | (Uint32)c.r << 16 | (Uint32)c.g << 8 | c.b;
        }
        tex = SDL_CreateTextureFromSurface(renderer, s);
        SDL_FreeSurface(s);
        if (!tex) return;
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(tex, SDL_ScaleModeLinear);
        if (g_gradients.size() > 32) {
            for (auto& kv : g_gradients) SDL_DestroyTexture(kv.second);
            g_gradients.clear();
        }
        g_gradients[key] = tex;
    }
    SDL_RenderCopy(renderer, tex, nullptr, &rect);
}

// ============================================================================
// Components
// ============================================================================
void DrawBackground(SDL_Renderer* renderer) {
    const ThemeColors& t = T();
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, t.background.r, t.background.g, t.background.b, 255);
    SDL_RenderClear(renderer);
    FillVerticalGradient(renderer, SDL_Rect{0, 0, SCREEN_W, SCREEN_H},
                         WithAlpha(t.background, 255), WithAlpha(t.backgroundDark, 255));
    // Soft accent glows for depth.
    const ShapeTex* glow = getShape(renderer, SK_SHADOW, 120, 260);
    drawNine(renderer, glow, SDL_Rect{-420, -560, 1100, 820}, WithAlpha(t.accentPrimary, 34));
    drawNine(renderer, glow, SDL_Rect{760, 470, 1000, 700}, WithAlpha(t.accentSecondary, 20));
}

static std::string upper(std::string s) {
    for (auto& ch : s) ch = (char)std::toupper((unsigned char)ch);
    return s;
}

void DrawHeader(SDL_Renderer* renderer, const std::string& title, const std::string& subtitle, const std::string& rightPill) {
    const ThemeColors& t = T();
    int x = MARGIN_X;
    int titleY = 40;
    if (!subtitle.empty()) {
        DrawText(renderer, GetFont(15, true), upper(subtitle), x, 26, t.accentPrimary);
        titleY = 46;
    }
    int maxTitleW = SCREEN_W - 2 * MARGIN_X - (rightPill.empty() ? 0 : PillWidth(GetFont(17), rightPill, 14) + 24);
    TTF_Font* titleFont = GetFont(32, true);
    DrawText(renderer, titleFont, Ellipsize(titleFont, title, maxTitleW), x, titleY, t.textPrimary);
    if (!rightPill.empty()) {
        TTF_Font* f = GetFont(17);
        int w = PillWidth(f, rightPill, 14);
        DrawPill(renderer, f, rightPill, SCREEN_W - MARGIN_X - w, titleY + 4, WithAlpha(t.cardBackground, 230), t.textSecondary, 14, 34);
    }
}

namespace {
SDL_Color buttonColor(const std::string& b) {
    if (b == "A") return SDL_Color{226, 84, 80, 255};
    if (b == "B") return SDL_Color{236, 176, 52, 255};
    if (b == "X") return SDL_Color{80, 136, 238, 255};
    if (b == "Y") return SDL_Color{60, 178, 112, 255};
    return SDL_Color{0, 0, 0, 0};
}

int hintIconWidth(const std::string& b) {
    if (b == "A" || b == "B" || b == "X" || b == "Y" || b == "DPAD") return 28;
    if (b == "LR") return 50;
    return 36;  // L / R / anything else as a pill
}

void drawHintIcon(SDL_Renderer* r, const std::string& b, int x, int cy) {
    const ThemeColors& t = T();
    SDL_Color face = buttonColor(b);
    if (face.a) {
        FillCircle(r, x + 14, cy, 14, face);
        RenderTextCentered(r, GetFont(15, true), b, x + 14, cy, Color(255, 255, 255));
        return;
    }
    if (b == "DPAD") {
        SDL_Color c = t.textPrimary;
        FillRoundedRect(r, SDL_Rect{x + 3, cy - 4, 22, 8}, 2, c);
        FillRoundedRect(r, SDL_Rect{x + 10, cy - 11, 8, 22}, 2, c);
        return;
    }
    int w = hintIconWidth(b);
    FillRoundedRect(r, SDL_Rect{x, cy - 13, w, 26}, 8, Lighten(t.cardBackground, 0.12f));
    RenderTextCentered(r, GetFont(14, true), b == "LR" ? "L / R" : b, x + w / 2, cy, Color(t.textPrimary));
}
}  // namespace

void DrawHints(SDL_Renderer* renderer, const std::vector<Hint>& hints) {
    const ThemeColors& t = T();
    TTF_Font* f = GetFont(17);
    const int cy = SCREEN_H - FOOTER_H / 2;
    int total = 0;
    for (size_t i = 0; i < hints.size(); ++i) {
        total += hintIconWidth(hints[i].button) + 9 + TextWidth(f, hints[i].label);
        if (i + 1 < hints.size()) total += 26;
    }
    int x = SCREEN_W - MARGIN_X - total;
    int textH = TextHeight(f);
    for (const auto& h : hints) {
        drawHintIcon(renderer, h.button, x, cy);
        x += hintIconWidth(h.button) + 9;
        x += DrawText(renderer, f, h.label, x, cy - textH / 2, t.textSecondary) + 26;
    }
}

void DrawFooterStatus(SDL_Renderer* renderer, const std::string& text, bool spinner) {
    const ThemeColors& t = T();
    TTF_Font* f = GetFont(17);
    int cy = SCREEN_H - FOOTER_H / 2;
    int x = MARGIN_X;
    if (spinner) { DrawSpinner(renderer, x + 10, cy, 10, t.accentPrimary); x += 30; }
    DrawText(renderer, f, text, x, cy - TextHeight(f) / 2, t.textSecondary);
}

void DrawScrollbar(SDL_Renderer* renderer, int x, int y, int h, float viewFrac, float posFrac) {
    const ThemeColors& t = T();
    viewFrac = clamp01(viewFrac);
    posFrac = clamp01(posFrac);
    FillRoundedRect(renderer, SDL_Rect{x, y, 5, h}, 2, WithAlpha(t.textSecondary, 40));
    int th = std::max(28, (int)(h * viewFrac));
    int ty = y + (int)((h - th) * posFrac);
    FillRoundedRect(renderer, SDL_Rect{x, ty, 5, th}, 2, t.accentPrimary);
}

void DrawProgressBar(SDL_Renderer* renderer, const SDL_Rect& rect, float progress) {
    const ThemeColors& t = T();
    int radius = rect.h / 2;
    FillRoundedRect(renderer, rect, radius, WithAlpha(t.textPrimary, 34));
    if (progress < 0.f) {
        float phase = (SDL_GetTicks() % 1600) / 1600.f;
        int segW = std::max(rect.h, rect.w / 3);
        float pos = 0.5f - 0.5f * std::cos(phase * 2.f * (float)M_PI);
        FillRoundedRect(renderer, SDL_Rect{rect.x + (int)((rect.w - segW) * pos), rect.y, segW, rect.h}, radius, t.accentPrimary);
    } else {
        int w = (int)(rect.w * clamp01(progress));
        if (w > 0) FillRoundedRect(renderer, SDL_Rect{rect.x, rect.y, std::max(w, rect.h), rect.h}, radius, t.accentPrimary);
    }
}

void DrawSpinner(SDL_Renderer* renderer, int cx, int cy, int radius, SDL_Color color) {
    const int n = 10;
    const float twoPi = 2.f * (float)M_PI;
    float head = std::fmod(SDL_GetTicks() / 1000.f * 1.25f, 1.f) * twoPi;
    int dot = std::max(2, radius / 4);
    for (int i = 0; i < n; ++i) {
        float a = i * twoPi / n;
        float behind = std::fmod(head - a + twoPi * 2, twoPi) / twoPi;  // 0 = at head
        float alpha = std::pow(1.f - behind, 1.6f);
        alpha = 0.12f + 0.88f * alpha;
        int x = cx + (int)std::lround(std::sin(a) * (radius - dot));
        int y = cy - (int)std::lround(std::cos(a) * (radius - dot));
        FillCircle(renderer, x, y, dot, WithAlpha(color, (Uint8)(color.a * alpha)));
    }
}

int PillWidth(TTF_Font* font, const std::string& text, int padX) {
    return TextWidth(font, text) + 2 * padX;
}

int DrawPill(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y, SDL_Color bg, SDL_Color fg, int padX, int height) {
    int w = PillWidth(font, text, padX);
    FillRoundedRect(renderer, SDL_Rect{x, y, w, height}, height / 2, bg);
    DrawText(renderer, font, text, x + padX, y + (height - TextHeight(font)) / 2, fg);
    return w;
}

void DrawEdgeFade(SDL_Renderer* renderer, int y, int h, bool fadeDown) {
    const ThemeColors& t = T();
    // Approximate the background gradient colour at this height.
    SDL_Color bg = Mix(t.background, t.backgroundDark, (y + h / 2) / (float)SCREEN_H);
    SDL_Color solid = WithAlpha(bg, 255), clear = WithAlpha(bg, 0);
    FillVerticalGradient(renderer, SDL_Rect{0, y, SCREEN_W, h}, fadeDown ? clear : solid, fadeDown ? solid : clear);
}

void DrawScrim(SDL_Renderer* renderer, Uint8 alpha) {
    SDL_Color c = Darken(T().backgroundDark, 0.5f);
    fillRectBlend(renderer, SDL_Rect{0, 0, SCREEN_W, SCREEN_H}, WithAlpha(c, alpha));
}

void DrawPanel(SDL_Renderer* renderer, const SDL_Rect& rect, int radius, bool elevated) {
    const ThemeColors& t = T();
    if (elevated) DrawShadow(renderer, rect, radius, 34, 150, 14);
    FillRoundedRect(renderer, rect, radius, WithAlpha(t.cardBackground, 255));
    StrokeRoundedRect(renderer, rect, radius, 1, WithAlpha(t.textPrimary, 20));
}

void DrawFocusRing(SDL_Renderer* renderer, const SDL_Rect& rect, int radius, SDL_Color color, int thickness, int gap) {
    int o = gap + thickness;
    StrokeRoundedRect(renderer, SDL_Rect{rect.x - o, rect.y - o, rect.w + 2 * o, rect.h + 2 * o}, radius + o, thickness, color);
}

}  // namespace UiUtils
