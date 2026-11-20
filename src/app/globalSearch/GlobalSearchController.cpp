#include "include/GlobalSearchController.h"

#include "../../catalog/include/LiveSearch.h"
#include "../../utils/include/Theme.h"
#include "../ControllerButtons.h"

#include <thread>

using namespace UiUtils;

namespace {
std::string withCommas(size_t n) {
    std::string s = std::to_string(n);
    for (int i = (int)s.size() - 3; i > 0; i -= 3) s.insert((size_t)i, ",");
    return s;
}

// Small magnifier glyph centred on (cx, cy).
void drawMagnifier(SDL_Renderer* r, int cx, int cy, SDL_Color c) {
    StrokeRoundedRect(r, SDL_Rect{cx - 9, cy - 9, 15, 15}, 7, 2, c);
    for (int i = 0; i < 6; ++i) FillRoundedRect(r, SDL_Rect{cx + 3 + i, cy + 3 + i, 3, 3}, 1, c);
}
}  // namespace

GlobalSearchController::GlobalSearchController(ApplyFn apply) : apply_(std::move(apply)) {}

void GlobalSearchController::start() {
    groups_ = Catalog::ConsoleGroups();
    sites_ = Catalog::Sites();
    mode_ = Mode::Typing;
    keyboardOpen_ = true;
    keyboard_.resetPosition();
    refresh();
}

// ---- Options -----------------------------------------------------------------------

const std::vector<std::string>& GlobalSearchController::sortOptions() {
    static const std::vector<std::string> opts = {"Best match", "Title A-Z", "Title Z-A"};
    return opts;
}

std::vector<std::string> GlobalSearchController::consoleOptions() const {
    std::vector<std::string> o = {"All consoles"};
    for (const auto& g : groups_) o.push_back(g.name + "  (" + withCommas(g.games) + ")");
    return o;
}

std::vector<std::string> GlobalSearchController::siteOptions() const {
    std::vector<std::string> o = {"All sites"};
    for (const auto& s : sites_) o.push_back(s);
    return o;
}

std::string GlobalSearchController::consoleKey() const {
    return consoleIdx_ > 0 && consoleIdx_ <= (int)groups_.size() ? groups_[consoleIdx_ - 1].key : "";
}

std::string GlobalSearchController::site() const {
    return siteIdx_ > 0 && siteIdx_ <= (int)sites_.size() ? sites_[siteIdx_ - 1] : "";
}

bool GlobalSearchController::filtersActive() const { return consoleIdx_ > 0 || siteIdx_ > 0; }

// ---- Searching -----------------------------------------------------------------------

void GlobalSearchController::applyResults(const std::vector<Catalog::GlobalResult>& results, bool live) {
    std::vector<ListItem> items;
    items.reserve(results.size());
    for (const auto& r : results) {
        const Catalog::Source& s = r.sources.front();
        ListItem it;
        it.label = r.title;
        it.imagePath = s.image;
        it.downloadUrl = s.url;
        it.site = s.site;
        it.console = s.consoleLabel;
        it.consoleName = r.consoleName.empty() ? Catalog::ConsoleNameFor(s.consoleLabel) : r.consoleName;
        items.push_back(std::move(it));
    }
    resultCount_ = items.size();
    liveMode_ = live;
    apply_(items);
}

void GlobalSearchController::refresh() {
    liveMessage_.clear();
    // Nothing typed and no filters: show nothing rather than every game.
    bool hasText = text_.find_first_not_of(' ') != std::string::npos;
    if (!hasText && !filtersActive()) { applyResults({}, false); return; }
    Catalog::GlobalQuery q;
    q.text = text_;
    q.consoleKey = consoleKey();
    q.site = site();
    q.sort = (Catalog::SortOrder)sortIdx_;
    applyResults(Catalog::GlobalSearch(q), false);
}

void GlobalSearchController::startLive() {
    if (text_.find_first_not_of(' ') == std::string::npos) {
        liveMessage_ = "Type something first, then search the sites live.";
        return;
    }
    std::shared_ptr<LiveState> state = live_;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->running) return;
        state->running = true;
        state->ready = false;
        state->query = text_;
    }
    liveMessage_.clear();
    std::string query = text_;
    std::thread([state, query]() {
        auto results = LiveSearch::SearchAllSites(query);
        std::lock_guard<std::mutex> lock(state->mutex);
        state->results = std::move(results);
        state->running = false;
        state->ready = true;
    }).detach();
}

