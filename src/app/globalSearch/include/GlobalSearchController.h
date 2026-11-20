#pragma once
#include <SDL2/SDL.h>

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "../../../catalog/include/Catalog.h"
#include "../../../model/ListItem.h"
#include "../../../utils/include/UiUtils.h"
#include "../../screenListing/include/OnscreenKeyboard.h"

// Drives the global "Search all sites" screen (a ListScreen in global mode).
//
//   Typing   - on-screen keyboard at the bottom; results update on every key
//              press in a compact list above it.
//   Browsing - keyboard closed; results as the normal cover grid.
//   Filters  - sheet with Console / Site / Sort and "Search sites live".
//
// Results come from the catalog (instant, offline). "Search sites live" asks
// every ROM site directly instead (slower, but always current).
class GlobalSearchController {
public:
    enum class Mode { Typing, Browsing, Filters };
    using ApplyFn = std::function<void(const std::vector<ListItem>&)>;

    explicit GlobalSearchController(ApplyFn apply);

    void start();                 // open in Typing mode
    void update();                // call every frame: picks up finished live searches
    bool handleInput(const SDL_Event& e);  // true if consumed

    Mode mode() const { return mode_; }
    bool compact() const { return mode_ == Mode::Typing; }

    // Layout
    static constexpr int kBarHeight = 56;
    int keyboardTop() const;      // top of the keyboard panel in Typing mode

    // Text for the hosting screen
    std::string title() const;
    std::string pill() const;
    std::string emptyTitle() const;
    std::string emptyHint() const;
    std::string footerStatus() const;
    bool liveRunning() const;
    std::vector<UiUtils::Hint> hints() const;

    // Drawing
    void renderBar(SDL_Renderer* renderer, int y);
    void renderOverlay(SDL_Renderer* renderer);

private:
    struct LiveState {
        std::mutex mutex;
        bool running = false;
        bool ready = false;
        std::string query;
        std::vector<Catalog::GlobalResult> results;
    };

    void refresh();
    void startLive();
    void applyResults(const std::vector<Catalog::GlobalResult>& results, bool live);
    bool filtersActive() const;
    std::string consoleKey() const;
    std::string site() const;
    void openPulldown();
    std::vector<std::string> consoleOptions() const;
    std::vector<std::string> siteOptions() const;
    static const std::vector<std::string>& sortOptions();

    ApplyFn apply_;
    Mode mode_ = Mode::Typing;
    std::string text_;
    bool keyboardOpen_ = true;
    OnscreenKeyboard keyboard_;

    std::vector<Catalog::ConsoleGroup> groups_;
    std::vector<std::string> sites_;
    int consoleIdx_ = 0, siteIdx_ = 0, sortIdx_ = 0;

    // Filters sheet
    int focus_ = 0;               // 0 console, 1 site, 2 sort, 3 live search, 4 done
    bool pulldown_ = false;
    int pullIdx_ = 0;
    std::vector<std::string> pullOptions_;

    size_t resultCount_ = 0;
    bool liveMode_ = false;
    std::string liveMessage_;
    std::shared_ptr<LiveState> live_ = std::make_shared<LiveState>();
};
