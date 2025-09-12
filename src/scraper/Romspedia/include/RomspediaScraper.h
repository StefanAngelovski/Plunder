#pragma once
#include <vector>
#include <utility>
#include <string>
#include <set>
#include <regex>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <iostream>
#include "../../SiteScraper.h"
#include "../../../model/ListItem.h"
#include "../../../model/PaginationInfo.h"
#include "../../../model/GameDetails.h"
#include "RomspediaScraperDownload.h"
#include "../../../model/ListItem.h"
#include "../../../model/PaginationInfo.h"
#include "../../../model/GameDetails.h"
#include "../../../utils/include/HttpUtils.h"
#include "../../../utils/include/StringUtils.h"
#include "../../../utils/include/UiUtils.h"
#include "../../../app/consolePolicies/ConsoleFolderMap.h"

class RomspediaScraper : public SiteScraper {
public:
    RomspediaScraper();
    std::string getName() const override { return "Romspedia"; }
    std::vector<ListItem> fetchConsoles() override;
    std::pair<std::vector<ListItem>, PaginationInfo> fetchGames(const std::string& consoleUrl, int page = 1) override;
    GameDetails fetchGameDetails(const std::string& gameUrl) override;
    bool downloadRom(const GameDetails& details);
};
