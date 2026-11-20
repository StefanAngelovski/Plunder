#include "include/MenuApplication.h"
#include "../../utils/include/Theme.h"
#include "../../Version.h"
#include <regex>

// ===================== Internal helper structs & constants ===================== //
struct DetailsWithTexture { GameDetails details; SDL_Texture* texture; };
enum EventCode { 
    EVENT_CONSOLES_LOADED = 1, 
    EVENT_GAMES_LOADED = 2, 
    EVENT_GAME_DETAILS_LOADED = 3, 
    EVENT_PATCH_NOTES_LOADED = 99 };
// =============================================================================== //

MenuApplication::MenuApplication() {}
MenuApplication::~MenuApplication() { cleanup(); }

// ===================== Initializing app window ===================== //
bool MenuApplication::initialize() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) != 0 || TTF_Init() == -1 || IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) == 0) {
        std::cerr << "Initialization Error" << std::endl;
        return false;
    }
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) != 0) {
        std::cerr << "SDL_mixer init failed: " << Mix_GetError() << std::endl;
    } 
    // Smooth (linear) scaling for cover art and icons.
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    window = SDL_CreateWindow("Trimui ROM Downloader", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    font = UiUtils::GetFont(20);
    if (!window || !renderer) {
        std::cerr << "Resource Creation Error" << std::endl;
        return false;
    }
    if (!font) {
        std::cerr << "Font not found, continuing without text rendering" << std::endl;
    }
    initController(); // Ensure controller is initialized
    menuSystem = new MenuSystem(renderer, font);
    buildSiteRegistry();
    setupScreens();
    
    // Start background update check
    UpdateChecker::checkForUpdatesAsync();
    
    running = true;
    return true;
}

// ===================== Site Registry Helpers ===================== //
void MenuApplication::buildSiteRegistry() {
    siteRegistry.clear();
    siteRegistry.push_back({"Hexrom", std::make_shared<HexromScraper>()});
    siteRegistry.push_back({"Romspedia", std::make_shared<RomspediaScraper>()});
}

std::shared_ptr<SiteScraper> MenuApplication::currentScraper() const {
    if (currentSiteIndex < 0 || currentSiteIndex >= (int)siteRegistry.size()) return nullptr;
    return siteRegistry[currentSiteIndex].scraper;
}

int MenuApplication::findSiteIndexById(const std::string& id) const {
    for (size_t i = 0; i < siteRegistry.size(); ++i) if (siteRegistry[i].id == id) return (int)i;
    return -1;
}

void MenuApplication::run() {
    SDL_Event e;
    Uint32 lastFrame = SDL_GetTicks();
    bool needsRedraw = true;
    while (running && menuSystem->isRunning()) {
        while (SDL_PollEvent(&e)) processEvent(e);
        updateCurrentScreenAnimations(needsRedraw);
    Uint32 now = SDL_GetTicks();
    const Uint32 frameDelay = 16; // fixed ~60 FPS
    if (needsRedraw || now - lastFrame >= frameDelay) {
            menuSystem->render();
            SDL_RenderPresent(renderer);
            lastFrame = now;
            needsRedraw = false;
        }
        SDL_Delay(1);
    }
}

// ===================== Event Processing ===================== //
void MenuApplication::processEvent(const SDL_Event& e) {
    if (e.type == SDL_QUIT) { running = false; return; }
    if (e.type == SDL_USEREVENT) {
        switch (e.user.code) {
            case EVENT_CONSOLES_LOADED: handleConsolesLoaded(static_cast<std::vector<ListItem>*>(e.user.data1)); return;
            case EVENT_GAMES_LOADED: handleGamesLoaded(static_cast<GameListData*>(e.user.data1)); return;
            case EVENT_GAME_DETAILS_LOADED: handleGameDetailsLoaded(static_cast<DetailsWithTexture*>(e.user.data1)); return;
            case EVENT_PATCH_NOTES_LOADED: handlePatchNotesLoaded(static_cast<std::vector<std::string>*>(e.user.data1)); return;
            default: break;
        }
    }
    menuSystem->handleInput(e);
}

