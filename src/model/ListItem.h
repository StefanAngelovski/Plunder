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
    // Global search results: which site / console (site's own label) this came from
    std::string site;
    std::string console;
    std::string consoleName;   // unified console name shown on cards ("NES" on every site)
};
