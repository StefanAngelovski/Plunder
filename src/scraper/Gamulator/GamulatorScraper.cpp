#include "include/GamulatorScraper.h"

std::unordered_set<std::string> loadBlacklist(const std::string& path) {
    std::unordered_set<std::string> blacklist;
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        auto hashPos = line.find('#');
        if (hashPos != std::string::npos) line = line.substr(0, hashPos);
        if (!line.empty()) blacklist.insert(line);
    }
    return blacklist;
}

std::vector<ListItem> GamulatorScraper::fetchConsoles() {
    std::vector<ListItem> result;
    std::string html = HttpUtils::fetchWebContent("https://www.gamulator.com/roms");
    if (html.empty()) return result;
    // Load blacklist
    static std::unordered_set<std::string> blacklist = loadBlacklist("res/blacklist.txt");
    // Match each <div class="thumbnail-home"> ... </div>
    std::regex blockRe(R"(<div class=\"thumbnail-home\">([\s\S]*?)<\/div>)", std::regex::icase);
    auto blockBegin = std::sregex_iterator(html.begin(), html.end(), blockRe);
    auto blockEnd = std::sregex_iterator();
    for (auto it = blockBegin; it != blockEnd; ++it) {
        std::string block = (*it)[1].str();
        // Extract <a href="..."> (first console link)
        std::smatch urlMatch;
        std::string url;
        if (std::regex_search(block, urlMatch, std::regex(R"(<a[^>]+href=\"([^\"]+)\")", std::regex::icase))) {
            url = urlMatch[1].str();
            if (url.find("http") != 0) url = "https://www.gamulator.com/" + url;
        }
        // Extract <img src="...">
        std::smatch imgMatch;
        std::string img;
        if (std::regex_search(block, imgMatch, std::regex(R"(<img[^>]+src=\"([^\"]+)\")", std::regex::icase))) {
            img = imgMatch[1].str();
            if (img.find("http") != 0) img = "https://www.gamulator.com" + img;
        }
        // Extract <h3>...</h3>
        std::smatch nameMatch;
        std::string name;
        if (std::regex_search(block, nameMatch, std::regex(R"(<h3[^>]*>([^<]+)<\/h3>)", std::regex::icase))) {
            name = StringUtils::cleanHtmlText(nameMatch[1].str());
        }
        // Blacklist filtering
        if (!name.empty() && !url.empty()) {
            if (blacklist.find(name) == blacklist.end()) {
                result.push_back(ListItem{name, img, url});
            }
        }
    }
    return result;
}

std::pair<std::vector<ListItem>, PaginationInfo> GamulatorScraper::fetchGames(const std::string& consoleUrl, int page) {
    std::vector<ListItem> games;
    PaginationInfo pagination;
    pagination.baseUrl = consoleUrl;
    std::string url = consoleUrl;
    printf("[GamulatorScraper::fetchGames] page=%d, baseUrl=%s\n", page, consoleUrl.c_str());
    if (page > 1) {
        // Append ?currentpage=2 etc. if not present
        if (url.find('?') == std::string::npos)
            url += "?currentpage=" + std::to_string(page);
        else
            url += "&currentpage=" + std::to_string(page);
    }
    printf("[GamulatorScraper::fetchGames] Final URL: %s\n", url.c_str());
    std::string html = HttpUtils::fetchWebContent(url);
    if (html.empty()) {
        printf("[GamulatorScraper::fetchGames] Empty HTML for URL: %s\n", url.c_str());
        return {games, pagination};
    }

    // Regex for each game card
    std::regex cardRe(R"(<div class=\"card\">[\s\S]*?<a href=\"([^"]+)\">[\s\S]*?<img[^>]+src=\"([^"]+)\"[^>]*alt=\"([^"]*)\"[\s\S]*?<h5 class=\"card-title\">([^<]+)<\/h5>[\s\S]*?<div class=\"opis\">([0-9,]+) downs \/\s*Rating <span class=\"zelena\">([0-9]+)%<\/span>[\s\S]*?<div class=\"hideOverflow\">([\s\S]*?)<\/div>)", std::regex::icase);
    auto begin = std::sregex_iterator(html.begin(), html.end(), cardRe);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        std::string gameUrl = (*it)[1].str();
        std::string img = (*it)[2].str();
        std::string alt = (*it)[3].str();
        std::string name = StringUtils::cleanHtmlText((*it)[4].str());
        std::string downloads = (*it)[5].str();
        std::string rating = (*it)[6].str();
        std::string tagsBlock = (*it)[7].str();
        if (gameUrl.find("http") != 0) gameUrl = "https://www.gamulator.com" + gameUrl;
        if (img.find("http") != 0) img = "https://www.gamulator.com" + img;
        // Extract genres from tagsBlock (all <a ... rel="tag">GENRE</a> except emulator)
        std::string genre;
        std::regex tagRe(R"(<a[^>]+rel=\"tag\"[^>]*>([^<]+)<\/a>)", std::regex::icase);
        auto tagBegin = std::sregex_iterator(tagsBlock.begin(), tagsBlock.end(), tagRe);
        auto tagEnd = std::sregex_iterator();
        for (auto tagIt = tagBegin; tagIt != tagEnd; ++tagIt) {
            std::string tag = StringUtils::cleanHtmlText((*tagIt)[1].str());
            if (tag != "PSP") {
                if (!genre.empty()) genre += ", ";
                genre += tag;
            }
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

    // Pagination: extract current and max page from pagination nav
    std::regex pageRe(R"(<li class='page-item (?:disabled|active)'><a class='page-link'>([0-9]+)<\/a><\/li>[\s\S]*?<li class='page-item'><a class='page-link' href='[^']*currentpage=([0-9]+))");
    std::smatch pageMatch;
    if (std::regex_search(html, pageMatch, pageRe)) {
        pagination.currentPage = std::stoi(pageMatch[1].str());
        pagination.totalPages = std::stoi(pageMatch[2].str());
        printf("[GamulatorScraper::fetchGames] Pagination found: currentPage=%d, totalPages=%d\n", pagination.currentPage, pagination.totalPages);
    } else {
        // Fallback: if this is a search (URL contains /search? or search_term_string=), set totalPages high so scrolling always tries to load more
        if (consoleUrl.find("/search?") != std::string::npos || consoleUrl.find("search_term_string=") != std::string::npos) {
            pagination.currentPage = page;
            pagination.totalPages = 999; // Arbitrary high value, will stop when no more results
            printf("[GamulatorScraper::fetchGames] Fallback pagination for search: currentPage=%d, totalPages=%d\n", pagination.currentPage, pagination.totalPages);
        } else {
            pagination.currentPage = page;
            pagination.totalPages = page;
            printf("[GamulatorScraper::fetchGames] Fallback pagination: currentPage=%d, totalPages=%d\n", pagination.currentPage, pagination.totalPages);
        }
    }
    printf("[GamulatorScraper::fetchGames] Returning %zu games\n", games.size());
    return {games, pagination};
}

