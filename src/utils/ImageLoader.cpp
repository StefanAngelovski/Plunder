#include "include/ImageLoader.h"

#include <SDL2/SDL_image.h>
#include <webp/decode.h>

#include <cstdio>
#include <cstring>
#include <vector>

namespace ImageLoader {

namespace {
// Covers are small; refuse anything absurd rather than allocating it.
constexpr int kMaxDimension = 8192;
constexpr long kMaxFileBytes = 32L * 1024 * 1024;

bool readFile(const std::string& path, std::vector<uint8_t>& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size <= 0 || size > kMaxFileBytes) { std::fclose(f); return false; }
    out.resize((size_t)size);
    bool ok = std::fread(out.data(), 1, out.size(), f) == out.size();
    std::fclose(f);
    return ok;
}

bool isWebP(const std::vector<uint8_t>& d) {
    return d.size() >= 12 && std::memcmp(d.data(), "RIFF", 4) == 0 && std::memcmp(d.data() + 8, "WEBP", 4) == 0;
}

SDL_Surface* decodeWebP(const std::vector<uint8_t>& d) {
    int w = 0, h = 0;
    if (!WebPGetInfo(d.data(), d.size(), &w, &h)) return nullptr;
    if (w <= 0 || h <= 0 || w > kMaxDimension || h > kMaxDimension) return nullptr;
    // RGBA32 = bytes R,G,B,A in memory on any endianness, matching WebPDecodeRGBA.
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    if (!s) return nullptr;
    if (!WebPDecodeRGBAInto(d.data(), d.size(), static_cast<uint8_t*>(s->pixels),
                            (size_t)s->pitch * (size_t)h, s->pitch)) {
        SDL_FreeSurface(s);
        return nullptr;
    }
    return s;
}
}  // namespace

SDL_Surface* Load(const std::string& path) {
    std::vector<uint8_t> data;
    if (!readFile(path, data)) return nullptr;
    if (isWebP(data)) return decodeWebP(data);
    SDL_RWops* rw = SDL_RWFromConstMem(data.data(), (int)data.size());
    return rw ? IMG_Load_RW(rw, 1) : nullptr;
}

}  // namespace ImageLoader
