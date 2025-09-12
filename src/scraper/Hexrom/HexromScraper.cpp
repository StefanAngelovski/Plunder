#include "include/HexromScraper.h"

HexromScraper::HexromScraper() {}

std::string HexromScraper::getName() const {
    return "Hexrom";
}

// Helper: normalize console name for blacklist (remove parens, brackets, ROMs, ISO, &, HTML entities, trim, lowercase)
static std::string normalizeConsoleName(const std::string& name) {
    std::string s = name;
    // Remove text in parentheses
    s = std::regex_replace(s, std::regex(R"(\s*\([^\)]*\))"), "");
    // Remove text in square brackets
    s = std::regex_replace(s, std::regex(R"(\s*\[[^\]]*\])"), "");
    // Remove 'ROM', 'ROMS', 'ISO', '&', and HTML entities
    s = std::regex_replace(s, std::regex(R"(ROMs?|ISO|&amp;|&|\s{2,})", std::regex_constants::icase), " ");
    // Remove extra spaces
    s = std::regex_replace(s, std::regex(R"(^\s+|\s+$)"), "");
    // Lowercase
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    // Collapse multiple spaces
    s = std::regex_replace(s, std::regex(R"(\s{2,})"), " ");
    return s;
}

// Global blacklist loader for Hexrom
static std::set<std::string> loadHexromBlacklist() {
    std::set<std::string> blacklist;
    std::ifstream file("res/blacklist.txt");
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            auto comment = line.find('#');
            if (comment != std::string::npos) line = line.substr(0, comment);
            // Trim whitespace
            line.erase(line.begin(), std::find_if(line.begin(), line.end(), [](int ch) { return !std::isspace(ch); }));
            line.erase(std::find_if(line.rbegin(), line.rend(), [](int ch) { return !std::isspace(ch); }).base(), line.end());
            if (!line.empty()) {
                // Normalize blacklist entry
                blacklist.insert(normalizeConsoleName(line));
            }
        }
    }
    return blacklist;
}

std::vector<ListItem> HexromScraper::fetchConsoles() {
    std::vector<ListItem> consoles;
    std::set<std::string> blacklist = loadHexromBlacklist();
    std::string html = HttpUtils::fetchWebContent("https://hexrom.com/rom-category/");
    if (html.empty()) return consoles;

    std::regex regex(R"(<li><a href=\"([^\"]+)\">([^<]+)</a><span class=\"my-game-count\">(\d+)</span></li>)");
    auto begin = std::sregex_iterator(html.begin(), html.end(), regex);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        std::smatch match = *it;
        std::string url = match[1];
        std::string name = match[2];
        std::string count = match[3];
        ListItem item;
        item.label = name + " (" + count + ")";
        item.downloadUrl = url;
        item.imagePath = "";
        // Blacklist filtering (case-insensitive, normalized) on the label as shown in UI
        std::string labelNorm = normalizeConsoleName(item.label);
        // DEBUG: Print normalized label and check if it's in the blacklist
        std::cout << "[DEBUG] Label: '" << item.label << "' | Normalized: '" << labelNorm << "'\n";
        if (blacklist.find(labelNorm) != blacklist.end()) {
            std::cout << "[DEBUG] -- BLACKLISTED\n";
        }
        if (blacklist.find(labelNorm) == blacklist.end()) {
            consoles.push_back(item);
        }
    }
    return consoles;
}

// Helper: case-insensitive substring search
static bool containsCaseInsensitive(const std::string& haystack, const std::string& needle) {
    auto it = std::search(
        haystack.begin(), haystack.end(),
        needle.begin(), needle.end(),
        [](char ch1, char ch2) { return std::tolower(ch1) == std::tolower(ch2); }
    );
    return (it != haystack.end());
}

