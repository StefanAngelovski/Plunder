#pragma once
#include <string>

// Release notes from a GitHub releases page, newest first, as text for the
// Patch Notes screen: "[H2]Heading[/H]" headings, "- item" list items, plain
// paragraphs, and a "---" line between releases. Returns a message on failure.
std::string ScrapeLatestGitHubReleaseNotes(const std::string& releasesPageUrl);

// Same, from an already-downloaded page (used by tests).
std::string ParseGitHubReleaseNotes(const std::string& html);
