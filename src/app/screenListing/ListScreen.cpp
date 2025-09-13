// ListScreen.cpp
// Responsibilities:
//  - Display a paginated grid of game items (with smooth scrolling & lazy image loading)
//  - Handle controller input for navigation, pagination, and filtering
//  - Asynchronous image downloading & texture creation
//

#include "include/ListScreen.h"  
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

inline bool isGamulator(const std::string& romSite) { return romSite == "Gamulator"; }

// Draw a very rough rounded rectangle (approximation by plotting corner points).
void drawApproxRoundedBorder(SDL_Renderer* renderer, const SDL_Rect& rect, int radius, SDL_Color fill, Uint8 alpha) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, (Uint8)(fill.a * (alpha / 255.0f)));
    SDL_RenderFillRect(renderer, &rect);
    for (int dx = 0; dx < radius; ++dx) {
        for (int dy = 0; dy < radius; ++dy) {
            if ((dx*dx + dy*dy) < radius*radius) {
                SDL_RenderDrawPoint(renderer, rect.x + dx, rect.y + dy);
                SDL_RenderDrawPoint(renderer, rect.x + rect.w - 1 - dx, rect.y + dy);
                SDL_RenderDrawPoint(renderer, rect.x + dx, rect.y + rect.h - 1 - dy);
                SDL_RenderDrawPoint(renderer, rect.x + rect.w - 1 - dx, rect.y + rect.h - 1 - dy);
            }
        }
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
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
    // Determine site type early
    if (romSite == "Gamulator") siteType = SiteType::Gamulator;
    else if (romSite == "Hexrom") siteType = SiteType::Hexrom;
    else if (romSite == "Romspedia") siteType = SiteType::Romspedia;
    else siteType = SiteType::Hexrom; // default fallback
    // Site-specific modal setup
    if (siteType == SiteType::Gamulator) setupGamulatorModal();
    else if (siteType == SiteType::Romspedia) setupRomspediaModal();
    else setupHexromModal();
    textures.resize(items.size(), nullptr);
    system("mkdir -p cache/images");
    queueInitialDownloads();
    startImageLoaderThreads(renderer);
    moveSound = Mix_LoadWAV("sounds/move.wav");
    if (moveSound) Mix_VolumeChunk(moveSound, MIX_MAX_VOLUME);
    Mix_Volume(-1, MIX_MAX_VOLUME);
}

ListScreen::~ListScreen() {
    if (moveSound) {
        Mix_FreeChunk(moveSound);
        moveSound = nullptr;
    }
    stopThreads = true;
    stopImageLoaderThreads();
    for (auto texture : textures) if (texture) SDL_DestroyTexture(texture);
    // unique_ptr modals auto-clean
}

