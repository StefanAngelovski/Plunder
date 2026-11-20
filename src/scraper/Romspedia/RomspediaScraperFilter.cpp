#include "include/RomspediaScraperFilter.h"

#include "../engine/Url.h"

// Plain search: the site's search results, paged by the site.
std::pair<std::vector<ListItem>, PaginationInfo> RomspediaScraperFilter::filterGames(
    const std::string& baseUrl, RomspediaFilterModal& modal, const std::string& search, int page) {
    (void)baseUrl;
    (void)modal;
    RomspediaScraper scraper;
    return scraper.fetchGames(scraper.definition().searchUrl(search), page);
}

// Search within one console. Romspedia's search covers every console, so
// results are fetched page by page and kept when their URL is under the
// console's path (/roms/gameboy-advance/...), then shown 15 at a time.
std::pair<std::vector<ListItem>, PaginationInfo> RomspediaScraperFilter::filterGames(
    const std::string& searchUrl, const std::string& consoleUrl, const std::string& search, int page) {
    (void)search;
    const int kPerPage = 15, kMaxSearchPages = 20;
    PaginationInfo pagination;
    pagination.baseUrl = searchUrl;
    pagination.currentPage = page;

    size_t roms = consoleUrl.find("/roms/");
    if (roms == std::string::npos) return {{}, pagination};
    std::string consolePath = consoleUrl.substr(roms);
    if (consolePath.back() != '/') consolePath += '/';

    RomspediaScraper scraper;
    std::vector<ListItem> matches;
    const size_t wanted = (size_t)page * kPerPage;
    for (int p = 1; p <= kMaxSearchPages && matches.size() < wanted; ++p) {
        std::vector<ListItem> results = scraper.fetchGames(searchUrl, p).first;
        if (results.empty()) break;
        for (auto& g : results)
            if (g.downloadUrl.find(consolePath) != std::string::npos) matches.push_back(g);
    }
    size_t start = (size_t)(page - 1) * kPerPage;
    std::vector<ListItem> games;
    if (start < matches.size())
        games.assign(matches.begin() + start, matches.begin() + std::min(matches.size(), start + kPerPage));
    pagination.totalPages = (int)((matches.size() + kPerPage - 1) / kPerPage);
    return {games, pagination};
}