void MenuApplication::handleConsolesLoaded(std::vector<ListItem>* consolesPtr) {
    if (!consolesPtr) return;
    auto listScreen = std::make_shared<ListScreen>(
        "Consoles", currentSite, *consolesPtr, renderer,
        [this](const ListItem& item) {
            std::string selectedConsoleName = item.label;
            std::string mappedFolder = getFolderForScrapedConsole(selectedConsoleName);
            if (!mappedFolder.empty()) {
                printf("[ConsoleMap] Selected '%s' maps to '%s'\n", selectedConsoleName.c_str(), mappedFolder.c_str());
            } else {
                printf("[ConsoleMap] Selected '%s' has NO mapping\n", selectedConsoleName.c_str());
            }
            currentConsoleName = selectedConsoleName; // Set member variable for persistence
            menuSystem->pushScreen(std::make_shared<LoadingScreen>("Loading games..."));
            fetchGamesAsync(item.downloadUrl, 1, false);
        },
        nullptr,
        PaginationInfo()
    );
    menuSystem->popScreen();
    menuSystem->pushScreen(listScreen);
    delete consolesPtr;
}

void MenuApplication::handleGamesLoaded(GameListData* gameData) {
    if (!gameData) return;
    auto currentScreen = menuSystem->getCurrentScreen();
    auto listScreen = std::dynamic_pointer_cast<ListScreen>(currentScreen);
    if (listScreen && gameData->pagination.currentPage > 1) {
        listScreen->setLoadingMore(false);
        listScreen->appendItems(gameData->games, gameData->pagination);
        delete gameData;
        return;
    }
    auto onPageChange = [this, gameData](int page) {
        auto currentScreen2 = menuSystem->getCurrentScreen();
        auto listScreen2 = std::dynamic_pointer_cast<ListScreen>(currentScreen2);
        if (listScreen2 && page > 1) {
            listScreen2->setLoadingMore(true);
            std::string currentBaseUrl = listScreen2->getCurrentPaginationBaseUrl();
            fetchGamesAsync(currentBaseUrl, page, false);
        } else {
            std::string baseUrl = gameData->baseUrl;
            menuSystem->pushScreen(std::make_shared<LoadingScreen>("Loading page " + std::to_string(page)));
            fetchGamesAsync(baseUrl, page, true);
        }
    };
    auto newListScreen = std::make_shared<ListScreen>(
        gameData->title,
        currentSite,
        gameData->games,
        renderer,
        [](const ListItem&) {},
        onPageChange,
        gameData->pagination
    );
    newListScreen->onItemSelected2 = [this, newListScreen](const ListItem& game, int index) {
        SDL_Texture* iconTexture = index >= 0 ? newListScreen->getTextureAt(index) : nullptr;
        menuSystem->pushScreen(std::make_shared<LoadingScreen>("Loading game details..."));
        fetchGameDetailsAsync(game, iconTexture, currentConsoleName);
    };
    newListScreen->setTallGridMode(true);
    menuSystem->popScreen();
    menuSystem->pushScreen(newListScreen);
    delete gameData;
}

void MenuApplication::handleGameDetailsLoaded(DetailsWithTexture* detailsWithTex) {
    if (!detailsWithTex) return;
    menuSystem->popScreen();
    menuSystem->pushScreen(std::make_shared<GameDetailsScreen>(detailsWithTex->details, detailsWithTex->texture));
    delete detailsWithTex;
}

void MenuApplication::handlePatchNotesLoaded(std::vector<std::string>* linesPtr) {
    auto currentScreen = menuSystem->getCurrentScreen();
    auto patchScreen = std::dynamic_pointer_cast<PatchNotesScreen>(currentScreen);
    if (patchScreen && linesPtr) patchScreen->setNotes(*linesPtr);
    if (linesPtr) delete linesPtr;
}

