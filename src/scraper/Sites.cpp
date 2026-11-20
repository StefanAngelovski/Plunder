#include "Sites.h"

#include "Hexrom/include/HexromScraper.h"
#include "Romspedia/include/RomspediaScraper.h"

namespace Sites {

std::vector<std::string> Ids() { return {"Hexrom", "Romspedia"}; }

std::shared_ptr<DefinedScraper> Create(const std::string& id) {
    if (id == "Hexrom") return std::make_shared<HexromScraper>();
    if (id == "Romspedia") return std::make_shared<RomspediaScraper>();
    return nullptr;
}

}  // namespace Sites
