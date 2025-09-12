#pragma once
#include <string>
#include "../../../utils/include/HttpUtils.h"

// Extracts the direct download link from a Gamulator /download/ page
std::string ExtractGamulatorDirectDownloadLink(const std::string& downloadPageUrl);
