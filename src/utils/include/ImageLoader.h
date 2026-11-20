#pragma once
#include <string>
#include <SDL2/SDL.h>

// Image loading that doesn't depend on the OS's SDL2_image codecs.
//
// The Trimui's SDL2_image (stock firmware and CrossMix) ships without a WebP
// codec, and most cover art from the ROM sites is WebP. WebP files are
// therefore decoded with libwebp, linked into Plunder; everything else
// (PNG, JPG, ...) still goes through SDL2_image. The format is detected from
// the file contents, not its extension.
namespace ImageLoader {
    // Returns a new surface (caller frees with SDL_FreeSurface) or nullptr.
    SDL_Surface* Load(const std::string& path);
}
