#pragma once
#include <string>
#include <vector>

#include "../../model/ListItem.h"

// Pre-built game catalog (see tools/catalog and .github/workflows/catalog.yml).
//
// A scheduled GitHub Action crawls the ROM sites and publishes catalog.tsv.gz
// plus a small catalog.json manifest on the repo's `catalog` branch. At
// startup the app loads its saved copy from the SD card, then checks the
// manifest in the background and downloads a new catalog only when it changed.
//
// When a site/console is in the catalog, consoles and full game lists load
// instantly and searching happens locally. Otherwise callers fall back to
// live scraping, so the app works without a catalog too.
namespace Catalog {
    // Load the saved catalog, then refresh from GitHub, on a background thread.
    void StartBackgroundRefresh();

    // True once a catalog is loaded in memory.
    bool IsReady();

    // Consoles of a site ("Hexrom", "Romspedia"); empty if the site isn't covered.
    std::vector<ListItem> Consoles(const std::string& site);

    // All games of a console (by its URL). Returns false if not covered.
    bool Games(const std::string& site, const std::string& consoleUrl, std::vector<ListItem>& out);

    // Games of a console whose title contains every word of `query`
    // (forgiving: case, accents and punctuation are ignored). False if not covered.
    bool Search(const std::string& site, const std::string& consoleUrl, const std::string& query, std::vector<ListItem>& out);

    // e.g. "catalog updated 3 h ago"; empty if no catalog is loaded.
    std::string AgeText();

    // ---- Global search across all sites -----------------------------------------

    // One place a game can be downloaded from.
    struct Source {
        std::string title;         // this site's own title (may include region tags like "(EU)")
        std::string site;          // "Hexrom", "Romspedia"
        std::string consoleLabel;  // the site's own console label (drives the download folder)
        std::string url;           // game page
        std::string image;         // cover URL
    };

    // One game listing from one site. A game listed by several sites appears
    // once per site (no merging).
    struct GlobalResult {
        std::string title;
        std::string consoleKey;    // unified console (device ROM folder, e.g. "GBA")
        std::string consoleName;   // display name, e.g. "GBA"
        std::vector<Source> sources;   // exactly one: the listing's site
    };

    // Unified consoles across sites (same device ROM folder = same console).
    struct ConsoleGroup {
        std::string key;
        std::string name;
        size_t games = 0;
    };

    enum class SortOrder { BestMatch = 0, TitleAZ = 1, TitleZA = 2 };

    struct GlobalQuery {
        std::string text;          // empty = everything
        std::string consoleKey;    // empty = all consoles
        std::string site;          // empty = all sites
        SortOrder sort = SortOrder::BestMatch;
    };

    std::vector<ConsoleGroup> ConsoleGroups();   // sorted by name
    std::vector<std::string> Sites();
    size_t TotalGames();                         // listings across all sites
    std::vector<GlobalResult> GlobalSearch(const GlobalQuery& query);

    // Lower-case, accents folded, punctuation removed, single spaces;
    // a trailing "rom"/"roms" word is dropped. Used for matching.
    std::string NormalizeForSearch(const std::string& s);

    // Display title: HTML entities decoded, trailing " ROM" and double spaces removed.
    std::string CleanTitle(const std::string& s);

    // Unified console key/name for a site's console label (works without a catalog).
    std::string ConsoleKeyFor(const std::string& consoleLabel);
    std::string ConsoleNameFor(const std::string& consoleLabel);

    // Site console label for a game page URL (e.g. Romspedia URLs nest under
    // their console's URL); empty if unknown.
    std::string ConsoleLabelForGameUrl(const std::string& site, const std::string& gameUrl);

    // Cover image URL the catalog has for a game page; empty if unknown.
    std::string ImageForGameUrl(const std::string& site, const std::string& gameUrl);

    // Rank results (e.g. from live site searches) for `query` like GlobalSearch does.
    std::vector<GlobalResult> Rank(const std::vector<GlobalResult>& results, const std::string& query);
}
