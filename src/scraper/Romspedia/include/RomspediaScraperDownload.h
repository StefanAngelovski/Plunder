#pragma once
#include <string>
#include <regex>
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>
#include "../../../app/consolePolicies/ConsoleFolderMap.h"
#include "../../../app/consolePolicies/ConsoleZipPolicy.h"
#include "../../../utils/include/HttpUtils.h"
class RomspediaScraperDownload {
public:
    RomspediaScraperDownload();
    void download(const std::string& url);
};
// Add C-style API for direct download link extraction
std::string ExtractRomspediaDirectDownloadLink(const std::string& detailsPageUrl);
bool downloadAndExtractRomspediaZip(const std::string& downloadUrl, const std::string& consoleName, const std::string& romName);
