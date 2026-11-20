#pragma once
#include <string>
#include <vector>

#include "Catalog.h"

// Fallback for global search: asks every ROM site's own search at the same
// time and combines the answers. Slower than the catalog (a request per site)
// but always current, e.g. for games added since the last catalog crawl.
namespace LiveSearch {
    // Blocking: call from a background thread.
    std::vector<Catalog::GlobalResult> SearchAllSites(const std::string& query);
}
