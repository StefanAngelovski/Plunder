#pragma once
#include <string>
#include <regex>
#include <iostream>
#include "../../../utils/include/HttpUtils.h"

// Function to scrape the latest release notes from a GitHub releases page
// Returns a formatted string with the latest release title and notes, or an error message on failure.
std::string ScrapeLatestGitHubReleaseNotes(const std::string& releasesPageUrl);
