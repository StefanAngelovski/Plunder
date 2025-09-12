#include "include/RomspediaScraper.h"

// Forward declarations
static std::set<std::string> loadRomspediaBlacklist();
static std::string normalizeConsoleName(const std::string& name);
static void parseSearchResults(const std::string& html, std::vector<ListItem>& games, const std::set<std::string>& blacklist);

// Parse search results from HTML
static void parseSearchResults(const std::string& html, std::vector<ListItem>& games, const std::set<std::string>& blacklist) {
    // First, let's count how many single-rom divs exist in total
    std::regex totalBlockRe(R"(<div\s+class=\"single-rom\">)", std::regex::icase);
    auto totalBlockBegin = std::sregex_iterator(html.begin(), html.end(), totalBlockRe);
    auto totalBlockEnd = std::sregex_iterator();
    int totalBlockCount = std::distance(totalBlockBegin, totalBlockEnd);
    std::cout << "[RomspediaSearchGames] Total single-rom blocks found: " << totalBlockCount << std::endl;
    
    // More specific approach: Find all <a href="/roms/..."> tags and then look for the single-rom content that follows
    std::regex linkRe(R"(<a\s+href=['\"]([^'\"]*\/roms\/[^'\"]+)['\"][^>]*>)", std::regex::icase);
    auto linkBegin = std::sregex_iterator(html.begin(), html.end(), linkRe);
    auto linkEnd = std::sregex_iterator();
    int blockCount = 0;
    
    for (auto linkIt = linkBegin; linkIt != linkEnd; ++linkIt) {
        std::string gameUrl = linkIt->str(1);
        size_t linkPos = linkIt->position();
        size_t linkLength = linkIt->length();
        
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
            blockCount++;
            std::string block = singleRomMatch[1].str();
        
            // URL was already extracted from outer <a> tag
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
        } // Close the if (std::regex_search(remaining, singleRomMatch, singleRomRe)) block
    }
}

// Helper: normalize console name for blacklist (same as Hexrom)
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
            if (line.find("[Romspedia]") != std::string::npos) {
                inRomspediaSection = true;
                continue;
            }
            if (line.find("[") != std::string::npos && line.find("]") != std::string::npos) {
                inRomspediaSection = false;
                continue;
            }
            if (inRomspediaSection && !line.empty()) {
                std::string normalized = normalizeConsoleName(line);
                blacklist.insert(normalized);
            }
        }
    }
    return blacklist;
}

