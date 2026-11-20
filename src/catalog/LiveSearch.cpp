#include "include/LiveSearch.h"

#include "../scraper/Romspedia/include/RomspediaScraper.h"
#include "../utils/include/HttpUtils.h"

#include <cctype>
#include <cstdio>
#include <future>
#include <set>

namespace LiveSearch {

namespace {
std::string urlEncode(const std::string& s) {
    std::string out;
    char buf[4];
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += (char)c;
        else if (c == ' ') out += '+';
        else { std::snprintf(buf, sizeof buf, "%%%02X", c); out += buf; }
    }
    return out;
}

std::string between(const std::string& s, const std::string& open, const std::string& close, size_t from, size_t limit) {
    size_t a = s.find(open, from);
    if (a == std::string::npos || a >= limit) return "";
    a += open.size();
    size_t b = s.find(close, a);
    if (b == std::string::npos || b > limit) return "";
    return s.substr(a, b - a);
}

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

// Hexrom's site-wide search (https://hexrom.com/?s=...) lists results as
//   <div class="custom-card"> ... <img src="COVER"> ... <h2>TITLE</h2>
//   <p> CONSOLE </p> <a href="GAME PAGE"> </div>
// Emulators and articles appear there too; they have no console line.
std::vector<Catalog::GlobalResult> searchHexrom(const std::string& query) {
    std::vector<Catalog::GlobalResult> out;
    std::string html = HttpUtils::fetchWebContent("https://hexrom.com/?s=" + urlEncode(query));
    const std::string marker = "class=\"custom-card\"";
    size_t pos = html.find(marker);
    while (pos != std::string::npos) {
        size_t next = html.find(marker, pos + marker.size());
        size_t end = next == std::string::npos ? html.size() : next;
        std::string image = between(html, "src=\"", "\"", pos, end);
        std::string title = trim(between(html, "<h2>", "</h2>", pos, end));
        std::string console = trim(between(html, "<p>", "</p>", pos, end));
        std::string url = between(html, "<a href=\"", "\"", pos, end);
        if (!title.empty() && !url.empty() && !console.empty() &&
            title.find("Emulator") == std::string::npos && console.find("Emulator") == std::string::npos) {
            Catalog::GlobalResult r;
            r.title = Catalog::CleanTitle(title);
            r.consoleKey = Catalog::ConsoleKeyFor(console);
            r.consoleName = Catalog::ConsoleNameFor(console);
            r.sources.push_back({r.title, "Hexrom", console, url, image});
            out.push_back(r);
        }
        pos = next;
    }
    return out;
}

// Romspedia's search page uses the same markup as its console lists, so the
// app's scraper parses it. The console comes from the game URL, which nests
// under the console's URL (via the catalog).
std::vector<Catalog::GlobalResult> searchRomspedia(const std::string& query) {
    std::vector<Catalog::GlobalResult> out;
    RomspediaScraper scraper;
    auto page = scraper.fetchGames("https://www.romspedia.com/search?search_term_string=" + urlEncode(query), 1);
    for (const auto& item : page.first) {
        std::string console = Catalog::ConsoleLabelForGameUrl("Romspedia", item.downloadUrl);
        Catalog::GlobalResult r;
        r.title = Catalog::CleanTitle(item.label);
        r.consoleKey = console.empty() ? "" : Catalog::ConsoleKeyFor(console);
        r.consoleName = console.empty() ? "" : Catalog::ConsoleNameFor(console);
        r.sources.push_back({r.title, "Romspedia", console, item.downloadUrl, item.imagePath});
        out.push_back(r);
    }
    return out;
}
}  // namespace

std::vector<Catalog::GlobalResult> SearchAllSites(const std::string& query) {
    auto hex = std::async(std::launch::async, searchHexrom, query);
    auto rom = std::async(std::launch::async, searchRomspedia, query);
    std::vector<Catalog::GlobalResult> found = hex.get();
    for (auto& r : rom.get()) found.push_back(std::move(r));

    // Same rules as browsing: only consoles the app supports (the catalog's
    // consoles; blacklisted ones, emulators and blog posts are dropped).
    std::set<std::string> supported;
    for (const auto& g : Catalog::ConsoleGroups()) supported.insert(g.key);
    std::vector<Catalog::GlobalResult> kept;
    for (auto& r : found) {
        if (!supported.empty() && !supported.count(r.consoleKey)) continue;
        // Romspedia's search page has no covers; the catalog usually does.
        if (r.sources[0].image.empty()) r.sources[0].image = Catalog::ImageForGameUrl(r.sources[0].site, r.sources[0].url);
        kept.push_back(std::move(r));
    }
    return Catalog::Rank(kept, query);
}

}  // namespace LiveSearch