bool GlobalSearchController::liveRunning() const {
    std::lock_guard<std::mutex> lock(live_->mutex);
    return live_->running;
}

void GlobalSearchController::update() {
    std::vector<Catalog::GlobalResult> results;
    {
        std::lock_guard<std::mutex> lock(live_->mutex);
        if (!live_->ready) return;
        live_->ready = false;
        if (live_->query != text_) return;   // user typed something else meanwhile
        results = std::move(live_->results);
    }
    // Respect the console / site filters for live results too.
    std::vector<Catalog::GlobalResult> kept;
    for (auto& r : results) {
        if (!consoleKey().empty() && r.consoleKey != consoleKey()) continue;
        if (!site().empty() && r.sources.front().site != site()) continue;
        kept.push_back(std::move(r));
    }
    if (kept.empty()) liveMessage_ = "The sites returned no matches either.";
    applyResults(kept, true);
}

// ---- Input -----------------------------------------------------------------------------

void GlobalSearchController::openPulldown() {
    if (focus_ == 0) { pullOptions_ = consoleOptions(); pullIdx_ = consoleIdx_; }
    else if (focus_ == 1) { pullOptions_ = siteOptions(); pullIdx_ = siteIdx_; }
    else { pullOptions_ = sortOptions(); pullIdx_ = sortIdx_; }
    pulldown_ = true;
}

bool GlobalSearchController::handleInput(const SDL_Event& e) {
    if (e.type != SDL_CONTROLLERBUTTONDOWN) return mode_ != Mode::Browsing;  // swallow other events while overlays are up
    const Uint8 b = e.cbutton.button;

    if (mode_ == Mode::Typing) {
        if (b == BUTTON_Y) { mode_ = Mode::Filters; focus_ = 0; pulldown_ = false; return true; }
        std::string before = text_;
        bool open = true;
        keyboard_.handleInput(e, text_, open);
        if (text_ != before) refresh();
        if (!open) { mode_ = Mode::Browsing; keyboardOpen_ = false; }
        return true;
    }

    if (mode_ == Mode::Filters) {
        if (pulldown_) {
            int n = (int)pullOptions_.size();
            if (b == BUTTON_DPAD_DOWN) pullIdx_ = (pullIdx_ + 1) % n;
            else if (b == BUTTON_DPAD_UP) pullIdx_ = (pullIdx_ + n - 1) % n;
            else if (b == BUTTON_RIGHTSHOULDER) pullIdx_ = std::min(n - 1, pullIdx_ + 8);
            else if (b == BUTTON_LEFTSHOULDER) pullIdx_ = std::max(0, pullIdx_ - 8);
            else if (b == BUTTON_A) {
                if (focus_ == 0) consoleIdx_ = pullIdx_;
                else if (focus_ == 1) siteIdx_ = pullIdx_;
                else sortIdx_ = pullIdx_;
                pulldown_ = false;
                refresh();
            } else if (b == BUTTON_B) pulldown_ = false;
            return true;
        }
        if (b == BUTTON_DPAD_DOWN) focus_ = (focus_ + 1) % 5;
        else if (b == BUTTON_DPAD_UP) focus_ = (focus_ + 4) % 5;
        else if (b == BUTTON_A) {
            if (focus_ <= 2) openPulldown();
            else if (focus_ == 3) { startLive(); mode_ = Mode::Browsing; }
            else mode_ = keyboardOpen_ ? Mode::Typing : Mode::Browsing;
        } else if (b == BUTTON_B || b == BUTTON_X) {
            mode_ = keyboardOpen_ ? Mode::Typing : Mode::Browsing;
        }
        return true;
    }

    // Browsing: only Y (type) and X (filters) are ours; the grid handles the rest.
    if (b == BUTTON_Y) { mode_ = Mode::Typing; keyboardOpen_ = true; return true; }
    if (b == BUTTON_X) { mode_ = Mode::Filters; focus_ = 0; pulldown_ = false; return true; }
    return false;
}

// ---- Text for the host screen ------------------------------------------------------------

int GlobalSearchController::keyboardTop() const { return 452; }

std::string GlobalSearchController::title() const { return "Search"; }

std::string GlobalSearchController::pill() const {
    if (liveRunning()) return "Searching sites…";
    if (resultCount_ == 0) return "";
    return withCommas(resultCount_) + (liveMode_ ? " live results" : (resultCount_ == 1 ? " result" : " results"));
}

