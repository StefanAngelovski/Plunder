#pragma once
#include <string>
#include <utility>
#include <vector>

#include "../SiteScraper.h"
#include "Html.h"
#include "SiteDefinition.h"

// A scraper driven entirely by a SiteDefinition.
//
// fetch* methods download pages (HttpUtils); parse* methods only read HTML,
// so tests can run them on saved pages.
class DefinedScraper : public SiteScraper {
public:
    explicit DefinedScraper(SiteDefinition definition);

    // SiteScraper
    std::string getName() const override { return def_.id; }
    std::vector<ListItem> fetchConsoles() override;
    std::pair<std::vector<ListItem>, PaginationInfo> fetchGames(const std::string& url, int page = 1) override;
    GameDetails fetchGameDetails(const std::string& gameUrl) override;

    // A console's games matching a search and filter choices (see FilterRules).
    std::pair<std::vector<ListItem>, PaginationInfo> filterGames(const std::string& consoleUrl, const std::string& search,
                                                                 const std::vector<int>& choices, int page = 1);

    // The site's own search (first page). ListItem::console holds each game's
    // console label when the site shows it.
    std::vector<ListItem> search(const std::string& query);
    std::string fetchDownloadLink(const std::string& gameUrl);

    // Parsing only
    std::vector<ListItem> parseConsoles(const std::string& html) const;
    // Games on a list/search page and the highest page number linked from it.
    std::pair<std::vector<ListItem>, int> parseCards(const std::string& html, const CardRules& rules) const;
    GameDetails parseDetails(const std::string& html) const;
    std::string parseDownloadLink(const std::string& html) const;

    const SiteDefinition& definition() const { return def_; }

private:
    std::string pick(const Html& page, Html::Node scope, const Picks& picks) const;
    std::string pickUrl(const Html& page, Html::Node scope, const Picks& picks) const;

    SiteDefinition def_;
};
