#include "include/GameDetailsScreen.h"
#include "../../utils/include/Theme.h"

GameDetailsScreen::GameDetailsScreen(const GameDetails& details, SDL_Texture* iconTexture)
    : details(details), iconTexture(iconTexture) {
    // Initialize SDL_mixer for cancel sound
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) == 0) {
        mixerInitialized = true;
        Mix_AllocateChannels(16);
        cancelSound = Mix_LoadWAV("sounds/cancel.wav");
        if (cancelSound) Mix_VolumeChunk(cancelSound, MIX_MAX_VOLUME);
        if (!cancelSound) {
            printf("[Sound] Failed to load cancel sound: %s\n", Mix_GetError());
        }
    } else {
        printf("[Sound] SDL_mixer init failed: %s\n", Mix_GetError());
    }
}

GameDetailsScreen::~GameDetailsScreen() {
    if (cancelSound) {
        Mix_FreeChunk(cancelSound);
        cancelSound = nullptr;
    }
    if (mixerInitialized) {
        Mix_CloseAudio();
        mixerInitialized = false;
    }
}

void DrawAnimatedLBorder(SDL_Renderer* renderer, const SDL_Rect& rect, float progress, SDL_Color color, int thickness = 4, int lLen = 24) {
    // progress: 0.0 = L corners, 1.0 = full border
    int x0 = rect.x, y0 = rect.y, x1 = rect.x + rect.w, y1 = rect.y + rect.h;
    int t = thickness;
    // Animate lengths
    int hLen = int(lLen + (rect.w - lLen) * progress); // horizontal
    int vLen = int(lLen + (rect.h - lLen) * progress); // vertical
    // Lower left
    SDL_Rect vertLL = {x0, y1 - vLen, t, vLen};
    SDL_Rect horzLL = {x0, y1 - t, hLen, t};
    // Upper right
    SDL_Rect vertUR = {x1 - t, y0, t, vLen};
    SDL_Rect horzUR = {x1 - hLen, y0, hLen, t};
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &vertLL);
    SDL_RenderFillRect(renderer, &horzLL);
    SDL_RenderFillRect(renderer, &vertUR);
    SDL_RenderFillRect(renderer, &horzUR);
    // If progress == 1, draw full border
    if (progress > 0.99f) {
        SDL_Rect top = {x0, y0, rect.w, t};
        SDL_Rect bot = {x0, y1 - t, rect.w, t};
        SDL_Rect left = {x0, y0, t, rect.h};
        SDL_Rect right = {x1 - t, y0, t, rect.h};
        SDL_RenderFillRect(renderer, &top);
        SDL_RenderFillRect(renderer, &bot);
        SDL_RenderFillRect(renderer, &left);
        SDL_RenderFillRect(renderer, &right);
    }
}

// Helper for restoring music volume after cancel sound
static void RestoreMusicVolume() {
    Mix_VolumeMusic(MIX_MAX_VOLUME);
}

