#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <dirent.h>
#include <fstream>
#include <future>
#include <iostream>
#include <limits.h>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

// SDL
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>


// Other imports
#include "../../Screen.h"
#include "../../../model/GameDetails.h"
#include "../../menuApp/include/MenuSystem.h"
#include "../../consolePolicies/ConsoleFolderMap.h"
#include "../../consolePolicies/ConsoleZipPolicy.h"
#include "../../../utils/include/UiUtils.h"
#include "../../../utils/include/HttpUtils.h"
#include "../../../utils/include/StringUtils.h"
#include "../../ControllerButtons.h"

/// ========================================================== ///
class MenuSystem;

struct DownloadOption {
    std::string label;
    std::string url;
};

class GameDetailsScreen : public Screen {
    public:
        GameDetailsScreen(const GameDetails& details, SDL_Texture* iconTexture = nullptr);
        ~GameDetailsScreen();

        void render(SDL_Renderer* renderer, TTF_Font* font) override;
        void handleInput(const SDL_Event& e, MenuSystem& menuSystem) override;

    private:
        GameDetails details;
        SDL_Texture* iconTexture = nullptr;
        int aboutScrollOffset = 0;
        int aboutMaxVisibleLines = 0;
        bool buttonFocused = false; // True if download button is focused
        Uint32 buttonFocusStart = 0; // For animation timing
        float buttonBorderAnim = 0.0f; // 0.0 = corners, 1.0 = full border
        Uint32 lastAnimTime = 0; // For smooth animation
        // Download popup state
        bool showDownloadPopup = false;
        std::vector<DownloadOption> downloadOptions;
        int selectedDownloadOption = 0;
        std::string downloadPopupMessage;
        // Download popup scroll state
        int downloadPopupScrollOffset = 0;
        int downloadPopupMaxVisible = 0;
        int downloadPopupLastAxisValue = 0;
        Uint32 downloadPopupLastAxisTime = 0;
        // Download progress state
        long downloadCurrentBytes = 0;
        long downloadTotalBytes = 0;
        bool downloadInProgress = false;
        bool downloadCancelRequested = false;
        std::string downloadProgressText;
    pid_t downloadPid = -1; // process id of curl
        // 0 = progress bar, 1 = cancel button
        int downloadButtonFocus = 0;
        // Store the current download output file path for cancel logic
        std::string downloadOutPath;
        Mix_Chunk* cancelSound = nullptr;
        Mix_Music* cancelMusic = nullptr;
        bool mixerInitialized = false;
    public:
        void setAboutScrollOffset(int offset) { aboutScrollOffset = offset; }
        int getAboutScrollOffset() const { return aboutScrollOffset; }
};