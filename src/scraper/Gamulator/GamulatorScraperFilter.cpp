#include "include/GamulatorScraperFilter.h"

std::pair<std::vector<ListItem>, PaginationInfo> GamulatorScraperFilter::filterGames(
    const std::string& baseUrl,
    const GamulatorFilterModal& filterModal,
    const std::string& search,
    int page)
{
    // Gamulator search URL: https://www.gamulator.com/search?search_term_string=SEARCH&currentpage=PAGE
    std::string url = "https://www.gamulator.com/search?search_term_string=";
    auto urlEncode = [](const std::string& value) -> std::string {
        std::ostringstream escaped;
        escaped.fill('0');
        escaped << std::hex;
        for (char c : value) {
            if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
                escaped << c;
            } else if (c == ' ') {
                escaped << '+';
            } else {
                escaped << '%' << std::uppercase << std::setw(2) << int((unsigned char)c) << std::nouppercase;
            }
        }
        return escaped.str();
    };
    std::string searchEncoded = urlEncode(search);
    url += searchEncoded;
    if (page > 1) {
        url += "&currentpage=" + std::to_string(page);
    }
    auto gamulatorScraper = GamulatorScraper();
    return gamulatorScraper.fetchGames(url, page);
}