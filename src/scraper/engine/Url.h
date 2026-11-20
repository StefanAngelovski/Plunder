#pragma once
#include <string>

// URL helpers shared by all scrapers.
namespace Url {
    // Resolve a link found on a page: absolute URLs stay as they are,
    // "//host/x" gets https, "/x" and "x" are resolved against `base`
    // (scheme + host, e.g. "https://hexrom.com").
    std::string Absolute(const std::string& url, const std::string& base);

    // An href/src/srcset attribute value as one URL: for srcset the first
    // candidate's URL; otherwise tabs/newlines removed and trimmed.
    std::string FromAttribute(const std::string& value, bool isSrcset);

    // Form-encode a search term for a query string ("zelda minish" -> "zelda+minish").
    std::string EncodeQuery(const std::string& text);

    // Percent-encode characters that aren't valid in a URL (spaces, quotes,
    // non-ASCII...) while keeping reserved characters and existing %XX escapes.
    // Romspedia serves download links with raw spaces, which curl rejects.
    std::string EncodeUnsafe(const std::string& url);
}