std::pair<std::vector<ListItem>, PaginationInfo> HexromScraper::fetchGames(const std::string& consoleUrl, int page) {
    std::vector<ListItem> games;
    PaginationInfo pagination;
    pagination.baseUrl = consoleUrl;
    pagination.currentPage = page;
    pagination.totalPages = 1;

    // Build URL for the requested page
    std::string url = consoleUrl;
    
    // Handle page parameter properly for URLs with or without existing page segments and query params
    std::regex pageRegex("/page/\\d+/?", std::regex_constants::icase);
    url = std::regex_replace(url, pageRegex, "/");
    
    // Find query parameters
    size_t queryPos = url.find('?');
    std::string baseUrl = (queryPos != std::string::npos) ? url.substr(0, queryPos) : url;
    std::string queryParams = (queryPos != std::string::npos) ? url.substr(queryPos) : "";
    
    // Ensure base URL ends with single slash
    if (baseUrl.back() != '/') baseUrl += "/";
    
    // Add page segment if needed
    if (page > 1) {
        baseUrl += "page/" + std::to_string(page) + "/";
    }
    
    // Reconstruct final URL
    url = baseUrl + queryParams;
    std::string html = HttpUtils::fetchWebContent(url);
    if (html.empty()) return {games, pagination};

    std::regex h2Regex(R"(<h2>(.*?)<\/h2>)");
    std::regex imgRegex(R"(<img[^>]+data-src=\"([^\"]+)\")");
    std::regex ratingRegex(R"(<span class=\"rating\">[\s\S]*?([0-9.]+)[^<]*<\/span>)");
    std::regex aHrefRegex(R"(<a[^>]+href=\"([^\"]+)\"[^>]*>\s*<h2>\s*.*?<\/h2>)");
    auto h2Begin = std::sregex_iterator(html.begin(), html.end(), h2Regex);
    auto h2End = std::sregex_iterator();
    size_t lastPos = 0;
    for (auto it = h2Begin; it != h2End; ++it) {
        std::string title = (*it)[1].str();
        title = StringUtils::cleanHtmlText(title);
        size_t h2Pos = it->position();
        size_t h2EndPos = h2Pos + it->length();
        std::string beforeH2 = html.substr(lastPos, h2Pos - lastPos);
        // Search for the last image before this h2
        std::string imageUrl = "";
        auto imgIt = std::sregex_iterator(beforeH2.begin(), beforeH2.end(), imgRegex);
        auto imgEnd = std::sregex_iterator();
        for (auto iit = imgIt; iit != imgEnd; ++iit) {
            imageUrl = (*iit)[1].str();
        }
        // Search for the last rating before this h2
        std::string rating = "";
        auto ratingIt = std::sregex_iterator(beforeH2.begin(), beforeH2.end(), ratingRegex);
        auto ratingEnd = std::sregex_iterator();
        for (auto rit = ratingIt; rit != ratingEnd; ++rit) {
            rating = (*rit)[1].str();
        }
        // Find the first <a href=...> after this h2
        std::string afterH2 = html.substr(h2EndPos, 500); // Search up to 500 chars after h2
        std::regex aAfterRegex(R"(<a[^>]+href=\"([^\"]+)\"[^>]*>)");
        std::smatch aAfterMatch;
        std::string downloadUrl = "";
        if (std::regex_search(afterH2, aAfterMatch, aAfterRegex) && aAfterMatch.size() > 1) {
            downloadUrl = aAfterMatch[1].str();
            if (downloadUrl.find("http") != 0) {
                if (!downloadUrl.empty() && downloadUrl[0] == '/')
                    downloadUrl = "https://hexrom.com" + downloadUrl;
                else
                    downloadUrl = "https://hexrom.com/" + downloadUrl;
            }
        }
        lastPos = h2EndPos;
        ListItem item;
        // Only use main page info, skip details fetch
        item.label = title;
        item.genre = "";
        item.imagePath = imageUrl;
        item.downloadUrl = downloadUrl;
        item.rating = rating;
        // Filtering: skip non-game entries
        bool isNonGame = false;
        if (item.imagePath.empty() || item.downloadUrl.empty()) {
            isNonGame = true;
        }
        // Check downloadUrl for non-game patterns
        std::vector<std::string> badUrlParts = {"/emulators/", "/how-to-", "/bios/", "/tutorial/"};
        for (const auto& part : badUrlParts) {
            if (containsCaseInsensitive(item.downloadUrl, part)) {
                isNonGame = true;
                break;
            }
        }
        // Check title for non-game patterns
        std::vector<std::string> badTitleParts = {"emulator", "how to", "install", "bios", "tutorial"};
        for (const auto& part : badTitleParts) {
            if (containsCaseInsensitive(item.label, part)) {
                isNonGame = true;
                break;
            }
        }
        if (!isNonGame) {
            games.push_back(item);
        }
    }
    // Pagination extraction for Hexrom
    std::regex navRegex(R"(<div class="navigation">([\s\S]*?)<\/div>)");
    std::smatch navMatch;
    if (std::regex_search(html, navMatch, navRegex) && navMatch.size() > 1) {
        std::string navHtml = navMatch[1].str();
        std::regex pageRegex(R"(<a[^>]+href=[^>]+>(\d+)<\/a>)");
        auto pageBegin = std::sregex_iterator(navHtml.begin(), navHtml.end(), pageRegex);
        auto pageEnd = std::sregex_iterator();
        int maxPage = 1;
        for (auto it = pageBegin; it != pageEnd; ++it) {
            int p = std::stoi((*it)[1].str());
            if (p > maxPage) maxPage = p;
        }
        pagination.totalPages = maxPage;
    }
    return {games, pagination};
}

