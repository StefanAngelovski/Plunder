// Plunder catalog crawler.
//
// Builds catalog.tsv: every console and every game listing from the supported
// ROM sites, using the exact scraper code the app uses (so parsing matches).
// Run by .github/workflows/catalog.yml; the app downloads the result.
//
//   crawler OUT.tsv [--previous OLD.tsv] [--jobs N] [--max-pages N] [--only TEXT]
//
// Output format (UTF-8, tab-separated, one record per line):
//   #plunder-catalog <TAB> 1 <TAB> <generated ISO-8601 UTC>
//   C <TAB> site <TAB> console label <TAB> console url
//   G <TAB> site <TAB> console url <TAB> title <TAB> game page url <TAB> image url
// Games appear in site order (as the site lists them, page by page).
//
// Resilience: if a console fails or returns far fewer games than in the
// previous catalog (site blocked us, network trouble), its previous entries
// are kept instead, so one bad run never wipes data.

#include "scraper/Hexrom/include/HexromScraper.h"
#include "scraper/Romspedia/include/RomspediaScraper.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <thread>
#include <vector>

namespace {

struct Game { std::string title, url, image; };
struct Console { std::string label, url; std::vector<Game> games; };
struct Site { std::string id; std::shared_ptr<SiteScraper> scraper; };

std::string clean(std::string s) {
    for (auto& c : s) if (c == '\t' || c == '\n' || c == '\r') c = ' ';
    size_t b = s.find_first_not_of(' '), e = s.find_last_not_of(' ');
    return b == std::string::npos ? "" : s.substr(b, e - b + 1);
}

std::string nowIso() {
    char buf[32];
    std::time_t t = std::time(nullptr);
    std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
    return buf;
}

// Previous catalog: (site, console url) -> games, plus console labels
using Previous = std::map<std::pair<std::string, std::string>, std::vector<Game>>;
std::map<std::pair<std::string, std::string>, std::string> g_prevLabels;

Previous loadPrevious(const std::string& path) {
    Previous prev;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        if (line.size() < 2 || line[1] != '\t' || (line[0] != 'G' && line[0] != 'C')) continue;
        std::vector<std::string> f;
        std::stringstream ss(line);
        std::string part;
        while (std::getline(ss, part, '\t')) f.push_back(part);
        if (line[0] == 'C' && f.size() >= 4) { g_prevLabels[{f[1], f[3]}] = f[2]; prev[{f[1], f[3]}]; }
        if (line[0] == 'G' && f.size() >= 6) prev[{f[1], f[2]}].push_back({f[3], f[4], f[5]});
    }
    return prev;
}

