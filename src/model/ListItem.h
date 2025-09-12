#pragma once
#include <string>

struct ListItem {
    std::string label;
    std::string imagePath;
    std::string downloadUrl;
    // For games, these fields are used; for consoles, they can be empty
    std::string genre;
    std::string rating;
    std::string size;
};
