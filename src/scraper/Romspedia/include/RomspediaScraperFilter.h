#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <regex>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include "RomspediaScraper.h"
#include "RomspediaFilterModal.h"
#include "../../../model/ListItem.h"
#include "../../../model/PaginationInfo.h"
#include "RomspediaFilterModal.h"

class RomspediaScraperFilter {
public:
    static std::pair<std::vector<ListItem>, PaginationInfo> filterGames(const std::string& baseUrl, RomspediaFilterModal& modal, const std::string& search, int page = 1);
};
