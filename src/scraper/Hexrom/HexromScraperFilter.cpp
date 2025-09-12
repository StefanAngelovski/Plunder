#include "include/HexromScraperFilter.h"

std::pair<std::vector<ListItem>, PaginationInfo> HexromScraperFilter::filterGames(
    const std::string& baseUrl,
    const HexromFilterModal& filterModal,
    const std::string& search,
    int region, int sort, int order, int genre)
{
    std::string url = baseUrl;
    
    // Remove any existing page segments and query parameters to start clean
    std::regex pageRegex("/page/\\d+/?", std::regex_constants::icase);
    url = std::regex_replace(url, pageRegex, "/");
    
    // Remove any existing query parameters (everything after ?)
    size_t queryPos = url.find('?');
    if (queryPos != std::string::npos) {
        url = url.substr(0, queryPos);
    }
    
    // Ensure URL ends with single slash, then add page segment
    if (url.back() != '/') url += "/";
    url += "page/1/";
    
    // Add query parameters
    url += "?";
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
    // Map genre index to string (skip 0 = "Select Genre")
    std::string genreEncoded = (genre > 0 && genre < (int)filterModal.genreOptions.size()) ? urlEncode(filterModal.genreOptions[genre]) : "";
    // Map region index to string (skip 0 = "Select Region")
    std::string regionEncoded = (region > 0 && region < (int)filterModal.regionOptions.size()) ? urlEncode(filterModal.regionOptions[region]) : "";
    // Map sort index to string
    std::string sort_by = (sort >= 0 && sort < (int)filterModal.sortOptions.size()) ? urlEncode(filterModal.sortOptions[sort]) : "";
    std::transform(sort_by.begin(), sort_by.end(), sort_by.begin(), ::tolower);
    // Map order index to string
    std::string orderStr = (order >= 0 && order < (int)filterModal.orderOptions.size()) ? urlEncode(filterModal.orderOptions[order]) : "";
    url += "search=" + searchEncoded + "&genre=" + genreEncoded + "&region=" + regionEncoded + "&sort_by=" + sort_by + "&order=" + orderStr;
    
    auto hexromScraper = HexromScraper();
    return hexromScraper.fetchGames(url, 1);
}