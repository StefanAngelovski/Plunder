#pragma once
#include <string>
#include <regex>
#include "../../../utils/include/HttpUtils.h"

// Extracts the direct .zip download link from a Hexrom game details page
std::string ExtractHexromDirectDownloadLink(const std::string& detailsPageUrl);
