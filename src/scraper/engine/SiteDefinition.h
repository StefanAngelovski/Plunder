#pragma once
#include <functional>
#include <string>
#include <vector>

// Describes a ROM site as data: where its pages are and where each value sits
// on them (CSS selectors). DefinedScraper turns a definition into a scraper,
// so adding or fixing a site means editing a definition, not parsing code.

// Where a value is: the first element matching `css` inside the current scope
// (empty css = the scope element itself), and its `attr` (empty = its text).
struct Pick {
    std::string css;
    std::string attr;
};
// Alternatives tried in order; the first non-empty value wins.
using Picks = std::vector<Pick>;

// The site's console list.
struct ConsoleRules {
    std::string url;        // page listing every console
    std::string item;       // one element per console
    Picks name, count, link;
};

// A page of game cards (a console's list, or search results).
struct CardRules {
    std::string card;       // one element per game
    Picks title, link, image;
    Picks console;          // search results only: the game's console label
};

// A game's own page.
struct DetailRules {
    Picks title, cover;
    // Description: the paragraphs matching the first selector that matches anything.
    std::vector<std::string> about;
    // "Label: value" facts (size, region, ...): label and value elements,
    // paired up in page order.
    std::string labels, values;
};

// The page holding the actual archive link.
struct DownloadRules {
    std::function<std::string(const std::string& gameUrl)> pageUrl;
    Picks link;
};

struct SiteDefinition {
    std::string id;         // "Hexrom"; also the blacklist section name
    std::string baseUrl;    // scheme + host, for relative links

    ConsoleRules consoles;
    CardRules games;        // a console's game list
    CardRules search;       // the site's own search results
    std::string pageLinks;  // pagination links; the highest number = page count

    std::function<std::string(const std::string& listUrl, int page)> pageUrl;
    std::function<std::string(const std::string& query)> searchUrl;
    std::function<bool(const std::string& url)> isSearchUrl;

    DetailRules details;
    DownloadRules download;

    // Hexrom has always been checked against the whole blacklist file.
    bool blacklistUsesWholeFile = false;
};
