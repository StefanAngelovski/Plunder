// ListScreen.cpp
// Responsibilities:
//  - Display a paginated grid of game items (with smooth scrolling & lazy image loading)
//  - Handle controller input for navigation, pagination, and filtering
//  - Asynchronous image downloading & texture creation
//

#include "include/ListScreen.h"
#include <set>
#include <sys/stat.h>  
#include "../../utils/include/Theme.h"
#include "../../utils/include/ImageLoader.h"
#include "../../catalog/include/Catalog.h"
#include "../../scraper/Romspedia/include/RomspediaScraperFilter.h"
#include "../../scraper/Romspedia/include/RomspediaScraper.h"


// === Helpers (anonymous namespace) ============================================================
namespace {

// Extract file extension from URL, defaulting to .jpg if unknown / unsupported.
std::string getFileExtension(const std::string& url) {
    size_t lastDot = url.find_last_of('.');
    if (lastDot == std::string::npos) return ".jpg";
    size_t lastSlash = url.find_last_of('/');
    if (lastSlash != std::string::npos && lastDot < lastSlash) return ".jpg";
    std::string ext = url.substr(lastDot);
    // Only allow common image extensions
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".webp" || ext == ".bmp" || ext == ".gif")
        return ext;
    return ".jpg";
}

} // namespace
// ==============================================================================================

// --- Constructor ----------------------------------------------------------------------------
ListScreen::ListScreen(const std::string& title, const std::string& romSite, const std::vector<ListItem>& initialItems, SDL_Renderer* renderer,
           std::function<void(const ListItem&)> onSelect,
           std::function<void(int)> pageChangeCallback,
           const PaginationInfo& paginationInfo)
    : title(title), items(initialItems), onItemSelected1(onSelect), onPageChange(pageChangeCallback), pagination(paginationInfo), romSite(romSite)
{
    catalogConsoleUrl = paginationInfo.baseUrl;
    // Determine site type early
    if (romSite == "Hexrom") siteType = SiteType::Hexrom;
    else if (romSite == "Romspedia") siteType = SiteType::Romspedia;
    else if (romSite == "Global") siteType = SiteType::Global;
    else siteType = SiteType::Hexrom; // default fallback
    // Site-specific modal setup
    if (siteType == SiteType::Global) {
        global = std::make_unique<GlobalSearchController>([this](const std::vector<ListItem>& results) {
            applyNewResult(results, PaginationInfo(), true);
        });
    } else if (siteType == SiteType::Romspedia) setupRomspediaModal();
    else setupHexromModal();
    textures.resize(items.size(), nullptr);
    mkdir("cache", 0755);
    mkdir("cache/images", 0755);
    queueInitialDownloads();
    startImageLoaderThreads(renderer);
    moveSound = Mix_LoadWAV("sounds/move.wav");
    if (moveSound) Mix_VolumeChunk(moveSound, MIX_MAX_VOLUME);
    Mix_Volume(-1, MIX_MAX_VOLUME);
    if (global) global->start();
}

ListScreen::~ListScreen() {
    if (moveSound) {
        Mix_FreeChunk(moveSound);
        moveSound = nullptr;
    }
    stopThreads = true;
    stopImageLoaderThreads();
    std::set<SDL_Texture*> owned;
    for (auto texture : textures) if (texture) owned.insert(texture);
    for (auto& kv : textureByUrl) if (kv.second) owned.insert(kv.second);
    for (auto texture : owned) SDL_DestroyTexture(texture);
    // unique_ptr modals auto-clean
}

// --- Site-specific modal setup helpers -----------------------------------------------------
void ListScreen::setupHexromModal() {
    hexromModal = std::make_unique<HexromFilterModal>();
    hexromModal->setOnFilter([this](const std::string& search, int region, int sort, int order, int genre){
        runSiteFilter(search, region, sort, order, genre);
    });
}

void ListScreen::setupRomspediaModal() {
    romspediaModal = std::make_unique<RomspediaFilterModal>();
    romspediaSearch.clear();
    romspediaFilterActive = false;
    romspediaOriginalConsoleUrl = pagination.baseUrl; // Store original console URL for filtering
    romspediaModal->setOnFilter([this](const std::string& search, int region, int sort, int order, int genre){
        runSiteFilter(search, region, sort, order, genre);
    });
}