std::string GlobalSearchController::emptyTitle() const {
    if (liveRunning()) return "Searching the sites…";
    if (!liveMessage_.empty()) return "No matches";
    bool hasText = text_.find_first_not_of(' ') != std::string::npos;
    if (!hasText && !filtersActive()) {
        size_t total = Catalog::TotalGames();
        return total ? "Search " + withCommas(total) + " games" : "Catalog not loaded yet";
    }
    return "No matches in the catalog";
}

std::string GlobalSearchController::emptyHint() const {
    if (liveRunning()) return "Asking every site directly. This takes a few seconds.";
    if (!liveMessage_.empty()) return liveMessage_;
    bool hasText = text_.find_first_not_of(' ') != std::string::npos;
    if (!hasText && !filtersActive())
        return Catalog::TotalGames() ? "Type a title, or pick a console in Filters." : "You can still search the sites live from Filters.";
    return "Try other words, or Filters > Search sites live.";
}

std::string GlobalSearchController::footerStatus() const {
    if (liveRunning()) return "Searching sites…";
    if (liveMode_) return "Live results from the sites";
    return Catalog::AgeText();
}

std::vector<Hint> GlobalSearchController::hints() const {
    return {{"DPAD", "Navigate"}, {"Y", "Type"}, {"X", "Filters"}, {"A", "Open"}, {"B", "Back"}};
}

// ---- Drawing -------------------------------------------------------------------------------

void GlobalSearchController::renderBar(SDL_Renderer* r, int y) {
    const ThemeColors& t = Theme::getInstance().colors();
    SDL_Rect bar{MARGIN_X, y, SCREEN_W - 2 * MARGIN_X, kBarHeight - 8};
    bool typing = mode_ == Mode::Typing;
    FillRoundedRect(r, bar, 14, typing ? Lighten(t.backgroundDark, 0.07f) : WithAlpha(t.cardBackground, 220));
    if (typing) DrawFocusRing(r, bar, 14, t.accentPrimary, 2, 2);
    drawMagnifier(r, bar.x + 26, bar.y + bar.h / 2, typing ? t.accentPrimary : t.textSecondary);

    // Active filter pills on the right
    TTF_Font* pf = GetFont(14, true);
    std::vector<std::string> pills;
    if (consoleIdx_ > 0) pills.push_back(groups_[consoleIdx_ - 1].name);
    if (siteIdx_ > 0) pills.push_back(sites_[siteIdx_ - 1]);
    if (sortIdx_ > 0) pills.push_back(sortOptions()[sortIdx_]);
    int px = bar.x + bar.w - 12;
    for (auto it = pills.rbegin(); it != pills.rend(); ++it) {
        int w = PillWidth(pf, *it, 10);
        px -= w;
        DrawPill(r, pf, *it, px, bar.y + (bar.h - 26) / 2, WithAlpha(t.accentPrimary, 50), t.textHighlight, 10, 26);
        px -= 8;
    }

    TTF_Font* f = GetFont(19);
    int textX = bar.x + 50, maxW = px - textX - 12;
    bool empty = text_.empty();
    std::string shown = empty ? "Search all sites" : text_;
    int w = DrawText(r, f, Ellipsize(f, shown, maxW), textX, bar.y + (bar.h - TextHeight(f)) / 2,
                     empty ? WithAlpha(t.textSecondary, 160) : t.textPrimary);
    if (typing && (SDL_GetTicks() / 500) % 2 == 0)
        FillRoundedRect(r, SDL_Rect{textX + (empty ? 0 : w) + 2, bar.y + 12, 2, bar.h - 24}, 0, t.accentPrimary);
}

