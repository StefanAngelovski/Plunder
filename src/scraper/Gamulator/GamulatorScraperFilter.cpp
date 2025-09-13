#include "include/GamulatorScraperFilter.h"
#include <iostream>
#include <fstream>
#include <set>
#include "../../utils/include/StringUtils.h"
#include "../../utils/include/HttpUtils.h"

// Forward declarations
static std::set<std::string> loadGamulatorBlacklist();
static std::pair<std::vector<ListItem>, bool> parseAndFilterSearchResults(const std::string& html, const std::string& consolePath, const std::set<std::string>& blacklist);

// Load only Gamulator blacklist entries from blacklist.txt
static std::set<std::string> loadGamulatorBlacklist() {
    std::set<std::string> blacklist;
    std::ifstream file("res/blacklist.txt");
    bool inGamulatorSection = false;
    if (file.is_open()) {
        std::string line;
        while (getline(file, line)) {
            if (line.find("# Gamulator unsupported consoles") != std::string::npos) {
                inGamulatorSection = true;
                continue;
            }
            if (line.find("# ") == 0 && line.find("Gamulator") == std::string::npos) {
                inGamulatorSection = false;
                continue;
            }
            if (inGamulatorSection && !line.empty() && line[0] != '#') {
                blacklist.insert(line);
            }
        }
    }
    return blacklist;
}

// Parse search results and filter by console path for Gamulator
static std::pair<std::vector<ListItem>, bool> parseAndFilterSearchResults(const std::string& html, const std::string& consolePath, const std::set<std::string>& blacklist) {
    std::vector<ListItem> games;
    
    // Regex for each game card (same as in GamulatorScraper::fetchGames)
    std::regex cardRe(R"(<div class=\"card\">[\s\S]*?<a href=\"([^"]+)\">[\s\S]*?<img[^>]+src=\"([^"]+)\"[^>]*alt=\"([^"]*)\"[\s\S]*?<h5 class=\"card-title\">([^<]+)<\/h5>[\s\S]*?<div class=\"opis\">([0-9,]+) downs \/\s*Rating <span class=\"zelena\">([0-9]+)%<\/span>[\s\S]*?<div class=\"hideOverflow\">([\s\S]*?)<\/div>)", std::regex::icase);
    auto begin = std::sregex_iterator(html.begin(), html.end(), cardRe);
    auto end = std::sregex_iterator();
    
    int totalGames = 0;
    int filteredGames = 0;
    
    for (auto it = begin; it != end; ++it) {
        totalGames++;
        
        std::string gameUrl = (*it)[1].str();
        std::string img = (*it)[2].str();
        std::string name = StringUtils::cleanHtmlText((*it)[4].str());
        std::string downloads = (*it)[5].str();
        std::string rating = (*it)[6].str();
        std::string tagsBlock = (*it)[7].str();
        
        if (gameUrl.find("http") != 0) gameUrl = "https://www.gamulator.com" + gameUrl;
        if (img.find("http") != 0) img = "https://www.gamulator.com" + img;
        
        // Extract console path from game URL
        std::string gameConsolePath;
        std::regex gameConsoleRegex(R"(/roms/([^/?]+))");
        std::smatch gameConsoleMatch;
        if (std::regex_search(gameUrl, gameConsoleMatch, gameConsoleRegex)) {
            gameConsolePath = "/roms/" + gameConsoleMatch[1].str();
        }
        
        // Skip if this game is not from the selected console (when console filtering is enabled)
        if (!consolePath.empty() && gameConsolePath != consolePath) {
            continue;
        }
        
        filteredGames++;
        
        // Extract console tag from tagsBlock (the emulator button)
        std::string consoleTag;
        std::regex consoleRe(R"(<a[^>]+class=\"btn btn-info1[^>]+emulator\"[^>]*>([^<]+)<\/a>)", std::regex::icase);
        std::smatch consoleMatch;
        if (std::regex_search(tagsBlock, consoleMatch, consoleRe)) {
            consoleTag = StringUtils::cleanHtmlText(consoleMatch[1].str());
        }
        
        // Extract genres from tagsBlock (all <a ... rel="tag">GENRE</a> that are NOT emulator buttons)
        std::string genre;
        std::regex tagRe(R"(<a[^>]+class=\"btn btn-info btn-xs\"[^>]+rel=\"tag\"[^>]*>([^<]+)<\/a>)", std::regex::icase);
        auto tagBegin = std::sregex_iterator(tagsBlock.begin(), tagsBlock.end(), tagRe);
        auto tagEnd = std::sregex_iterator();
        for (auto tagIt = tagBegin; tagIt != tagEnd; ++tagIt) {
            std::string tag = StringUtils::cleanHtmlText((*tagIt)[1].str());
            if (!genre.empty()) genre += ", ";
            genre += tag;
        }
        
        // Check blacklist
        if (blacklist.find(name) != blacklist.end()) {
            continue;
        }
        
        ListItem item;
        item.label = name;
        item.imagePath = img;
        item.downloadUrl = gameUrl;
        item.size = downloads + " downloads";
        item.rating = rating;
        item.genre = genre;
        games.push_back(item);
    }
    
    // Return the filtered games and whether there were any total games found
    return {games, totalGames > 0};
}

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

