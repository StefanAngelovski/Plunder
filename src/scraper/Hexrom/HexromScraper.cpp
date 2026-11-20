#include "include/HexromScraper.h"

#include "../engine/Url.h"

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
    return d;
}
}  // namespace

HexromScraper::HexromScraper() : DefinedScraper(hexrom()) {}
