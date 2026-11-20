#include "DefinedScraper.h"

#include "Blacklist.h"
#include "Url.h"
#include "../../utils/include/HttpUtils.h"

#include <algorithm>
#include <cctype>
#include <set>

namespace {
// Detail labels ("Size:", "Release Year:") -> GameDetails members.
std::string* detailField(GameDetails& d, std::string label) {
    std::transform(label.begin(), label.end(), label.begin(), ::tolower);
    while (!label.empty() && (label.back() == ':' || label.back() == ' ')) label.pop_back();
    if (label == "console") return &d.consoleName;
    if (label == "size" || label == "file size") return &d.fileSize;
    if (label == "publish" || label == "release" || label == "released" || label == "release year" || label == "release date") return &d.releaseDate;
    if (label == "region" || label == "language") return &d.language;
    if (label == "downloads") return &d.downloads;
    if (label == "genre" || label == "category") return &d.genre;
    if (label == "publisher") return &d.publisher;
    return nullptr;
}

// "Pokemon Emerald ROM Download" -> "Pokemon Emerald"
std::string stripTitleSuffix(std::string t) {
    for (const char* suffix : {" ROM Download", " Rom Download", " Download", " ROM", " Rom"}) {
        std::string s = suffix;
        if (t.size() > s.size() && t.compare(t.size() - s.size(), s.size(), s) == 0) { t.erase(t.size() - s.size()); break; }
    }
    return t;
}

int parsePageNumber(const std::string& text) {
    if (text.empty() || text.size() > 6) return 0;
    for (char c : text) if (!std::isdigit((unsigned char)c)) return 0;
    return std::stoi(text);
}
}  // namespace

DefinedScraper::DefinedScraper(SiteDefinition definition) : def_(std::move(definition)) {}

// ---- Picking values -------------------------------------------------------------------

std::string DefinedScraper::pick(const Html& page, Html::Node scope, const Picks& picks) const {
    for (const Pick& p : picks) {
        Html::Node node = p.css.empty() ? scope : page.first(p.css, scope);
        if (!node) continue;
        std::string v = p.attr.empty() ? Html::text(node) : Html::attr(node, p.attr);
        if (!p.attr.empty()) v = Url::FromAttribute(v, p.attr == "srcset");
        if (v.empty() || v.rfind("data:", 0) == 0) continue;  // skip lazy-load placeholders
        return v;
    }
    return "";
}

std::string DefinedScraper::pickUrl(const Html& page, Html::Node scope, const Picks& picks) const {
    std::string v = pick(page, scope, picks);
    return v.empty() ? "" : Url::EncodeUnsafe(Url::Absolute(v, def_.baseUrl));
}

// ---- Parsing -----------------------------------------------------------------------------

std::vector<ListItem> DefinedScraper::parseConsoles(const std::string& html) const {
    std::vector<ListItem> out;
    Html page(html);
    for (Html::Node item : page.all(def_.consoles.item)) {
        std::string name = pick(page, item, def_.consoles.name);
        std::string count = pick(page, item, def_.consoles.count);
        std::string link = pickUrl(page, item, def_.consoles.link);
        if (name.empty() || link.empty()) continue;
        ListItem console;
        console.label = count.empty() ? name : name + " (" + count + ")";
        console.downloadUrl = link;
        if (Blacklist::Contains(def_.id, console.label, def_.blacklistUsesWholeFile)) continue;
        out.push_back(console);
    }
    return out;
}

std::pair<std::vector<ListItem>, int> DefinedScraper::parseCards(const std::string& html, const CardRules& rules) const {
    std::vector<ListItem> games;
    std::set<std::string> seen;
    Html page(html);
    for (Html::Node card : page.all(rules.card)) {
        ListItem g;
        g.label = pick(page, card, rules.title);
        g.downloadUrl = pickUrl(page, card, rules.link);
        if (g.label.empty() || g.downloadUrl.empty() || !seen.insert(g.downloadUrl).second) continue;
        g.imagePath = pickUrl(page, card, rules.image);
        g.console = pick(page, card, rules.console);
        games.push_back(g);
    }
    int maxPage = 0;
    if (!def_.pageLinks.empty())
        for (Html::Node a : page.all(def_.pageLinks)) maxPage = std::max(maxPage, parsePageNumber(Html::text(a)));
    return {games, maxPage};
}

