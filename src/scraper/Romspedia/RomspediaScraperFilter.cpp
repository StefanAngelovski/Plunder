#include "include/RomspediaScraperFilter.h"

// Forward declarations
static std::set<std::string> loadRomspediaBlacklist();
static std::string normalizeConsoleName(const std::string& name);
static std::vector<ListItem> parseAndFilterSearchResults(const std::string& html, const std::string& consolePath, const std::set<std::string>& blacklist);

// Helper: normalize console name for blacklist
static std::string normalizeConsoleName(const std::string& name) {
    std::string s = name;
    s = std::regex_replace(s, std::regex(R"(\s*\([^)]+\))"), "");
    s = std::regex_replace(s, std::regex(R"(\s*\[[^]]+\])"), "");
    s = std::regex_replace(s, std::regex(R"(ROMs?|ISO|&amp;|&|\s{2,})", std::regex_constants::icase), " ");
    s = std::regex_replace(s, std::regex(R"(^\s+|\s+$)"), "");
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    s = std::regex_replace(s, std::regex(R"(\s{2,})"), " ");
    return s;
}

// Load only Romspedia blacklist entries from blacklist.txt
static std::set<std::string> loadRomspediaBlacklist() {
    std::set<std::string> blacklist;
    std::ifstream file("res/blacklist.txt");
    bool inRomspediaSection = false;
    if (file.is_open()) {
        std::string line;
        while (getline(file, line)) {
            if (line.find("# Romspedia unsupported consoles") != std::string::npos) {
                inRomspediaSection = true;
                continue;
            }
            if (line.find("# ") == 0 && line.find("Romspedia") == std::string::npos) {
                inRomspediaSection = false;
                continue;
            }
            if (inRomspediaSection && !line.empty() && line[0] != '#') {
                std::string normalized = normalizeConsoleName(line);
                blacklist.insert(normalized);
            }
        }
    }
    return blacklist;
}

// Parse search results and filter by console path
static std::vector<ListItem> parseAndFilterSearchResults(const std::string& html, const std::string& consolePath, const std::set<std::string>& blacklist) {
    std::vector<ListItem> games;
    
    // Find all <a href="/roms/..."> tags and process single-rom content
    std::regex linkRe(R"(<a\s+href=['\"]([^'\"]*\/roms\/[^'\"]+)['\"][^>]*>)", std::regex::icase);
    auto linkBegin = std::sregex_iterator(html.begin(), html.end(), linkRe);
    auto linkEnd = std::sregex_iterator();
    int totalLinks = 0;
    int filteredGames = 0;
    
    for (auto linkIt = linkBegin; linkIt != linkEnd; ++linkIt) {
        totalLinks++;
        std::string gameUrl = linkIt->str(1);
        size_t linkPos = linkIt->position();
        size_t linkLength = linkIt->length();
        
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
        
        // Extract name from the <a> tag title attribute
        std::string linkText = linkIt->str(0); // Full match including <a> tag
        std::string name;
        
        // Look for title attribute in the <a> tag
        std::regex titleRe(R"(title=\"([^\"]+)\")", std::regex::icase);
        std::smatch titleMatch;
        if (std::regex_search(linkText, titleMatch, titleRe)) {
            name = StringUtils::cleanHtmlText(titleMatch[1].str());
            // Remove " ROM" suffix if present
            if (name.length() > 4 && name.substr(name.length() - 4) == " ROM") {
                name = name.substr(0, name.length() - 4);
            }
        }
        
        // Look for a single-rom div after this link (within reasonable distance)
        std::string remaining = html.substr(linkPos + linkLength, std::min(size_t(8000), html.length() - linkPos - linkLength));
        std::regex singleRomRe(R"(<div\s+class=\"single-rom\">([\s\S]{0,5000}?)<\/div>)", std::regex::icase);
        std::smatch singleRomMatch;
        
        if (std::regex_search(remaining, singleRomMatch, singleRomRe)) {
            std::string block = singleRomMatch[1].str();
            
            // Fix game URL to be absolute
            if (!gameUrl.empty() && gameUrl.find("http") != 0) gameUrl = "https://www.romspedia.com" + gameUrl;
            
            std::smatch downloadsMatch;
            std::string downloads;
            if (std::regex_search(block, downloadsMatch, std::regex(R"(<span\s+class=\"down-number\">([0-9,]+)<\/span>)", std::regex::icase))) {
                downloads = StringUtils::cleanHtmlText(downloadsMatch[1].str());
            }
            
            std::smatch ratingMatch;
            std::string rating;
            if (std::regex_search(block, ratingMatch, std::regex(R"(<div\s+class=\"list-rom-rating\"[^>]*data-rating=\"([0-9.]+)\")", std::regex::icase))) {
                rating = StringUtils::cleanHtmlText(ratingMatch[1].str());
            }
            
            std::smatch romsImgMatch;
            std::string romsImgBlock;
            if (std::regex_search(block, romsImgMatch, std::regex(R"(<div\s+class=\"roms-img\">([\s\S]*?)<\/div>)", std::regex::icase))) {
                romsImgBlock = romsImgMatch[1].str();
            }
            
            std::string img;
            std::smatch sourceMatch;
            if (!romsImgBlock.empty() && std::regex_search(romsImgBlock, sourceMatch, std::regex(R"(<source[^>]+(?:srcset|data-srcset)=\"([^\"]+\.webp)\")", std::regex::icase))) {
                img = sourceMatch[1].str();
                if (img.find("http") != 0) img = "https://www.romspedia.com" + img;
            } else if (!romsImgBlock.empty()) {
                std::smatch imgMatch;
                if (std::regex_search(romsImgBlock, imgMatch, std::regex(R"(<img[^>]+(?:src|data-src)=\"([^\"]+\.webp)\")", std::regex::icase))) {
                    img = imgMatch[1].str();
                    if (img.find("http") != 0) img = "https://www.romspedia.com" + img;
                }
            }
            
            // Skip category/console links (those without specific game names in URL)
            bool isGameLink = gameUrl.find("/roms/") != std::string::npos && 
                             gameUrl.rfind("/") != gameUrl.find("/roms/") + 5; // More than just /roms/console
            
            if (!name.empty() && !gameUrl.empty() && isGameLink) {
                std::string labelNorm = normalizeConsoleName(name);
                if (blacklist.find(labelNorm) == blacklist.end()) {
                    ListItem item;
                    item.label = name;
                    item.imagePath = img;
                    item.downloadUrl = gameUrl;
                    item.size = downloads;
                    item.rating = rating;
                    games.push_back(item);
                }
            }
        }
    }
    
    if (!consolePath.empty()) {
        std::cout << "[RomspediaFilter] Found " << totalLinks << " total links, " << filteredGames 
                  << " matching console " << consolePath << ", " << games.size() << " after blacklist filtering" << std::endl;
    }
    
    return games;
}

