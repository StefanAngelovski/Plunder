#include "include/RomspediaScraper.h"

#include "../engine/Url.h"

namespace {
const char* kArchiveLinks = "a[href$='.zip'], a[href$='.7z'], a[href$='.rar']";
const std::string kSearchParam = "search_term_string=";

bool isSearchUrl(const std::string& url) { return url.find(kSearchParam) != std::string::npos; }

// Console lists page as …/gameboy-advance/page/2/; search results as &currentpage=2.
std::string pageUrl(const std::string& listUrl, int page) {
    std::string url = listUrl;
    size_t cp = url.find("&currentpage=");
    if (cp != std::string::npos) url.erase(cp, url.find('&', cp + 1) == std::string::npos ? std::string::npos : url.find('&', cp + 1) - cp);
    if (page <= 1) return url;
    if (isSearchUrl(url)) return url + "&currentpage=" + std::to_string(page);
    if (url.back() != '/') url += '/';
    return url + "page/" + std::to_string(page) + "/";
}

SiteDefinition romspedia() {
    SiteDefinition d;
    d.id = "Romspedia";
    d.baseUrl = "https://www.romspedia.com";

    // <a href="roms/playstation-portable"><div class="pop-slide"> … <h2>PSP ROMs</h2> …
    d.consoles = {"https://www.romspedia.com/roms", "a:has(> div.pop-slide)",
                  {{"h2", ""}}, {}, {{"", "href"}}};

    // Console list card: <div class="single-rom"> … <picture><source srcset="cover"> …
    //   <div class="roms-ftr"><a href="game page"><h2 class="roms-title">Title</h2></a>
    // (an older, commented-out <picture> holds thumbnails; the parser skips comments)
    d.games = {"div.single-rom", {{"h2.roms-title", ""}},
               {{".roms-ftr a[href]", "href"}, {".roms-img a[href]", "href"}},
               {{"picture source[srcset]", "srcset"}, {"picture img", "src"}}, {}};
    // Search card: the whole card is wrapped in the game link and a tag names the console:
    //   <a href="game page"><div class="single-rom"> … <a class="btn … emulator">GBA</a>
    // Links inside links are invalid HTML, so (like browsers) the parser re-creates
    // the outer link inside the card: its first non-button link is the game page.
    d.search = {"div.single-rom", {{"h2.roms-title", ""}}, {{"a[href]:not(.btn)", "href"}},
                {{"picture source[srcset]", "srcset"}, {"picture img", "src"}}, {{"a.emulator", ""}}};
    d.pageLinks = "ul.pagination a";

    d.pageUrl = pageUrl;
    d.searchUrl = [](const std::string& q) { return "https://www.romspedia.com/search?" + kSearchParam + Url::EncodeQuery(q); };
    d.isSearchUrl = isSearchUrl;

    // Game page: <h2 class="main-title-big">, label/value <div>s, description article.
    d.details.title = {{"h2.main-title-big", ""}, {"h1", ""}};
    d.details.cover = {{"meta[property='og:image']", "content"}, {"picture source[srcset]", "srcset"}};
    d.details.about = {"div.descSec article p", "div.descSec article"};
    d.details.labels = ".view-emulator-detail-name";
    d.details.values = ".view-emulator-detail-value";

    // The archive link is on …/game/download (its URL has raw spaces; Url encodes them).
    d.download.pageUrl = [](const std::string& gameUrl) {
        std::string url = gameUrl;
        while (!url.empty() && url.back() == '/') url.pop_back();
        if (url.size() < 9 || url.compare(url.size() - 9, 9, "/download") != 0) url += "/download";
        return url;
    };
    d.download.link = {{kArchiveLinks, "href"}};
    return d;
}
}  // namespace

RomspediaScraper::RomspediaScraper() : DefinedScraper(romspedia()) {}