GameDetails GamulatorScraper::fetchGameDetails(const std::string& gameUrl) {
    GameDetails details;
    std::string html = HttpUtils::fetchWebContent(gameUrl);
    if (html.empty()) return details;

    // Title
    std::smatch m;
    if (std::regex_search(html, m, std::regex(R"(<h1[^>]*itemprop=["']name["'][^>]*>([^<]+)</h1>)", std::regex::icase))) {
        details.title = StringUtils::cleanHtmlText(m[1].str());
    }

    // Image
    if (std::regex_search(html, m, std::regex(R"(<img[^>]+itemprop=["']image["'][^>]+src=["']([^"']+)["'])", std::regex::icase))) {
        std::string img = m[1].str();
        if (img.find("http") != 0) img = "https://www.gamulator.com" + img;
        details.iconUrl = img;
    }

    // Table fields
    auto extractTableField = [&](const char* label) -> std::string {
        std::regex re(std::string(R"(<td[^>]*>)") + label + R"(</td>\s*<td[^>]*>(.*?)</td>)", std::regex::icase);
        std::smatch m2;
        if (std::regex_search(html, m2, re)) {
            return StringUtils::cleanHtmlText(m2[1].str());
        }
        return "";
    };
    details.consoleName = extractTableField("Console/System:");
    details.genre = extractTableField("Genre:");
    details.fileSize = extractTableField("Filesize:");
    details.language = extractTableField("Region:"); 
    details.releaseDate = extractTableField("Year of release:");
    details.downloads = extractTableField("Downloads:");

    // Download URL
    std::string downloadPageUrl = gameUrl;
    if (!downloadPageUrl.empty() && downloadPageUrl.back() == '/')
        downloadPageUrl.pop_back();
    downloadPageUrl += "/download";

    // Try to extract direct .zip link
    std::string directZip = ExtractGamulatorDirectDownloadLink(downloadPageUrl);
    if (!directZip.empty()) {
        details.downloadUrl = directZip;
    } else {
        details.downloadUrl = downloadPageUrl;
    }

    // Fill fields vector for UI
    details.fields.clear();
    if (!details.consoleName.empty()) details.fields.push_back({"Console", details.consoleName, UiUtils::Color(200,200,200)});
    if (!details.genre.empty()) details.fields.push_back({"Genre", details.genre, UiUtils::Color(200,200,200)});
    if (!details.language.empty()) details.fields.push_back({"Language", details.language, UiUtils::Color(180,200,220)});
    if (!details.fileSize.empty()) details.fields.push_back({"File Size", details.fileSize, UiUtils::Color(220,220,180)});
    if (!details.releaseDate.empty()) details.fields.push_back({"Release", details.releaseDate, UiUtils::Color(220,180,180)});
    if (!details.downloads.empty()) details.fields.push_back({"Downloads", details.downloads, UiUtils::Color(180,220,180)});
    // Add more fields as needed for Gamulator

    return details;
}
