#include "Blacklist.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <mutex>
#include <regex>
#include <set>

namespace Blacklist {

namespace {
const char* kPath = "res/blacklist.txt";   // relative to the app folder (cwd)

struct Lists {
    std::set<std::string> all;
    std::map<std::string, std::set<std::string>> bySite;
};

std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n"), e = s.find_last_not_of(" \t\r\n");
    return b == std::string::npos ? "" : s.substr(b, e - b + 1);
}

const Lists& lists() {
    static Lists l;
    static std::once_flag once;
    std::call_once(once, [] {
        std::ifstream in(kPath);
        std::string line, section;
        while (std::getline(in, line)) {
            std::string t = trim(line);
            // Section headers look like "# Hexrom unsupported consoles".
            if (t.rfind("#", 0) == 0) {
                const std::string suffix = " unsupported consoles";
                std::string h = trim(t.substr(1));
                if (h.size() > suffix.size() && h.compare(h.size() - suffix.size(), suffix.size(), suffix) == 0)
                    section = h.substr(0, h.size() - suffix.size());
                continue;
            }
            t = trim(t.substr(0, t.find('#')));
            if (t.empty()) continue;
            std::string n = Normalize(t);
            l.all.insert(n);
            if (!section.empty()) l.bySite[section].insert(n);
        }
    });
    return l;
}
}  // namespace

std::string Normalize(const std::string& name) {
    std::string s = name;
    s = std::regex_replace(s, std::regex(R"(\s*\([^\)]*\))"), "");
    s = std::regex_replace(s, std::regex(R"(\s*\[[^\]]*\])"), "");
    s = std::regex_replace(s, std::regex(R"(ROMs?|ISO|&amp;|&|\s{2,})", std::regex_constants::icase), " ");
    s = std::regex_replace(s, std::regex(R"(^\s+|\s+$)"), "");
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    s = std::regex_replace(s, std::regex(R"(\s{2,})"), " ");
    return s;
}

bool Contains(const std::string& site, const std::string& consoleLabel, bool wholeFile) {
    const Lists& l = lists();
    std::string n = Normalize(consoleLabel);
    if (wholeFile) return l.all.count(n) > 0;
    auto it = l.bySite.find(site);
    return it != l.bySite.end() && it->second.count(n) > 0;
}

}  // namespace Blacklist