// ===================== Screen / Animation Update ===================== //
void MenuApplication::updateCurrentScreenAnimations(bool& needsRedraw) {
    auto currentScreen = menuSystem->getCurrentScreen();
    if (!currentScreen) return;
    if (auto listScreen = std::dynamic_pointer_cast<ListScreen>(currentScreen)) {
        listScreen->updateAnalogScroll();
        needsRedraw = true;
    } else if (auto patchScreen = std::dynamic_pointer_cast<PatchNotesScreen>(currentScreen)) {
        patchScreen->update();
        needsRedraw = true;
    } else if (auto carouselScreen = std::dynamic_pointer_cast<CarouselMenuScreen>(currentScreen)) {
        if (carouselScreen->update()) needsRedraw = true;
    }
}

// ===================== Async Fetch Helpers ===================== //
void MenuApplication::fetchConsolesAsync() {
    std::thread([this]() {
        int idx = currentSiteIndex;
        auto scraper = (idx >= 0 && idx < (int)siteRegistry.size()) ? siteRegistry[idx].scraper : nullptr;
        auto consoles = scraper ? scraper->fetchConsoles() : std::vector<ListItem>();
        SDL_Event event; SDL_zero(event);
        event.type = SDL_USEREVENT; event.user.code = EVENT_CONSOLES_LOADED; event.user.data1 = new std::vector<ListItem>(consoles);
        SDL_PushEvent(&event);
    }).detach();
}


void MenuApplication::fetchGamesAsync(const std::string& baseUrl, int page, bool showLoadingScreen) {
    std::thread([this, baseUrl, page]() {
        auto scraper = currentScraper();
        auto result = scraper ? scraper->fetchGames(baseUrl, page) : std::make_pair(std::vector<ListItem>(), PaginationInfo());
        GameListData* gameData = new GameListData();
        gameData->games = result.first;
        gameData->pagination = result.second;
        // Console label without its trailing game count, e.g. "GBA ROMs (1963)" -> "GBA ROMs"
        gameData->title = std::regex_replace(currentConsoleName, std::regex(R"(\s*\(\d+\)\s*$)"), "");
        if (gameData->title.empty()) gameData->title = "Games";
        gameData->baseUrl = baseUrl;
        gameData->consoleName = currentConsoleName; // Use member variable for persistence
        SDL_Event event; SDL_zero(event);
        event.type = SDL_USEREVENT; event.user.code = EVENT_GAMES_LOADED; event.user.data1 = gameData;
        SDL_PushEvent(&event);
    }).detach();
}

void MenuApplication::fetchGameDetailsAsync(const ListItem& game, SDL_Texture* iconTexture, const std::string& consoleName) {
    std::cout << "[MenuApplication] fetchGameDetailsAsync called for game: " << game.label << std::endl;
    std::cout << "[MenuApplication] downloadUrl: " << game.downloadUrl << std::endl;
    std::cout << "[MenuApplication] consoleName: " << consoleName << std::endl;
    std::thread([this, game, iconTexture, consoleName]() {
        auto scraper = currentScraper();
        std::cout << "[MenuApplication] Current scraper available: " << (scraper != nullptr) << std::endl;
        GameDetails details = scraper ? scraper->fetchGameDetails(game.downloadUrl) : GameDetails{};
        details.consoleName = consoleName;
        details.mappedFolder = getFolderForScrapedConsole(consoleName);
        SDL_Event event; SDL_zero(event);
        event.type = SDL_USEREVENT; event.user.code = EVENT_GAME_DETAILS_LOADED; event.user.data1 = new DetailsWithTexture{details, iconTexture};
        SDL_PushEvent(&event);
    }).detach();
}

void MenuApplication::fetchPatchNotesAsync(std::shared_ptr<PatchNotesScreen> patchScreen) {
    std::thread([patchScreen]() {
        std::string notes = ScrapeLatestGitHubReleaseNotes("https://github.com/StefanAngelovski/Plunder/releases/");
        std::vector<std::string> lines; size_t pos = 0, prev = 0;
        while ((pos = notes.find('\n', prev)) != std::string::npos) { lines.push_back(notes.substr(prev, pos - prev)); prev = pos + 1; }
        if (prev < notes.size()) lines.push_back(notes.substr(prev));
        SDL_Event event; SDL_zero(event);
        event.type = SDL_USEREVENT; event.user.code = EVENT_PATCH_NOTES_LOADED; event.user.data1 = new std::vector<std::string>(lines);
        SDL_PushEvent(&event);
    }).detach();
}

