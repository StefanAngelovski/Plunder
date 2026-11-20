#include "include/Catalog.h"

#include "../Version.h"
#include "../utils/include/HttpUtils.h"
#include "../app/consolePolicies/ConsoleFolderMap.h"

#include <json-c/json.h>
#include <zlib.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <sys/stat.h>
#include <thread>
#include <unordered_map>

namespace Catalog {

namespace {
const std::string kDir = "catalog";                    // relative to the app folder (cwd)
const std::string kData = kDir + "/catalog.tsv.gz";
const std::string kManifest = kDir + "/catalog.json";
const std::string kBaseUrl = PLUNDER_CATALOG_BASE_URL;  // see Version.h

struct ConsoleData { std::string label, url; std::vector<ListItem> games; };
struct SiteData {
    std::vector<ConsoleData> consoles;
    std::unordered_map<std::string, size_t> byUrl;
};
// One merged game for global search. `norm` is " " + normalized title, so
// " word" finds matches at the start of any word.
struct Entry {
    std::string title, norm, squashed, consoleKey;   // squashed = norm without spaces
    std::vector<Source> sources;
};
struct Data {
    std::map<std::string, SiteData> sites;
    std::time_t generated = 0;
    size_t games = 0;
    std::vector<Entry> entries;
    std::vector<ConsoleGroup> groups;
    std::unordered_map<std::string, std::string> groupNames;
};

std::mutex g_mutex;
std::shared_ptr<const Data> g_data;   // swapped atomically under g_mutex
std::atomic<bool> g_started{false};

std::shared_ptr<const Data> current() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_data;
}

std::time_t parseIsoUtc(const std::string& s) {
    struct tm tm = {};
    if (sscanf(s.c_str(), "%d-%d-%dT%d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6) return 0;
    tm.tm_year -= 1900; tm.tm_mon -= 1;
    return timegm(&tm);
}

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> f;
    size_t start = 0;
    while (true) {
        size_t tab = line.find('\t', start);
        f.push_back(line.substr(start, tab == std::string::npos ? std::string::npos : tab - start));
        if (tab == std::string::npos) break;
        start = tab + 1;
    }
    return f;
}

// Latin-1 supplement letters (UTF-8 0xC3 0x80..0xBF) folded to ASCII.
char foldC3(unsigned char d) {
    if (d >= 0x80 && d <= 0x86) return 'a';
    if (d == 0x87) return 'c';
    if (d >= 0x88 && d <= 0x8B) return 'e';
    if (d >= 0x8C && d <= 0x8F) return 'i';
    if (d == 0x90) return 'd';
    if (d == 0x91) return 'n';
    if ((d >= 0x92 && d <= 0x96) || d == 0x98) return 'o';
    if (d >= 0x99 && d <= 0x9C) return 'u';
    if (d == 0x9D) return 'y';
    if (d == 0x9F) return 's';
    if (d >= 0xA0 && d <= 0xA6) return 'a';
    if (d == 0xA7) return 'c';
    if (d >= 0xA8 && d <= 0xAB) return 'e';
    if (d >= 0xAC && d <= 0xAF) return 'i';
    if (d == 0xB1) return 'n';
    if ((d >= 0xB2 && d <= 0xB6) || d == 0xB8) return 'o';
    if (d >= 0xB9 && d <= 0xBC) return 'u';
    if (d == 0xBD || d == 0xBF) return 'y';
    return 0;
}

// "GBA ROMs (1963)" -> "GBA ROMs"
std::string stripCount(const std::string& label) {
    size_t end = label.find_last_not_of(' ');
    if (end == std::string::npos || label[end] != ')') return label;
    size_t open = label.rfind('(', end);
    if (open == std::string::npos || open + 1 >= end) return label;
    for (size_t i = open + 1; i < end; ++i) if (!std::isdigit((unsigned char)label[i])) return label;
    std::string s = label.substr(0, open);
    while (!s.empty() && s.back() == ' ') s.pop_back();
    return s;
}

std::string squash(const std::string& s) {
    std::string out;
    for (char c : s) if (c != ' ') out += c;
    return out;
}

int scoreFor(const std::string& norm, const std::string& squashed, const std::string& q, const std::vector<std::string>& words) {
    if (q.empty()) return 0;
    bool allWords = true;
    for (const auto& w : words) if (norm.find(w) == std::string::npos) { allWords = false; break; }
    if (!allWords) {
        // Spacing-insensitive fallback: "fire red" finds "FireRed".
        std::string qs = squash(q);
        if (qs.size() < 3 || squashed.find(qs) == std::string::npos) return -1;
        return 500 - (int)norm.size();
    }
    int s = 0;
    if (norm.compare(1, std::string::npos, q) == 0) s += 5000;            // exact title
    else if (norm.compare(1, q.size(), q) == 0) s += 2000;                // title starts with query
    else if (norm.find(" " + q) != std::string::npos) s += 1000;          // phrase at a word start
    for (const auto& w : words) if (norm.find(" " + w) != std::string::npos) s += 100;  // word starts
    return s - (int)norm.size();                                          // shorter titles first
}

void buildGlobal(Data& data) {
    std::map<std::string, std::vector<std::string>> names;  // consoleKey -> display name candidates
    std::map<std::string, size_t> counts;
    for (auto& sitePair : data.sites) {
        for (auto& console : sitePair.second.consoles) {
            std::string key = ConsoleKeyFor(console.label);
            names[key].push_back(ConsoleNameFor(console.label));
            for (auto& g : console.games) {
                // One entry per listing: a game on two sites appears twice.
                std::string norm = NormalizeForSearch(g.label);
                std::string title = CleanTitle(g.label);
                data.entries.push_back({title, " " + norm, squash(norm), key,
                                        {{title, sitePair.first, console.label, g.downloadUrl, g.imagePath}}});
                ++counts[key];
            }
        }
    }
    for (auto& kv : names) {
        auto& v = kv.second;
        std::sort(v.begin(), v.end(), [](const std::string& a, const std::string& b) {
            return a.size() != b.size() ? a.size() < b.size() : a < b;
        });
        data.groupNames[kv.first] = v.front();
        data.groups.push_back({kv.first, v.front(), counts[kv.first]});
    }
    std::sort(data.groups.begin(), data.groups.end(), [](const ConsoleGroup& a, const ConsoleGroup& b) {
        std::string x = a.name, y = b.name;
        std::transform(x.begin(), x.end(), x.begin(), ::tolower);
        std::transform(y.begin(), y.end(), y.begin(), ::tolower);
        return x < y;
    });
}

// Parses catalog.tsv.gz; returns nullptr if missing, corrupt or an unknown format.
std::shared_ptr<Data> loadFile(const std::string& path) {
    gzFile gz = gzopen(path.c_str(), "rb");
    if (!gz) return nullptr;
    auto data = std::make_shared<Data>();
    std::string line;
    char buf[8192];
    bool header = false, ok = true;
    while (gzgets(gz, buf, sizeof buf)) {
        line += buf;
        if (line.back() != '\n' && !gzeof(gz)) continue;  // long line: keep reading
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
        std::vector<std::string> f = splitTabs(line);
        line.clear();
        if (!header) {
            if (f.size() < 3 || f[0] != "#plunder-catalog" || f[1] != "1") { ok = false; break; }
            data->generated = parseIsoUtc(f[2]);
            header = true;
            continue;
        }
        if (f[0] == "C" && f.size() >= 4) {
            SiteData& site = data->sites[f[1]];
            site.byUrl[f[3]] = site.consoles.size();
            site.consoles.push_back({f[2], f[3], {}});
        } else if (f[0] == "G" && f.size() >= 6) {
            auto s = data->sites.find(f[1]);
            if (s == data->sites.end()) continue;
            auto c = s->second.byUrl.find(f[2]);
            if (c == s->second.byUrl.end()) continue;
            ListItem item;
            item.label = f[3];
            item.downloadUrl = f[4];
            item.imagePath = f[5];
            s->second.consoles[c->second].games.push_back(std::move(item));
            ++data->games;
        }
    }
    int err = 0;
    gzerror(gz, &err);
    gzclose(gz);
    if (!ok || !header || (err != Z_OK && err != Z_STREAM_END) || data->games == 0) return nullptr;
    buildGlobal(*data);
    return data;
}

void install(std::shared_ptr<Data> data, const char* source) {
    if (!data) return;
    std::cerr << "[Catalog] loaded " << data->games << " games (" << data->entries.size()
              << " unique, " << data->groups.size() << " consoles) from " << source << std::endl;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_data = std::move(data);
}

std::string manifestSha(const std::string& json) {
    std::string sha;
    if (json_object* obj = json_tokener_parse(json.c_str())) {
        json_object* v = nullptr;
        if (json_object_object_get_ex(obj, "sha256", &v)) sha = json_object_get_string(v) ? json_object_get_string(v) : "";
        int format = 0;
        if (json_object_object_get_ex(obj, "format", &v)) format = json_object_get_int(v);
        if (format != 1) sha.clear();  // unknown format: ignore
        json_object_put(obj);
    }
    return sha;
}

std::string readFile(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return "";
    std::string s;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) s.append(buf, n);
    fclose(f);
    return s;
}

void writeFile(const std::string& path, const std::string& content) {
    if (FILE* f = fopen(path.c_str(), "wb")) { fwrite(content.data(), 1, content.size(), f); fclose(f); }
}

void refresh() {
    mkdir(kDir.c_str(), 0755);
    // 1. Saved copy first: works offline and is instant.
    install(loadFile(kData), "SD card");

    // 2. Small manifest tells us whether a newer catalog exists.
    std::string remote = HttpUtils::fetchWebContent(kBaseUrl + "/catalog.json");
    std::string remoteSha = manifestSha(remote);
    if (remoteSha.empty()) { std::cerr << "[Catalog] no remote manifest (offline?)" << std::endl; return; }
    if (remoteSha == manifestSha(readFile(kManifest)) && current()) return;  // up to date

    // 3. Download to a temp file; only replace the saved copy if it parses.
    const std::string tmp = kData + ".part";
    if (!HttpUtils::downloadFile(kBaseUrl + "/catalog.tsv.gz", tmp)) { remove(tmp.c_str()); return; }
    std::shared_ptr<Data> fresh = loadFile(tmp);
    if (!fresh) { std::cerr << "[Catalog] downloaded catalog is invalid; keeping old one" << std::endl; remove(tmp.c_str()); return; }
    rename(tmp.c_str(), kData.c_str());
    writeFile(kManifest, remote);
    install(fresh, "GitHub");
}

std::vector<std::string> queryWords(const std::string& q) {
    std::vector<std::string> words;
    std::istringstream ss(q);
    for (std::string w; ss >> w;) words.push_back(w);
    return words;
}

void sortResults(std::vector<std::pair<int, GlobalResult>>& scored, SortOrder sort, bool hasQuery) {
    auto byTitle = [](const GlobalResult& a, const GlobalResult& b) {
        return NormalizeForSearch(a.title) < NormalizeForSearch(b.title);
    };
    if (sort == SortOrder::TitleAZ) {
        std::stable_sort(scored.begin(), scored.end(), [&](const auto& a, const auto& b) { return byTitle(a.second, b.second); });
    } else if (sort == SortOrder::TitleZA) {
        std::stable_sort(scored.begin(), scored.end(), [&](const auto& a, const auto& b) { return byTitle(b.second, a.second); });
    } else if (hasQuery) {
        std::stable_sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
            if (a.first != b.first) return a.first > b.first;
            return a.second.sources.size() > b.second.sources.size();
        });
    }
}
}  // namespace

