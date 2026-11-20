#include "include/GitHubReleasesScraper.h"

#include "../engine/Html.h"
#include "../../utils/include/HttpUtils.h"

// Each release's notes are a rendered Markdown block:
//   <div class="markdown-body …"><h2>…</h2><p>…</p><ul><li>…</li></ul></div>
std::string ParseGitHubReleaseNotes(const std::string& html) {
    Html page(html);
    std::string notes;
    for (Html::Node release : page.all("div.markdown-body")) {
        std::string block;
        for (Html::Node el : page.all("h1, h2, h3, h4, h5, h6, p, li", release)) {
            std::string tag = Html::tag(el), text = Html::text(el);
            if (text.empty()) continue;
            if (tag == "p" && Html::tag(Html::parent(el)) == "li") continue;  // already part of its list item
            if (tag.size() == 2 && tag[0] == 'h') block += "\n[H" + tag.substr(1) + "]" + text + "[/H]\n";
            else if (tag == "li") block += "- " + text + "\n";
            else block += text + "\n";
        }
        if (block.empty()) continue;
        if (!notes.empty()) notes += "\n---\n";
        notes += block;
    }
    return notes.empty() ? "No release notes found." : notes;
}

std::string ScrapeLatestGitHubReleaseNotes(const std::string& releasesPageUrl) {
    std::string html = HttpUtils::fetchWebContent(releasesPageUrl);
    if (html.empty()) return "Failed to fetch release notes.";
    return ParseGitHubReleaseNotes(html);
}
