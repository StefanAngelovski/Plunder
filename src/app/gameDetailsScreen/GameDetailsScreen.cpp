#include "include/GameDetailsScreen.h"

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

// Helper: Draw popup overlay with scrolling and scrollbar
static void RenderDownloadPopup(SDL_Renderer* renderer, TTF_Font* font, const std::vector<DownloadOption>& options, int selected, int scrollOffset, const std::string& message) {
    int w = 1100;
    int x = 1280/2 - w/2;
    int y = 0, h = 0;
    if (!options.empty()) {
        // Standard modal for options
        h = 600;
        y = 720/2 - h/2;
    } else {
        // Dynamic height for confirmation/error
        int msgMaxWidth = w - 120;
        int textW = 0, textH = 0;
        TTF_SizeText(font, message.c_str(), &textW, &textH);
        // Estimate wrapped height: TTF_RenderText_Blended_Wrapped wraps at msgMaxWidth
        int lines = 1;
        if (textW > msgMaxWidth && msgMaxWidth > 0) lines = (textW + msgMaxWidth - 1) / msgMaxWidth;
        int lineH = textH > 0 ? textH : 36;
        h = 60 + lines * lineH + 60; // top/bottom padding + text
        if (h < 180) h = 180; // minimum height
        y = 720/2 - h/2;
    }
    SDL_SetRenderDrawColor(renderer, 20, 20, 40, 250); // Increased alpha from 230 to 250
    SDL_Rect bg = {x, y, w, h};
    SDL_RenderFillRect(renderer, &bg);
    if (!options.empty()) {
    UiUtils::RenderTextCentered(renderer, font, "Select Download Version", x + w/2, y + 40, UiUtils::Color(255,255,255));
        int listX = x + 60;
        int listY = y + 90;
        int listW = w - 180;
        int itemH = 44;
        int maxVisible = (h - 180) / itemH;
        int total = options.size();
        int start = scrollOffset;
        int end = std::min(start + maxVisible, total);
        for (int i = start; i < end; ++i) {
            UiUtils::Color c = (i == selected) ? UiUtils::Color(0,255,255) : UiUtils::Color(220,220,220);
            UiUtils::RenderText(renderer, font, options[i].label, listX, listY + (i - start)*itemH, c);
        }
        // Draw scrollbar if needed
        if (total > maxVisible) {
            int barX = x + w - 60;
            int barY = listY;
            int barW = 12;
            int barH = maxVisible * itemH;
            float thumbH = (float)maxVisible / total * barH;
            float thumbY = barY + ((float)scrollOffset / (total - maxVisible)) * (barH - thumbH);
            SDL_SetRenderDrawColor(renderer, 80, 80, 100, 100);
            SDL_Rect bgRect = {barX, barY, barW, barH};
            SDL_RenderFillRect(renderer, &bgRect);
            SDL_SetRenderDrawColor(renderer, 220, 220, 80, 160);
            SDL_Rect thumbRect = {barX, (int)thumbY, barW, (int)thumbH};
            SDL_RenderFillRect(renderer, &thumbRect);
        }
    }
    // For confirmation/error, center and wrap the message
    if (!message.empty()) {
        int msgMaxWidth = w - 120;
        int msgY = y + h/2 - 40;
    UiUtils::RenderTextWrapped(renderer, font, message, x + 60, msgY, msgMaxWidth, UiUtils::Color(255,180,180));
    }
}

