#pragma once
#include <vector>
#include <string>
#include "../model/ListItem.h"
#include "../model/PaginationInfo.h"
#include "../model/GameDetails.h"

class SiteScraper {
public:
    virtual ~SiteScraper() {}
    virtual std::string getName() const = 0;
    virtual std::vector<ListItem> fetchConsoles() = 0;
    virtual std::pair<std::vector<ListItem>, PaginationInfo> fetchGames(const std::string& consoleUrl, int page = 1) = 0;
    virtual GameDetails fetchGameDetails(const std::string& gameUrl) = 0;
};
