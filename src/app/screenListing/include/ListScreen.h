#pragma once
#include <string>
#include <fstream>           
#include <iostream>   
#include <vector>
#include <functional>
#include <thread>
#include <mutex>
#include <queue>
#include <map>
#include <atomic>

// SDL
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_image.h> 

// Other imports
#include "OnscreenKeyboard.h"
#include "../../globalSearch/include/GlobalSearchController.h"
#include <unordered_map>
#include "../../Screen.h"
#include "../../../model/ListItem.h"
#include "../../../model/PaginationInfo.h"
#include "../../../utils/include/HttpUtils.h"   
#include "../../../scraper/engine/DefinedScraper.h"
#include "FilterForm.h"
#include "../../../utils/include/StringUtils.h"
#include "../../../utils/include/UiUtils.h"
#include "../../menuApp/include/MenuSystem.h"
#include "../../ControllerButtons.h"
#include "../../loadingScreen/include/LoadingScreen.h"
#include "../../../model/GameDetails.h"

class ListScreen : public Screen {
public:
    SDL_Renderer* renderer;
private:
    std::string title;
    std::vector<ListItem> items;
    std::vector<SDL_Texture*> textures;
    int selectedIndex = 0;
    int scrollOffset = 0;
    float scrollOffsetAnim = 0.0f; // For smooth vertical transition
    PaginationInfo pagination;
    std::function<void(int)> onPageChange;

    static constexpr int GRID_COLUMNS = 3;
    static constexpr int TILE_WIDTH = 240; // smaller for more grid cells
    static constexpr int TILE_HEIGHT = 120; // smaller for more rows
    static constexpr int TILE_PADDING = 16; // less padding
    static constexpr int TILE_VERTICAL_SPACING = 12; // less vertical space
    static constexpr int VISIBLE_ROWS = 5;
    static constexpr int ICON_SIZE = 120;
    static constexpr int GAME_ICON_SIZE = 180; // Larger icon for games

    void loadTexture(SDL_Renderer* renderer, int index);

    // Updated grid helpers as inline to avoid linker errors
    inline void getGridPosition(int index, int gridColumns, int& row, int& col) {
        row = index / gridColumns;
        col = index % gridColumns;
    }
    inline int getIndexFromGrid(int row, int col, int gridColumns) {
        int index = row * gridColumns + col;
        return (index < items.size()) ? index : -1;
    }
    inline int getTotalRows(int gridColumns) {
        return (items.size() + gridColumns - 1) / gridColumns;
    }

    // Multithreaded image loading
    std::map<int, std::string> pendingDownloads; // index -> image URL
    struct ReadyImage { int index; std::string path; std::string url; };
    std::queue<ReadyImage> readyImages;           // finished downloads (url guards against stale indices)
    // Every texture created, by image URL: reused when the list is replaced
    // (search results) and freed in the destructor.
    std::unordered_map<std::string, SDL_Texture*> textureByUrl;
    std::mutex downloadMutex;
    std::atomic<bool> stopThreads{false};
    void startImageLoaderThreads(SDL_Renderer* renderer);
    void stopImageLoaderThreads();
    void updateTextures(SDL_Renderer* renderer);
    std::vector<std::thread> loaderThreads;
    void queueInitialDownloads();

    int lastAxisValue = 0;
    Uint32 lastAxisTime = 0;
    int currentGridColumns = GRID_COLUMNS;
    int currentVisibleRows = VISIBLE_ROWS;

    bool loadingMore = false;

    Mix_Chunk* moveSound = nullptr;

    // The site this list belongs to (null for global search), and its
    // search & filter dialog (Y) with the last search/choices applied.
    std::shared_ptr<DefinedScraper> scraper;
    std::unique_ptr<FilterForm> filterForm;
    std::string filterSearch;
    std::vector<int> filterChoices;
    bool filterActive = false;            // the list currently shows filter results from the site
    // Global search across all sites (romSite == "Global")
    std::unique_ptr<GlobalSearchController> global;
    std::string sourceNote;
    std::string consoleUrl;               // console this list came from

    // --- On-screen keyboard integration ---
    bool showOnscreenKeyboard = false;
    OnscreenKeyboard onscreenKeyboard;

public:
    // Overload: support both callback signatures
    ListScreen(const std::string& title, const std::string& romSite, const std::vector<ListItem>& initialItems, SDL_Renderer* renderer,
               std::function<void(const ListItem&)> onSelect,
               std::function<void(int)> pageChangeCallback = nullptr,
               const PaginationInfo& paginationInfo = PaginationInfo());
    std::string romSite;
    ~ListScreen();
    void render(SDL_Renderer* renderer, TTF_Font* font) override;
    void handleInput(const SDL_Event& e, MenuSystem& menuSystem) override;

    // Add a getter for the selected texture
    SDL_Texture* getSelectedTexture() const { return (selectedIndex >= 0 && selectedIndex < textures.size()) ? textures[selectedIndex] : nullptr; }
    int getSelectedIndex() const { return selectedIndex; }
    SDL_Texture* getTextureAt(int idx) const { return (idx >= 0 && idx < textures.size()) ? textures[idx] : nullptr; }

    // Allow setting the item selected callbacks after construction
    std::function<void(const ListItem&)> onItemSelected1;
    std::function<void(const ListItem&, int)> onItemSelected2;

    // Tall grid mode for games listing
    bool tallGridMode = false;
    void setTallGridMode(bool tall) { tallGridMode = tall; }

    void updateAnalogScroll();

    void appendItems(const std::vector<ListItem>& newItems, const PaginationInfo& newPagination);

    void setLoadingMore(bool loading) { loadingMore = loading; }

    // Shown in the footer, e.g. "catalog updated 3 h ago".
    void setSourceNote(const std::string& note) { sourceNote = note; }

    // Add a helper to determine if this is a game listing screen
    bool isGameListingScreen() const {
        // Heuristic: if the title contains "ROMs" or "Games", treat as game listing
        return title.find("ROM") != std::string::npos || title.find("Game") != std::string::npos;
    }

    PaginationInfo const& getPagination() const { return pagination; }
    
    // Get the current pagination base URL (for MenuApplication onPageChange callback)
    std::string getCurrentPaginationBaseUrl() const { return pagination.baseUrl; }

private:
    // Split out bulky constructor site-specific code for readability
    void runSiteFilter(const std::string& search, const std::vector<int>& choices);
    void runSitePaginate(int page);
    void applyNewResult(const std::vector<ListItem>& newItems, const PaginationInfo& newPagination, bool replace = true);
};