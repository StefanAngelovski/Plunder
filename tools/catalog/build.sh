#!/usr/bin/env bash
# Builds the catalog crawler natively (needs g++, gcc, SDL2 + SDL2_ttf headers).
#   tools/catalog/build.sh [output path]   (default: build/crawler)
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
out=${1:-$root/build/crawler}
mkdir -p "$(dirname "$out")" "$root/build/obj/crawler"
cd "$root"
# lexbor (via htmlq.c) is C: compile it once on its own.
htmlq_obj="$root/build/obj/crawler/htmlq.o"
gcc -O2 -c src/scraper/engine/htmlq.c -o "$htmlq_obj"
scrapers=$(ls src/scraper/Hexrom/*.cpp src/scraper/Romspedia/*.cpp | grep -v Filter)
g++ -O2 -std=c++17 -Isrc $(sdl2-config --cflags) \
    tools/catalog/crawler.cpp tools/catalog/stubs.cpp $scrapers \
    src/scraper/engine/*.cpp \
    src/utils/HttpUtils.cpp src/utils/Process.cpp src/utils/StringUtils.cpp \
    "$htmlq_obj" -o "$out" -lpthread
echo "built $out"
