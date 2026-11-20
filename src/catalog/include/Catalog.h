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
    // (case-insensitive). Returns false if the console isn't covered.
    bool Search(const std::string& site, const std::string& consoleUrl, const std::string& query, std::vector<ListItem>& out);

    // e.g. "catalog updated 3 h ago"; empty if no catalog is loaded.
    std::string AgeText();
}