GameDetails RomspediaScraper::fetchGameDetails(const std::string& gameUrl) {
    GameDetails details;
    std::string html = HttpUtils::fetchWebContent(gameUrl);
    if (html.empty()) return details;

    // Title
    std::smatch titleMatch;
    if (std::regex_search(html, titleMatch, std::regex(R"(<h2[^>]*class=\"main-title-big\">([^<]+)<\/h2>)", std::regex::icase))) {
        details.title = StringUtils::cleanHtmlText(titleMatch[1].str());
    }

    // Image (prefer largest <source>, fallback to <img>) - but skip .webp since not supported
    std::smatch imgMatch;
    if (std::regex_search(html, imgMatch, std::regex(R"(<source[^>]+srcset=\"([^\"]+)\")", std::regex::icase))) {
        std::string imageUrl = imgMatch[1].str();
        // Skip .webp images since they're not supported yet
        if (imageUrl.find(".webp") == std::string::npos) {
            details.iconUrl = imageUrl;
        }
    } else if (std::regex_search(html, imgMatch, std::regex(R"(<img[^>]+src=\"([^\"]+)\")", std::regex::icase))) {
        std::string imageUrl = imgMatch[1].str();
        // Skip .webp images since they're not supported yet
        if (imageUrl.find(".webp") == std::string::npos) {
            details.iconUrl = imageUrl;
        }
    }

    // File Size
    std::smatch sizeMatch;
    if (std::regex_search(html, sizeMatch, std::regex(R"(<div class=\"view-emulator-detail-name\">Size:<\/div>\s*<div class=\"view-emulator-detail-value\">([^<]+)<\/div>)", std::regex::icase))) {
        details.fileSize = StringUtils::cleanHtmlText(sizeMatch[1].str());
    }

    // Console
    std::smatch consoleMatch;
    if (std::regex_search(html, consoleMatch, std::regex(R"(<div class=\"view-emulator-detail-name\">Console<\/div>\s*<div class=\"view-emulator-detail-value\">([^<]+)<\/div>)", std::regex::icase))) {
        details.consoleName = StringUtils::cleanHtmlText(consoleMatch[1].str());
    }

    // Genre/Category
    std::smatch genreMatch;
    if (std::regex_search(html, genreMatch, std::regex(R"(<div class=\"view-emulator-detail-name\">(Category:|Genre:)<\/div>\s*<div class=\"view-emulator-detail-value\">([^<]+)<\/div>)", std::regex::icase))) {
        details.genre = StringUtils::cleanHtmlText(genreMatch[2].str());
    }

    // Release Year
    std::smatch releaseMatch;
    if (std::regex_search(html, releaseMatch, std::regex(R"(<div class=\"view-emulator-detail-name\">Release Year:<\/div>\s*<div class=\"view-emulator-detail-value\">([^<]+)<\/div>)", std::regex::icase))) {
        details.releaseDate = StringUtils::cleanHtmlText(releaseMatch[1].str());
    }

    // Downloads
    std::smatch downloadsMatch;
    if (std::regex_search(html, downloadsMatch, std::regex(R"(<div class=\"view-emulator-detail-name\">Downloads:<\/div>\s*<div class=\"view-emulator-detail-value\">([^<]+)<\/div>)", std::regex::icase))) {
        details.downloads = StringUtils::cleanHtmlText(downloadsMatch[1].str());
    }

    // About section (game description)
    std::smatch aboutMatch;
    if (std::regex_search(html, aboutMatch, std::regex(R"(<div class=\"padinzi-gore-dole descSec\">[\s\S]*?<article[^>]*>([\s\S]*?)<\/article>)", std::regex::icase))) {
        std::string aboutHtml = aboutMatch[1].str();
        // Remove all HTML tags
        aboutHtml = std::regex_replace(aboutHtml, std::regex(R"(<[^>]+>)"), "");
        details.about = StringUtils::cleanHtmlText(aboutHtml);
    }

    // Connect Romspedia direct download link
    details.downloadUrl = ExtractRomspediaDirectDownloadLink(gameUrl);

    // Fill fields vector for UI
    details.fields.clear();
    if (!details.consoleName.empty()) details.fields.push_back({"Console", details.consoleName, UiUtils::Color(200,200,200)});
    if (!details.language.empty()) details.fields.push_back({"Region", details.language, UiUtils::Color(180,200,220)});
    if (!details.fileSize.empty()) details.fields.push_back({"File Size", details.fileSize, UiUtils::Color(220,220,180)});
    if (!details.releaseDate.empty()) details.fields.push_back({"Release", details.releaseDate, UiUtils::Color(220,180,180)});
    if (!details.downloads.empty()) details.fields.push_back({"Downloads", details.downloads, UiUtils::Color(180,220,180)});
    if (!details.genre.empty()) details.fields.push_back({"Genre", details.genre, UiUtils::Color(180,180,220)});
    if (!details.about.empty()) details.fields.push_back({"About", details.about, UiUtils::Color(220,200,220)});

    return details;
}


RomspediaScraper::RomspediaScraper() {}

std::vector<ListItem> RomspediaScraper::fetchConsoles() {
	std::vector<ListItem> result;
	std::string html = HttpUtils::fetchWebContent("https://www.romspedia.com/roms");
	if (html.empty()) return result;

	// Each block: <div class="col-12 col-sm-6 col-md-4 col-lg-3 col-xl-3"> ... <div class="pop-slide"> ... </div> ... </div>
	std::regex blockRe(R"(<div class=\"col-12 col-sm-6 col-md-4 col-lg-3 col-xl-3\">([\s\S]*?<div class=\"pop-slide\">[\s\S]*?<\/div>[\s\S]*?)<\/div>)", std::regex::icase);
	auto blockBegin = std::sregex_iterator(html.begin(), html.end(), blockRe);
	auto blockEnd = std::sregex_iterator();
	for (auto it = blockBegin; it != blockEnd; ++it) {
		std::string block = (*it)[1].str();
		// Extract <a href="...">
		std::smatch urlMatch;
		std::string url;
		if (std::regex_search(block, urlMatch, std::regex(R"(<a[^>]+href=\"([^\"]+)\")", std::regex::icase))) {
			url = urlMatch[1].str();
			if (url.find("http") != 0) url = "https://www.romspedia.com/" + url;
		}
		// Extract <img ... data-src="...">
		std::smatch imgMatch;
		std::string img;
		if (std::regex_search(block, imgMatch, std::regex(R"(data-src=\"([^\"]+)\")", std::regex::icase))) {
			img = imgMatch[1].str();
		} else if (std::regex_search(block, imgMatch, std::regex(R"(src=\"([^\"]+)\")", std::regex::icase))) {
			img = imgMatch[1].str();
		}
		if (!img.empty() && img.find("http") != 0) img = "https://www.romspedia.com" + img;
		// Extract <h2 class="emulator-title">...</h2>
		std::smatch nameMatch;
		std::string name;
		if (std::regex_search(block, nameMatch, std::regex(R"(<h2[^>]*>([^<]+)<\/h2>)", std::regex::icase))) {
			name = StringUtils::cleanHtmlText(nameMatch[1].str());
		}
        if (!name.empty() && !url.empty()) {
            static std::set<std::string> blacklist = loadRomspediaBlacklist();
            // Map console name to folder (replace with your actual mapping function)
            std::string folder = getFolderForScrapedConsole(name); // <-- ensure this function exists and works
            if (blacklist.find(name) == blacklist.end()) {
                result.push_back(ListItem{name, img, url});
            }
        }
	}
	return result;
}