void ListScreen::applyNewResult(const std::vector<ListItem>& newItems, const PaginationInfo& newPagination, bool replace) {
    if (replace) {
        items = newItems;
        pagination = newPagination;
        selectedIndex = 0;
        scrollOffset = 0;
        scrollOffsetAnim = 0.0f;
        textures.assign(items.size(), nullptr);
        for (size_t i = 0; i < items.size(); ++i) {
            auto it = textureByUrl.find(items[i].imagePath);
            if (it != textureByUrl.end()) textures[i] = it->second;
        }
        std::lock_guard<std::mutex> lock(downloadMutex);
        pendingDownloads.clear();   // indices refer to the old list
    } else {
        // When appending (pagination), check if we got empty results
        if (newItems.empty() && newPagination.currentPage > 1) {
            // No more results found - adjust totalPages to prevent further pagination attempts
            pagination.totalPages = pagination.currentPage - 1;
        } else {
            appendItems(newItems, newPagination);
        }
    }
}

void ListScreen::runSiteFilter(const std::string& search, int region, int sort, int order, int genre) {
    // Catalog: search locally (instant) when it covers this console and no
    // site-only filters (Hexrom region / sort / order / genre) are set.
    bool siteOnlyFilters = siteType == SiteType::Hexrom && (region || sort || order || genre);
    if (!siteOnlyFilters && !catalogConsoleUrl.empty()) {
        std::vector<ListItem> results;
        bool covered = search.empty() ? Catalog::Games(romSite, catalogConsoleUrl, results)
                                      : Catalog::Search(romSite, catalogConsoleUrl, search, results);
        if (covered) {
            PaginationInfo local;
            local.baseUrl = catalogConsoleUrl;
            applyNewResult(results, local, true);
            if (siteType == SiteType::Romspedia) { romspediaSearch = search; romspediaFilterActive = false; }
            if (hexromModal) hexromModal->showModal(false);
            if (romspediaModal) romspediaModal->showModal(false);
            return;
        }
    }
    if (siteType == SiteType::Romspedia) {
        romspediaSearch = search;
        // Always use filter for Romspedia if we're in a specific console (baseUrl contains /roms/console-name)
        // or if there's a search term
        bool isConsoleSpecific = pagination.baseUrl.find("/roms/") != std::string::npos && 
                                pagination.baseUrl.find("romspedia.com/roms") != std::string::npos &&
                                pagination.baseUrl != "https://www.romspedia.com/roms";
        romspediaFilterActive = !search.empty() || isConsoleSpecific;
        
        if (!search.empty() && isConsoleSpecific) {
            // Use console-specific filtering
            std::string searchUrl = "https://www.romspedia.com/search?search_term_string=";
            auto urlEncode = [](const std::string& value) -> std::string {
                std::ostringstream escaped;
                escaped.fill('0');
                escaped << std::hex;
                for (char c : value) {
                    if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
                        escaped << c;
                    } else if (c == ' ') {
                        escaped << '+';
                    } else {
                        escaped << '%' << std::uppercase << std::setw(2) << int((unsigned char)c) << std::nouppercase;
                    }
                }
                return escaped.str();
            };
            searchUrl += urlEncode(search);
            
            // Use the original console URL for filtering, not the current pagination.baseUrl
            std::string filterBaseUrl = romspediaOriginalConsoleUrl.empty() ? pagination.baseUrl : romspediaOriginalConsoleUrl;
            auto result = RomspediaScraperFilter::filterGames(searchUrl, filterBaseUrl, romspediaSearch, 1);
            applyNewResult(result.first, result.second, true);
        } else {
            // Use the original filter function for non-console specific searches
            auto result = RomspediaScraperFilter::filterGames(pagination.baseUrl, *romspediaModal, romspediaSearch);
            applyNewResult(result.first, result.second, true);
        }
        romspediaModal->showModal(false);
    } else {
        auto result = HexromScraperFilter::filterGames(pagination.baseUrl, *hexromModal, search, region, sort, order, genre);
        applyNewResult(result.first, result.second, true);
        hexromModal->showModal(false);
    }
}

void ListScreen::runSitePaginate(int page) {
    if (siteType == SiteType::Romspedia) {
        // Check if we have a search URL in pagination.baseUrl (this means we're in search mode)
        bool isSearchUrl = pagination.baseUrl.find("search?search_term_string=") != std::string::npos;
        if (romspediaFilterActive || isSearchUrl) {
            // Check if we're filtering within a specific console
            bool isConsoleSpecific = !romspediaOriginalConsoleUrl.empty() && 
                                   romspediaOriginalConsoleUrl.find("/roms/") != std::string::npos && 
                                   romspediaOriginalConsoleUrl.find("romspedia.com/roms") != std::string::npos &&
                                   romspediaOriginalConsoleUrl != "https://www.romspedia.com/roms";
            
            if (isConsoleSpecific && !romspediaSearch.empty()) {
                // Use console-specific filtering with the original console URL
                auto result = RomspediaScraperFilter::filterGames(pagination.baseUrl, romspediaOriginalConsoleUrl, romspediaSearch, page);
                applyNewResult(result.first, result.second, page == 1);
            } else {
                // Use regular search filtering
                auto result = RomspediaScraperFilter::filterGames(pagination.baseUrl, *romspediaModal, romspediaSearch, page);
                applyNewResult(result.first, result.second, page == 1);
            }
        } else {
            RomspediaScraper s; auto result = s.fetchGames(pagination.baseUrl, page);
            applyNewResult(result.first, result.second, page == 1);
        }
    } else { // Hexrom standard pagination (no custom backward implemented yet)
        HexromScraper s; auto result = s.fetchGames(pagination.baseUrl, page);
        applyNewResult(result.first, result.second, page == 1);
    }
    loadingMore = false;
}

