#pragma once
#include <string>

struct PaginationInfo {
    int currentPage = 1;
    int totalPages = 1;
    std::string baseUrl;
};