// ---- Normalisation / console naming ---------------------------------------------

std::string CleanTitle(const std::string& in) {
    static const std::pair<const char*, const char*> entities[] = {
        {"&amp;", "&"}, {"&#038;", "&"}, {"&#38;", "&"}, {"&#8211;", "-"}, {"&#8212;", "-"},
        {"&#8217;", "'"}, {"&#8216;", "'"}, {"&#039;", "'"}, {"&#39;", "'"}, {"&quot;", "\""}, {"&#8220;", "\""}, {"&#8221;", "\""}};
    std::string s = in;
    for (const auto& e : entities)
        for (size_t p; (p = s.find(e.first)) != std::string::npos;) s.replace(p, std::char_traits<char>::length(e.first), e.second);
    std::string out;
    for (char c : s) if (!(c == ' ' && !out.empty() && out.back() == ' ')) out += c;
    while (!out.empty() && out.back() == ' ') out.pop_back();
    if (out.size() > 4 && out.compare(out.size() - 4, 4, " ROM") == 0) out.erase(out.size() - 4);
    while (!out.empty() && out.back() == ' ') out.pop_back();
    size_t start = out.find_first_not_of(' ');
    return start == std::string::npos ? "" : out.substr(start);
}

std::string NormalizeForSearch(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    bool lastSpace = true;
    auto push = [&](char c) {
        if (c == ' ') { if (!lastSpace) { out += ' '; lastSpace = true; } }
        else { out += c; lastSpace = false; }
    };
    for (size_t i = 0; i < in.size();) {
        unsigned char c = (unsigned char)in[i];
        if (c == '&') {  // HTML entity like &amp; or &#8211;
            size_t semi = in.find(';', i);
            if (semi != std::string::npos && semi - i <= 8) { push(' '); i = semi + 1; continue; }
        }
        if (c < 0x80) { push(std::isalnum(c) ? (char)std::tolower(c) : ' '); ++i; continue; }
        if (c == 0xC3 && i + 1 < in.size()) {
            char base = foldC3((unsigned char)in[i + 1]);
            push(base ? base : ' ');
            i += 2;
            continue;
        }
        size_t len = (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 1;
        push(' ');
        i += len;
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    for (const char* suffix : {" roms", " rom"}) {
        size_t n = std::char_traits<char>::length(suffix);
        if (out.size() > n && out.compare(out.size() - n, n, suffix) == 0) { out.erase(out.size() - n); break; }
    }
    return out;
}

std::string ConsoleKeyFor(const std::string& consoleLabel) {
    std::string folder = getFolderForScrapedConsole(consoleLabel);
    if (!folder.empty()) return folder;
    return "~" + NormalizeForSearch(stripCount(consoleLabel));  // no device folder: group by name
}

std::string ConsoleNameFor(const std::string& consoleLabel) {
    std::string s = stripCount(consoleLabel);
    for (size_t p; (p = s.find("&amp;")) != std::string::npos;) s.replace(p, 5, "&");
    // Drop trailing "ROMs", "ISOs", "&" words: "PS2 ROMs & ISOs" -> "PS2"
    while (true) {
        while (!s.empty() && s.back() == ' ') s.pop_back();
        size_t sp = s.find_last_of(' ');
        std::string last = sp == std::string::npos ? s : s.substr(sp + 1);
        std::string lower = last;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (sp != std::string::npos && (lower == "roms" || lower == "rom" || lower == "isos" || lower == "iso" || lower == "&")) {
            s.erase(sp);
            continue;
        }
        break;
    }
    return s.empty() ? stripCount(consoleLabel) : s;
}

// ---- Catalog access ---------------------------------------------------------------

void StartBackgroundRefresh() {
    if (g_started.exchange(true)) return;
    std::thread(refresh).detach();
}

bool IsReady() { return (bool)current(); }

std::vector<ListItem> Consoles(const std::string& site) {
    std::vector<ListItem> out;
    auto data = current();
    if (!data) return out;
    auto s = data->sites.find(site);
    if (s == data->sites.end()) return out;
    for (const auto& c : s->second.consoles) {
        ListItem item;
        item.label = c.label;
        item.downloadUrl = c.url;
        out.push_back(item);
    }
    return out;
}

bool Games(const std::string& site, const std::string& consoleUrl, std::vector<ListItem>& out) {
    auto data = current();
    if (!data) return false;
    auto s = data->sites.find(site);
    if (s == data->sites.end()) return false;
    auto c = s->second.byUrl.find(consoleUrl);
    if (c == s->second.byUrl.end()) return false;
    out = s->second.consoles[c->second].games;
    return true;
}

bool Search(const std::string& site, const std::string& consoleUrl, const std::string& query, std::vector<ListItem>& out) {
    std::vector<ListItem> all;
    if (!Games(site, consoleUrl, all)) return false;
    std::vector<std::string> words = queryWords(NormalizeForSearch(query));
    out.clear();
    for (auto& g : all) {
        std::string t = NormalizeForSearch(g.label);
        bool match = true;
        for (const auto& w : words) if (t.find(w) == std::string::npos) { match = false; break; }
        if (match) out.push_back(std::move(g));
    }
    return true;
}

std::string AgeText() {
    auto data = current();
    if (!data || !data->generated) return "";
    long mins = (long)((std::time(nullptr) - data->generated) / 60);
    if (mins < 0) mins = 0;
    std::string age = mins < 60 ? std::to_string(mins) + " min" : mins < 48 * 60 ? std::to_string(mins / 60) + " h" : std::to_string(mins / 1440) + " days";
    return "catalog updated " + age + " ago";
}

std::vector<ConsoleGroup> ConsoleGroups() {
    auto data = current();
    return data ? data->groups : std::vector<ConsoleGroup>{};
}

std::vector<std::string> Sites() {
    std::vector<std::string> out;
    if (auto data = current()) for (auto& kv : data->sites) out.push_back(kv.first);
    return out;
}

size_t TotalGames() {
    auto data = current();
    return data ? data->entries.size() : 0;
}

std::vector<GlobalResult> GlobalSearch(const GlobalQuery& query) {
    std::vector<GlobalResult> out;
    auto data = current();
    if (!data) return out;
    const std::string q = NormalizeForSearch(query.text);
    const std::vector<std::string> words = queryWords(q);
    std::vector<std::pair<int, GlobalResult>> scored;
    for (const Entry& e : data->entries) {
        if (!query.consoleKey.empty() && e.consoleKey != query.consoleKey) continue;
        int score = scoreFor(e.norm, e.squashed, q, words);
        if (score < 0 && !q.empty()) continue;
        GlobalResult r;
        r.title = e.title;
        r.consoleKey = e.consoleKey;
        auto name = data->groupNames.find(e.consoleKey);
        r.consoleName = name != data->groupNames.end() ? name->second : e.consoleKey;
        for (const Source& s : e.sources)
            if (query.site.empty() || s.site == query.site) r.sources.push_back(s);
        if (r.sources.empty()) continue;
        scored.emplace_back(score, std::move(r));
    }
    sortResults(scored, query.sort, !q.empty());
    out.reserve(scored.size());
    for (auto& p : scored) out.push_back(std::move(p.second));
    return out;
}

std::string ConsoleLabelForGameUrl(const std::string& site, const std::string& gameUrl) {
    auto data = current();
    if (!data) return "";
    auto s = data->sites.find(site);
    if (s == data->sites.end()) return "";
    for (const auto& c : s->second.consoles) {
        std::string prefix = c.url;
        if (!prefix.empty() && prefix.back() != '/') prefix += '/';
        if (gameUrl.compare(0, prefix.size(), prefix) == 0) return c.label;
    }
    return "";
}

std::string ImageForGameUrl(const std::string& site, const std::string& gameUrl) {
    auto data = current();
    if (!data) return "";
    auto s = data->sites.find(site);
    if (s == data->sites.end()) return "";
    for (const auto& c : s->second.consoles)
        for (const auto& g : c.games)
            if (g.downloadUrl == gameUrl) return g.imagePath;
    return "";
}

std::vector<GlobalResult> Rank(const std::vector<GlobalResult>& results, const std::string& query) {
    const std::string q = NormalizeForSearch(query);
    const std::vector<std::string> words = queryWords(q);
    std::vector<std::pair<int, GlobalResult>> scored;
    for (const auto& r : results) {
        std::string norm = " " + NormalizeForSearch(r.title);
        // Live results already matched on the site's side; keep even ones our matcher wouldn't.
        scored.emplace_back(scoreFor(norm, squash(norm), q, words), r);
    }
    sortResults(scored, SortOrder::BestMatch, !q.empty());
    std::vector<GlobalResult> out;
    for (auto& p : scored) out.push_back(std::move(p.second));
    return out;
}

}  // namespace Catalog
