#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <regex>

namespace StringUtils {
    // Clean HTML-ish text (decl; impl in .cpp)
    std::string cleanHtmlText(const std::string& input);

    // Split UTF-8 string into code point substrings (invalid bytes -> U+FFFD)
    std::vector<std::string> splitUtf8ToCodepoints(const std::string& str);

    // Join with pre-reserve optimization
    inline std::string join(const std::vector<std::string>& parts, const std::string& delim) {
        if (parts.empty()) return {};
        size_t total = (parts.size() - 1) * delim.size();
        for (auto &p : parts) total += p.size();
        std::string out;
        out.reserve(total);
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i) out.append(delim);
            out.append(parts[i]);
        }
        return out;
    }
}

// Backward compatibility: previously splitUtf8ToCodepoints was at global scope
inline std::vector<std::string> splitUtf8ToCodepoints(const std::string& s) {
    return StringUtils::splitUtf8ToCodepoints(s);
}