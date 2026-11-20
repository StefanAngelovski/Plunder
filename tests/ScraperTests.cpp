// Scraper tests: run the real site definitions on saved pages (tests/fixtures)
// and check what they extract. When a site changes its layout, re-save the page
// (see tests/README.md) and these show exactly what broke.
//
// Build & run from the repo root: tests/run.sh
#include "scraper/Hexrom/include/HexromScraper.h"
#include "scraper/Romspedia/include/RomspediaScraper.h"
#include "scraper/GithubReleases/include/GitHubReleasesScraper.h"
#include "scraper/engine/Blacklist.h"
#include "scraper/engine/Url.h"

#include <cstdio>
#include <fstream>
#include <functional>
#include <sstream>

namespace {
int g_failures = 0, g_checks = 0;

void check(bool ok, const std::string& what, const std::string& detail = "") {
    ++g_checks;
    if (ok) return;
    ++g_failures;
    std::printf("  FAIL %s%s%s\n", what.c_str(), detail.empty() ? "" : ": ", detail.c_str());
}
void checkEq(const std::string& actual, const std::string& expected, const std::string& what) {
    check(actual == expected, what, "expected \"" + expected + "\", got \"" + actual + "\"");
}
void checkPrefix(const std::string& actual, const std::string& prefix, const std::string& what) {
    check(actual.compare(0, prefix.size(), prefix) == 0, what, "expected to start with \"" + prefix + "\", got \"" + actual.substr(0, 80) + "\"");
}
void checkCount(size_t actual, size_t expected, const std::string& what) {
    check(actual == expected, what, "expected " + std::to_string(expected) + ", got " + std::to_string(actual));
}

std::string fixture(const std::string& name) {
    std::ifstream f("tests/fixtures/" + name, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    if (ss.str().empty()) std::printf("  missing fixture %s (run from the repo root)\n", name.c_str());
    return ss.str();
}

const ListItem* find(const std::vector<ListItem>& items, const std::string& label) {
    for (const auto& i : items) if (i.label == label) return &i;
    return nullptr;
}

void test(const char* name, const std::function<void()>& body) {
    int before = g_failures;
    body();
    std::printf("%s %s\n", g_failures == before ? "ok  " : "FAIL", name);
}
}  // namespace

int main() {
    HexromScraper hexrom;
    RomspediaScraper romspedia;

    // ---- Hexrom -----------------------------------------------------------------------
    test("hexrom: consoles", [&] {
        auto consoles = hexrom.parseConsoles(fixture("hexrom/consoles.html"));
        checkCount(consoles.size(), 45, "consoles after blacklist (99 on the page)");
        const ListItem* gba = find(consoles, "GBA ROMs (1963)");
        check(gba != nullptr, "GBA listed with its count");
        if (gba) checkEq(gba->downloadUrl, "https://hexrom.com/roms/gameboy-advance/", "GBA url");
        check(find(consoles, "Nintendo 3DS Roms (564)") == nullptr, "3DS blacklisted");
    });
    test("hexrom: game list", [&] {
        auto page = hexrom.parseCards(fixture("hexrom/list_gba.html"), hexrom.definition().games);
        checkCount(page.first.size(), 12, "games on the page");
        checkCount(page.second, 164, "page count");
        if (!page.first.empty()) checkEq(page.first[0].label, "Pokemon Infinite Fusion", "first game");
        const ListItem* fr = find(page.first, "Pokemon FireRed");
        check(fr != nullptr, "FireRed listed");
        if (fr) {
            checkEq(fr->downloadUrl, "https://hexrom.com/pokemon-fire-red/", "FireRed page");
            checkEq(fr->imagePath, "https://hexrom.com/wp-content/uploads/2025/04/Pokemon-FireRed.webp", "FireRed cover (lazy-load data-src)");
        }
        check(find(page.first, "Pokemon Sword & Shield") != nullptr, "entities decoded in titles");
    });
    test("hexrom: game details", [&] {
        GameDetails d = hexrom.parseDetails(fixture("hexrom/details_firered.html"));
        checkEq(d.title, "Pokemon FireRed", "title");
        checkEq(d.consoleName, "GBA ROMs", "console (first link of the cell)");
        checkEq(d.fileSize, "5 MB", "size");
        checkEq(d.releaseDate, "29 Apr 2025", "release");
        checkEq(d.language, "USA", "region");
        checkEq(d.genre, "Role Playing", "genre");
        check(!d.downloads.empty(), "downloads");
        checkPrefix(d.about, "Pokemon FireRed ROM brings back", "description (not the 'Download Rom' label)");
        checkEq(d.iconUrl, "https://hexrom.com/wp-content/uploads/2025/04/Pokemon-FireRed.webp", "cover");
    });
    test("hexrom: download link", [&] {
        checkEq(hexrom.parseDownloadLink(fixture("hexrom/download_firered.html")),
                "https://s1.hexrom.com/rom/gba/Pokemon_FireRed_Version_%5Bhexrom.com%5D.zip", "archive link");
        checkEq(hexrom.definition().download.pageUrl("https://hexrom.com/pokemon-fire-red/"),
                "https://hexrom.com/pokemon-fire-red/download/", "download page url");
    });
    test("hexrom: site search", [&] {
        auto results = hexrom.parseCards(fixture("hexrom/search_minish.html"), hexrom.definition().search).first;
        checkCount(results.size(), 5, "result cards (games, emulators, blog post)");
        const ListItem* minish = find(results, "The Legend Of Zelda – The Minish Cap");
        check(minish != nullptr, "Minish Cap found");
        if (minish) {
            checkEq(minish->console, "GBA", "console line");
            checkEq(minish->downloadUrl, "https://hexrom.com/the-legend-of-zelda-the-minish-cap-rom/", "game page");
            checkEq(minish->imagePath, "https://hexrom.com/images/icon/n/Legend%20Of%20Zelda,%20The%20-%20The%20Minish%20Cap.jpg",
                    "cover with spaces/commas in the file name, encoded");
        }
    });
    test("hexrom: page and search URLs", [&] {
        auto& d = hexrom.definition();
        checkEq(d.pageUrl("https://hexrom.com/roms/gameboy-advance/", 1), "https://hexrom.com/roms/gameboy-advance/", "page 1");
        checkEq(d.pageUrl("https://hexrom.com/roms/gameboy-advance/", 3), "https://hexrom.com/roms/gameboy-advance/page/3/", "page 3");
        checkEq(d.pageUrl("https://hexrom.com/roms/gameboy-advance/page/1/?search=zelda&genre=", 2),
                "https://hexrom.com/roms/gameboy-advance/page/2/?search=zelda&genre=", "filter url keeps its query");
        checkEq(d.searchUrl("zelda minish"), "https://hexrom.com/?s=zelda+minish", "search url");
    });

    // ---- Romspedia ----------------------------------------------------------------------
    test("romspedia: consoles", [&] {
        auto consoles = romspedia.parseConsoles(fixture("romspedia/consoles.html"));
        // 61: the old regex scraper showed 62 because it mangled "SAM Coupé ROMs" into
        // "SAM Coup ROMs", which slipped past the blacklist entry for it.
        checkCount(consoles.size(), 61, "consoles after blacklist (131 on the page)");
        check(find(consoles, "SAM Coupé ROMs") == nullptr, "SAM Coupé blacklisted (accent read correctly)");
        const ListItem* gba = find(consoles, "GBA ROMs");
        check(gba != nullptr, "GBA listed");
        if (gba) checkEq(gba->downloadUrl, "https://www.romspedia.com/roms/gameboy-advance", "GBA url (relative link resolved)");
        check(find(consoles, "PS2 ROMs") == nullptr, "PS2 blacklisted");
    });
    test("romspedia: game list", [&] {
        auto page = romspedia.parseCards(fixture("romspedia/list_gba.html"), romspedia.definition().games);
        checkCount(page.first.size(), 47, "unique games (50 cards, 3 repeated; old regex found 25)");
        checkCount(page.second, 3, "highest page linked");
        const ListItem* em = find(page.first, "Pokemon Emerald");
        check(em != nullptr, "Emerald listed (title without ' ROM')");
        if (em) {
            checkEq(em->downloadUrl, "https://www.romspedia.com/roms/gameboy-advance/pokemon-emerald-usa", "Emerald page");
            checkEq(em->imagePath, "https://static.romspedia.com/webp/roms/pokemon-emerlad-cover-340x483.webp",
                    "real cover, not the commented-out thumbnail");
        }
    });
    test("romspedia: site search", [&] {
        auto results = romspedia.parseCards(fixture("romspedia/search_pokemon.html"), romspedia.definition().search).first;
        checkCount(results.size(), 17, "unique results (18 cards)");
        if (!results.empty()) {
            checkEq(results[0].label, "Pokemon X", "first result");
            checkEq(results[0].console, "3DS", "console tag");
            checkEq(results[0].downloadUrl, "https://www.romspedia.com/roms/nintendo-3ds/pokemon-x-game", "game page (card is the link)");
        }
    });
    test("romspedia: game details", [&] {
        GameDetails d = romspedia.parseDetails(fixture("romspedia/details_emerald.html"));
        checkEq(d.title, "Pokemon Emerald", "title (' ROM Download' removed)");
        checkEq(d.consoleName, "GBA", "console");
        checkEq(d.fileSize, "6.65MB", "size");
        checkEq(d.genre, "Role-Playing", "genre (Category)");
        checkEq(d.language, "Japanese", "region (never read before)");
        checkEq(d.releaseDate, "2004", "release year");
        check(!d.downloads.empty(), "downloads");
        check(d.about.size() > 200, "description", std::to_string(d.about.size()) + " chars");
    });
    test("romspedia: download link", [&] {
        checkEq(romspedia.parseDownloadLink(fixture("romspedia/download_emerald.html")),
                "https://downloads.romspedia.com/roms/1986%20-%20Pokemon%20Emerald%20(U)(TrashMan).zip", "archive link, spaces encoded");
        checkEq(romspedia.definition().download.pageUrl("https://www.romspedia.com/roms/gameboy-advance/pokemon-emerald-usa"),
                "https://www.romspedia.com/roms/gameboy-advance/pokemon-emerald-usa/download", "download page url");
    });
    test("romspedia: page and search URLs", [&] {
        auto& d = romspedia.definition();
        checkEq(d.pageUrl("https://www.romspedia.com/roms/gameboy-advance", 2), "https://www.romspedia.com/roms/gameboy-advance/page/2/", "list page 2");
        std::string s = d.searchUrl("pokemon emerald");
        checkEq(s, "https://www.romspedia.com/search?search_term_string=pokemon+emerald", "search url");
        checkEq(d.pageUrl(s, 2), s + "&currentpage=2", "search page 2");
        checkEq(d.pageUrl(s + "&currentpage=5", 1), s, "search page 1 drops an old page number");
        check(d.isSearchUrl(s) && !d.isSearchUrl("https://www.romspedia.com/roms/gameboy-advance"), "isSearchUrl");
    });

    // ---- GitHub release notes -------------------------------------------------------------
    test("github: release notes", [&] {
        std::string notes = ParseGitHubReleaseNotes(fixture("github/releases.html"));
        check(notes.find("[H2]🧪 Plunder v2.4.0[/H]") != std::string::npos, "newest release heading");
        check(notes.find("[H2]🧪 Plunder v1.0.0 Beta[/H]") != std::string::npos, "oldest release heading");
        size_t separators = 0;
        for (size_t p = 0; (p = notes.find("\n---\n", p)) != std::string::npos; ++p) ++separators;
        checkCount(separators, 5, "separators between 6 releases");
        check(notes.find("- Game catalog:") != std::string::npos, "list items");
        checkEq(ParseGitHubReleaseNotes("<html><body>nothing</body></html>"), "No release notes found.", "empty page");
    });

    // ---- Helpers ------------------------------------------------------------------------------
    test("url helpers", [&] {
        checkEq(Url::Absolute("/roms/gba", "https://x.com"), "https://x.com/roms/gba", "root-relative");
        checkEq(Url::Absolute("roms/gba", "https://x.com"), "https://x.com/roms/gba", "relative");
        checkEq(Url::Absolute("//cdn.x.com/a.png", "https://x.com"), "https://cdn.x.com/a.png", "protocol-relative");
        checkEq(Url::FromAttribute("https://a/1.webp 340w, https://a/2.webp 680w", true), "https://a/1.webp", "srcset first candidate");
        checkEq(Url::FromAttribute("https://static.x.com\t\t\t/webp/a.webp", false), "https://static.x.com/webp/a.webp", "tabs removed");
        checkEq(Url::EncodeUnsafe("https://x/a b%20c.zip"), "https://x/a%20b%20c.zip", "spaces encoded, escapes kept");
        checkEq(Url::EncodeQuery("zelda & link"), "zelda+%26+link", "query encoding");
    });
    test("blacklist", [&] {
        checkEq(Blacklist::Normalize("GBA ROMs (1963)"), "gba", "normalize");
        checkEq(Blacklist::Normalize("PS2 ROMs &amp; ISOs (124)"), Blacklist::Normalize("PS2 ROMs & ISOs (124)"), "entities don't matter");
        check(Blacklist::Contains("Romspedia", "PS2 ROMs", false), "Romspedia section");
        check(!Blacklist::Contains("Romspedia", "GBA ROMs", false), "GBA allowed");
    });

    std::printf("\n%d checks, %d failed\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