GameDetails DefinedScraper::parseDetails(const std::string& html) const {
    GameDetails d;
    Html page(html);
    d.title = stripTitleSuffix(pick(page, nullptr, def_.details.title));
    d.iconUrl = pickUrl(page, nullptr, def_.details.cover);

    std::vector<Html::Node> labels = page.all(def_.details.labels), values = page.all(def_.details.values);
    for (size_t i = 0; i < labels.size() && i < values.size(); ++i) {
        std::string* field = detailField(d, Html::text(labels[i]));
        if (!field || !field->empty()) continue;
        // Values that are links ("GBA ROMs > ROMs") use the first link's text.
        Html::Node link = page.first("a", values[i]);
        *field = Html::text(link ? link : values[i]);
    }

    for (const std::string& css : def_.details.about) {
        std::vector<Html::Node> paragraphs = page.all(css);
        std::string about;
        int used = 0;
        for (Html::Node p : paragraphs) {
            std::string t = Html::text(p);
            if (t.empty()) continue;
            about += (about.empty() ? "" : "\n\n") + t;
            if (++used == 4) break;
        }
        if (!about.empty()) { d.about = about; break; }
    }

    const UiUtils::Color grey(200, 200, 200);
    if (!d.consoleName.empty()) d.fields.push_back({"Console", d.consoleName, grey});
    if (!d.language.empty()) d.fields.push_back({"Region", d.language, grey});
    if (!d.fileSize.empty()) d.fields.push_back({"File Size", d.fileSize, grey});
    if (!d.releaseDate.empty()) d.fields.push_back({"Release", d.releaseDate, grey});
    if (!d.downloads.empty()) d.fields.push_back({"Downloads", d.downloads, grey});
    if (!d.genre.empty()) d.fields.push_back({"Genre", d.genre, grey});
    return d;
}

std::string DefinedScraper::parseDownloadLink(const std::string& html) const {
    Html page(html);
    return pickUrl(page, nullptr, def_.download.link);
}

// ---- Fetching -------------------------------------------------------------------------------

std::vector<ListItem> DefinedScraper::fetchConsoles() {
    std::string html = HttpUtils::fetchWebContent(def_.consoles.url);
    return html.empty() ? std::vector<ListItem>{} : parseConsoles(html);
}

std::pair<std::vector<ListItem>, PaginationInfo> DefinedScraper::fetchGames(const std::string& url, int page) {
    const bool isSearch = def_.isSearchUrl && def_.isSearchUrl(url);
    PaginationInfo pagination;
    pagination.baseUrl = url;
    pagination.currentPage = page;
    pagination.totalPages = 1;
    std::string html = HttpUtils::fetchWebContent(def_.pageUrl ? def_.pageUrl(url, page) : url);
    if (html.empty()) return {{}, pagination};
    auto parsed = parseCards(html, isSearch ? def_.search : def_.games);
    // Search results have no reliable page count: keep paging until a page is empty.
    pagination.totalPages = isSearch ? 999 : std::max(page, parsed.second);
    return {parsed.first, pagination};
}

GameDetails DefinedScraper::fetchGameDetails(const std::string& gameUrl) {
    std::string html = HttpUtils::fetchWebContent(gameUrl);
    if (html.empty()) return {};
    GameDetails d = parseDetails(html);
    d.downloadUrl = fetchDownloadLink(gameUrl);
    return d;
}

std::pair<std::vector<ListItem>, PaginationInfo> DefinedScraper::filterGames(
    const std::string& consoleUrl, const std::string& search, const std::vector<int>& choices, int page) {
    const FilterRules& f = def_.filters;
    std::string url = f.url ? f.url(consoleUrl, search, choices) : consoleUrl;
    if (!f.siteWideSearch) return fetchGames(url, page);

    // Site-wide search narrowed to one console: keep results under the console's
    // path (…/roms/gameboy-advance/…), fetching search pages until there are enough.
    const size_t kPerPage = 15;
    const int kMaxSearchPages = 20;
    PaginationInfo pagination;
    pagination.baseUrl = url;
    pagination.currentPage = page;
    std::string path = consoleUrl.substr(std::min(consoleUrl.size(), def_.baseUrl.size()));
    if (path.empty() || path.back() != '/') path += '/';
    std::vector<ListItem> matches;
    for (int p = 1; p <= kMaxSearchPages && matches.size() < (size_t)page * kPerPage; ++p) {
        std::vector<ListItem> results = fetchGames(url, p).first;
        if (results.empty()) break;
        for (auto& g : results)
            if (g.downloadUrl.find(path) != std::string::npos) matches.push_back(g);
    }
    size_t start = (size_t)(page - 1) * kPerPage;
    std::vector<ListItem> games;
    if (start < matches.size())
        games.assign(matches.begin() + start, matches.begin() + std::min(matches.size(), start + kPerPage));
    pagination.totalPages = (int)((matches.size() + kPerPage - 1) / kPerPage);
    return {games, pagination};
}

std::vector<ListItem> DefinedScraper::search(const std::string& query) {
    if (!def_.searchUrl) return {};
    std::string html = HttpUtils::fetchWebContent(def_.searchUrl(query));
    return html.empty() ? std::vector<ListItem>{} : parseCards(html, def_.search).first;
}

std::string DefinedScraper::fetchDownloadLink(const std::string& gameUrl) {
    std::string pageUrl = def_.download.pageUrl ? def_.download.pageUrl(gameUrl) : gameUrl;
    std::string html = HttpUtils::fetchWebContent(pageUrl);
    return html.empty() ? "" : parseDownloadLink(html);
}