std::vector<ListItem> fetchPage(SiteScraper& s, const std::string& url, int page) {
    for (int attempt = 0; attempt < 2; ++attempt) {
        auto items = s.fetchGames(url, page).first;
        if (!items.empty()) return items;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    return {};
}

// Fetch all pages of one console: parallel batches until a page comes back empty
// or repeats an already-seen game (some sites serve the last page again).
std::vector<Game> crawlConsole(SiteScraper& s, const std::string& url, int jobs, int maxPages, int& pagesFetched) {
    std::vector<Game> games;
    std::set<std::string> seen;
    pagesFetched = 0;
    int next = 1;
    bool done = false;
    while (!done && next <= maxPages) {
        int batch = std::min(jobs, maxPages - next + 1);
        std::vector<std::vector<ListItem>> results(batch);
        std::vector<std::thread> threads;
        for (int i = 0; i < batch; ++i)
            threads.emplace_back([&, i] { results[i] = fetchPage(s, url, next + i); });
        for (auto& t : threads) t.join();
        for (int i = 0; i < batch && !done; ++i) {
            ++pagesFetched;
            if (results[i].empty()) { done = true; break; }
            int added = 0;
            for (auto& it : results[i]) {
                std::string key = clean(it.downloadUrl);
                if (key.empty() || !seen.insert(key).second) continue;
                games.push_back({clean(it.label), key, clean(it.imagePath)});
                ++added;
            }
            if (added == 0) done = true;  // page repeated earlier results
        }
        next += batch;
    }
    return games;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s OUT.tsv [--previous OLD.tsv] [--jobs N] [--max-pages N] [--only TEXT]\n", argv[0]);
        return 2;
    }
    std::string out = argv[1], previousPath, only;
    int jobs = 4, maxPages = 1500;
    for (int i = 2; i + 1 < argc; i += 2) {
        std::string a = argv[i];
        if (a == "--previous") previousPath = argv[i + 1];
        else if (a == "--jobs") jobs = std::max(1, std::atoi(argv[i + 1]));
        else if (a == "--max-pages") maxPages = std::max(1, std::atoi(argv[i + 1]));
        else if (a == "--only") only = argv[i + 1];
    }
    Previous prev = previousPath.empty() ? Previous{} : loadPrevious(previousPath);
    std::fprintf(stderr, "previous catalog: %zu consoles\n", prev.size());

    std::vector<Site> sites = {
        {"Hexrom", std::make_shared<HexromScraper>()},
        {"Romspedia", std::make_shared<RomspediaScraper>()},
    };

    auto started = std::chrono::steady_clock::now();
    std::ostringstream body;
    size_t totalGames = 0, totalConsoles = 0, reused = 0;
    for (auto& site : sites) {
        std::vector<ListItem> consoles = site.scraper->fetchConsoles();
        std::fprintf(stderr, "== %s: %zu consoles\n", site.id.c_str(), consoles.size());
        if (consoles.empty()) {
            // Site unreachable: keep every console we had for it.
            for (auto& kv : prev) {
                if (kv.first.first != site.id) continue;
                auto lbl = g_prevLabels.find(kv.first);
                body << "C\t" << site.id << "\t" << (lbl != g_prevLabels.end() ? lbl->second : kv.first.second)
                     << "\t" << kv.first.second << "\n";
                for (auto& g : kv.second)
                    body << "G\t" << site.id << "\t" << kv.first.second << "\t" << g.title << "\t" << g.url << "\t" << g.image << "\n";
                totalGames += kv.second.size(); ++totalConsoles; ++reused;
            }
            std::fprintf(stderr, "   WARN site returned no consoles; kept previous data\n");
            continue;
        }
        for (auto& c : consoles) {
            std::string label = clean(c.label), url = clean(c.downloadUrl);
            if (url.empty()) continue;
            if (!only.empty() && label.find(only) == std::string::npos) continue;
            auto t0 = std::chrono::steady_clock::now();
            int pages = 0;
            std::vector<Game> games = crawlConsole(*site.scraper, url, jobs, maxPages, pages);
            double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            auto old = prev.find({site.id, url});
            bool useOld = old != prev.end() && games.size() < old->second.size() / 2;
            if (useOld) { games = old->second; ++reused; }
            std::fprintf(stderr, "   %-45s %6zu games  %4d pages  %6.1fs%s\n", label.c_str(), games.size(), pages, secs,
                         useOld ? "  WARN kept previous (crawl came back short)" : "");
            body << "C\t" << site.id << "\t" << label << "\t" << url << "\n";
            for (auto& g : games)
                body << "G\t" << site.id << "\t" << url << "\t" << g.title << "\t" << g.url << "\t" << g.image << "\n";
            totalGames += games.size();
            ++totalConsoles;
        }
    }

    std::ofstream f(out, std::ios::binary);
    f << "#plunder-catalog\t1\t" << nowIso() << "\n" << body.str();
    f.close();
    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    std::fprintf(stderr, "wrote %s: %zu consoles, %zu games (%zu consoles reused from previous) in %.0fs\n",
                 out.c_str(), totalConsoles, totalGames, reused, secs);
    // Machine-readable summary on stdout for the workflow.
    std::printf("consoles=%zu\ngames=%zu\nreused=%zu\n", totalConsoles, totalGames, reused);
    return totalGames > 0 ? 0 : 1;
}