// --- Site-specific modal setup helpers -----------------------------------------------------
void ListScreen::setupGamulatorModal() {
    gamulatorModal = std::make_unique<GamulatorFilterModal>();
    gamulatorSearch.clear();
    // Set initial filter state - always active if we're in a specific console
    bool isConsoleSpecific = pagination.baseUrl.find("/roms/") != std::string::npos && 
                            pagination.baseUrl.find("gamulator.com/roms") != std::string::npos &&
                            pagination.baseUrl != "https://www.gamulator.com/roms";
    gamulatorFilterActive = isConsoleSpecific;
    // Store the original console URL for filtering
    if (isConsoleSpecific) {
        gamulatorOriginalConsoleUrl = pagination.baseUrl;
    }
    gamulatorModal->setOnFilter([this](const std::string& search, int region, int sort, int order, int genre){
        (void)region; (void)sort; (void)order; (void)genre;
        runSiteFilter(search, region, sort, order, genre);
    });
    onPageChange = [this](int page){ runSitePaginate(page); };
}

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
        textures.clear();
        textures.resize(items.size(), nullptr);
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
    if (siteType == SiteType::Gamulator) {
        gamulatorSearch = search;
        // Always use filter for Gamulator if we're in a specific console (baseUrl contains /roms/console-name)
        // or if there's a search term
        bool isConsoleSpecific = pagination.baseUrl.find("/roms/") != std::string::npos && 
                                pagination.baseUrl.find("gamulator.com/roms") != std::string::npos &&
                                pagination.baseUrl != "https://www.gamulator.com/roms";
        gamulatorFilterActive = !search.empty() || isConsoleSpecific;
        // Use the original console URL for filtering, not the current pagination.baseUrl
        std::string filterBaseUrl = gamulatorOriginalConsoleUrl.empty() ? pagination.baseUrl : gamulatorOriginalConsoleUrl;
        auto result = GamulatorScraperFilter::filterGames(filterBaseUrl, *gamulatorModal, gamulatorSearch, 1);
        applyNewResult(result.first, result.second, true);
        gamulatorModal->showModal(false);
    } else if (siteType == SiteType::Romspedia) {
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
    if (siteType == SiteType::Gamulator) {
        if (gamulatorFilterActive) {
            // Use the original console URL for filtering, not the current pagination.baseUrl
            std::string filterBaseUrl = gamulatorOriginalConsoleUrl.empty() ? pagination.baseUrl : gamulatorOriginalConsoleUrl;
            auto result = GamulatorScraperFilter::filterGames(filterBaseUrl, *gamulatorModal, gamulatorSearch, page);
            applyNewResult(result.first, result.second, page == 1);
        } else {
            GamulatorScraper s; auto result = s.fetchGames(pagination.baseUrl, page);
            applyNewResult(result.first, result.second, page == 1);
        }
    } else if (siteType == SiteType::Romspedia) {
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
                    if (HttpUtils::downloadImage(url, filename)) {
                        std::lock_guard<std::mutex> lock(downloadMutex);
                        readyImages.push({idx, filename});
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
        auto tmp = readyImages.front();
        int idx = tmp.first;
        std::string path = tmp.second;
        readyImages.pop();
        if (idx >= 0 && idx < items.size() && idx < textures.size() && !textures[idx]) {
            SDL_Surface* surface = nullptr;
            if (path.size() > 5 && path.substr(path.size() - 5) == ".webp") {
                SDL_RWops* rw = SDL_RWFromFile(path.c_str(), "rb");
                if (rw) {
                    surface = IMG_LoadWEBP_RW(rw);
                    SDL_RWclose(rw);
                }
            } else {
                surface = IMG_Load(path.c_str());
            }
            if (surface) {
                textures[idx] = SDL_CreateTextureFromSurface(renderer, surface);
                SDL_FreeSurface(surface);
            } else {
                // Suppressed verbose SDL_image shader compile errors (noise on device)
                // std::cerr << "[Image] fail: " << path << std::endl;
            }
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
            SDL_Surface* surface = IMG_Load(filename.c_str());
            if (surface) {
                textures[index] = SDL_CreateTextureFromSurface(renderer, surface);
                SDL_FreeSurface(surface);
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
        SDL_Surface* surface = IMG_Load(items[index].imagePath.c_str());
        if (surface) {
            textures[index] = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_FreeSurface(surface);
        }
    }
}

// --- Render -------------------------------------------------------------------------------
void ListScreen::render(SDL_Renderer* renderer, TTF_Font* font) {
    updateTextures(renderer);
    // Use a solid teal background to match the main menu and reference image
    SDL_SetRenderDrawColor(renderer, 32, 170, 180, 255); // Main menu teal
    SDL_RenderClear(renderer);
    int centerX = 1280 / 2;
    // Draw title with shadow for prominence
    float titleScale = 1.53f; // 10% smaller than before
    int titleY = 18; // Move title higher (was 32)
    // Center the title horizontally by measuring its width
    int titleW = 0, titleH = 0;
    TTF_SizeText(font, title.c_str(), &titleW, &titleH);
    int titleX = centerX - int((titleW * titleScale) / 2);
    // Shadow
    UiUtils::RenderText(renderer, font, title, titleX + 2, titleY + 2, UiUtils::Color(0, 0, 0, 120), titleScale);
    // Main title
    UiUtils::RenderText(renderer, font, title, titleX, titleY, UiUtils::Color(255, 255, 220), titleScale);

    // Dynamic grid settings based on image presence
    int gridColumns = 3; // 3 columns for image grid (with images)
    int tileWidth = 380; // Adjusted for 3 columns
    int tileHeight = 270; // Reduced height for image cards
    int tilePadding = 32;
    int tileVerticalSpacing = 32; // More vertical spacing
    int visibleRows = 2;
    bool allNoImages = true;
    for (const auto& tex : textures) {
        if (tex) { allNoImages = false; break; }
    }
    if (allNoImages) {
        gridColumns = 3; // Use 3 columns for text-only grids
        tileWidth = 340; // Wider for more text
        tileHeight = 110;
        tilePadding = 18;
        tileVerticalSpacing = 12;
        visibleRows = 5;
    }
    currentGridColumns = gridColumns;
    currentVisibleRows = visibleRows;

    // --- Smooth scroll animation ---
    // Lerp scrollOffsetAnim toward scrollOffset
    float lerpSpeed = 0.33f; // Lower = slower, higher = snappier (increased for smoother, faster animation)
    scrollOffsetAnim += (scrollOffset - scrollOffsetAnim) * lerpSpeed;
    if (fabs(scrollOffsetAnim - scrollOffset) < 0.01f) scrollOffsetAnim = (float)scrollOffset;

    visibleRows = std::min(visibleRows, getTotalRows(gridColumns) - scrollOffset);
    int gridHeight = visibleRows * tileHeight + (visibleRows - 1) * tileVerticalSpacing;
    int bottomMargin = 170; // More space to ensure cards never overlap help text
    int startY;
    if (scrollOffset + visibleRows >= getTotalRows(gridColumns)) {
        startY = 720 - bottomMargin - gridHeight;
        if (startY < 50) startY = 50;
    } else {
        startY = std::max(50, (720 - gridHeight) / 2);
    }
    int startX = (1280 - (gridColumns * tileWidth)) / 2;
    // Show partial rows during scroll animation for smoothness, only full rows when settled
    int scrollBase = (int)scrollOffsetAnim;
    float scrollFrac = scrollOffsetAnim - scrollBase;
    bool isSettled = fabs(scrollOffsetAnim - scrollOffset) < 0.01f;
    int rowsToDraw = visibleRows + (isSettled ? 0 : 1);
    // Preload/queue image loads for visible and just-offscreen items (but do NOT synchronously load in render)
    int preloadRows = rowsToDraw + 1; // Preload one extra row for smoothness
    for (int row = 0; row < preloadRows; row++) {
        for (int col = 0; col < gridColumns; col++) {
            int index = getIndexFromGrid(scrollBase + row, col, gridColumns);
            if (index >= 0 && index < items.size()) {
                // Only queue if not already loaded or queued
                if (!textures[index] && !items[index].imagePath.empty() && items[index].imagePath.find("http") == 0) {
                    std::lock_guard<std::mutex> lock(downloadMutex);
                    if (pendingDownloads.find(index) == pendingDownloads.end()) {
                        pendingDownloads[index] = items[index].imagePath;
                    }
                }
            }
        }
    }
    for (int row = 0; row < rowsToDraw; row++) {
        // --- Fade effect for partial rows ---
        float rowAlpha = 1.0f;
        if (!isSettled) {
            if (row == 0 && scrollFrac > 0.01f) {
                // Top partial row fades out as it leaves
                rowAlpha = 1.0f - scrollFrac;
            } else if (row == rowsToDraw - 1 && scrollFrac > 0.01f) {
                // Bottom partial row fades in as it appears
                rowAlpha = scrollFrac;
            }
        }
        Uint8 alpha = (Uint8)(rowAlpha * 255);
    for (int col = 0; col < gridColumns; col++) {
            int index = getIndexFromGrid(scrollBase + row, col, gridColumns);
            if (index >= 0 && index < items.size()) {
                int y = startY + row * (tileHeight + tileVerticalSpacing) - (int)((tileHeight + tileVerticalSpacing) * scrollFrac);
                SDL_Rect tileRect = {startX + col * tileWidth + tilePadding / 2, y, tileWidth - tilePadding, tileHeight};
                bool isSelected = (index == selectedIndex);
                // --- Card background with modern color and drop shadow ---
                SDL_Rect shadowRect = tileRect;
                shadowRect.x += 6; shadowRect.y += 8;
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, (Uint8)(60 * (rowAlpha)) ); // Drop shadow
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_RenderFillRect(renderer, &shadowRect);
                SDL_SetRenderDrawColor(renderer, 32, 120, 140, (Uint8)(210 * (rowAlpha)) );
                SDL_RenderFillRect(renderer, &tileRect);
                // --- Rounded corners (approximate) ---
                // Rounded corners (approx)
                drawApproxRoundedBorder(renderer, tileRect, 24, SDL_Color{32,120,140,(Uint8)(210 * (rowAlpha))}, alpha);
                // --- Selection border ---
                if (isSelected) {
                    SDL_SetRenderDrawColor(renderer, 255, 220, 60, alpha);
                    SDL_RenderDrawRect(renderer, &tileRect);
                }
                // --- Render image or centered text ---
                if (index < textures.size() && textures[index]) {
                    // --- Image area: centered, cover, aspect ratio preserved, cropped if needed ---
                    int imageMargin = 8;
                    int helpTextMargin = 10;
                    int textHeight = 36;
                    int availableHeight = tileRect.h - imageMargin * 2 - textHeight - helpTextMargin;
                    int availableWidth = tileRect.w - imageMargin * 2;
                    int boxX = tileRect.x + imageMargin;
                    int boxY = tileRect.y + imageMargin;
                    int boxW = availableWidth;
                    int boxH = availableHeight;
                    int texW = 0, texH = 0;
                    SDL_QueryTexture(textures[index], NULL, NULL, &texW, &texH);
                    float boxAspect = (float)boxW / boxH;
                    float texAspect = (float)texW / texH;
                    SDL_Rect dstRect = {boxX, boxY, boxW, boxH};
                    SDL_Rect srcRect = {0, 0, texW, texH};
                    if (texAspect > boxAspect) {
                        int cropW = (int)(texH * boxAspect);
                        srcRect.x = (texW - cropW) / 2;
                        srcRect.w = cropW;
                    } else {
                        int cropH = (int)(texW / boxAspect);
                        srcRect.y = (texH - cropH) / 2;
                        srcRect.h = cropH;
                    }
                    SDL_SetTextureAlphaMod(textures[index], alpha);
                    SDL_RenderCopy(renderer, textures[index], &srcRect, &dstRect);
                    SDL_SetTextureAlphaMod(textures[index], 255);
                    // Inner rounding mask (inverse) for image corners
                    int ir = 18;
                    for (int dx = 0; dx < ir; ++dx) for (int dy = 0; dy < ir; ++dy) if ((dx*dx + dy*dy) > ir*ir) {
                        SDL_SetRenderDrawColor(renderer, 32, 120, 140, (Uint8)(210 * (rowAlpha)) );
                        SDL_RenderDrawPoint(renderer, dstRect.x + dx, dstRect.y + dy);
                        SDL_RenderDrawPoint(renderer, dstRect.x + dstRect.w-1-dx, dstRect.y + dy);
                        SDL_RenderDrawPoint(renderer, dstRect.x + dx, dstRect.y + dstRect.h-1-dy);
                        SDL_RenderDrawPoint(renderer, dstRect.x + dstRect.w-1-dx, dstRect.y + dstRect.h-1-dy);
                    }
                    int titleY = tileRect.y + tileRect.h - textHeight - 8;
                    std::string labelText = items[index].label.empty() ? "(No Title)" : items[index].label;
    std::string clampedLabel = UiUtils::ClampTextToLines(labelText, font, tileRect.w - 8, 2);
                    SDL_Rect textBox = {tileRect.x, titleY, tileRect.w, textHeight};
    UiUtils::Color textColor = isSelected ? UiUtils::Color(255, 255, 80, alpha) : UiUtils::Color(255, 255, 255, alpha);
    UiUtils::RenderTextCenteredInBox(renderer, font, clampedLabel, textBox, textColor);
                } else {
                    // --- Placeholder for not-yet-loaded images ---
                    SDL_SetRenderDrawColor(renderer, 60, 60, 80, alpha); // Subtle placeholder color
                    SDL_RenderFillRect(renderer, &tileRect);
                    std::string labelText = items[index].label.empty() ? "(No Title)" : items[index].label;
                    std::string clampedLabel = UiUtils::ClampTextToLines(labelText, font, tileRect.w - 8, 2);
                    SDL_Rect textBox = tileRect;
                    UiUtils::Color textColor = isSelected ? UiUtils::Color(255, 255, 80, alpha) : UiUtils::Color(200, 200, 200, alpha);
                    UiUtils::RenderTextCenteredInBox(renderer, font, clampedLabel, textBox, textColor);
                }
            }
        }
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }
    // --- Draw vertical scrollbar ---
    int totalRows = getTotalRows(gridColumns);
    if (totalRows > visibleRows) {
        int barX = 1260; // near right edge
        int barY = startY;
        int barW = 12;
        int barH = gridHeight;
        SDL_SetRenderDrawColor(renderer, 80, 80, 100, 180);
        SDL_Rect bgRect = {barX, barY, barW, barH};
        SDL_RenderFillRect(renderer, &bgRect);
        // Thumb
        float thumbH = (float)visibleRows / totalRows * barH;
        float thumbY = barY + ((float)scrollOffset / totalRows) * barH;
        SDL_SetRenderDrawColor(renderer, 220, 220, 80, 220);
        SDL_Rect thumbRect = {barX, (int)thumbY, barW, (int)thumbH};
        SDL_RenderFillRect(renderer, &thumbRect);
    }
    // Move help text slightly lower to visually align with page info
    std::string helpText = "Press A to select, B to go back, Y to search/filter";
    UiUtils::RenderTextCentered(renderer, font, helpText, centerX, 720 - 20, UiUtils::Color(255, 255, 255));

    // Move page info to the far left
    if (pagination.totalPages > 1) {
        std::string pageInfo = "Page " + std::to_string(pagination.currentPage) + " of " + std::to_string(pagination.totalPages);
        UiUtils::RenderText(renderer, font, pageInfo, 30, 720 - 30, UiUtils::Color(255, 255, 255));
        if (pagination.currentPage > 1) {
            UiUtils::RenderTextCentered(renderer, font, "<", 100, 720 - 30, UiUtils::Color(255, 255, 255));
        }
        if (pagination.currentPage < pagination.totalPages) {
            UiUtils::RenderTextCentered(renderer, font, ">", 1280 - 100, 720 - 30, UiUtils::Color(255, 255, 255));
        }
    }
    if (loadingMore) {
        // Move 'Loading more...' to the far right, aligned with help text and page info
        UiUtils::RenderText(renderer, font, "Loading more...", 1280 - 260, 720 - 30, UiUtils::Color(220, 220, 120));
    }

    // --- Filter Modal UI ---
    if (siteType == SiteType::Gamulator && gamulatorModal && gamulatorModal->isVisible()) {
        int modalW = 800, modalH = 500; int modalX = (1280 - modalW)/2, modalY = (720 - modalH)/2;
        gamulatorModal->render(renderer, font, modalX, modalY, modalW, modalH);
    } else if (siteType == SiteType::Hexrom && hexromModal && hexromModal->isVisible()) {
        int modalW = 800, modalH = 500; int modalX = (1280 - modalW)/2, modalY = (720 - modalH)/2;
        hexromModal->render(renderer, font, modalX, modalY, modalW, modalH);
    } else if (siteType == ListScreen::SiteType::Romspedia && romspediaModal && romspediaModal->isVisible()) {
        int modalW = 800, modalH = 500; int modalX = (1280 - modalW)/2, modalY = (720 - modalH)/2;
        romspediaModal->render(renderer, font, modalX, modalY, modalW, modalH);
    }
}

// Append a new page of items and adjust scroll so new content becomes visible seamlessly.
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
    // --- Trigger Filter modal only if not in console menu ---
    if (e.type == SDL_CONTROLLERBUTTONDOWN && e.cbutton.button == BUTTON_Y) {
        bool isConsoleMenu = false;
        if (title == "Consoles" || title == "Console Menu" || title == "Select Console") isConsoleMenu = true;
        if (!isConsoleMenu) {
            if (siteType == SiteType::Gamulator && gamulatorModal) {
                gamulatorModal->showModal(!gamulatorModal->isVisible());
            } else if (siteType == SiteType::Hexrom && hexromModal) {
                hexromModal->showModal(!hexromModal->isVisible());
            } else if (siteType == SiteType::Romspedia && romspediaModal) {
                romspediaModal->showModal(!romspediaModal->isVisible());
            }
        }
        return;
    }

    // Delegate filter modal input
    if (siteType == SiteType::Gamulator && gamulatorModal && gamulatorModal->isVisible()) {
        gamulatorModal->handleInput(e); return; }
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
                    // For Hexrom use scraper directly; Gamulator simple backward not implemented (could be added)
                    if (!isGamulator(romSite)) {
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