void GlobalSearchController::renderOverlay(SDL_Renderer* r) {
    const ThemeColors& t = Theme::getInstance().colors();
    if (mode_ == Mode::Typing) {
        // Keyboard docked at the bottom, with its controls listed just above it.
        TTF_Font* hf = GetFont(15);
        std::string help = "A type   X delete   Y filters   B done";
        DrawText(r, hf, help, SCREEN_W - MARGIN_X - TextWidth(hf, help), keyboardTop() - 24, t.textSecondary);
        keyboard_.render(r, GetFont(20), (SCREEN_W - 712) / 2, keyboardTop() + 16, 712, 224);
        return;
    }
    if (mode_ != Mode::Filters) return;

    DrawScrim(r);
    const int w = 640, h = 420;
    const int x = (SCREEN_W - w) / 2, y = (SCREEN_H - h) / 2 - 10;
    DrawPanel(r, SDL_Rect{x, y, w, h}, 24);
    DrawText(r, GetFont(24, true), "Filters", x + 32, y + 26, t.textPrimary);
    DrawText(r, GetFont(15), "Apply to every site", x + 32, y + 60, t.textSecondary);

    TTF_Font* lf = GetFont(17);
    TTF_Font* vf = GetFont(18);
    const char* labels[3] = {"Console", "Site", "Sort"};
    const std::string values[3] = {consoleOptions()[consoleIdx_], siteOptions()[siteIdx_], sortOptions()[sortIdx_]};
    const int fieldX = x + 150, fieldW = w - 150 - 32, rowsTop = y + 96, step = 58;
    for (int i = 0; i < 3; ++i) {
        SDL_Rect field{fieldX, rowsTop + i * step, fieldW, 46};
        bool f = focus_ == i && !pulldown_;
        DrawText(r, lf, labels[i], x + 32, field.y + (field.h - TextHeight(lf)) / 2, f ? t.textPrimary : t.textSecondary);
        FillRoundedRect(r, field, 12, f ? Lighten(t.backgroundDark, 0.06f) : WithAlpha(t.backgroundDark, 170));
        if (f) DrawFocusRing(r, field, 12, t.accentPrimary, 2, 2);
        DrawText(r, vf, Ellipsize(vf, values[i], fieldW - 50), field.x + 16, field.y + (field.h - TextHeight(vf)) / 2, t.textPrimary);
        for (int k = 0; k < 5; ++k)   // chevron
            FillRoundedRect(r, SDL_Rect{field.x + field.w - 29 + k, field.y + field.h / 2 - 2 + k, 11 - 2 * k, 2}, 0, t.textSecondary);
    }

    // Buttons
    SDL_Rect live{x + 32, y + h - 32 - 50, 260, 50};
    SDL_Rect done{x + w - 32 - 160, y + h - 32 - 50, 160, 50};
    bool fLive = focus_ == 3 && !pulldown_, fDone = focus_ == 4 && !pulldown_;
    FillRoundedRect(r, live, 14, fLive ? t.accentPrimary : WithAlpha(t.accentPrimary, 36));
    if (fLive) DrawFocusRing(r, live, 14, t.accentPrimary, 3, 3);
    RenderTextCenteredInBox(r, GetFont(17, true), "Search sites live", live, Color(fLive ? t.backgroundDark : t.textPrimary));
    FillRoundedRect(r, done, 14, fDone ? t.accentPrimary : Lighten(t.cardBackground, 0.10f));
    if (fDone) DrawFocusRing(r, done, 14, t.accentPrimary, 3, 3);
    RenderTextCenteredInBox(r, GetFont(17, true), "Done", done, Color(fDone ? t.backgroundDark : t.textPrimary));

    if (pulldown_) {
        const int maxVisible = 7, itemH = 40;
        int total = (int)pullOptions_.size();
        int visible = std::min(maxVisible, total);
        int first = std::min(std::max(0, pullIdx_ - visible / 2), std::max(0, total - visible));
        SDL_Rect list{fieldX, rowsTop + focus_ * step + 50, fieldW, visible * itemH + 12};
        if (list.y + list.h > SCREEN_H - 20) list.y = rowsTop + focus_ * step - list.h - 4;
        DrawShadow(r, list, 14, 24, 150, 8);
        FillRoundedRect(r, list, 14, Lighten(t.cardBackground, 0.06f));
        StrokeRoundedRect(r, list, 14, 1, WithAlpha(t.textPrimary, 26));
        for (int i = 0; i < visible; ++i) {
            int idx = first + i;
            SDL_Rect row{list.x + 6, list.y + 6 + i * itemH, list.w - 12 - (total > visible ? 12 : 0), itemH};
            bool sel = idx == pullIdx_;
            if (sel) FillRoundedRect(r, row, 10, WithAlpha(t.accentPrimary, 60));
            DrawText(r, vf, Ellipsize(vf, pullOptions_[idx], row.w - 24), row.x + 14, row.y + (itemH - TextHeight(vf)) / 2,
                     sel ? t.textPrimary : t.textSecondary);
        }
        if (total > visible)
            DrawScrollbar(r, list.x + list.w - 14, list.y + 8, list.h - 16, (float)visible / total, (float)first / std::max(1, total - visible));
        DrawHints(r, {{"DPAD", "Choose"}, {"LR", "Jump"}, {"A", "Select"}, {"B", "Cancel"}});
    } else {
        DrawHints(r, {{"DPAD", "Move"}, {"A", "Select"}, {"B", "Close"}});
    }
}