// Helper: modal for choosing a download version, or for a result/error message.
static void RenderDownloadPopup(SDL_Renderer* renderer, TTF_Font* font, const std::vector<DownloadOption>& options, int selected, int scrollOffset, const std::string& message) {
    (void)font;
    using namespace UiUtils;
    const ThemeColors& t = Theme::getInstance().colors();
    DrawScrim(renderer);
    const int w = 760;
    const int x = (SCREEN_W - w) / 2;
    if (!options.empty()) {
        const int itemH = 50, maxVisible = 7;
        int visible = std::min(maxVisible, (int)options.size());
        int h = 96 + visible * itemH + 28;
        int y = (SCREEN_H - h) / 2 - 10;
        DrawPanel(renderer, SDL_Rect{x, y, w, h}, 24);
        DrawText(renderer, GetFont(24, true), "Choose a version", x + 32, y + 28, t.textPrimary);
        DrawText(renderer, GetFont(16), std::to_string(options.size()) + " available", x + 32, y + 62, t.textSecondary);
        int listY = y + 96;
        int end = std::min(scrollOffset + visible, (int)options.size());
        for (int i = scrollOffset; i < end; ++i) {
            SDL_Rect row{x + 20, listY + (i - scrollOffset) * itemH, w - 40 - 16, itemH - 6};
            bool sel = (i == selected);
            if (sel) {
                FillRoundedRect(renderer, row, 12, WithAlpha(t.accentPrimary, 46));
                FillRoundedRect(renderer, SDL_Rect{row.x + 8, row.y + 12, 4, row.h - 24}, 2, t.accentPrimary);
            }
            TTF_Font* f = GetFont(18, sel);
            DrawText(renderer, f, Ellipsize(f, options[i].label, row.w - 40), row.x + 24, row.y + (row.h - TextHeight(f)) / 2, sel ? t.textPrimary : t.textSecondary);
        }
        if ((int)options.size() > visible) {
            float pos = (float)scrollOffset / std::max(1, (int)options.size() - visible);
            DrawScrollbar(renderer, x + w - 26, listY, visible * itemH - 6, (float)visible / options.size(), pos);
        }
        DrawHints(renderer, {{"DPAD", "Choose"}, {"A", "Download"}, {"B", "Cancel"}});
        return;
    }
    // Message dialog
    std::string lower = message;
    for (auto& ch : lower) ch = (char)std::tolower((unsigned char)ch);
    bool isError = lower.find("fail") != std::string::npos || lower.find("error") != std::string::npos ||
                   lower.find("no download") != std::string::npos || lower.find("cancel") != std::string::npos;
    TTF_Font* bodyFont = GetFont(18);
    std::vector<std::string> lines = WrapText(bodyFont, message, w - 64);
    if (lines.size() > 6) lines.resize(6);
    int h = 150 + (int)lines.size() * 26;
    int y = (SCREEN_H - h) / 2 - 10;
    DrawPanel(renderer, SDL_Rect{x, y, w, h}, 24);
    SDL_Color tone = isError ? SDL_Color{239, 93, 93, 255} : t.accentPrimary;
    FillCircle(renderer, x + 50, y + 50, 18, WithAlpha(tone, 50));
    FillCircle(renderer, x + 50, y + 50, 7, tone);
    DrawText(renderer, GetFont(24, true), isError ? "Something went wrong" : "Download", x + 82, y + 34, t.textPrimary);
    int ty = y + 92;
    for (const auto& l : lines) { DrawText(renderer, bodyFont, l, x + 32, ty, t.textSecondary); ty += 26; }
    DrawHints(renderer, {{"A", "OK"}});
}