void GameDetailsScreen::render(SDL_Renderer* renderer, TTF_Font* font) {
    SDL_SetRenderDrawColor(renderer, 0, 150, 150, 255); // Teal blue color
    SDL_RenderClear(renderer);
    
    int x = 60, y = 40;
    // Precompute if About text has any non-whitespace so we can adjust layout & image size
    bool aboutHasText = std::any_of(details.about.begin(), details.about.end(), [](unsigned char c){ return !std::isspace(c); });
    // Icon (retain aspect ratio)
    int iconW = 180, iconH = 180; // default fallback
    if (!aboutHasText) {
        iconW *= 2; // double width when no About section
    }
    // Check if texture is valid and get dimensions
    bool hasValidTexture = false;
    if (iconTexture) {
        int texW = 0, texH = 0;
        SDL_QueryTexture(iconTexture, NULL, NULL, &texW, &texH);
        if (texW > 0 && texH > 0) {
            hasValidTexture = true;
            iconH = texH * iconW / texW;
        }
    }
    // Only create icon rect and render if we have a valid texture
    if (hasValidTexture) {
        SDL_Rect iconRect = {x, y, iconW, iconH};
        SDL_RenderCopy(renderer, iconTexture, NULL, &iconRect);
    }
    
    // Adjust layout based on whether we have a valid image
    int fieldsXRight = hasValidTexture ? x + 200 : x; // right-of-image column (or just x if no image)
    int currentYRight = y;      // y tracker for right column
    int belowImageStartY = hasValidTexture ? y + iconH + 20 : y; // start y for below-image layout (or just y if no image)

    if (aboutHasText) {
        // Title to right of image
        UiUtils::RenderText(renderer, font, details.title, fieldsXRight, currentYRight, UiUtils::Color(255,255,80), 1.5f);
        currentYRight += 40;
        // Fields in right column
        auto renderField = [&](const std::string &label, const std::string &value, const UiUtils::Color &color){
            UiUtils::RenderText(renderer, font, label + value, fieldsXRight, currentYRight, color);
            currentYRight += 30;
        };
    if (!details.publisher.empty()) renderField("Publisher: ", details.publisher, UiUtils::Color(200,200,200));
    if (!details.genre.empty())     renderField("Genre: ", details.genre, UiUtils::Color(200,200,200));
    if (!details.language.empty())  renderField("Language: ", details.language, UiUtils::Color(180,200,220));
    if (!details.downloads.empty()) renderField("Downloads: ", details.downloads, UiUtils::Color(180,220,180));
    if (!details.releaseDate.empty()) renderField("Release: ", details.releaseDate, UiUtils::Color(220,180,180));
    if (!details.fileSize.empty())  renderField("File Size: ", details.fileSize, UiUtils::Color(220,220,180));
    } else {
        // Title below image (full-width start)
        UiUtils::RenderText(renderer, font, details.title, x, belowImageStartY, UiUtils::Color(255,255,80), 1.5f);
        int curY = belowImageStartY + 40;
        auto renderFieldBelow = [&](const std::string &label, const std::string &value, const UiUtils::Color &color){
            UiUtils::RenderText(renderer, font, label + value, x, curY, color);
            curY += 30;
        };
    if (!details.publisher.empty()) renderFieldBelow("Publisher: ", details.publisher, UiUtils::Color(200,200,200));
    if (!details.genre.empty())     renderFieldBelow("Genre: ", details.genre, UiUtils::Color(200,200,200));
    if (!details.language.empty())  renderFieldBelow("Language: ", details.language, UiUtils::Color(180,200,220));
    if (!details.downloads.empty()) renderFieldBelow("Downloads: ", details.downloads, UiUtils::Color(180,220,180));
    if (!details.releaseDate.empty()) renderFieldBelow("Release: ", details.releaseDate, UiUtils::Color(220,180,180));
    if (!details.fileSize.empty())  renderFieldBelow("File Size: ", details.fileSize, UiUtils::Color(220,220,180));
        // Update y so subsequent positioning uses the maximum of right-column logic (not used now) and below layout
        y = curY; // This ensures button stays consistent if later logic references y
    }

    // About section positioning depends on layout
    int fileSizeBottom = y + 30; // 30px below last field
    int imageBottom = hasValidTexture ? y + iconH + 20 : y; // 20px below image if image exists, otherwise just y
    
    // Adjust aboutY based on whether we have an image and about text
    int aboutY;
    if (hasValidTexture && aboutHasText) {
        // Layout A with image: About goes below both image and right-column fields
        aboutY = std::max(fileSizeBottom, imageBottom);
    } else if (hasValidTexture && !aboutHasText) {
        // Layout B with image: fields are below image, so aboutY not used
        aboutY = std::max(fileSizeBottom, imageBottom);
    } else if (!hasValidTexture && aboutHasText) {
        // Layout A without image: About goes below the fields (which are now on left starting from top)
        aboutY = currentYRight + 20; // 20px below the last field rendered in right column (which is now left column)
    } else {
        // Layout B without image: aboutY not used
        aboutY = y;
    }
    
    int aboutAreaHeight = 650 - (aboutY + 30) - 20; // 20px margin above Download button
    int aboutAreaWidth = 860; // Full width since About always uses full width
    int aboutAreaX = x;
    // Determine if About text has any non-whitespace characters
    // aboutHasText already computed above
    if (aboutHasText) {
        // Render 'About:' label only if there is real content
        UiUtils::RenderText(renderer, font, "About:", aboutAreaX, aboutY, UiUtils::Color(255,255,255));
    } else {
    }
    // Word wrap about text to pixel width (only if content exists)
    std::vector<std::string> aboutLines;
    if (aboutHasText) {
        std::istringstream aboutStream(details.about);
        std::string paragraph;
        while (std::getline(aboutStream, paragraph)) {
            std::istringstream wordStream(paragraph);
            std::string word, line;
            while (wordStream >> word) {
                std::string testLine = line.empty() ? word : line + " " + word;
                int w = 0, h = 0;
                TTF_SizeText(font, testLine.c_str(), &w, &h);
                if (w > aboutAreaWidth && aboutAreaWidth > 0) {
                    aboutLines.push_back(line);
                    line = word;
                } else {
                    line = testLine;
                }
            }
            if (!line.empty()) aboutLines.push_back(line);
        }
    }
    // Render lines with scrolling
    int lineHeight = 0;
    TTF_SizeText(font, "Ag", nullptr, &lineHeight);
    if (lineHeight < 1) lineHeight = 28;
    int maxVisibleLines = aboutAreaHeight / lineHeight;
    aboutMaxVisibleLines = maxVisibleLines;
    int totalLines = aboutLines.size();
    int scrollOffset = aboutScrollOffset;
    if (scrollOffset > totalLines - maxVisibleLines) scrollOffset = std::max(0, totalLines - maxVisibleLines);
    if (scrollOffset < 0) scrollOffset = 0;
    aboutScrollOffset = scrollOffset;
    if (aboutHasText) {
        int visibleEnd = std::min(totalLines, scrollOffset + maxVisibleLines);
        for (int i = scrollOffset; i < visibleEnd; ++i) {
            UiUtils::RenderText(renderer, font, aboutLines[i], aboutAreaX, aboutY + 30 + (i - scrollOffset) * lineHeight, UiUtils::Color(220,220,220));
        }
    }
    // Draw scrollbar
    if (aboutHasText && totalLines > maxVisibleLines) {
        int barX = 1280 - 60; // Move scrollbar all the way to the right edge
        int barY = aboutY + 30;
        int barW = 10;
        int barH = maxVisibleLines * lineHeight; // Only the visible area height
        float thumbH = (float)maxVisibleLines / totalLines * barH;
        float thumbY = barY + ((float)scrollOffset / (totalLines - maxVisibleLines)) * (barH - thumbH);
        SDL_SetRenderDrawColor(renderer, 80, 80, 100, 100);
        SDL_Rect bgRect = {barX, barY, barW, barH};
        SDL_RenderFillRect(renderer, &bgRect);
        SDL_SetRenderDrawColor(renderer, 220, 220, 80, 160);
        SDL_Rect thumbRect = {barX, (int)thumbY, barW, (int)thumbH};
        SDL_RenderFillRect(renderer, &thumbRect);
    }
    // Download button
    SDL_Rect btnRect = {x, 650, 260, 50};
    SDL_SetRenderDrawColor(renderer, 60, 120, 220, 160);
    SDL_RenderFillRect(renderer, &btnRect);
    // Animate border progress (smoother)
    Uint32 now = SDL_GetTicks();
    float target = buttonFocused ? 1.0f : 0.0f;
    float animSpeed = 0.004f * (now - lastAnimTime); // slower for smoother
    if (buttonBorderAnim < target) buttonBorderAnim = std::min(target, buttonBorderAnim + animSpeed);
    if (buttonBorderAnim > target) buttonBorderAnim = std::max(target, buttonBorderAnim - animSpeed);
    lastAnimTime = now;
    SDL_Color cyan = {0, 255, 255, 255};
    DrawAnimatedLBorder(renderer, btnRect, buttonBorderAnim, cyan);
    const char* btnLabel = details.downloadUrl.empty() ? "Unavailable" : "Download now";
    UiUtils::RenderTextCenteredInBox(renderer, font, btnLabel, btnRect, UiUtils::Color(255,255,255));
    if (showDownloadPopup) {
        // Only suppress the modal for HexRom when showing options, not for confirmation
        bool isHexrom = !details.downloadUrl.empty() && details.downloadUrl.find("hexrom.com") != std::string::npos;
        int maxVisible = (600 - 180) / 44;
        int total = downloadOptions.size();
        int scrollOffset = downloadPopupScrollOffset;
        if (scrollOffset > total - maxVisible) scrollOffset = std::max(0, total - maxVisible);
        if (scrollOffset < 0) scrollOffset = 0;
        RenderDownloadPopup(renderer, font, downloadOptions, selectedDownloadOption, scrollOffset, downloadPopupMessage);
    }
    // Draw Downloading... progress bar if in progress
    if (downloadInProgress) {
        int barW = 600, barH = 32;
        int barX = 340, barY = 650;
        SDL_Rect barBg = {barX, barY, barW, barH};
        SDL_SetRenderDrawColor(renderer, 60, 60, 80, 220);
        SDL_RenderFillRect(renderer, &barBg);
        float progress = 0.0f;
        static float indet = 0.0f;
        if (downloadTotalBytes > 0) {
            progress = std::min(1.0f, float(downloadCurrentBytes) / float(downloadTotalBytes));
        } else {
            // Indeterminate: animate bar
            indet += 0.02f; if (indet > 1.0f) indet = 0.0f;
            progress = 0.2f + 0.6f * std::abs(std::sin(indet * 3.14159f));
        }
        SDL_SetRenderDrawColor(renderer, 80, 200, 255, 255);
        SDL_Rect barFill = {barX, barY, int(barW * progress), barH};
        SDL_RenderFillRect(renderer, &barFill);

        // --- Unzipping message logic ---
        // Show "Game is being unzipped" above the loading bar if unzipping is in progress
        static bool wasUnzipping = false;
        static Uint32 unzipMsgTimer = 0;
        bool showUnzipMsg = false;
        // Heuristic: if downloadProgressText contains "Unzipping...", show the message
        if (downloadProgressText.find("Unzipping...") != std::string::npos) {
            showUnzipMsg = true;
            wasUnzipping = true;
            unzipMsgTimer = SDL_GetTicks();
        } else if (wasUnzipping && SDL_GetTicks() - unzipMsgTimer < 2000) {
            // Keep message for 2 seconds after unzipping
            showUnzipMsg = true;
        } else {
            wasUnzipping = false;
        }
        if (showUnzipMsg) {
            UiUtils::RenderText(renderer, font, "Game is being unzipped", barX, barY - 72, UiUtils::Color(255,255,180));
        }

        UiUtils::RenderText(renderer, font, "Downloading...", barX, barY - 36, UiUtils::Color(255,255,255));
        UiUtils::RenderText(renderer, font, downloadProgressText, barX, barY + barH + 8, UiUtils::Color(200,255,200));
        // Render Cancel button
        int cancelW = 160, cancelH = 44;
        int cancelX = barX + barW + 24;
        int cancelY = barY;
        SDL_Rect cancelRect = {cancelX, cancelY, cancelW, cancelH};
        SDL_SetRenderDrawColor(renderer, 200, 60, 60, 220);
        SDL_RenderFillRect(renderer, &cancelRect);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &cancelRect);
        UiUtils::RenderTextCenteredInBox(renderer, font, "Cancel", cancelRect, UiUtils::Color(255,255,255));
        // Draw focus/hover effect
        SDL_Color focusColor = {255, 255, 0, 255};
        float borderAnim = 1.0f;
        if (downloadButtonFocus == 0) {
            SDL_Rect focusRect = {barX-4, barY-4, barW+8, barH+8};
            DrawAnimatedLBorder(renderer, focusRect, borderAnim, focusColor);
        } else if (downloadButtonFocus == 1) {
            SDL_Rect focusRect = {cancelX-4, cancelY-4, cancelW+8, cancelH+8};
            DrawAnimatedLBorder(renderer, focusRect, borderAnim, focusColor);
        }
    }

    // --- Debug overlay (draw last so it appears on top) ---
}

