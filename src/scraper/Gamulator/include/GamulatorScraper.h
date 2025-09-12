#pragma once
#include "../../SiteScraper.h"
#include <vector>
#include <string>
#include <regex>
#include <fstream>
#include <unordered_set>

#include "../../../model/ListItem.h"
#include "../../../model/PaginationInfo.h"
#include "../../../model/GameDetails.h"
#include "GamulatorScraperDownload.h"
#include "../../../utils/include/HttpUtils.h"
#include "../../../utils/include/StringUtils.h"

class GamulatorScraper : public SiteScraper {
public:
    std::string getName() const override { return "Gamulator"; }
    std::vector<ListItem> fetchConsoles() override;
    std::pair<std::vector<ListItem>, PaginationInfo> fetchGames(const std::string& consoleUrl, int page) override;
    GameDetails fetchGameDetails(const std::string& gameUrl) override;
};
