#pragma once

#include "MenuSystem.h"
#include "../../Screen.h"
#include "MenuScreen.h"
#include "../../carousel/include/CarouselMenuScreen.h"
#include "../../screenListing/include/ListScreen.h"
#include "../../loadingScreen/include/LoadingScreen.h"
#include "../../gameDetailsScreen/include/GameDetailsScreen.h"
#include "../../settingsScreen/include/SettingsScreen.h"
#include "../../patchNotesScreen/include/PatchNotesScreen.h"
#include "../../consolePolicies/GameListData.h"
#include "../../../utils/include/UiUtils.h"
#include "../../../utils/include/HttpUtils.h"
#include "../../../scraper/SiteScraper.h"
#include "../../../scraper/Hexrom/include/HexromScraper.h"
#include "../../../scraper/Gamulator/include/GamulatorScraper.h"
#include "../../../scraper/GithubReleases/include/GitHubReleasesScraper.h"
#include "../../../scraper/Romspedia/include/RomspediaScraper.h"
#include "../../consolePolicies/ConsoleFolderMap.h"

// SDL
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>

// STL
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <iostream>
#include <utility>

// Descriptor for a ROM site (scraper + metadata)
struct SiteDescriptor {
    std::string id;        // stable id & display label
    std::shared_ptr<SiteScraper> scraper;
};

class MenuApplication {
private:
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    TTF_Font* font = nullptr;
    SDL_GameController* controller = nullptr;
    MenuSystem* menuSystem = nullptr;
    bool running = false;
    std::vector<SiteDescriptor> siteRegistry;
    int currentSiteIndex = -1; // index into siteRegistry
    std::string currentSite;   // cached convenience (display or id)
public:
    MenuApplication();
    ~MenuApplication();
    bool initialize();
    void run();
private:
    void cleanup();
    void initController();
    void setupScreens();
    void buildSiteRegistry();
    std::shared_ptr<SiteScraper> currentScraper() const;
    int findSiteIndexById(const std::string& id) const;
    void processEvent(const SDL_Event& e);
    void handleConsolesLoaded(std::vector<ListItem>* consolesPtr);
    void handleGamesLoaded(GameListData* gameData);
    void handleGameDetailsLoaded(struct DetailsWithTexture* detailsWithTex);
    void handlePatchNotesLoaded(std::vector<std::string>* linesPtr);
    void updateCurrentScreenAnimations(bool& needsRedraw);
    void fetchConsolesAsync();
    void fetchGamesAsync(const std::string& baseUrl, int page, bool showLoadingScreen, const std::string& consoleName = "");
    void fetchGameDetailsAsync(const ListItem& game, SDL_Texture* iconTexture, const std::string& consoleName);
    static void fetchPatchNotesAsync(std::shared_ptr<PatchNotesScreen> patchScreen);
    void showNoInternetAndExit();
};