// --- Scraping logic (fixed for C++14) ---
static std::vector<DownloadOption> ScrapeDownloadOptions(const std::string& pageUrl) {
    std::vector<DownloadOption> result;
    std::string html = HttpUtils::fetchWebContent(pageUrl);
    if (html.empty()) return result;
    // Find the main table with download links
    std::smatch tableMatch;
    std::regex tableRe(R"(<table[^>]*>([\s\S]*?)</table>)", std::regex::icase);
    if (std::regex_search(html, tableMatch, tableRe)) {
        std::string table = tableMatch[1].str();
        std::regex rowRe(R"(<tr>([\s\S]*?)</tr>)", std::regex::icase);
        auto rowsBegin = std::sregex_iterator(table.begin(), table.end(), rowRe);
        auto rowsEnd = std::sregex_iterator();
        for (auto it = rowsBegin; it != rowsEnd; ++it) {
            std::string row = (*it)[1].str();
            std::smatch linkMatch;
            std::regex linkRe(R"(<a[^>]+href=["']([^"']+/download/[^"']+)["'][^>]*>([\s\S]*?)</a>)", std::regex::icase);
            if (std::regex_search(row, linkMatch, linkRe)) {
                std::string url = linkMatch[1].str();
                // Only prepend romsfun.com for Romsfun, not Hexrom
                if (url.find("http") != 0 && url.find("/download/") != std::string::npos) {
                    url = "https://hexrom.com" + url;
                }
                std::string label = StringUtils::cleanHtmlText(linkMatch[2].str());
                result.push_back({label, url});
            }
        }
    }
    return result;
}

static std::string ScrapeFinalDownloadLink(const std::string& versionUrl) {
    std::string html = HttpUtils::fetchWebContent(versionUrl);
    if (html.empty()) return "";
    std::smatch linkMatch;
    std::regex linkRe(R"(<a[^>]+id=["']download["'][^>]+href=["']([^"']+)["'])", std::regex::icase);
    if (std::regex_search(html, linkMatch, linkRe)) {
        return linkMatch[1].str();
    }
    return "";
}

