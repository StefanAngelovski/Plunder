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
    // Look for <a href=\"...zip|rar\" ...>
    std::regex archiveRe(R"(<a[^>]+href=[\"']([^\"']+\.(zip|rar))[\"'])", std::regex::icase);
    std::smatch match;
    if (std::regex_search(html, match, archiveRe) && match.size() > 1) {
        std::string link = match[1].str();
        printf("[RomspediaDownload] Direct archive anchor match: %s\n", link.c_str());
        return link;
    }
    // Fallback: look for any .zip or .rar link
    std::regex urlArchiveRe(R"(https?://[^\"']+\.(zip|rar))", std::regex::icase);
    if (std::regex_search(html, match, urlArchiveRe) && match.size() > 0) {
        std::string link = match[0].str();
        printf("[RomspediaDownload] Fallback absolute archive match: %s\n", link.c_str());
        return link;
    }
    printf("[RomspediaDownload] No zip/rar link found on page: %s\n", downloadPageUrl.c_str());
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

    // Detect file extension from downloadUrl
    std::string ext = ".zip";
    size_t dotPos = downloadUrl.find_last_of('.');
    if (dotPos != std::string::npos) {
        std::string urlExt = downloadUrl.substr(dotPos);
        if (urlExt == ".rar" || urlExt == ".zip") ext = urlExt;
    }
    std::string archivePath = basePath + "/" + romName + ext;
    printf("[RomspediaDownload] Downloading to: %s\n", archivePath.c_str());
    if (!HttpUtils::downloadFile(downloadUrl, archivePath)) {
        printf("[RomspediaDownload] Download failed: %s\n", downloadUrl.c_str());
        return false;
    }
    if (shouldUnzipForFolder(folder)) {
        int ret = -1;
        if (ext == ".zip") {
            std::string unzipCmd = "unzip -o '" + archivePath + "' -d '" + basePath + "'";
            printf("[RomspediaDownload] Unzipping: %s\n", unzipCmd.c_str());
            ret = system(unzipCmd.c_str());
        } else if (ext == ".rar") {
            // Try unrar first, fallback to 7z if not available
            std::string unrarCmd = "unrar x -o+ '" + archivePath + "' '" + basePath + "'";
            printf("[RomspediaDownload] Unraring: %s\n", unrarCmd.c_str());
            ret = system(unrarCmd.c_str());
            if (ret != 0) {
                std::string sevenzCmd = "7z x -y -o'" + basePath + "' '" + archivePath + "'";
                printf("[RomspediaDownload] 7z fallback: %s\n", sevenzCmd.c_str());
                ret = system(sevenzCmd.c_str());
            }
        }
        if (ret != 0) {
            printf("[RomspediaDownload] Extraction failed\n");
            return false;
        }
        unlink(archivePath.c_str());
    }
    return true;
}
