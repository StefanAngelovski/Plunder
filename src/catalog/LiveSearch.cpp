#include "include/LiveSearch.h"

#include "../scraper/Hexrom/include/HexromScraper.h"
#include "../scraper/Romspedia/include/RomspediaScraper.h"

#include <future>
#include <set>

namespace LiveSearch {

namespace {
// One site's own search, as global-search results (console read from each card).
std::vector<Catalog::GlobalResult> searchSite(DefinedScraper* scraper, const std::string& query) {
    std::vector<Catalog::GlobalResult> out;
    for (const ListItem& item : scraper->search(query)) {
        if (item.console.empty()) continue;   // emulators, blog posts
        Catalog::GlobalResult r;
        r.title = Catalog::CleanTitle(item.label);
        r.consoleKey = Catalog::ConsoleKeyFor(item.console);
        r.consoleName = Catalog::ConsoleNameFor(item.console);
        std::string image = item.imagePath.empty() ? Catalog::ImageForGameUrl(scraper->getName(), item.downloadUrl) : item.imagePath;
        r.sources.push_back({r.title, scraper->getName(), item.console, item.downloadUrl, image});
        out.push_back(r);
    }
    return out;
}
}  // namespace

std::vector<Catalog::GlobalResult> SearchAllSites(const std::string& query) {
    HexromScraper hexrom;
    RomspediaScraper romspedia;
    auto a = std::async(std::launch::async, searchSite, &hexrom, query);
    auto b = std::async(std::launch::async, searchSite, &romspedia, query);
    std::vector<Catalog::GlobalResult> found = a.get();
    for (auto& r : b.get()) found.push_back(std::move(r));

    // Same rules as browsing: only consoles the app supports (the catalog's consoles).
    std::set<std::string> supported;
    for (const auto& g : Catalog::ConsoleGroups()) supported.insert(g.key);
    std::vector<Catalog::GlobalResult> kept;
    for (auto& r : found)
        if (supported.empty() || supported.count(r.consoleKey)) kept.push_back(std::move(r));
    return Catalog::Rank(kept, query);
}

}  // namespace LiveSearch