// Helper: Download file from URL to disk (simple blocking version)
static void DownloadFileToDisk(const std::string& url, const std::string& outPath) {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
    } else {
    }
    std::string data = HttpUtils::fetchWebContent(url);
    if (!data.empty()) {
        std::ofstream out(outPath, std::ios::binary);
        out.write(data.data(), data.size());
        out.close();
    } else {
    }
}

// Helper: Get remote file size using curl HEAD
static long GetRemoteFileSize(const std::string& url) {
    std::string command = "curl --cacert /etc/ssl/certs/ca-certificates.crt -sI \"" + url + "\"";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return -1;
    char buffer[256];
    long size = -1;
    while (fgets(buffer, sizeof(buffer), pipe)) {
        std::string line(buffer);
        // Make case-insensitive and trim whitespace
        std::string lowerLine = line;
        std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);
        size_t pos = lowerLine.find("content-length:");
        if (pos != std::string::npos) {
            std::string val = line.substr(pos + 15);
            val.erase(0, val.find_first_not_of(" \t\r\n"));
            val.erase(val.find_last_not_of(" \t\r\n") + 1);
            try {
                size = std::stol(val);
            } catch (...) {
                size = -1;
            }
            break;
        }
    }
    pclose(pipe);
    return size;
}

void GameDetailsScreen::handleInput(const SDL_Event& e, MenuSystem& menuSystem) {
    // --- Download/cancel button navigation always takes priority when downloading ---
    if (downloadInProgress) {
        if (e.type == SDL_KEYDOWN || e.type == SDL_CONTROLLERBUTTONDOWN) {
            // Left/right to move focus
            if ((e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_LEFT) ||
                (e.type == SDL_CONTROLLERBUTTONDOWN && e.cbutton.button == BUTTON_DPAD_LEFT)) {
                if (downloadButtonFocus > 0) downloadButtonFocus--;
                return;
            }
            if ((e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_RIGHT) ||
                (e.type == SDL_CONTROLLERBUTTONDOWN && e.cbutton.button == BUTTON_DPAD_RIGHT)) {
                if (downloadButtonFocus < 1) downloadButtonFocus++;
                return;
            }
            // A or B or X on Cancel
            if (downloadButtonFocus == 1 &&
                ((e.type == SDL_KEYDOWN && (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_SPACE || e.key.keysym.sym == SDLK_x)) ||
                 (e.type == SDL_CONTROLLERBUTTONDOWN && (e.cbutton.button == BUTTON_A || e.cbutton.button == BUTTON_B || e.cbutton.button == BUTTON_X)))) {
                if (cancelSound) {
                    Mix_PlayChannel(-1, cancelSound, 0);
                }
                downloadCancelRequested = true;
                downloadInProgress = false;
                if (downloadPid > 0) { kill(downloadPid, SIGTERM); downloadPid = -1; }
                downloadProgressText = "Download canceled.";
                downloadTotalBytes = 0;
                downloadCurrentBytes = 0;
                downloadButtonFocus = 0;
                // --- Ensure partial file is deleted immediately ---
                if (!downloadOutPath.empty()) {
                    std::remove(downloadOutPath.c_str());
                }
                return;
            }
        }
        // Block all other input while downloading
        return;
    }
    // --- MODAL HANDLING: Always first! ---
    if (showDownloadPopup) {
        int itemH = 44;
        int maxVisible = (600 - 180) / itemH;
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
                        std::string finalLink = ScrapeFinalDownloadLink(versionUrl);
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
    int totalLines = aboutLines.size();
    if (e.type == SDL_CONTROLLERBUTTONDOWN) {
        // Back/close (now B is back)
        if (e.cbutton.button == BUTTON_B) { // B = back
            menuSystem.popScreen();
        }
        // Activate download button (A = select)
        if (buttonFocused && e.cbutton.button == BUTTON_A) {
            if (downloadInProgress) return; // Guard
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
            downloadOutPath = std::string("downloads/") + filename;
            downloadProgressText = "Starting...";
            downloadInProgress = true;
            downloadCancelRequested = false;
            downloadCurrentBytes = 0;
            downloadTotalBytes = 0;
            std::thread([this, url]() {
                // Try to discover size (HEAD)
                long size = GetRemoteFileSize(url);
                if (size > 0) downloadTotalBytes = size;
                std::string referer = url.find("gamulator.com") != std::string::npos ? "https://www.gamulator.com/" : "https://hexrom.com/";
                auto humanSize = [](long bytes) {
                    const char* units[] = {"B","KB","MB","GB","TB"};
                    double v = (double)bytes;
                    int unit = 0;
                    while (v >= 1024.0 && unit < 4) { v /= 1024.0; ++unit; }
                    char buf[64];
                    if (v < 10.0) snprintf(buf, sizeof(buf), "%.2f %s", v, units[unit]);
                    else if (v < 100.0) snprintf(buf, sizeof(buf), "%.1f %s", v, units[unit]);
                    else snprintf(buf, sizeof(buf), "%.0f %s", v, units[unit]);
                    return std::string(buf);
                };
                // Fork & exec curl so we can poll file size
                pid_t pid = fork();
                if (pid == 0) {
                    // Child
                    execlp("curl", "curl",
                           "--globoff", "-L", "--retry", "2",
                           "--cacert", "/etc/ssl/certs/ca-certificates.crt",
                           "-A", "Mozilla/5.0 (X11; Linux x86_64)",
                           "-e", referer.c_str(),
                           "-o", downloadOutPath.c_str(),
                           url.c_str(),
                           (char*)nullptr);
                    _exit(127);
                } else if (pid < 0) {
                    downloadProgressText = "Failed to fork.";
                    downloadInProgress = false;
                    return;
                }
                downloadPid = pid;
                // Parent: poll
                while (true) {
                    if (downloadCancelRequested) {
                        kill(pid, SIGTERM);
                    }
                    int status = 0;
                    pid_t r = waitpid(pid, &status, WNOHANG);
                    struct stat st{};
                    if (stat(downloadOutPath.c_str(), &st) == 0) {
                        downloadCurrentBytes = st.st_size;
                        if (downloadTotalBytes > 0) {
                            double pct = downloadTotalBytes > 0 ? (double)downloadCurrentBytes / (double)downloadTotalBytes * 100.0 : 0.0;
                            downloadProgressText = humanSize(downloadCurrentBytes) + " / " + humanSize(downloadTotalBytes) + " (" + std::to_string((int)std::round(pct)) + "%)";
                        } else {
                            downloadProgressText = humanSize(downloadCurrentBytes);
                        }
                    }
                    if (r == pid) break; // finished
                    if (downloadCancelRequested) {
                        downloadProgressText = "Download canceled.";
                        break;
                    }
                    SDL_Delay(200); // sleep a bit
                }
                // Finalize
                if (downloadCancelRequested) {
                    std::remove(downloadOutPath.c_str());
                    if (downloadPid > 0) { kill(downloadPid, SIGTERM); }
                    downloadInProgress = false;
                    return;
                }
                struct stat stFinal{};
                if (stat(downloadOutPath.c_str(), &stFinal) == 0 && stFinal.st_size > 0) {
                    downloadCurrentBytes = stFinal.st_size;
                    if (downloadTotalBytes == 0) downloadTotalBytes = stFinal.st_size;
                    // Attempt relocate using persisted mappedFolder (if any)
                    if (!details.mappedFolder.empty()) {
                        std::vector<std::string> roots = {"/mnt/SDCARD/Roms","/mnt/SDCARD/ROMS","Roms","ROMS","../Roms","../ROMS"};
                        std::string baseDir;
                        struct stat stDir{};
                        for (auto &r : roots) {
                            std::string candidate = r + "/" + details.mappedFolder;
                            if (stat(candidate.c_str(), &stDir) == 0 && S_ISDIR(stDir.st_mode)) { baseDir = candidate; break; }
                        }
                        if (!baseDir.empty()) {
                            std::string finalFileName = downloadOutPath.substr(downloadOutPath.find_last_of('/')+1);
                            std::string newPath = baseDir + "/" + finalFileName;
                            bool isZip = false;
                            if (finalFileName.size() > 4) {
                                std::string ext = finalFileName.substr(finalFileName.size()-4);
                                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                                if (ext == ".zip") isZip = true;
                            }
                            bool unzip = isZip && shouldUnzipForFolder(details.mappedFolder);
                            printf("[Download] Post-processing: file=%s isZip=%d unzip=%d folder=%s\n", finalFileName.c_str(), (int)isZip, (int)unzip, details.mappedFolder.c_str());
                            if (unzip) {
                                // Create temp extraction dir
                                std::string tmpDir = baseDir + "/.__extract_tmp";
                                mkdir(tmpDir.c_str(), 0755);
                                // Use busybox/unzip if available; try unzip then 7z
                                auto cmdExists = [](const char* c){ return system((std::string("command -v ") + c + " >/dev/null 2>&1").c_str()) == 0; };
                                bool haveUnzip = cmdExists("unzip");
                                bool have7z = cmdExists("7z");
                                int ret = -1;
                                if (haveUnzip) {
                                    // Show unzip message in UI
                                    downloadProgressText = "Unzipping...";
                                    ret = system((std::string("unzip -o \"") + downloadOutPath + "\" -d \"" + tmpDir + "\" >/dev/null 2>&1").c_str());
                                }
                                if (ret != 0 && have7z) {
                                    ret = system((std::string("7z x -y \"") + downloadOutPath + "\" -o\"" + tmpDir + "\" >/dev/null 2>&1").c_str());
                                }
                                if (ret != 0) {
                                    printf("[Download] Extraction skipped/failed: unzip=%d 7z=%d ret=%d (keeping zip)\n", haveUnzip?1:0, have7z?1:0, ret);
                                }
                                if (ret == 0) {
                                    // Move extracted files into baseDir
                                    DIR* d = opendir(tmpDir.c_str());
                                    if (d) {
                                        struct dirent* de;
                                        while ((de = readdir(d))) {
                                            if (std::string(de->d_name) == "." || std::string(de->d_name) == "..") continue;
                                            std::string from = tmpDir + "/" + de->d_name;
                                            std::string to = baseDir + "/" + de->d_name;
                                            rename(from.c_str(), to.c_str());
                                        }
                                        closedir(d);
                                    }
                                    // Delete original zip
                                    std::remove(downloadOutPath.c_str());
                                    // Remove temp dir
                                    rmdir(tmpDir.c_str());
                                    downloadProgressText = std::string("Extracted to ") + baseDir;
                                    printf("[Download] Extracted contents to %s (removed zip)\n", baseDir.c_str());
                                } else {
                                    // Extraction failed -> attempt plain move of zip
                                    if (rename(downloadOutPath.c_str(), newPath.c_str()) == 0) {
                                        downloadOutPath = newPath;
                                        downloadProgressText = std::string("Saved to ") + newPath + " (zip kept)";
                                        printf("[Download] Extraction failed; moved zip to %s\n", newPath.c_str());
                                    } else {
                                        if (std::remove(downloadOutPath.c_str()) == 0) printf("[Download] Extraction+move failed; zip deleted %s\n", downloadOutPath.c_str());
                                        downloadProgressText = "Download failed (extract+move).";
                                    }
                                }
                            } else {
                                // Just move the zip or non-zip file
                                if (rename(downloadOutPath.c_str(), newPath.c_str()) == 0) {
                                    downloadOutPath = newPath;
                                    downloadProgressText = std::string("Saved to ") + newPath;
                                    printf("[Download] Moved file to %s\n", newPath.c_str());
                                } else {
                                    if (std::remove(downloadOutPath.c_str()) == 0) printf("[Download] Move failed, file deleted: %s\n", downloadOutPath.c_str());
                                    downloadProgressText = "Download failed (move error).";
                                }
                            }
                        } else {
                            if (std::remove(downloadOutPath.c_str()) == 0) printf("[Download] Console folder missing, file deleted: %s (mappedFolder=%s)\n", downloadOutPath.c_str(), details.mappedFolder.c_str());
                            downloadProgressText = "Download failed (console folder missing).";
                        }
                    } else {
                        // No mapping persisted -> delete per earlier rule
                        if (std::remove(downloadOutPath.c_str()) == 0) printf("[Download] No mapping (persisted empty), file deleted: %s\n", downloadOutPath.c_str());
                        downloadProgressText = "Download failed (no console mapping).";
                    }
                } else {
                    downloadProgressText = "Download failed.";
                }
                downloadPopupMessage = downloadProgressText;
                showDownloadPopup = true;
                downloadInProgress = false;
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