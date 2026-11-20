#include "include/Catalog.h"

#include "../Version.h"
#include "../utils/include/HttpUtils.h"

#include <json-c/json.h>
#include <zlib.h>

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
struct Data {
    std::map<std::string, SiteData> sites;
    std::time_t generated = 0;
    size_t games = 0;
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
        if (line.empty() || line.back() != '\n') {
            if (gzeof(gz)) { /* last line without newline */ } else continue;
        }
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
    return data;
}

void install(std::shared_ptr<Data> data, const char* source) {
    if (!data) return;
    std::cerr << "[Catalog] loaded " << data->games << " games from " << source << std::endl;
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

std::string lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}
}  // namespace

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
    std::vector<std::string> words;
    std::istringstream ss(lower(query));
    for (std::string w; ss >> w;) words.push_back(w);
    out.clear();
    for (auto& g : all) {
        std::string t = lower(g.label);
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

}  // namespace Catalog
