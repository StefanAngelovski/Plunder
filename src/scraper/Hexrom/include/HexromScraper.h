#pragma once
#include "../../SiteScraper.h"
#include <vector>
#include <string>
#include <regex>
#include <set>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <future>
#include "HexromScraperDownload.h"
#include "../../../model/ListItem.h"
#include "../../../model/GameDetails.h"
#include "../../../utils/include/HttpUtils.h"
#include "../../../utils/include/StringUtils.h"


class HexromScraper : public SiteScraper {
public:
    HexromScraper();
    std::string getName() const override;
    std::vector<ListItem> fetchConsoles() override;
    std::pair<std::vector<ListItem>, PaginationInfo> fetchGames(const std::string& consoleUrl, int page = 1) override;
    GameDetails fetchGameDetails(const std::string& gameUrl) override;
};
