#include "include/RomspediaScraperDownload.h"

RomspediaScraperDownload::RomspediaScraperDownload() {}
void RomspediaScraperDownload::download(const std::string&) {}

// Extracts the direct .zip download link from a Romspedia /download/ page
std::string ExtractRomspediaDirectDownloadLink(const std::string& detailsPageUrl) {
    // Build the download page URL (append /download if not already there)
    std::string downloadPageUrl = detailsPageUrl;
    if (!downloadPageUrl.empty() && downloadPageUrl.back() != '/') downloadPageUrl += '/';
    if (downloadPageUrl.rfind("/download", downloadPageUrl.size() - 9) == std::string::npos) {
        downloadPageUrl += "download";
        printf("[RomspediaDownload] Appended /download -> %s\n", downloadPageUrl.c_str());
    } else {
        printf("[RomspediaDownload] URL already points to download page: %s\n", downloadPageUrl.c_str());
    }

    printf("[RomspediaDownload] Fetching download page: %s\n", downloadPageUrl.c_str());
    std::string html = HttpUtils::fetchWebContent(downloadPageUrl);
    if (html.empty()) {
        printf("[RomspediaDownload] Empty HTML for: %s\n", downloadPageUrl.c_str());
        return "";
    }
    // Look for <a href=\"...zip\" ...>
    std::regex zipRe(R"(<a[^>]+href=[\"']([^\"']+\.zip)[\"'])", std::regex::icase);
    std::smatch match;
    if (std::regex_search(html, match, zipRe) && match.size() > 1) {
        std::string link = match[1].str();
        printf("[RomspediaDownload] Direct zip anchor match: %s\n", link.c_str());
        return link;
    }
    // Fallback: look for any .zip link
    std::regex urlZipRe(R"(https?://[^\"']+\.zip)", std::regex::icase);
    if (std::regex_search(html, match, urlZipRe) && match.size() > 0) {
        std::string link = match[0].str();
        printf("[RomspediaDownload] Fallback absolute zip match: %s\n", link.c_str());
        return link;
    }
    printf("[RomspediaDownload] No zip link found on page: %s\n", downloadPageUrl.c_str());
    return "";
}

// Downloads the .zip and extracts if needed, matching Hexrom/Gamulator logic
bool downloadAndExtractRomspediaZip(const std::string& downloadUrl, const std::string& consoleName, const std::string& romName) {
    std::string folder = getFolderForScrapedConsole(consoleName);
    if (folder.empty()) {
        printf("[RomspediaDownload] Could not map console '%s' to folder\n", consoleName.c_str());
        return false;
    }
    std::string basePath = "build/Plunder/roms/" + folder;
    // Ensure folder exists
    mkdir(basePath.c_str(), 0755);
    std::string zipPath = basePath + "/" + romName + ".zip";
    printf("[RomspediaDownload] Downloading to: %s\n", zipPath.c_str());
    if (!HttpUtils::downloadFile(downloadUrl, zipPath)) {
        printf("[RomspediaDownload] Download failed: %s\n", downloadUrl.c_str());
        return false;
    }
    if (shouldUnzipForFolder(folder)) {
        // Unzip and remove .zip
        std::string unzipCmd = "unzip -o '" + zipPath + "' -d '" + basePath + "'";
        printf("[RomspediaDownload] Unzipping: %s\n", unzipCmd.c_str());
        int ret = system(unzipCmd.c_str());
        if (ret != 0) {
            printf("[RomspediaDownload] Unzip failed\n");
            return false;
        }
        unlink(zipPath.c_str());
    }
    return true;
}