// Queue up images for visible and just-offscreen items (preload extra rows).
void ListScreen::queueInitialDownloads() {
    int preloadRows = VISIBLE_ROWS + 2; // Preload 2 extra rows
    for (int row = 0; row < preloadRows; row++) {
        for (int col = 0; col < GRID_COLUMNS; col++) {
            int index = getIndexFromGrid(row, col, GRID_COLUMNS);
            if (index >= 0 && index < items.size() && !items[index].imagePath.empty() && items[index].imagePath.find("http") == 0) {
                std::lock_guard<std::mutex> lock(downloadMutex);
                pendingDownloads[index] = items[index].imagePath;
            }
        }
    }
}

// Spawn a small pool of threads to download images in the background.
void ListScreen::startImageLoaderThreads(SDL_Renderer* renderer) {
    int numThreads = 8; // Tuned for faster icon loading
    for (int i = 0; i < numThreads; ++i) {
        loaderThreads.emplace_back([this]() {
            while (!stopThreads) {
                int idx = -1;
                std::string url;
                {
                    std::lock_guard<std::mutex> lock(downloadMutex);
                    if (!pendingDownloads.empty()) {
                        auto it = pendingDownloads.begin();
                        idx = it->first;
                        url = it->second;
                        pendingDownloads.erase(it);
                    }
                }
                if (idx >= 0 && !url.empty()) {
                    std::string ext = getFileExtension(url);
                    std::string filename = "cache/images/" + std::to_string(std::hash<std::string>{}(url)) + ext;
                    struct stat st;
                    bool cached = stat(filename.c_str(), &st) == 0 && st.st_size > 0;
                    if (cached || HttpUtils::downloadImage(url, filename)) {
                        std::lock_guard<std::mutex> lock(downloadMutex);
                        readyImages.push({idx, filename, url});
                    }
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
        });
    }
}

void ListScreen::stopImageLoaderThreads() {
    for (auto& t : loaderThreads) {
        if (t.joinable()) t.join();
    }
    loaderThreads.clear();
}

// Consume finished downloads and convert them to SDL_Textures.
void ListScreen::updateTextures(SDL_Renderer* renderer) {
    std::lock_guard<std::mutex> lock(downloadMutex);
    while (!readyImages.empty()) {
        ReadyImage ready = readyImages.front();
        readyImages.pop();
        SDL_Texture* tex = nullptr;
        auto known = textureByUrl.find(ready.url);
        if (known != textureByUrl.end()) {
            tex = known->second;
        } else if (SDL_Surface* surface = ImageLoader::Load(ready.path)) {  // WebP decoded by bundled libwebp
            tex = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_FreeSurface(surface);
            textureByUrl[ready.url] = tex;
        }
        // The list may have been replaced since this download was queued.
        if (tex && ready.index >= 0 && ready.index < (int)items.size() && ready.index < (int)textures.size() &&
            items[ready.index].imagePath == ready.url && !textures[ready.index]) {
            textures[ready.index] = tex;
        }
    }
}

// Opportunistic single-item texture load (used when ensuring a specific index is ready).
void ListScreen::loadTexture(SDL_Renderer* renderer, int index) {
    if (index < 0 || index >= items.size() || index >= textures.size()) return;
    if (textures[index]) return;
    // If image is already cached locally, load instantly
    std::string filename;
    if (!items[index].imagePath.empty() && items[index].imagePath.find("http") == 0) {
        std::string ext = getFileExtension(items[index].imagePath);
        filename = "cache/images/" + std::to_string(std::hash<std::string>{}(items[index].imagePath)) + ext;
        std::ifstream file(filename);
        if (file.good()) {
            file.close();
            SDL_Surface* surface = ImageLoader::Load(filename);
            if (surface) {
                textures[index] = SDL_CreateTextureFromSurface(renderer, surface);
                SDL_FreeSurface(surface);
                textureByUrl[items[index].imagePath] = textures[index];
            }
            return;
        }
        // Not cached, queue for download
        std::lock_guard<std::mutex> lock(downloadMutex);
        if (pendingDownloads.find(index) == pendingDownloads.end()) {
            pendingDownloads[index] = items[index].imagePath;
        }
        return;
    }
    // If imagePath is already a local file
    std::ifstream fileLocal(items[index].imagePath);
    if (fileLocal.good()) {
        fileLocal.close();
        SDL_Surface* surface = ImageLoader::Load(items[index].imagePath);
        if (surface) {
            textures[index] = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_FreeSurface(surface);
        }
    }
}

// --- Render -------------------------------------------------------------------------------

void ListScreen::render(SDL_Renderer* renderer, TTF_Font* font) {
    (void)font;
    using namespace UiUtils;
    updateTextures(renderer);
    const ThemeColors& t = Theme::getInstance().colors();
    DrawBackground(renderer);

    // ---- Layout ----------------------------------------------------------------
    if (global) global->update();
    const bool imageMode = global ? !global->compact() : tallGridMode;
    const int contentTop = HEADER_H + 8 + (global ? GlobalSearchController::kBarHeight + 6 : 0);
    const int contentBottom = (global && global->compact()) ? global->keyboardTop() - 34 : SCREEN_H - FOOTER_H - 4;
    int gridColumns, cardW, cardH, gapX, gapY, visibleRows;
    if (imageMode) {
        gridColumns = 4; gapX = 24; gapY = 24;
        cardW = (SCREEN_W - 2 * MARGIN_X - (gridColumns - 1) * gapX) / gridColumns;
        cardH = 254;
    } else {
        gridColumns = 3; gapX = 16; gapY = 12;
        cardW = (SCREEN_W - 2 * MARGIN_X - (gridColumns - 1) * gapX) / gridColumns;
        cardH = 60;
    }
    visibleRows = std::max(1, (contentBottom - contentTop + gapY) / (cardH + gapY));
    currentGridColumns = gridColumns;
    currentVisibleRows = visibleRows;
    const int rowStep = cardH + gapY;
    const int totalRows = getTotalRows(gridColumns);

    // ---- Header ----------------------------------------------------------------
    std::string pill;
    if (pagination.totalPages > 1)
        pill = "Page " + std::to_string(pagination.currentPage) + " of " + std::to_string(pagination.totalPages);
    else if (!items.empty())
        pill = std::to_string(items.size()) + (imageMode ? " games" : " consoles");
    if (global) DrawHeader(renderer, global->title(), "All sites", global->pill());
    else DrawHeader(renderer, title, romSite, pill);

    // ---- Smooth scroll -----------------------------------------------------------
    scrollOffsetAnim += (scrollOffset - scrollOffsetAnim) * 0.3f;
    if (std::fabs(scrollOffsetAnim - scrollOffset) < 0.01f) scrollOffsetAnim = (float)scrollOffset;
    const int scrollBase = (int)std::floor(scrollOffsetAnim);
    const float scrollFrac = scrollOffsetAnim - scrollBase;

    // Queue cover downloads for visible + next rows (never load synchronously here).
    for (int row = 0; row < visibleRows + 2; ++row) {
        for (int col = 0; col < gridColumns; ++col) {
            int index = getIndexFromGrid(scrollBase + row, col, gridColumns);
            if (index >= 0 && index < (int)items.size() && !textures[index] &&
                items[index].imagePath.find("http") == 0) {
                std::lock_guard<std::mutex> lock(downloadMutex);
                if (pendingDownloads.find(index) == pendingDownloads.end())
                    pendingDownloads[index] = items[index].imagePath;
            }
        }
    }

    // ---- Cards (clipped to the content area so rows scroll under the header) ----
    SDL_Rect clip{0, contentTop - 16, SCREEN_W, contentBottom - contentTop + 24};
    SDL_RenderSetClipRect(renderer, &clip);
    const int gridW = gridColumns * cardW + (gridColumns - 1) * gapX;
    const int startX = (SCREEN_W - gridW) / 2;
    const int pulse = (int)(18 * (0.5f + 0.5f * std::sin(SDL_GetTicks() / 260.0f)));
    TTF_Font* titleFont = GetFont(17);
    TTF_Font* titleFontBold = GetFont(17, true);

    for (int row = -1; row <= visibleRows + 1; ++row) {
        for (int col = 0; col < gridColumns; ++col) {
            int index = getIndexFromGrid(scrollBase + row, col, gridColumns);
            if (index < 0 || index >= (int)items.size()) continue;
            int y = contentTop + (int)std::lround((row - scrollFrac) * rowStep);
            if (y > contentBottom || y + cardH < contentTop - 16) continue;
            SDL_Rect card{startX + col * (cardW + gapX), y, cardW, cardH};
            const bool sel = (index == selectedIndex);
            const int radius = imageMode ? 16 : 12;
            SDL_Color fill = sel ? Mix(t.cardBackground, t.accentPrimary, 0.12f) : t.cardBackground;

            DrawShadow(renderer, card, radius, sel ? 26 : 16, sel ? 140 : 80, sel ? 10 : 6);
            FillRoundedRect(renderer, card, radius, fill);
            StrokeRoundedRect(renderer, card, radius, 1, WithAlpha(t.textPrimary, 18));
            if (sel) DrawFocusRing(renderer, card, radius, t.accentPrimary, 3, 3);

            const std::string label = items[index].label.empty() ? "(No title)" : items[index].label;
            if (imageMode) {
                const int inset = 8;
                SDL_Rect art{card.x + inset, card.y + inset, card.w - 2 * inset, card.h - 2 * inset - 58};
                SDL_Texture* tex = index < (int)textures.size() ? textures[index] : nullptr;
                if (tex) {
                    int texW = 0, texH = 0;
                    SDL_QueryTexture(tex, nullptr, nullptr, &texW, &texH);
                    SDL_Rect src{0, 0, texW, texH};
                    float boxAspect = (float)art.w / art.h, texAspect = texH ? (float)texW / texH : boxAspect;
                    if (texAspect > boxAspect) { int cw = (int)(texH * boxAspect); src.x = (texW - cw) / 2; src.w = cw; }
                    else { int ch = (int)(texW / boxAspect); src.y = (texH - ch) / 6; src.h = ch; }  // keep box-art logos (top) visible
                    SDL_SetTextureAlphaMod(tex, 255);
                    SDL_RenderCopy(renderer, tex, &src, &art);
                } else {
                    FillRoundedRect(renderer, art, 10, Lighten(fill, 0.04f + pulse / 900.0f));
                    if (!items[index].imagePath.empty())
                        DrawSpinner(renderer, art.x + art.w / 2, art.y + art.h / 2, 12, WithAlpha(t.textSecondary, 160));
                }
                MaskCorners(renderer, art, 10, fill);

                TTF_Font* tf = sel ? titleFontBold : titleFont;
                if (!items[index].site.empty()) {
                    // Global result: one title line + "Console · Site"
                    int ty = art.y + art.h + 8;
                    DrawText(renderer, tf, Ellipsize(tf, label, art.w), art.x + 2, ty, sel ? t.textPrimary : t.textSecondary);
                    TTF_Font* mf = GetFont(14);
                    std::string meta = items[index].consoleName + "  ·  " + items[index].site;
                    DrawText(renderer, mf, Ellipsize(mf, meta, art.w), art.x + 2, ty + 24, WithAlpha(t.textSecondary, 200));
                    continue;
                }
                std::vector<std::string> lines = WrapText(tf, label, art.w);
                if (lines.size() > 2) {
                    std::string rest = lines[1];
                    for (size_t i = 2; i < lines.size(); ++i) rest += " " + lines[i];
                    lines.resize(2);
                    lines[1] = rest;
                }
                int ty = art.y + art.h + 8;
                for (auto& l : lines) {
                    DrawText(renderer, tf, Ellipsize(tf, l, art.w), art.x + 2, ty, sel ? t.textPrimary : t.textSecondary);
                    ty += 22;
                }
            } else {
                std::string name, count;
                SplitTrailingCount(label, name, count);
                if (!items[index].site.empty()) {
                    // Global result (compact list while typing)
                    TTF_Font* nf = sel ? GetFont(17, true) : GetFont(17);
                    TTF_Font* mf = GetFont(13);
                    TTF_Font* pf = GetFont(13, true);
                    const int pad = 16;
                    int pillW = PillWidth(pf, items[index].site, 9);
                    if (sel) FillRoundedRect(renderer, SDL_Rect{card.x + 7, card.y + 12, 4, card.h - 24}, 2, t.accentPrimary);
                    int maxW = card.w - 2 * pad - pillW - 10;
                    DrawText(renderer, nf, Ellipsize(nf, label, maxW), card.x + pad, card.y + 9, sel ? t.textPrimary : Mix(t.textSecondary, t.textPrimary, 0.45f));
                    DrawText(renderer, mf, Ellipsize(mf, items[index].consoleName, maxW), card.x + pad, card.y + 33, WithAlpha(t.textSecondary, 200));
                    DrawPill(renderer, pf, items[index].site, card.x + card.w - pad - pillW, card.y + (card.h - 22) / 2,
                             WithAlpha(t.textPrimary, 16), t.textSecondary, 9, 22);
                    continue;
                }
                TTF_Font* nf = sel ? GetFont(18, true) : GetFont(18);
                TTF_Font* cf = GetFont(14, true);
                int pad = 18;
                int countW = count.empty() ? 0 : PillWidth(cf, count, 10) + 12;
                // accent bar on the selected row
                if (sel) FillRoundedRect(renderer, SDL_Rect{card.x + 8, card.y + 14, 4, card.h - 28}, 2, t.accentPrimary);
                std::string shown = Ellipsize(nf, name, card.w - 2 * pad - countW);
                DrawText(renderer, nf, shown, card.x + pad, card.y + (card.h - TextHeight(nf)) / 2, sel ? t.textPrimary : Mix(t.textSecondary, t.textPrimary, 0.45f));
                if (!count.empty()) {
                    int pw = PillWidth(cf, count, 10);
                    DrawPill(renderer, cf, count, card.x + card.w - pad - pw, card.y + (card.h - 24) / 2,
                             sel ? WithAlpha(t.accentPrimary, 60) : WithAlpha(t.textPrimary, 16),
                             sel ? t.textHighlight : t.textSecondary, 10, 24);
                }
            }
        }
    }
    SDL_RenderSetClipRect(renderer, nullptr);
    // Soft edges where rows scroll under the header / footer.
    if (scrollOffsetAnim > 0.01f) DrawEdgeFade(renderer, contentTop - 16, 22, false);
    if (totalRows > scrollBase + visibleRows) DrawEdgeFade(renderer, contentBottom - 26, 34, true);

    // ---- Empty state ---------------------------------------------------------------
    if (items.empty()) {
        int cy = (contentTop + contentBottom) / 2;
        std::string head = global ? global->emptyTitle() : "Nothing here";
        std::string sub = global ? global->emptyHint() : "Try another search or filter.";
        if (global && global->liveRunning()) DrawSpinner(renderer, SCREEN_W / 2, cy - 58, 16, t.accentPrimary);
        RenderTextCentered(renderer, GetFont(24, true), head, SCREEN_W / 2, cy - 16, Color(t.textPrimary));
        RenderTextCentered(renderer, GetFont(18), sub, SCREEN_W / 2, cy + 18, Color(t.textSecondary));
    }

    // ---- Scrollbar ---------------------------------------------------------------------
    if (totalRows > visibleRows) {
        int barH = visibleRows * rowStep - gapY;
        float pos = scrollOffsetAnim / std::max(1, totalRows - visibleRows);
        DrawScrollbar(renderer, SCREEN_W - 22, contentTop, barH, (float)visibleRows / totalRows, pos);
    }

    bool modalVisible = (siteType == SiteType::Hexrom && hexromModal && hexromModal->isVisible()) ||
                        (siteType == SiteType::Romspedia && romspediaModal && romspediaModal->isVisible());

    // ---- Footer (the modal draws its own hints) -----------------------------------
    if (global) {
        if (!global->compact() && global->mode() != GlobalSearchController::Mode::Filters) {
            std::string status = items.empty() ? global->footerStatus()
                                               : std::to_string(selectedIndex + 1) + " / " + std::to_string(items.size()) + "  ·  " + global->footerStatus();
            DrawFooterStatus(renderer, status, global->liveRunning());
            DrawHints(renderer, global->hints());
        }
    } else if (modalVisible) {
        // skip
    } else if (loadingMore) {
        DrawFooterStatus(renderer, "Loading more…", true);
    } else if (!items.empty()) {
        DrawFooterStatus(renderer, std::to_string(selectedIndex + 1) + " / " + std::to_string(items.size()) +
                                   (sourceNote.empty() ? "" : "  ·  " + sourceNote));
    }
    if (!modalVisible && !global) {
        std::vector<Hint> hints = {{"DPAD", "Navigate"}};
        if (pagination.totalPages > 1) hints.push_back({"LR", "Page"});
        hints.push_back({"Y", "Search"});
        hints.push_back({"A", "Open"});
        hints.push_back({"B", "Back"});
        DrawHints(renderer, hints);
    }

    // ---- Global search bar + keyboard / filters -------------------------------------
    if (global) {
        global->renderBar(renderer, HEADER_H + 2);
        global->renderOverlay(renderer);
    }

    // ---- Filter modal --------------------------------------------------------------
    if (modalVisible) {
        DrawScrim(renderer);
        int modalW = 800, modalH = 472;
        int modalX = (SCREEN_W - modalW) / 2, modalY = (SCREEN_H - modalH) / 2;
        if (siteType == SiteType::Hexrom) hexromModal->render(renderer, GetFont(20), modalX, modalY, modalW, modalH);
        else romspediaModal->render(renderer, GetFont(20), modalX, modalY, modalW, modalH);
    }
}

void ListScreen::appendItems(const std::vector<ListItem>& newItems, const PaginationInfo& newPagination) {
    // Prevent multiple appends at once
    if (loadingMore) {
        loadingMore = false; // Reset loading state as we're now appending
    }
    // Remember previous size and selection
    size_t prevSize = items.size();
    int prevScrollOffset = scrollOffset;
    int prevGridColumns = currentGridColumns > 0 ? currentGridColumns : 3;
    int prevVisibleRows = currentVisibleRows > 0 ? currentVisibleRows : 2;

    // Append new items and textures
    items.insert(items.end(), newItems.begin(), newItems.end());
    textures.resize(items.size(), nullptr);
    pagination = newPagination;

    // Always advance scrollOffset by one page (prevVisibleRows) after appending, so the first new row appears after the last visible row
    if (prevSize > 0 && newItems.size() > 0) {
        scrollOffset = prevScrollOffset + prevVisibleRows;
    }
    // Clamp scrollOffset to max possible value
    int totalRows = (int)((items.size() + prevGridColumns - 1) / prevGridColumns);
    int maxScrollOffset = std::max(0, totalRows - prevVisibleRows);
    if (scrollOffset > maxScrollOffset) scrollOffset = maxScrollOffset;
    if (scrollOffset < 0) scrollOffset = 0;
    // Clamp selectedIndex to last real item
    if (selectedIndex >= (int)items.size()) selectedIndex = (int)items.size() - 1;
}

// --- Fast analog stick scroll state ---
static Uint32 axisHeldStart = 0;
static Uint32 lastAxisTime = 0;
static int lastAxisValue = 0;

// Handle continuous analog stick scrolling with acceleration.
void ListScreen::updateAnalogScroll() {
    // Poll the left stick Y axis for continuous scrolling
    SDL_GameController* controller = SDL_GameControllerFromInstanceID(0); // 0 = first controller
    int value = 0;
    if (controller) {
        value = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
    }
    Uint32 now = SDL_GetTicks();
    const int deadzone = 8000;
    const Uint32 initialDelay = 180; // ms before repeat starts
    const Uint32 repeatDelay = 40;   // ms between repeats
    const Uint32 accelDelay = 600;   // ms after which to accelerate
    int scrollAmount = 1;
    if (abs(value) > deadzone) {
        if (axisHeldStart == 0) {
            axisHeldStart = now;
            lastAxisTime = now - initialDelay; // allow immediate scroll
        }
        if (now - axisHeldStart > accelDelay) {
            scrollAmount = 3;
        }
        if (now - lastAxisTime >= ((axisHeldStart == lastAxisTime) ? initialDelay : repeatDelay)) {
            for (int i = 0; i < scrollAmount; ++i) {
                if (value > 0) {
                    // Scroll down
                    if (selectedIndex + currentGridColumns < items.size()) {
                        selectedIndex += currentGridColumns;
                        if (selectedIndex >= (scrollOffset + currentVisibleRows) * currentGridColumns) {
                            scrollOffset++;
                        }
                    } else if (pagination.totalPages > 1 && pagination.currentPage < pagination.totalPages && onPageChange) {
                        if (!loadingMore) {
                            loadingMore = true;
                            onPageChange(pagination.currentPage + 1);
                        }
                    }
                } else if (value < 0) {
                    // Scroll up
                    if (selectedIndex - currentGridColumns >= 0) {
                        selectedIndex -= currentGridColumns;
                        if (selectedIndex < scrollOffset * currentGridColumns) {
                            scrollOffset--;
                        }
                    }
                }
            }
            lastAxisTime = now;
        }
        lastAxisValue = value;
    } else {
        lastAxisValue = 0;
        axisHeldStart = 0;
    }
}

// --- Input Handling -----------------------------------------------------------------------
void ListScreen::handleInput(const SDL_Event& e, MenuSystem& menuSystem) {
    if (global && global->handleInput(e)) return;
    // --- Trigger Filter modal only if not in console menu ---
    if (e.type == SDL_CONTROLLERBUTTONDOWN && e.cbutton.button == BUTTON_Y) {
        bool isConsoleMenu = false;
        if (title == "Consoles" || title == "Console Menu" || title == "Select Console") isConsoleMenu = true;
        if (!isConsoleMenu) {
            if (siteType == SiteType::Hexrom && hexromModal) {
                hexromModal->showModal(!hexromModal->isVisible());
            } else if (siteType == SiteType::Romspedia && romspediaModal) {
                romspediaModal->showModal(!romspediaModal->isVisible());
            }
        }
        return;
    }

    // Delegate filter modal input
    if (siteType == SiteType::Hexrom && hexromModal && hexromModal->isVisible()) {
        hexromModal->handleInput(e); return; }
    if (siteType == ListScreen::SiteType::Romspedia && romspediaModal && romspediaModal->isVisible()) {
        romspediaModal->handleInput(e); return; }
    int currentRow, currentCol;
    getGridPosition(selectedIndex, currentGridColumns, currentRow, currentCol);
    if (e.type == SDL_CONTROLLERBUTTONDOWN) {
        switch (e.cbutton.button) {
            case BUTTON_DPAD_UP:
                if (selectedIndex - currentGridColumns >= 0) {
                    selectedIndex -= currentGridColumns;
                    if (moveSound) { Mix_PlayChannel(-1, moveSound, 0); }
                    if (selectedIndex < scrollOffset * currentGridColumns) {
                        scrollOffset--;
                    }
                }
                break;
            case BUTTON_DPAD_DOWN:
                if (selectedIndex + currentGridColumns < items.size()) {
                    selectedIndex += currentGridColumns;
                    if (moveSound) { Mix_PlayChannel(-1, moveSound, 0); }
                    if (selectedIndex >= (scrollOffset + currentVisibleRows) * currentGridColumns) {
                        scrollOffset++;
                    }
                } else if (pagination.totalPages > 1 && pagination.currentPage < pagination.totalPages && onPageChange) {
                    if (!loadingMore) {
                        loadingMore = true;
                        onPageChange(pagination.currentPage + 1);
                    }
                }
                break;
            case BUTTON_DPAD_LEFT:
                if (selectedIndex % currentGridColumns > 0) {
                    selectedIndex--;
                    if (moveSound) { Mix_PlayChannel(-1, moveSound, 0); }
                }
                break;
            case BUTTON_DPAD_RIGHT:
                if (selectedIndex % currentGridColumns < currentGridColumns - 1 && selectedIndex + 1 < items.size()) {
                    selectedIndex++;
                    if (moveSound) { Mix_PlayChannel(-1, moveSound, 0); }
                }
                break;
            case BUTTON_LEFTSHOULDER:
                if (pagination.totalPages > 1 && pagination.currentPage > 1) {
                    int targetPage = pagination.currentPage - 1;
                    if (targetPage < 1) break;
                    {
                        HexromScraper s; auto result = s.fetchGames(pagination.baseUrl, targetPage);
                        items = result.first; pagination = result.second;
                        selectedIndex = 0; scrollOffset = 0; textures.clear(); textures.resize(items.size(), nullptr);
                    }
                    return;
                }
                break;
            case BUTTON_RIGHTSHOULDER:
                if (pagination.totalPages > 1 && pagination.currentPage < pagination.totalPages && onPageChange) {
                    int itemsPerPage = currentGridColumns * currentVisibleRows;
                    int newIndex = (pagination.currentPage) * itemsPerPage;
                    if (newIndex >= items.size()) newIndex = items.size() - 1;
                    selectedIndex = newIndex;
                    scrollOffset = (selectedIndex / currentGridColumns);
                    onPageChange(pagination.currentPage + 1);
                }
                break;
            case BUTTON_B: // B = Go back
                menuSystem.popScreen();
                break;
            case BUTTON_A: // A = Select
                {
                    int index = selectedIndex;
                    if (index >= 0 && index < items.size()) {
                        if (onItemSelected1) onItemSelected1(items[index]);
                        if (onItemSelected2) onItemSelected2(items[index], index);
                    }
                }
                break;
        }
    } else if (e.type == SDL_CONTROLLERAXISMOTION) {
        if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
            int value = e.caxis.value;
            const int DEADZONE = 8000;
            if (abs(value) > DEADZONE) {
                if (lastAxisValue == 0) {
                    if (value > 0) {
                        if (selectedIndex + currentGridColumns < items.size()) {
                            selectedIndex += currentGridColumns;
                            if (selectedIndex >= (scrollOffset + currentVisibleRows) * currentGridColumns) {
                                scrollOffset++;
                            }
                        } else if (pagination.totalPages > 1 && pagination.currentPage < pagination.totalPages && onPageChange) {
                            onPageChange(pagination.currentPage + 1);
                        }
                    } else {
                        if (selectedIndex - currentGridColumns >= 0) {
                            selectedIndex -= currentGridColumns;
                            if (selectedIndex < scrollOffset * currentGridColumns) {
                                scrollOffset--;
                            }
                        }
                    }
                    lastAxisTime = SDL_GetTicks();
                }
                lastAxisValue = value;
            } else {
                lastAxisValue = 0;
            }
        }
    }
}