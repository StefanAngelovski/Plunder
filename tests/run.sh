#!/usr/bin/env bash
# Builds and runs the scraper tests natively. Run from anywhere.
# Needs g++, gcc and the SDL2 + SDL2_ttf development headers.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
out="$root/build/tests"
mkdir -p "$out"
cd "$root"
[ -f "$out/htmlq.o" ] && [ "$out/htmlq.o" -nt src/scraper/engine/htmlq.c ] && [ "$out/htmlq.o" -nt third_party/lexbor/lexbor.c ] ||
    gcc -O1 -c src/scraper/engine/htmlq.c -o "$out/htmlq.o"
g++ -O1 -std=c++17 -Isrc $(sdl2-config --cflags) \
    tests/ScraperTests.cpp tools/catalog/stubs.cpp \
    src/scraper/engine/*.cpp src/scraper/Hexrom/HexromScraper.cpp src/scraper/Romspedia/RomspediaScraper.cpp \
    src/scraper/GithubReleases/GitHubReleasesScraper.cpp \
    src/utils/HttpUtils.cpp src/utils/Process.cpp \
    "$out/htmlq.o" -o "$out/scraper_tests"
"$out/scraper_tests"   # run from the repo root: fixtures and res/blacklist.txt are relative
