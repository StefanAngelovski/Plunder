#include "include/GamulatorScraperDownload.h"
#include <regex>

// Extracts the direct download link (.zip, .7z, .rar) from a Gamulator /download/ page
std::string ExtractGamulatorDirectDownloadLink(const std::string& downloadPageUrl) {
    std::string html = HttpUtils::fetchWebContent(downloadPageUrl);
    if (html.empty()) {
    printf("[GamulatorDownload] Empty HTML for: %s\n", downloadPageUrl.c_str());
        return "";
    }
    printf("[GamulatorDownload] Fetching download page: %s\n", downloadPageUrl.c_str());

    // Look for <a class="download_link" href="...zip/7z/rar" ...>
    std::regex linkRe(R"(<a[^>]+class=["']download_link["'][^>]+href=["']([^"']+\.(zip|7z|rar))["'])", std::regex::icase);
    std::smatch match;
    if (std::regex_search(html, match, linkRe) && match.size() > 1) {
    printf("[GamulatorDownload] Found direct download_link anchor: %s\n", match[1].str().c_str());
        return match[1].str();
    }
    // Fallback: look for any .zip/.7z/.rar link
    std::regex archiveRe(R"(https?://[^"]+\.(zip|7z|rar))", std::regex::icase);
    if (std::regex_search(html, match, archiveRe) && match.size() > 0) {
    printf("[GamulatorDownload] Fallback archive match: %s\n", match[0].str().c_str());
        return match[0].str();
    }
    printf("[GamulatorDownload] No archive link found: %s\n", downloadPageUrl.c_str());
    return "";
}