// ===================== Misc Helpers ===================== //
void MenuApplication::showNoInternetAndExit() {
    using namespace UiUtils;
    while (!HttpUtils::hasInternet()) {
        const ThemeColors& t = Theme::getInstance().colors();
        DrawBackground(renderer);
        SDL_Rect panel{SCREEN_W / 2 - 300, SCREEN_H / 2 - 150, 600, 270};
        DrawPanel(renderer, panel, 24);
        // warning glyph: "!" in a ring
        const SDL_Color red{239, 93, 93, 255};
        int cx = panel.x + panel.w / 2, cy = panel.y + 66;
        FillCircle(renderer, cx, cy, 32, WithAlpha(red, 45));
        StrokeRoundedRect(renderer, SDL_Rect{cx - 22, cy - 22, 44, 44}, 22, 4, red);
        FillRoundedRect(renderer, SDL_Rect{cx - 2, cy - 12, 5, 16}, 2, red);
        FillCircle(renderer, cx, cy + 10, 3, red);
        RenderTextCentered(renderer, GetFont(26, true), "No internet connection", cx, panel.y + 132, Color(t.textPrimary));
        RenderTextCentered(renderer, GetFont(18), "Connect to Wi-Fi to browse ROM sites.", cx, panel.y + 172, Color(t.textSecondary));
        RenderTextCentered(renderer, GetFont(16), "Press any button to exit", cx, panel.y + 222, Color(WithAlpha(t.textSecondary, 180)));
        SDL_RenderPresent(renderer);
        SDL_Event e; bool exitLoop = false;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) { running = false; exitLoop = true; break; }
            if (e.type == SDL_KEYDOWN || e.type == SDL_CONTROLLERBUTTONDOWN) { running = false; exitLoop = true; break; }
        }
        if (exitLoop) return;
        SDL_Delay(16);
    }
}

void MenuApplication::cleanup() {
    UiUtils::ShutdownFonts();
    font = nullptr;
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    if (menuSystem) delete menuSystem;
    Mix_CloseAudio();
}

void MenuApplication::initController() {
    controller = nullptr;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            controller = SDL_GameControllerOpen(i);
            if (controller) break;
        }
    }
}

void MenuApplication::setupScreens() {
    auto carousel = std::make_shared<CarouselMenuScreen>("Home");
    carousel->setSubtitle("Plunder  v" + std::string(PLUNDER_VERSION));
    carousel->setShowBackHint(false);
    auto loadRomSite = [this](size_t siteIdx) {
        if (siteIdx >= siteRegistry.size()) return;
        currentSiteIndex = (int)siteIdx;
        currentSite = siteRegistry[siteIdx].id;
        showNoInternetAndExit();
        if (!running) return; // user chose to exit
    menuSystem->pushScreen(std::make_shared<LoadingScreen>("Loading consoles from " + siteRegistry[siteIdx].id));
        fetchConsolesAsync();
    };
    carousel->addItem("Rom Sites", "", [this, loadRomSite]() {
        auto romSites = std::make_shared<CarouselMenuScreen>("ROM Sites");
        romSites->setSubtitle("Choose a source");
        for (size_t i = 0; i < siteRegistry.size(); ++i) {
            const auto& sd = siteRegistry[i];
            romSites->addItem(sd.id, "", [this, loadRomSite, i]() { loadRomSite(i); });
        }
        menuSystem->pushScreen(romSites);
    });
    carousel->addItem("Patch Notes", "", [this]() {
        auto patchScreen = std::make_shared<PatchNotesScreen>();
        menuSystem->pushScreen(patchScreen);
        fetchPatchNotesAsync(patchScreen);
    });
    carousel->addItem("Settings", "", [this]() {
        auto showThemes = [this]() {
            menuSystem->pushScreen(std::make_shared<ThemesScreen>([this]() { menuSystem->popScreen(); }));
        };
        menuSystem->pushScreen(std::make_shared<SettingsScreen>(
            [this]() { menuSystem->popScreen(); },
            showThemes
        ));
    });
    carousel->addItem("Exit", "", [this]() { running = false; });
    menuSystem->pushScreen(carousel);
}