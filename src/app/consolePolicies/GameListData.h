#pragma once
#include "../../model/ListItem.h"
#include "../../model/PaginationInfo.h"
#include <vector>
#include <string>

struct GameListData {
    std::vector<ListItem> games;
    PaginationInfo pagination;
    std::string title;
    std::string baseUrl;
    std::string consoleName;
};
