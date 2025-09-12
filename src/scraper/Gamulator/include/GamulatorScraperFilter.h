#pragma once
#include <string>
#include <vector>
#include <utility>
#include <regex>
#include <sstream>
#include <iomanip>
#include <algorithm>

#include "GamulatorFilterModal.h"
#include "../../../model/ListItem.h"
#include "../../../model/PaginationInfo.h"
#include "GamulatorFilterModal.h"
#include "GamulatorScraper.h"

class GamulatorScraperFilter {
public:
    static std::pair<std::vector<ListItem>, PaginationInfo> filterGames(
        const std::string& baseUrl,
        const GamulatorFilterModal& filterModal,
        const std::string& search,
        int page = 1);
};
