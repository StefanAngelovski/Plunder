#include "include/StringUtils.h"

namespace StringUtils {

    std::vector<std::string> splitUtf8ToCodepoints(const std::string& s) {
        std::vector<std::string> out;
        out.reserve(s.size()); // worst-case all ASCII
        for (size_t i = 0; i < s.size();) {
            unsigned char c = static_cast<unsigned char>(s[i]);
            size_t len = 1;
            if ((c & 0x80) == 0x00) len = 1;
            else if ((c & 0xE0) == 0xC0) len = 2;
            else if ((c & 0xF0) == 0xE0) len = 3;
            else if ((c & 0xF8) == 0xF0) len = 4;
            else { // invalid start
                out.emplace_back("\xEF\xBF\xBD");
                ++i;
                continue;
            }
            if (i + len > s.size()) { // truncated sequence
                out.emplace_back("\xEF\xBF\xBD");
                break;
            }
            bool valid = true;
            for (size_t k = 1; k < len; ++k) {
                unsigned char cc = static_cast<unsigned char>(s[i + k]);
                if ((cc & 0xC0) != 0x80) { valid = false; break; }
            }
            if (!valid) {
                out.emplace_back("\xEF\xBF\xBD");
                ++i;
                continue;
            }
            out.emplace_back(s.substr(i, len));
            i += len;
        }
        return out;
    }

    std::string cleanHtmlText(const std::string& input) {
        std::string result = input;
        auto start = result.find_first_not_of(" \t\n\r\f\v");
        if (start != std::string::npos) result = result.substr(start); else return "";
        auto end = result.find_last_not_of(" \t\n\r\f\v");
        if (end != std::string::npos) result = result.substr(0, end + 1);
        std::vector<std::pair<std::string, std::string>> replacements = {
            {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&nbsp;", " "}, {"\r", ""} // removed {"\n", " "}
        };
        for (const auto& rep : replacements) {
            size_t pos = 0;
            while ((pos = result.find(rep.first, pos)) != std::string::npos) {
                result.replace(pos, rep.first.length(), rep.second);
                pos += rep.second.length();
            }
        }
        // Decode numeric HTML entities (e.g., &#8211;)
        std::regex numEntity(R"(&#(\d+);)");
        std::smatch m;
        while (std::regex_search(result, m, numEntity)) {
            if (m.size() > 1) {
                int code = std::stoi(m[1].str());
                std::string utf8;
                if (code < 0x80) utf8 += static_cast<char>(code);
                else if (code < 0x800) {
                    utf8 += static_cast<char>(0xC0 | (code >> 6));
                    utf8 += static_cast<char>(0x80 | (code & 0x3F));
                } else if (code < 0x10000) {
                    utf8 += static_cast<char>(0xE0 | (code >> 12));
                    utf8 += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                    utf8 += static_cast<char>(0x80 | (code & 0x3F));
                } else {
                    utf8 += static_cast<char>(0xF0 | (code >> 18));
                    utf8 += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
                    utf8 += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                    utf8 += static_cast<char>(0x80 | (code & 0x3F));
                }
                result.replace(m.position(0), m.length(0), utf8);
            } else {
                break;
            }
        }
        // Decode hexadecimal HTML entities (e.g., &#x27;)
        std::regex hexEntity(R"(&#x([0-9A-Fa-f]+);)");
        while (std::regex_search(result, m, hexEntity)) {
            if (m.size() > 1) {
                int code = std::stoi(m[1].str(), nullptr, 16);
                std::string utf8;
                if (code < 0x80) utf8 += static_cast<char>(code);
                else if (code < 0x800) {
                    utf8 += static_cast<char>(0xC0 | (code >> 6));
                    utf8 += static_cast<char>(0x80 | (code & 0x3F));
                } else if (code < 0x10000) {
                    utf8 += static_cast<char>(0xE0 | (code >> 12));
                    utf8 += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                    utf8 += static_cast<char>(0x80 | (code & 0x3F));
                } else {
                    utf8 += static_cast<char>(0xF0 | (code >> 18));
                    utf8 += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
                    utf8 += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                    utf8 += static_cast<char>(0x80 | (code & 0x3F));
                }
                result.replace(m.position(0), m.length(0), utf8);
            } else {
                break;
            }
        }
        // Remove non-printable except newlines
        result.erase(std::remove_if(result.begin(), result.end(), [](unsigned char c) { return c != '\n' && !std::isprint(c); }), result.end());
        return result;
    }
}