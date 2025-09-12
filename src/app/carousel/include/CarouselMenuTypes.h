#pragma once
#include <string>
#include <functional>
#include <SDL2/SDL.h>

struct CarouselMenuItem {
    std::string label;
    std::string imagePath; // optional, can be empty
    std::function<void()> action;
};

