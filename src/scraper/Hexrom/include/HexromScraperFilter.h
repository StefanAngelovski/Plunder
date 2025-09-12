#pragma once
#include <string>
#include <vector>
#include <utility>
#include <regex>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include "HexromFilterModal.h"
#include "../../../model/ListItem.h"
#include "../../../model/PaginationInfo.h"
#include "HexromScraper.h"

class HexromScraperFilter {
public:
    static std::pair<std::vector<ListItem>, PaginationInfo> filterGames(
        const std::string& baseUrl,
        const HexromFilterModal& filterModal,
        const std::string& search,
        int region, int sort, int order, int genre);
};