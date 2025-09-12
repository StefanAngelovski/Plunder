#pragma once
#include <string>

#include <string>
#include <vector>
#include "../utils/include/UiUtils.h"

struct GameDetailField {
    std::string label;
    std::string value;
    UiUtils::Color color;
};

struct GameDetails {
    std::string title;
    std::string iconUrl;
    std::string publisher;
    std::string genre;
    std::string views;
    std::string downloads;
    std::string releaseDate;
    std::string fileSize;
    std::string about;
    std::string downloadUrl;
    std::string language; 
    std::string consoleName; 
    std::string mappedFolder; // persistent resolved folder mapping for console
    std::vector<GameDetailField> fields;
};
