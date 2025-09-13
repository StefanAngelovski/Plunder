#include "include/GamulatorScraperFilter.h"

std::pair<std::vector<ListItem>, PaginationInfo> GamulatorScraperFilter::filterGames(
    const std::string& baseUrl,
    const GamulatorFilterModal& filterModal,
    const std::string& search,
    int page)
{
    // Static variable to store previous page results for duplicate detection
    static std::vector<std::string> previousPageUrls;
    static int lastPage = 0;
    
    // Reset tracking when starting a new search (page 1) or going backwards
    if (page == 1 || page < lastPage) {
        previousPageUrls.clear();
    }
    lastPage = page;
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
        
        // Check for duplicate results by comparing URLs with previous page
        bool hasDuplicates = false;
        if (page > 1 && !filteredGames.empty()) {
            std::vector<std::string> currentUrls;
            for (const auto& game : filteredGames) {
                currentUrls.push_back(game.downloadUrl);
            }
            
            // Check if any of the current URLs were in the previous page
            for (const auto& url : currentUrls) {
                if (std::find(previousPageUrls.begin(), previousPageUrls.end(), url) != previousPageUrls.end()) {
                    hasDuplicates = true;
                    break;
                }
            }
            
            if (hasDuplicates) {
                result.second.totalPages = page - 1;
                result.first.clear(); // Return empty results to prevent showing duplicates
            } else {
                // Store current URLs for next iteration
                previousPageUrls = currentUrls;
            }
        } else if (!filteredGames.empty()) {
            // Store URLs from page 1
            previousPageUrls.clear();
            for (const auto& game : filteredGames) {
                previousPageUrls.push_back(game.downloadUrl);
            }
        }
        
        // More robust end-of-pages detection:
        // 1. If we got no filtered results on page > 1, we've reached the end
        // 2. If the original scraper says we're on the last page of actual results, use that
        // 3. If we got fewer results than expected (< 15), we might be near the end
        
        if (filteredGames.empty() && page > 1 && !hasDuplicates) {
            result.second.totalPages = page - 1;
        } else if (result.second.totalPages != 999) {
            // If the original scraper detected actual pagination limits, respect them
        } else if (result.first.size() < 15 && page > 1) {
            // If we got fewer than a full page of original results, we're probably near the end
            // But be conservative - only adjust if we have very few results
            if (result.first.size() < 5) {
                result.second.totalPages = page;
            }
        }
    }
    
    return result;
}