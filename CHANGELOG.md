# Changelog

Release notes for Plunder. Each `## vX.Y.Z` section becomes the body of the
GitHub release for that version (wrapped in `.github/release-template.md`),
and is what the in-app **Patch Notes** screen shows.

**How to release:** add notes under `## Unreleased` as you work. When you're
ready, rename that heading to `## vX.Y.Z`, set the same version in
`src/Version.h` (`PLUNDER_VERSION`), and push to `master`. The release
workflow builds `Plunder.zip` and publishes the release. Pushes that don't
change the version don't release anything.

Preview a release body locally with `scripts/release-notes.sh X.Y.Z`.

## Unreleased

### Improvements in this update
- Global search: a new Search screen on Home looks through every game on every site at once. Results update as you type, can be filtered by console, site and sort order, and open straight into the right site. "Search sites live" asks the sites directly for games newer than the catalog.
- Covers that were already downloaded are no longer downloaded again.
- Security: web addresses and file names from the ROM sites are no longer passed through a shell, so a malicious page can't run commands on your device.

### ⚠️ Known Issues
- Regex-based scraping can be messy (plan to improve modularity + readability)

## v2.4.0

### Improvements in this update
- Game catalog: consoles and complete game lists now open instantly, and searching within a console is instant too. Plunder downloads a pre-built catalog of every game (updated daily) in the background and falls back to the live sites when needed.
- Faster page loading: Plunder is now built with compiler optimisations, so it processes site pages about 3–5× faster on the device.
- Cover art now loads on every firmware: Plunder decodes WebP images itself instead of relying on the system's image library, which can't read them on stock or CrossMix.

### ⚠️ Known Issues
- Regex-based scraping can be messy (plan to improve modularity + readability)

## v2.3.0

### Improvements in this update
- Brand-new look: a modern, dark interface across every screen, with smooth text, rounded cards and button hints at the bottom of each screen.
- Game lists now show cover art in a four-column grid; consoles show their game count at a glance.
- Redesigned game details page, search & filter window and on-screen keyboard.
- All five themes refreshed, with live previews on the Themes screen.
- Hexrom game pages show the title, size, release date and region again.
- Romspedia downloads work again for files with spaces in their names.
- Patch Notes load from GitHub again.
- Removed Gamulator, which is no longer online.

### ⚠️ Known Issues
- Some images may not load due to missing WebP support in CrossMix `SDL2_Image`
- Regex-based scraping can be messy (plan to improve modularity + readability)

## v2.2.0

### Improvements in this update
- MAJOR improvements to compatibility, more games will be available to download from every scraper because of the next fact.
- Found a 7z binary we can use, works much better than unzip, allowing us to unzip .7z, .rar and potentially others if needed, now implemented.

### ⚠️ Known Issues
- Some images may not load due to missing WebP support in CrossMix `SDL2_Image`
- Regex-based scraping can be messy (plan to improve modularity + readability)

## v2.1.0

### Improvements in this update
- Added a themes button and new carousel in the settings menu.
- Select different themes to change the look of your interface

### ⚠️ Known Issues
- Some images may not load due to missing WebP support in CrossMix `SDL2_Image`
- Regex-based scraping can be messy (plan to improve modularity + readability)

## v2.0.0

### Improvements in this update
- "Check for updates" button in settings was added, you can see the current information below it
- Automatically checks for updates when Plunder starts and it informs you in the main screen
- Updating straight from github releases

### 🚀 Features
- Scrapes several romsites for games
- Modular scraper system (more romsites can be added)
- Filtering system to quickly find games
- Direct mapping of consoles to device folders
- One-button **cache clearing** for performance
- Auto updating

### ⚠️ Known Issues
- Some images may not load due to missing WebP support in CrossMix `SDL2_Image`
- Regex-based scraping can be messy (plan to improve modularity + readability)

## v1.0.0-beta

### 🚀 Features
- Scrapes several romsites for games
- Modular scraper system (more romsites can be added)
- Filtering system to quickly find games
- Direct mapping of consoles to device folders
- One-button **cache clearing** for performance

### ⚠️ Known Issues
- Some images may not load due to missing WebP support in CrossMix `SDL2_Image`
- Regex-based scraping can be messy (plan to improve modularity + readability)
- No auto-update yet (planned for future releases)
