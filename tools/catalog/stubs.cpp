// The scrapers tag detail fields with UiUtils::Color; the crawler has no UI,
// so provide just that type instead of linking the whole drawing toolkit.
#include "utils/include/UiUtils.h"
namespace UiUtils {
Color::Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) : r(r), g(g), b(b), a(a) {}
Color::Color(const SDL_Color& c) : r(c.r), g(c.g), b(c.b), a(c.a) {}
SDL_Color Color::toSDLColor() const { return {r, g, b, a}; }
}
