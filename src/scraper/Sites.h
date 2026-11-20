#pragma once
#include <memory>
#include <string>
#include <vector>

#include "engine/DefinedScraper.h"

// Every supported ROM site, by id ("Hexrom", "Romspedia").
namespace Sites {
    std::vector<std::string> Ids();
    // A scraper for the site, or nullptr for an unknown id.
    std::shared_ptr<DefinedScraper> Create(const std::string& id);
}
