#!/usr/bin/env bash
# Builds the catalog crawler natively (needs g++, SDL2 + SDL2_ttf headers).
#   tools/catalog/build.sh [output path]   (default: build/crawler)
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
out=${1:-$root/build/crawler}
mkdir -p "$(dirname "$out")"
cd "$root"
scrapers=$(ls src/scraper/Hexrom/*.cpp src/scraper/Romspedia/*.cpp | grep -v FilterModal)
g++ -O2 -std=c++17 -Isrc $(sdl2-config --cflags) \
    tools/catalog/crawler.cpp tools/catalog/stubs.cpp $scrapers \
    src/utils/HttpUtils.cpp src/utils/StringUtils.cpp \
    -o "$out" -lpthread
echo "built $out"
