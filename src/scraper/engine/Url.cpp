#include "Url.h"

#include <cctype>
#include <cstdio>

namespace Url {

std::string Absolute(const std::string& url, const std::string& base) {
    if (url.empty() || url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0) return url;
    if (url.rfind("//", 0) == 0) return "https:" + url;
    return base + (url[0] == '/' ? "" : "/") + url;
}

std::string FromAttribute(const std::string& value, bool isSrcset) {
    std::string v = value;
    if (isSrcset) {
        // "url1 340w, url2 2x": the first candidate's URL is everything before
        // its descriptor (candidates are separated by commas).
        v = v.substr(0, v.find(','));
        size_t start = v.find_first_not_of(" \t\r\n");
        v = start == std::string::npos ? "" : v.substr(start);
        size_t end = v.find_first_of(" \t\r\n");
        if (end != std::string::npos) v = v.substr(0, end);
        return v;
    }
    // href/src: drop tabs/newlines (Romspedia pads some URLs with tabs) and
    // surrounding spaces; inner spaces are real and get percent-encoded later.
    std::string out;
    for (char c : v) if (c != '\t' && c != '\r' && c != '\n') out += c;
    size_t b2 = out.find_first_not_of(' '), e2 = out.find_last_not_of(' ');
    return b2 == std::string::npos ? "" : out.substr(b2, e2 - b2 + 1);
}

std::string EncodeQuery(const std::string& text) {
    std::string out;
    char buf[4];
    for (unsigned char c : text) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += (char)c;
        else if (c == ' ') out += '+';
        else { std::snprintf(buf, sizeof buf, "%%%02X", c); out += buf; }
    }
    return out;
}

std::string EncodeUnsafe(const std::string& url) {
    static const char* hex = "0123456789ABCDEF";
    static const std::string safe = "-._~:/?#[]@!$&'()*+,;=%";
    std::string out;
    out.reserve(url.size());
    for (unsigned char c : url) {
        if (std::isalnum(c) || safe.find((char)c) != std::string::npos) {
            out += (char)c;
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0x0F];
        }
    }
    return out;
}

}  // namespace Url