// Console-specific filtering function (similar to GamulatorScraperFilter)
std::pair<std::vector<ListItem>, PaginationInfo> RomspediaScraperFilter::filterGames(
    const std::string& searchUrl, 
    const std::string& consoleUrl, 
    const std::string& search, 
    int page) 
{
    std::vector<ListItem> games;
    static std::set<std::string> blacklist = loadRomspediaBlacklist();
    PaginationInfo pagination;
    
    // Extract console path from consoleUrl (e.g., "/roms/nintendo-ds" from "https://www.romspedia.com/roms/nintendo-ds")
    std::string consolePath;
    std::regex consolePathRegex(R"(/roms/([^/?]+))");
    std::smatch consoleMatch;
    if (std::regex_search(consoleUrl, consoleMatch, consolePathRegex)) {
        consolePath = "/roms/" + consoleMatch[1].str();
    }
    
    if (consolePath.empty()) {
        std::cout << "[RomspediaFilter] Could not extract console path from: " << consoleUrl << std::endl;
        return {games, pagination};
    }
    
    std::cout << "[RomspediaFilter] Console filtering for: " << consolePath << " (requested page " << page << ")" << std::endl;
    
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
        
        std::cout << "[RomspediaFilter] Fetching search page " << searchPage << ": " << url << std::endl;
        std::string html = HttpUtils::fetchWebContent(url);
        if (html.empty()) break;
        
        // Parse and filter search results by console
        std::vector<ListItem> pageGames = parseAndFilterSearchResults(html, consolePath, blacklist);
        
        if (pageGames.empty()) {
            std::cout << "[RomspediaFilter] No more games found on search page " << searchPage << ", stopping" << std::endl;
            break; // No more results
        }
        
        // Add games from this search page to our collection
        allFilteredGames.insert(allFilteredGames.end(), pageGames.begin(), pageGames.end());
        
        std::cout << "[RomspediaFilter] Total filtered games so far: " << allFilteredGames.size() << std::endl;
        
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
    
    std::cout << "[RomspediaFilter] Returning " << games.size() << " games for virtual page " << page 
              << " (total filtered games: " << allFilteredGames.size() << ", total pages: " << pagination.totalPages << ")" << std::endl;
    
    return {games, pagination};
}

// Original function for backward compatibility (no console filtering)
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