void GameDetailsScreen::render(SDL_Renderer* renderer, TTF_Font* font) {
    (void)font;
    using namespace UiUtils;
    const ThemeColors& t = Theme::getInstance().colors();
    DrawBackground(renderer);
    const std::string consoleLabel = StripTrailingCount(details.consoleName);
    DrawHeader(renderer, details.title.empty() ? "Game" : details.title, consoleLabel);

    const int top = HEADER_H + 12;
    const int bottom = SCREEN_H - FOOTER_H - 8;

    // ---- Left column: cover + action ----------------------------------------------
    const int colX = MARGIN_X, colW = 340, inset = 14;
    int coverW = colW - 2 * inset, coverH = 0;
    bool hasCover = false;
    if (iconTexture) {
        int tw = 0, th = 0;
        SDL_QueryTexture(iconTexture, nullptr, nullptr, &tw, &th);
        if (tw > 0 && th > 0) { hasCover = true; coverH = std::min(300, th * coverW / tw); }
    }
    if (!hasCover) coverH = 180;
    const bool downloading = downloadManager.isDownloading();
    const int actionH = downloading ? 132 : 56;
    SDL_Rect panel{colX, top, colW, inset + coverH + 16 + actionH + inset};
    DrawShadow(renderer, panel, 22, 26, 120, 10);
    FillRoundedRect(renderer, panel, 22, t.cardBackground);
    StrokeRoundedRect(renderer, panel, 22, 1, WithAlpha(t.textPrimary, 18));

    SDL_Rect cover{colX + inset, top + inset, coverW, coverH};
    if (hasCover) {
        int tw = 0, th = 0;
        SDL_QueryTexture(iconTexture, nullptr, nullptr, &tw, &th);
        SDL_Rect src{0, 0, tw, th};
        float boxAspect = (float)cover.w / cover.h, texAspect = (float)tw / th;
        if (texAspect > boxAspect) { int cw = (int)(th * boxAspect); src.x = (tw - cw) / 2; src.w = cw; }
        else { int ch = (int)(tw / boxAspect); src.y = (th - ch) / 2; src.h = ch; }
        SDL_SetTextureAlphaMod(iconTexture, 255);
        SDL_RenderCopy(renderer, iconTexture, &src, &cover);
    } else {
        FillRoundedRect(renderer, cover, 14, Lighten(t.cardBackground, 0.05f));
        RenderTextCentered(renderer, GetFont(16), "No cover art", cover.x + cover.w / 2, cover.y + cover.h / 2, Color(t.textSecondary));
    }
    MaskCorners(renderer, cover, 14, t.cardBackground);

    const int actionY = cover.y + cover.h + 16;
    const SDL_Rect actionArea{colX + inset, actionY, coverW, actionH};
    if (!downloading) {
        bool available = !details.downloadUrl.empty();
        SDL_Rect btn = actionArea;
        if (buttonFocused) {
            DrawShadow(renderer, btn, 14, 18, 120, 6);
            FillRoundedRect(renderer, btn, 14, available ? t.accentPrimary : Lighten(t.cardBackground, 0.12f));
            DrawFocusRing(renderer, btn, 14, t.accentPrimary, 3, 3);
        } else {
            FillRoundedRect(renderer, btn, 14, available ? WithAlpha(t.accentPrimary, 40) : Lighten(t.cardBackground, 0.08f));
            StrokeRoundedRect(renderer, btn, 14, 2, available ? WithAlpha(t.accentPrimary, 140) : WithAlpha(t.textSecondary, 60));
        }
        SDL_Color fg = !available ? t.textSecondary : (buttonFocused ? t.backgroundDark : t.textPrimary);
        RenderTextCenteredInBox(renderer, GetFont(19, true), available ? "Download" : "Unavailable", btn, Color(fg));
    } else {
        std::string progressText = downloadManager.getProgressText();
        bool unzipping = progressText.find("Unzipping") != std::string::npos;
        float progress = -1.0f;
        std::string pct;
        if (downloadManager.getTotalBytes() > 0) {
            progress = std::min(1.0f, float(downloadManager.getCurrentBytes()) / float(downloadManager.getTotalBytes()));
            pct = std::to_string((int)(progress * 100)) + "%";
        }
        if (unzipping) progress = -1.0f;
        TTF_Font* lf = GetFont(17, true);
        DrawText(renderer, lf, unzipping ? "Unzipping…" : "Downloading…", actionArea.x, actionArea.y, t.textPrimary);
        if (!pct.empty() && !unzipping) {
            int pw = TextWidth(lf, pct);
            DrawText(renderer, lf, pct, actionArea.x + actionArea.w - pw, actionArea.y, t.accentPrimary);
        }
        SDL_Rect bar{actionArea.x, actionArea.y + 30, actionArea.w, 10};
        DrawProgressBar(renderer, bar, progress);
        TTF_Font* sf = GetFont(14);
        DrawText(renderer, sf, Ellipsize(sf, progressText, actionArea.w), actionArea.x, bar.y + 18, t.textSecondary);
        SDL_Rect cancel{actionArea.x, actionArea.y + actionArea.h - 44, actionArea.w, 44};
        bool cancelFocused = downloadManager.getButtonFocus() == 1;
        SDL_Color red{239, 93, 93, 255};
        FillRoundedRect(renderer, cancel, 12, cancelFocused ? red : WithAlpha(red, 36));
        if (cancelFocused) DrawFocusRing(renderer, cancel, 12, red, 3, 3);
        RenderTextCenteredInBox(renderer, GetFont(17, true), "Cancel", cancel, Color(cancelFocused ? SDL_Color{255, 255, 255, 255} : red));
    }

    // ---- Right column: facts + about ------------------------------------------------
    const int rx = colX + colW + 36;
    const int rw = SCREEN_W - MARGIN_X - rx;
    int y = top;
    std::vector<std::pair<std::string, std::string>> facts;
    auto add = [&facts](const char* label, const std::string& v) { if (!v.empty()) facts.push_back({label, v}); };
    add("CONSOLE", consoleLabel);
    add("REGION", details.language);
    add("SIZE", details.fileSize);
    add("RELEASED", details.releaseDate);
    add("DOWNLOADS", details.downloads);
    add("GENRE", details.genre);
    add("PUBLISHER", details.publisher);
    if (!facts.empty()) {
        const int cols = 3, cellH = 66, gap = 12;
        const int cellW = (rw - (cols - 1) * gap) / cols;
        TTF_Font* lf = GetFont(13, true);
        TTF_Font* vf = GetFont(19, true);
        for (size_t i = 0; i < facts.size(); ++i) {
            SDL_Rect cell{rx + (int)(i % cols) * (cellW + gap), y + (int)(i / cols) * (cellH + gap), cellW, cellH};
            FillRoundedRect(renderer, cell, 14, WithAlpha(t.cardBackground, 200));
            DrawText(renderer, lf, facts[i].first, cell.x + 16, cell.y + 12, t.textSecondary);
            DrawText(renderer, vf, Ellipsize(vf, facts[i].second, cell.w - 32), cell.x + 16, cell.y + 32, t.textPrimary);
        }
        y += (int)((facts.size() + cols - 1) / cols) * (cellH + gap) + 10;
    }

    bool aboutHasText = std::any_of(details.about.begin(), details.about.end(), [](unsigned char c) { return !std::isspace(c); });
    TTF_Font* bodyFont = GetFont(18);
    const int lineH = 27;
    SDL_Rect aboutBox{rx, y, rw, bottom - y};
    if (aboutHasText && aboutBox.h > 80) {
        DrawText(renderer, GetFont(20, true), "About", rx, y, t.textPrimary);
        aboutBox.y += 36; aboutBox.h -= 36;
        const int padX = 20, padY = 14;
        int textW = aboutBox.w - 2 * padX - 14;
        if (aboutWrapWidth != textW) {
            aboutWrapped = WrapText(bodyFont, details.about, textW);
            while (!aboutWrapped.empty() && aboutWrapped.back().empty()) aboutWrapped.erase(aboutWrapped.end() - 1);
            while (!aboutWrapped.empty() && aboutWrapped.front().empty()) aboutWrapped.erase(aboutWrapped.begin());
            aboutWrapWidth = textW;
        }
        aboutBox.h = std::min(aboutBox.h, (int)aboutWrapped.size() * lineH + 2 * padY);  // shrink to fit short text
        FillRoundedRect(renderer, aboutBox, 16, WithAlpha(t.cardBackground, 160));
        int maxVisible = std::max(1, (aboutBox.h - 2 * padY) / lineH);
        aboutMaxVisibleLines = maxVisible;
        aboutTotalLines = (int)aboutWrapped.size();
        int maxOffset = std::max(0, aboutTotalLines - maxVisible);
        aboutScrollOffset = std::max(0, std::min(aboutScrollOffset, maxOffset));
        int end = std::min(aboutTotalLines, aboutScrollOffset + maxVisible);
        for (int i = aboutScrollOffset; i < end; ++i)
            DrawText(renderer, bodyFont, aboutWrapped[i], aboutBox.x + padX, aboutBox.y + padY + (i - aboutScrollOffset) * lineH, Mix(t.textSecondary, t.textPrimary, 0.55f));
        if (aboutTotalLines > maxVisible)
            DrawScrollbar(renderer, aboutBox.x + aboutBox.w - 16, aboutBox.y + padY, maxVisible * lineH, (float)maxVisible / aboutTotalLines, (float)aboutScrollOffset / maxOffset);
    } else {
        aboutMaxVisibleLines = 0;
        aboutTotalLines = 0;
    }

    // ---- Footer (the popup draws its own hints) ---------------------------------------
    if (showDownloadPopup) {
        // skip
    } else if (downloading) {
        DrawHints(renderer, {{"DPAD", "Focus"}, {"A", "Confirm"}, {"B", "Back"}});
    } else if (buttonFocused) {
        DrawHints(renderer, {{"DPAD", "Read about"}, {"A", "Download"}, {"B", "Back"}});
    } else {
        DrawHints(renderer, {{"DPAD", "Scroll, down to Download"}, {"B", "Back"}});
    }

    if (showDownloadPopup) {
        int maxVisible = 7;
        int total = (int)downloadOptions.size();
        int scrollOffset = downloadPopupScrollOffset;
        if (scrollOffset > total - maxVisible) scrollOffset = std::max(0, total - maxVisible);
        if (scrollOffset < 0) scrollOffset = 0;
        RenderDownloadPopup(renderer, font, downloadOptions, selectedDownloadOption, scrollOffset, downloadPopupMessage);
    }
}