std::pair<std::vector<ListItem>, PaginationInfo> RomspediaScraper::fetchGames(const std::string& consoleUrl, int page) {
    std::vector<ListItem> games;
    std::set<std::string> blacklist = loadRomspediaBlacklist();
    PaginationInfo pagination;
    
    std::cout << "[RomspediaScraper] DEBUG: fetchGames called with consoleUrl=" << consoleUrl << " page=" << page << std::endl;
    
    // Check if this is a search URL
    if (consoleUrl.find("search?search_term_string=") != std::string::npos) {
        // Handle search URLs directly in fetchGames (like Gamulator does)
        std::cout << "[RomspediaScraper] fetchGames handling search URL: " << consoleUrl << std::endl;
        
        // Set up base URL without page parameter for pagination
        std::string baseSearchUrl = consoleUrl;
        if (baseSearchUrl.find("&currentpage=") != std::string::npos) {
            baseSearchUrl = std::regex_replace(baseSearchUrl, std::regex(R"(&currentpage=\d+)"), "");
        }
        pagination.baseUrl = baseSearchUrl;
        pagination.currentPage = page;
        pagination.totalPages = 999; // High value like Gamulator, will stop when no more results
        
        std::cout << "[RomspediaScraper] DEBUG: Set pagination.baseUrl to: " << pagination.baseUrl << std::endl;
        
        // Build URL for the requested page
        std::string url = baseSearchUrl;
        if (page > 1) {
            if (url.find("&currentpage=") != std::string::npos) {
                url = std::regex_replace(url, std::regex(R"(&currentpage=\d+)"), "&currentpage=" + std::to_string(page));
            } else if (url.find("?") != std::string::npos) {
                url += "&currentpage=" + std::to_string(page);
            } else {
                url += "?currentpage=" + std::to_string(page);
            }
        }
        
        std::cout << "[RomspediaScraper] Search page " << page << " URL: " << url << std::endl;
        std::string html = HttpUtils::fetchWebContent(url);
        if (html.empty()) return {games, pagination};
        
        // Parse search results (extract games from HTML using title attributes)
        parseSearchResults(html, games, blacklist);
        std::cout << "[RomspediaScraper] Found " << games.size() << " games on search page " << page << std::endl;
        
        return {games, pagination};
    }
    
    // Handle regular console URLs
    pagination.baseUrl = consoleUrl;
    pagination.currentPage = page;
    pagination.totalPages = 1;

    // Build URL for the requested page (Hexrom-style: /page/[pagenumber]/)
    std::string url = consoleUrl;
    if (page > 1) {
        if (url.back() != '/') url += "/";
        url += "page/" + std::to_string(page) + "/";
    }
    std::cout << "[RomspediaGames] Page " << page << " URL: " << url << std::endl;
    std::string html = HttpUtils::fetchWebContent(url);
    if (html.empty()) return {games, pagination};

    // Step 1: Extract each game card block (outer div)
    // Chunk-based extraction: match <div class="single-rom"> and capture up to 5000 chars
    std::regex blockRe(R"(<div\s+class=\"single-rom\">([\s\S]{0,5000})<\/div>)", std::regex::icase);
    auto blockBegin = std::sregex_iterator(html.begin(), html.end(), blockRe);
    auto blockEnd = std::sregex_iterator();
    int matchCount = 0;
    for (auto it = blockBegin; it != blockEnd; ++it) {
    ++matchCount;
    std::string block = it->str();
        // Extract downloads
        std::smatch downloadsMatch;
        std::string downloads;
        if (std::regex_search(block, downloadsMatch, std::regex(R"(<span\s+class=\"down-number\">([0-9,]+)<\/span>)", std::regex::icase))) {
            downloads = StringUtils::cleanHtmlText(downloadsMatch[1].str());
        }
        // Extract rating
        std::smatch ratingMatch;
        std::string rating;
        if (std::regex_search(block, ratingMatch, std::regex(R"(<div\s+class=\"list-rom-rating\"[^>]*data-rating=\"([0-9.]+)\")", std::regex::icase))) {
            rating = StringUtils::cleanHtmlText(ratingMatch[1].str());
        }
        // Extract .roms-img block
        std::smatch romsImgMatch;
        std::string romsImgBlock;
        if (std::regex_search(block, romsImgMatch, std::regex(R"(<div\s+class=\"roms-img\">([\s\S]*?)<\/div>)", std::regex::icase))) {
            romsImgBlock = romsImgMatch[1].str();
        }
        // Declare gameUrl, img, name at start of loop
        std::string gameUrl;
        std::string img;
        std::string name;
        // Extract game URL from .roms-img block
        std::smatch urlMatch;
        if (!romsImgBlock.empty() && std::regex_search(romsImgBlock, urlMatch, std::regex(R"(<a[^>]+href=\"([^\"]+)\")", std::regex::icase))) {
            gameUrl = urlMatch[1].str();
            if (gameUrl.find("http") != 0) gameUrl = "https://www.romspedia.com" + gameUrl;
        }
        // Extract first .webp image from <source srcset=...> in <picture> inside .roms-img
        std::smatch sourceMatch;
        if (!romsImgBlock.empty() && std::regex_search(romsImgBlock, sourceMatch, std::regex(R"(<source[^>]+srcset=\"([^\"]+\.webp)\")", std::regex::icase))) {
            img = sourceMatch[1].str();
            if (img.find("http") != 0) img = "https://www.romspedia.com" + img;
        } else if (!romsImgBlock.empty()) {
            std::smatch imgMatch;
            if (std::regex_search(romsImgBlock, imgMatch, std::regex(R"(<img[^>]+src=\"([^\"]+\.webp)\")", std::regex::icase))) {
                img = imgMatch[1].str();
                if (img.find("http") != 0) img = "https://www.romspedia.com" + img;
            }
        }
        // Extract name from 'title' attribute of <a> in .roms-img
        std::smatch nameMatch;
        if (!romsImgBlock.empty() && std::regex_search(romsImgBlock, nameMatch, std::regex(R"(<a[^>]+title=\"([^\"]+)\")", std::regex::icase))) {
            name = StringUtils::cleanHtmlText(nameMatch[1].str());
        }
        if (!name.empty() && !gameUrl.empty()) {
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

    // Pagination extraction 
    std::regex navRegex(R"(<ul class=['\"]pagination['\"][^>]*>([\s\S]*?)<\/ul>)");
    std::smatch navMatch;
    int maxPage = page;
    if (std::regex_search(html, navMatch, navRegex) && navMatch.size() > 1) {
        std::string navHtml = navMatch[1].str();
        std::regex pageRegex(R"(<a[^>]+href=[^>]+>(\d+)<\/a>)");
        auto pageBegin = std::sregex_iterator(navHtml.begin(), navHtml.end(), pageRegex);
        auto pageEnd = std::sregex_iterator();
        for (auto it = pageBegin; it != pageEnd; ++it) {
            int p = std::stoi((*it)[1].str());
            if (p > maxPage) maxPage = p;
        }
    }
    pagination.currentPage = page;
    pagination.totalPages = maxPage;
    return {games, pagination};
}

// Download and extract ROM for Romspedia, matching Hexrom/Gamulator
bool RomspediaScraper::downloadRom(const GameDetails& details) {
    // Use title for ROM name, sanitize for filesystem
    std::string romName = details.title;
    romName = std::regex_replace(romName, std::regex(R"([^A-Za-z0-9_\-\.])"), "_");
    return downloadAndExtractRomspediaZip(details.downloadUrl, details.consoleName, romName);
}