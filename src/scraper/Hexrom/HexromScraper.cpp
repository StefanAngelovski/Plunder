#include "include/HexromScraper.h"

#include "../engine/Url.h"

#include <cctype>

namespace {
const char* kArchiveLinks = "a[href$='.zip'], a[href$='.7z'], a[href$='.rar']";

// Console list and filter URLs ("…/gameboy-advance/?search=…") take the page
// number as a path segment before the query: …/gameboy-advance/page/2/?search=…
std::string pageUrl(const std::string& listUrl, int page) {
    std::string url = listUrl;
    size_t q = url.find('?');
    std::string query = q == std::string::npos ? "" : url.substr(q);
    std::string path = q == std::string::npos ? url : url.substr(0, q);
    size_t p = path.find("/page/");
    if (p != std::string::npos) path.erase(p + 1);   // drop an existing /page/N/
    if (path.empty() || path.back() != '/') path += '/';
    if (page > 1) path += "page/" + std::to_string(page) + "/";
    return path + query;
}

SiteDefinition hexrom() {
    SiteDefinition d;
    d.id = "Hexrom";
    d.baseUrl = "https://hexrom.com";
    d.blacklistUsesWholeFile = true;

    // <li><a href="…/roms/gameboy-advance/">GBA ROMs</a><span class="my-game-count">1963</span></li>
    d.consoles = {"https://hexrom.com/rom-category/", "li:has(> span.my-game-count)",
                  {{"a", ""}}, {{"span.my-game-count", ""}}, {{"a", "href"}}};

    // <div class="custom-card"> <img data-src="cover"> … <h2>Title</h2> <a href="game page"> </div>
    d.games = {"div.custom-card", {{"h2", ""}}, {{"a[href]", "href"}}, {{"img", "data-src"}, {"img", "src"}}, {}};
    // Site search: same cards plus a <p>Console</p> line (emulators/blog posts have other text there).
    d.search = d.games;
    d.search.console = {{"p", ""}};
    d.pageLinks = "div.navigation a";

    d.pageUrl = pageUrl;
    d.searchUrl = [](const std::string& q) { return "https://hexrom.com/?s=" + Url::EncodeQuery(q); };
    d.isSearchUrl = [](const std::string& url) { return url.find("/?s=") != std::string::npos; };

    // Game page: <h1>Title</h1>, a <table> of <th>Label</th><td>Value</td> rows,
    // and the description in the post body.
    d.details.title = {{"h1", ""}};
    d.details.cover = {{"meta[property='og:image']", "content"}};
    d.details.about = {"div[id^='matn-post'] p"};
    d.details.labels = "table th";
    d.details.values = "table td";

    // The archive link is on the game's /download/ page.
    d.download.pageUrl = [](const std::string& gameUrl) {
        std::string url = gameUrl;
        if (url.empty() || url.back() != '/') url += '/';
        if (url.size() < 10 || url.compare(url.size() - 10, 10, "/download/") != 0) url += "download/";
        return url;
    };
    d.download.link = {{kArchiveLinks, "href"}};

    // Filters: …/gameboy-advance/?search=…&genre=…&region=…&sort_by=…&order=…
    d.filters.fields = {
        {"Region", {"Select Region", "USA", "English(USA)", "English", "Europe", "USA Europe", "Germany", "Italy",
                    "France", "Japan", "Korea", "USA & Japan", "Global", "English (USA)"}},
        {"Sort by", {"Date", "Title", "Most Popular"}},
        {"Order", {"Ascending", "Descending"}},
        {"Genre", {
        "Select Genre",
        "Role Playing",
        "Adventure, Role Playing",
        "Fighting",
        "Action",
        "Adventure",
        "Action, Simulation",
        "Adventure, Platform",
        "Hack, Role Playing",
        "Misc",
        "Platform",
        "Action, Racing",
        "Card Game",
        "Action, Adventure",
        "Action, Fighting",
        "Strategy, Turn Based Tactics",
        "Sports",
        "Role Playing, Shooter",
        "Beat Em Up",
        "Board Game",
        "Action, Beat Em Up",
        "Racing",
        "Role Playing, Strategy",
        "Turn Based Tactics",
        "Shooter",
        "Action, Shooter",
        "Role Playing, Simulation",
        "Action, Role Playing",
        "Action, Platform",
        "Compilation",
        "Action, Adventure, Platform",
        "Action, Pinball",
        "Puzzle",
        "Action, Fighting, Role Playing",
        "Adventure, Fighting",
        "Platform, Shooter",
        "Strategy",
        "Card Game, Compilation",
        "Simulation",
        "Compilation, Misc",
        "Sports, Simulation",
        "Racing, Simulation",
        "Action, Puzzle",
        "Board Game, Card Game",
        "Adventure, Beat Em Up",
        "Action, Puzzle, Simulation, Strategy",
        "Music, Puzzle, Simulation, Strategy",
        "Card Game, Simulation",
        "Racing, Sports",
        "Card Game, Puzzle",
        "Sports, Strategy",
        "Puzzle, Simulation, Strategy",
        "Puzzle, Shooter",
        "Action, Music",
        "Simulation, Strategy",
        "Music",
        "Board Game, Card Game, Misc",
        "Educational",
        "Puzzle, Sports",
        "Board Game, Misc",
        "Role-Playing",
        "RPG"
        }},
    };
    d.filters.url = [fields = d.filters.fields](const std::string& consoleUrl, const std::string& search, const std::vector<int>& c) {
        auto option = [&](size_t field, bool skipDefault) -> std::string {
            int i = field < c.size() ? c[field] : 0;
            if (i < 0 || i >= (int)fields[field].options.size() || (skipDefault && i == 0)) return "";
            return Url::EncodeQuery(fields[field].options[i]);
        };
        std::string sortBy = option(1, false);
        for (auto& ch : sortBy) ch = (char)std::tolower((unsigned char)ch);
        std::string base = pageUrl(consoleUrl.substr(0, consoleUrl.find('?')), 1);
        return base + "?search=" + Url::EncodeQuery(search) + "&genre=" + option(3, true) + "&region=" + option(0, true) +
               "&sort_by=" + sortBy + "&order=" + option(2, false);
    };
    return d;
}
}  // namespace

HexromScraper::HexromScraper() : DefinedScraper(hexrom()) {}