void GameDetailsScreen::handleInput(const SDL_Event& e, MenuSystem& menuSystem) {
    // --- Download/cancel button navigation always takes priority when downloading ---
    if (downloadManager.isDownloading()) {
        if (e.type == SDL_KEYDOWN || e.type == SDL_CONTROLLERBUTTONDOWN) {
            // Left/right to move focus
            if ((e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_LEFT) ||
                (e.type == SDL_CONTROLLERBUTTONDOWN && e.cbutton.button == BUTTON_DPAD_LEFT)) {
                int focus = downloadManager.getButtonFocus();
                if (focus > 0) downloadManager.setButtonFocus(focus - 1);
                return;
            }
            if ((e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_RIGHT) ||
                (e.type == SDL_CONTROLLERBUTTONDOWN && e.cbutton.button == BUTTON_DPAD_RIGHT)) {
                int focus = downloadManager.getButtonFocus();
                if (focus < 1) downloadManager.setButtonFocus(focus + 1);
                return;
            }
            // A or B or X on Cancel
            if (downloadManager.getButtonFocus() == 1 &&
                ((e.type == SDL_KEYDOWN && (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_SPACE || e.key.keysym.sym == SDLK_x)) ||
                 (e.type == SDL_CONTROLLERBUTTONDOWN && (e.cbutton.button == BUTTON_A || e.cbutton.button == BUTTON_B || e.cbutton.button == BUTTON_X)))) {
                if (cancelSound) {
                    Mix_PlayChannel(-1, cancelSound, 0);
                }
                downloadManager.cancelDownload();
                return;
            }
        }
        // Block all other input while downloading
        return;
    }
    // --- MODAL HANDLING: Always first! ---
    if (showDownloadPopup) {
        int maxVisible = 7;  // must match RenderDownloadPopup
        int total = downloadOptions.size();
        // D-pad navigation
        if (e.type == SDL_KEYDOWN || e.type == SDL_CONTROLLERBUTTONDOWN) {
            // B = back/close popup (now B closes modal, does NOT pop screen)
            if ((e.type == SDL_KEYDOWN && (e.key.keysym.sym == SDLK_SPACE)) ||
                (e.type == SDL_CONTROLLERBUTTONDOWN && e.cbutton.button == BUTTON_B)) {
                showDownloadPopup = false;
                return;
            }
            // A = select/download
            if ((e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_RETURN) ||
                (e.type == SDL_CONTROLLERBUTTONDOWN && e.cbutton.button == BUTTON_A)) {
                if (!downloadOptions.empty()) {
                    downloadPopupMessage = "Fetching download link...";
                    std::string versionUrl = downloadOptions[selectedDownloadOption].url;
                    std::thread([this, versionUrl]() {
                        std::string finalLink = DownloadManager::scrapeFinalDownloadLink(versionUrl);
                        downloadPopupMessage = "Download link: " + finalLink;
                    }).detach();
                }
                return;
            }
            // Down
            if ((e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_DOWN) ||
                (e.type == SDL_CONTROLLERBUTTONDOWN && e.cbutton.button == BUTTON_DPAD_DOWN)) {
                if (selectedDownloadOption + 1 < total) {
                    selectedDownloadOption++;
                    if (selectedDownloadOption >= downloadPopupScrollOffset + maxVisible) {
                        downloadPopupScrollOffset = std::min(selectedDownloadOption, total - maxVisible);
                    }
                }
                return;
            }
            // Up
            if ((e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_UP) ||
                (e.type == SDL_CONTROLLERBUTTONDOWN && e.cbutton.button == BUTTON_DPAD_UP)) {
                if (selectedDownloadOption > 0) {
                    selectedDownloadOption--;
                    if (selectedDownloadOption < downloadPopupScrollOffset) {
                        downloadPopupScrollOffset = selectedDownloadOption;
                    }
                }
                return;
            }
        }
        // Analog stick navigation
        if (e.type == SDL_CONTROLLERAXISMOTION && e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
            int value = e.caxis.value;
            if (abs(value) > 8000) {
                Uint32 now = SDL_GetTicks();
                if (downloadPopupLastAxisValue == 0 || now - downloadPopupLastAxisTime > 120) {
                    if (value > 0 && selectedDownloadOption + 1 < total) {
                        selectedDownloadOption++;
                        if (selectedDownloadOption >= downloadPopupScrollOffset + maxVisible) {
                            downloadPopupScrollOffset = std::min(selectedDownloadOption, total - maxVisible);
                        }
                    } else if (value < 0 && selectedDownloadOption > 0) {
                        selectedDownloadOption--;
                        if (selectedDownloadOption < downloadPopupScrollOffset) {
                            downloadPopupScrollOffset = selectedDownloadOption;
                        }
                    }
                    downloadPopupLastAxisTime = now;
                }
                downloadPopupLastAxisValue = value;
            } else {
                downloadPopupLastAxisValue = 0;
            }
            return;
        }
        // Block all other input from reaching the main screen while popup is open
        return;
    }
    // Use a fixed average character width for line wrapping in handleInput
    int aboutAreaWidth = 900;
    int avgCharWidth = 12; // Approximate width for 20pt font
    int maxCharsPerLine = aboutAreaWidth / avgCharWidth;
    // Prepare aboutLines for scrolling bounds
    std::vector<std::string> aboutLines;
    {
        std::istringstream aboutStream(details.about);
        std::string paragraph;
        while (std::getline(aboutStream, paragraph)) {
            std::istringstream wordStream(paragraph);
            std::string word, line;
            while (wordStream >> word) {
                std::string testLine = line.empty() ? word : line + " " + word;
                if ((int)testLine.size() > maxCharsPerLine && !line.empty()) {
                    aboutLines.push_back(line);
                    line = word;
                } else {
                    line = testLine;
                }
            }
            if (!line.empty()) aboutLines.push_back(line);
        }
    }
    int totalLines = aboutTotalLines;  // measured in render() with the real font
    (void)aboutLines;
    if (e.type == SDL_CONTROLLERBUTTONDOWN) {
        // Back/close (now B is back)
        if (e.cbutton.button == BUTTON_B) { // B = back
            menuSystem.popScreen();
        }
        // Activate download button (A = select)
        if (buttonFocused && e.cbutton.button == BUTTON_A) {
            if (downloadManager.isDownloading()) return; // Guard
            const std::string url = details.downloadUrl; // Should already be a direct link supplied by scraper
            if (url.empty()) {
                showDownloadPopup = true;
                downloadPopupMessage = "No download link available.";
                return;
            }
            // Debug planned destination BEFORE starting download
            if (!details.mappedFolder.empty()) {
                printf("[Download] Will attempt move to mapped folder '%s' after completion.\n", details.mappedFolder.c_str());
            } else {
                printf("[Download] No mapped folder stored; file will be deleted after download per policy.\n");
            }
            // Derive filename from URL
            std::string filename = "download.bin";
            size_t slash = url.find_last_of('/');
            if (slash != std::string::npos && slash + 1 < url.size()) filename = url.substr(slash + 1);
            size_t q = filename.find('?'); if (q != std::string::npos) filename = filename.substr(0, q);
            if (filename.empty()) filename = "download.bin";
            mkdir("downloads", 0755);
            std::string downloadOutPath = std::string("downloads/") + filename;
            
            // Start download using DownloadManager
            downloadManager.startDownload(url, downloadOutPath, details);
            
            // Setup callback to show completion popup
            std::thread([this]() {
                while (downloadManager.isDownloading()) {
                    SDL_Delay(500); // Check every 500ms
                }
                downloadPopupMessage = downloadManager.getProgressText();
                showDownloadPopup = true;
            }).detach();
            return;
        }
        // Focus/unfocus download button
        if (e.cbutton.button == BUTTON_DPAD_DOWN) {
            if (!buttonFocused) { buttonFocused = true; buttonFocusStart = SDL_GetTicks(); }
        } else if (e.cbutton.button == BUTTON_DPAD_UP) {
            if (buttonFocused) { buttonFocused = false; }
        }
        // Scroll about text (only if not focused on button)
        if (!buttonFocused) {
            if (e.cbutton.button == BUTTON_DPAD_DOWN) {
                if (aboutScrollOffset + aboutMaxVisibleLines < totalLines) {
                    aboutScrollOffset++;
                }
            }
            if (e.cbutton.button == BUTTON_DPAD_UP) {
                if (aboutScrollOffset > 0) {
                    aboutScrollOffset--;
                }
            }
        }
    } else if (e.type == SDL_CONTROLLERAXISMOTION) {
        if (!buttonFocused && e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
            int value = e.caxis.value;
            const int DEADZONE = 8000;
            static Uint32 lastAxisTime = 0;
            static int lastAxisValue = 0;
            Uint32 now = SDL_GetTicks();
            if (abs(value) > DEADZONE) {
                if (lastAxisValue == 0 || now - lastAxisTime > 120) {
                    if (value > 0 && aboutScrollOffset + aboutMaxVisibleLines < totalLines) {
                        aboutScrollOffset++;
                    } else if (value < 0 && aboutScrollOffset > 0) {
                        aboutScrollOffset--;
                    }
                    lastAxisTime = now;
                }
                lastAxisValue = value;
            } else {
                lastAxisValue = 0;
            }
        }
    }
}