#include "include/RomspediaScraperFilter.h"

std::pair<std::vector<ListItem>, PaginationInfo> RomspediaScraperFilter::filterGames(
    const std::string& baseUrl,
    RomspediaFilterModal& filterModal,
    const std::string& search,
    int page)
{
    // Romspedia search URL: https://www.romspedia.com/search?search_term_string=SEARCH&currentpage=PAGE
    std::string url = "https://www.romspedia.com/search?search_term_string=";
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
    url += "&currentpage=" + std::to_string(page);
    std::cout << "[RomspediaFilter] Search URL: " << url << std::endl;
    RomspediaScraper romspediaScraper;
    return romspediaScraper.fetchGames(url, page);
}
