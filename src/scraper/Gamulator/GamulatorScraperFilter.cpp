#include "include/GamulatorScraperFilter.h"

std::pair<std::vector<ListItem>, PaginationInfo> GamulatorScraperFilter::filterGames(
    const std::string& baseUrl,
    const GamulatorFilterModal& filterModal,
    const std::string& search,
    int page)
{
    // Extract console path from baseUrl (e.g., "https://www.gamulator.com/roms/game-boy-advance" -> "/roms/game-boy-advance")
    std::string selectedConsolePath;
    std::regex consoleRe(R"(/roms/([^/?]+))");
    std::smatch match;
    if (std::regex_search(baseUrl, match, consoleRe)) {
        selectedConsolePath = "/roms/" + match[1].str();
    }
    
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
    auto result = gamulatorScraper.fetchGames(url, page);
    
    // If we have a console filter, filter the results by matching the game's console path
    if (!selectedConsolePath.empty()) {
        std::vector<ListItem> filteredGames;
        
        for (const auto& game : result.first) {
            // Extract console path from game URL (stored in downloadUrl)
            // e.g., "/roms/nintendo-ds/pokemon-heartgold" -> "/roms/nintendo-ds"
            std::regex gameConsoleRe(R"(/roms/([^/]+))");
            std::smatch gameMatch;
            if (std::regex_search(game.downloadUrl, gameMatch, gameConsoleRe)) {
                std::string gameConsolePath = "/roms/" + gameMatch[1].str();
                if (gameConsolePath == selectedConsolePath) {
                    filteredGames.push_back(game);
                }
            }
        }
        
        result.first = filteredGames;
    }
    
    return result;
}