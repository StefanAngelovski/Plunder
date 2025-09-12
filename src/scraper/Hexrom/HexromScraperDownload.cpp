#include "include/HexromScraperDownload.h"

// Extracts the direct .zip download link from a Hexrom game details page.
// NOTE: Real direct download button lives on the /download/ subpage of the game URL.
std::string ExtractHexromDirectDownloadLink(const std::string& detailsPageUrl) {
    // Build the download page URL (append download/ if not already there)
    std::string downloadPageUrl = detailsPageUrl;
    // Normalize (ensure trailing slash before possible download/ append)
    if (!downloadPageUrl.empty() && downloadPageUrl.back() != '/') downloadPageUrl += '/';
    // Append download/ if the url doesn't already end with it
    if (downloadPageUrl.rfind("/download/", downloadPageUrl.size() - 10) == std::string::npos) {
        downloadPageUrl += "download/";
        printf("[HexromDownload] Appended /download/ -> %s\n", downloadPageUrl.c_str());
    } else {
        printf("[HexromDownload] URL already points to download page: %s\n", downloadPageUrl.c_str());
    }

    printf("[HexromDownload] Fetching download page: %s\n", downloadPageUrl.c_str());
    std::string html = HttpUtils::fetchWebContent(downloadPageUrl);
    if (html.empty()) {
        printf("[HexromDownload] Empty HTML for: %s\n", downloadPageUrl.c_str());
        return "";
    }
    // Look for <a href="...zip" ...>
    std::regex zipRe(R"(<a[^>]+href=["']([^"']+\.zip)["'])", std::regex::icase);
    std::smatch match;
    if (std::regex_search(html, match, zipRe) && match.size() > 1) {
    std::string link = match[1].str();
    printf("[HexromDownload] Direct zip anchor match: %s\n", link.c_str());
    return link;
    }
    // Fallback: look for any .zip link
    std::regex urlZipRe(R"(https?://[^"']+\.zip)", std::regex::icase);
    if (std::regex_search(html, match, urlZipRe) && match.size() > 0) {
    std::string link = match[0].str();
    printf("[HexromDownload] Fallback absolute zip match: %s\n", link.c_str());
    return link;
    }
    // Legacy logic: find the first <a href=...> after the details table (for older Hexrom pages)
    std::regex tableRegex(R"(<table[\s\S]*?<\/table>)", std::regex::icase);
    std::smatch tableMatch;
    if (std::regex_search(html, tableMatch, tableRegex) && tableMatch.size() > 0) {
        size_t tableEnd = tableMatch.position(0) + tableMatch.length(0);
        std::string afterTable = html.substr(tableEnd);
        std::regex aAfterRegex(R"(<a[^>]+href=["']([^"']+\.zip)["'][^>]*>)", std::regex::icase);
        std::smatch aAfterMatch;
        if (std::regex_search(afterTable, aAfterMatch, aAfterRegex) && aAfterMatch.size() > 1) {
        std::string link = aAfterMatch[1].str();
        printf("[HexromDownload] Legacy after-table zip match: %s\n", link.c_str());
        return link;
        }
    }
    printf("[HexromDownload] No zip link found on page: %s\n", downloadPageUrl.c_str());
    return "";
}