// Fetch and parse game details from a game details page URL
GameDetails HexromScraper::fetchGameDetails(const std::string& gameUrl) {
    GameDetails details;
    std::string html = HttpUtils::fetchWebContent(gameUrl);
    if (html.empty()) return details;

    // Only extract title from the <th>Name<td> row in the details table
    std::regex nameRowRegex(R"(<th>\s*Name\s*<td>([^<]+))", std::regex::icase);
    std::smatch nameMatch;
    if (std::regex_search(html, nameMatch, nameRowRegex) && nameMatch.size() > 1) {
        details.title = StringUtils::cleanHtmlText(nameMatch[1].str());
    } else {
        details.title = ""; // No fallback, leave empty if not found
    }
    // Extract Size field from details table
    std::regex sizeRowRegex(R"(<th>\s*Size\s*<td>([^<\n]+))", std::regex::icase);
    std::smatch sizeMatch;
    if (std::regex_search(html, sizeMatch, sizeRowRegex) && sizeMatch.size() > 1) {
        details.fileSize = StringUtils::cleanHtmlText(sizeMatch[1].str());
    } else {
        details.fileSize = "";
    }
    // Extract Publish (release date)
    std::regex publishRowRegex(R"(<th>\s*Publish\s*<td>([^<\n]+))", std::regex::icase);
    std::smatch publishMatch;
    if (std::regex_search(html, publishMatch, publishRowRegex) && publishMatch.size() > 1) {
        details.releaseDate = StringUtils::cleanHtmlText(publishMatch[1].str());
    }
    // Extract Console (genre or new field)
    std::regex consoleRowRegex(R"(<th>\s*Console(?:\s*<\/th>|)\s*<td>(.*?)<\/td>)", std::regex::icase);
    std::smatch consoleMatch;
    if (std::regex_search(html, consoleMatch, consoleRowRegex) && consoleMatch.size() > 1) {
        std::string consoleHtml = consoleMatch[1].str();
        // Extract all <a>...</a> text and join with ' > '
        std::regex aTagRegex(R"(<a[^>]*>([^<]+)<\/a>)");
        std::sregex_iterator aIt(consoleHtml.begin(), consoleHtml.end(), aTagRegex);
        std::sregex_iterator aEnd;
        std::vector<std::string> parts;
        for (; aIt != aEnd; ++aIt) {
            parts.push_back(StringUtils::cleanHtmlText((*aIt)[1].str()));
        }
        if (!parts.empty()) {
            details.consoleName = parts[0];
        } else {
            details.consoleName = StringUtils::cleanHtmlText(consoleHtml);
        }
    }
    // Extract Language
    std::regex langRowRegex(R"(<th>\s*Language\s*<td>([^<\n]+))", std::regex::icase);
    std::smatch langMatch;
    if (std::regex_search(html, langMatch, langRowRegex) && langMatch.size() > 1) {
        details.language = StringUtils::cleanHtmlText(langMatch[1].str());
    }
    // Extract Downloads (support both <th>Downloads<td> and <th>Downloads</th><td>)
    std::regex downloadsRowRegex(R"(<th>\s*Downloads(?:\s*<\/th>|)\s*<td>([^<\n]+))", std::regex::icase);
    std::smatch downloadsMatch;
    if (std::regex_search(html, downloadsMatch, downloadsRowRegex) && downloadsMatch.size() > 1) {
        details.downloads = StringUtils::cleanHtmlText(downloadsMatch[1].str());
    }
    // Extract About/Description section after the details table
    std::regex tableRegex(R"(<table[\s\S]*?<\/table>)", std::regex::icase);
    std::smatch tableMatch;
    std::string aboutText = "";
    if (std::regex_search(html, tableMatch, tableRegex) && tableMatch.size() > 0) {
        size_t tableEnd = tableMatch.position(0) + tableMatch.length(0);
        std::string afterTable = html.substr(tableEnd);
        std::smatch aboutMatch;
        std::regex pRegex(R"(<p[^>]*>([\s\S]*?)<\/p>)", std::regex::icase);
        if (std::regex_search(afterTable, aboutMatch, pRegex) && aboutMatch.size() > 1) {
            aboutText = aboutMatch[1].str();
        } else {
            aboutText = "";
        }
        aboutText = StringUtils::cleanHtmlText(std::regex_replace(aboutText, std::regex(R"(<[^>]+>)"), "\n"));
        aboutText = std::regex_replace(aboutText, std::regex(R"(\n{3,})"), "\n\n");
        details.about = aboutText;
    } else {
        details.about = "";
    }
    // Extract direct .zip download link using HexromScraperDownload
    details.downloadUrl = ExtractHexromDirectDownloadLink(gameUrl);

    // Fill fields vector for UI
    details.fields.clear();
    if (!details.consoleName.empty()) details.fields.push_back({"Console", details.consoleName, UiUtils::Color(200,200,200)});
    if (!details.language.empty()) details.fields.push_back({"Language", details.language, UiUtils::Color(180,200,220)});
    if (!details.fileSize.empty()) details.fields.push_back({"File Size", details.fileSize, UiUtils::Color(220,220,180)});
    if (!details.releaseDate.empty()) details.fields.push_back({"Release", details.releaseDate, UiUtils::Color(220,180,180)});
    if (!details.downloads.empty()) details.fields.push_back({"Downloads", details.downloads, UiUtils::Color(180,220,180)});

    return details;
}