// Console-specific filtering function (similar to RomspediaScraperFilter)
std::pair<std::vector<ListItem>, PaginationInfo> GamulatorScraperFilter::filterGames(
    const std::string& searchUrl, 
    const std::string& consoleUrl, 
    const std::string& search, 
    int page) 
{
    std::vector<ListItem> games;
    static std::set<std::string> blacklist = loadGamulatorBlacklist();
    PaginationInfo pagination;
    
    // Extract console path from consoleUrl (e.g., "/roms/game-boy-advance" from "https://www.gamulator.com/roms/game-boy-advance")
    std::string consolePath;
    std::regex consolePathRegex(R"(/roms/([^/?]+))");
    std::smatch consoleMatch;
    if (std::regex_search(consoleUrl, consoleMatch, consolePathRegex)) {
        consolePath = "/roms/" + consoleMatch[1].str();
    }
    
    if (consolePath.empty()) {
        return {games, pagination};
    }
    
    // Build base search URL - remove any existing currentpage parameter first
    std::string baseUrl = searchUrl;
    if (baseUrl.find("&currentpage=") != std::string::npos) {
        baseUrl = std::regex_replace(baseUrl, std::regex(R"(&currentpage=\d+)"), "");
    }
    
    // Set up pagination info
    pagination.baseUrl = baseUrl;
    pagination.currentPage = page;
    pagination.totalPages = 999; // High value, will stop when no more results
    
    const int TARGET_GAMES_PER_PAGE = 15; // Target number of games to show per "virtual" page
    const int MAX_SEARCH_PAGES = 20;      // Limit search to prevent infinite loops
    
    // Calculate which "virtual" range of filtered games we want
    int startIndex = (page - 1) * TARGET_GAMES_PER_PAGE;
    int endIndex = startIndex + TARGET_GAMES_PER_PAGE;
    
    std::vector<ListItem> allFilteredGames;
    std::set<std::string> seenUrls; // Track URLs to prevent duplicates
    std::string previousPageContent; // Track previous page content to detect when pagination stops working
    int searchPage = 1;
    bool foundEnoughGames = false;
    
    // Keep fetching search pages until we have enough filtered games or hit limits
    while (searchPage <= MAX_SEARCH_PAGES && !foundEnoughGames) {
        std::string url = baseUrl;
        if (searchPage > 1) {
            if (url.find("?") != std::string::npos) {
                url += "&currentpage=" + std::to_string(searchPage);
            } else {
                url += "?currentpage=" + std::to_string(searchPage);
            }
        }
        
        std::string html = HttpUtils::fetchWebContent(url);
        if (html.empty()) break;
        
        // Check if this page content is identical to the previous page (pagination limit reached)
        if (searchPage > 1 && html == previousPageContent) {
            break;
        }
        previousPageContent = html;
        
        // Parse and filter search results by console
        auto parseResult = parseAndFilterSearchResults(html, consolePath, blacklist);
        std::vector<ListItem> pageGames = parseResult.first;
        bool hasAnyGamesOnPage = parseResult.second;
        
        // If there are no games at all on this page, we've reached the end of search results
        if (!hasAnyGamesOnPage) {
            break;
        }
        
        // If there are games on the page but none match our filter, continue to next page
        if (pageGames.empty()) {
            searchPage++;
            continue;
        }
        
        // Add games from this search page to our collection, but only if not duplicates
        int newGamesAdded = 0;
        for (const auto& game : pageGames) {
            if (seenUrls.find(game.downloadUrl) == seenUrls.end()) {
                seenUrls.insert(game.downloadUrl);
                allFilteredGames.push_back(game);
                newGamesAdded++;
            }
        }
        
        // If we didn't add any new games from this page, we might be in a loop
        if (newGamesAdded == 0) {
            break;
        }
        
        // Check if we have enough games to satisfy the current virtual page request
        if (allFilteredGames.size() >= endIndex) {
            foundEnoughGames = true;
        }
        
        searchPage++;
    }
    
    // Extract the games for the requested virtual page
    if (startIndex < allFilteredGames.size()) {
        int actualEndIndex = std::min(endIndex, (int)allFilteredGames.size());
        games.assign(allFilteredGames.begin() + startIndex, allFilteredGames.begin() + actualEndIndex);
        
        // Update total pages based on how many filtered games we found
        pagination.totalPages = (allFilteredGames.size() + TARGET_GAMES_PER_PAGE - 1) / TARGET_GAMES_PER_PAGE;
    }
    
    return {games, pagination};
}