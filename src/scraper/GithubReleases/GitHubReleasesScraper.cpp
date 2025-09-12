#include "include/GitHubReleasesScraper.h"

// Scrapes the latest release notes from a GitHub releases page
// Returns a formatted string with the latest release title and notes, or an error message on failure.
std::string ScrapeLatestGitHubReleaseNotes(const std::string& releasesPageUrl) {
    try {
        std::string html = HttpUtils::fetchWebContent(releasesPageUrl);
        std::cerr << "[GitHubReleasesScraper] HTML size: " << html.size() << std::endl;
        if (html.empty()) {
            std::cerr << "[GitHubReleasesScraper] HTML fetch failed." << std::endl;
            return "Failed to fetch release notes.";
        }

            // Use string search to find all <div class="markdown-body my-3"> blocks, handling nested divs
            std::string allBodies;
            size_t mdCount = 0;
            const std::string divTag = "<div";
            const std::string classStr = "class=\"markdown-body my-3\"";
            size_t pos = 0;
            const size_t maxBlocks = 20;
            while (mdCount < maxBlocks) {
                size_t divStart = html.find(divTag, pos);
                if (divStart == std::string::npos) break;
                size_t classPos = html.find(classStr, divStart);
                if (classPos == std::string::npos || classPos - divStart > 200) {
                    pos = divStart + divTag.size();
                    continue;
                }
                size_t tagEnd = html.find('>', classPos);
                if (tagEnd == std::string::npos) break;
                size_t contentStart = tagEnd + 1;
                // Find the matching closing </div> (handle nested divs)
                size_t searchPos = contentStart;
                int depth = 1;
                while (depth > 0) {
                    size_t nextOpen = html.find("<div", searchPos);
                    size_t nextClose = html.find("</div>", searchPos);
                    if (nextClose == std::string::npos) break;
                    if (nextOpen != std::string::npos && nextOpen < nextClose) {
                        depth++;
                        searchPos = nextOpen + 4;
                    } else {
                        depth--;
                        searchPos = nextClose + 6;
                    }
                }
                size_t divEnd = searchPos;
                if (depth != 0) break; // Unbalanced divs
                std::string body = html.substr(contentStart, (divEnd - 6) - contentStart);
                if (!allBodies.empty()) allBodies += "\n\n---\n\n";
                allBodies += body;
                ++mdCount;
                pos = divEnd;
            }
            std::cerr << "[GitHubReleasesScraper] markdown-body count: " << mdCount << ", total body size: " << allBodies.size() << std::endl;
            if (allBodies.empty()) {
                std::cerr << "[GitHubReleasesScraper] No markdown-body divs found in HTML." << std::endl;
                return "No release notes found.";
            }

        // Preprocess for better list and heading formatting
        std::string formatted = allBodies;
        // 1. Mark <h1>-<h6> for bold/larger rendering in the UI (replace with tokens)
        for (int h = 1; h <= 6; ++h) {
            std::string open = "<h" + std::to_string(h) + ">";
            std::string close = "</h" + std::to_string(h) + ">";
            size_t idx = 0;
            while ((idx = formatted.find(open, idx)) != std::string::npos) {
                formatted.replace(idx, open.size(), "\n[H" + std::to_string(h) + "]");
            }
            idx = 0;
            while ((idx = formatted.find(close, idx)) != std::string::npos) {
                formatted.replace(idx, close.size(), "[/H]");
            }
        }
        // 2. Replace <li> with a single newline and dash (no extra blank lines)
        size_t idx = 0;
        while ((idx = formatted.find("<li>", idx)) != std::string::npos) {
            formatted.replace(idx, 4, "\n- ");
            idx += 3; // skip past the dash
        }
        // Remove </li>
        while ((idx = formatted.find("</li>")) != std::string::npos) {
            formatted.replace(idx, 5, "");
        }
        // 3. Replace <ul>, <ol> with a single newline (no extra blank lines)
        while ((idx = formatted.find("<ul>")) != std::string::npos) {
            formatted.replace(idx, 4, "\n");
        }
        while ((idx = formatted.find("<ol>")) != std::string::npos) {
            formatted.replace(idx, 4, "\n");
        }
        // Remove </ul>, </ol>
        while ((idx = formatted.find("</ul>")) != std::string::npos) {
            formatted.replace(idx, 5, "");
        }
        while ((idx = formatted.find("</ol>")) != std::string::npos) {
            formatted.replace(idx, 5, "");
        }
        // 4. Remove excess blank lines (collapse 3+ newlines to 2)
        while ((idx = formatted.find("\n\n\n")) != std::string::npos) {
            formatted.replace(idx, 3, "\n\n");
        }
        // 5. Decode common HTML entities
        auto decode_html_entities = [](std::string s) -> std::string {
            struct Entity { const char* code; const char* val; };
            static const Entity entities[] = {
                {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&#39;", "'"}, {"&apos;", "'"}, {nullptr, nullptr}
            };
            for (const Entity* e = entities; e->code; ++e) {
                size_t pos = 0;
                while ((pos = s.find(e->code, pos)) != std::string::npos) {
                    s.replace(pos, strlen(e->code), e->val);
                    pos += strlen(e->val);
                }
            }
            return s;
        };
        // 6. Remove HTML tags for plain text display
        auto strip_html = [](const std::string& input) -> std::string {
            return std::regex_replace(input, std::regex("<[^>]+>"), "");
        };
        std::string plain = strip_html(decode_html_entities(formatted));

        if (plain.empty() || plain.find_first_not_of(" \n\r\t") == std::string::npos) {
            std::cerr << "[GitHubReleasesScraper] All markdown-body blocks are empty after stripping HTML." << std::endl;
            return "No release notes found.";
        }

        return plain;
    } catch (const std::exception& ex) {
        std::cerr << "[GitHubReleasesScraper] Exception: " << ex.what() << std::endl;
        return "Error parsing release notes.";
    } catch (...) {
        std::cerr << "[GitHubReleasesScraper] Unknown exception." << std::endl;
        return "Error parsing release notes.";
    }
}
