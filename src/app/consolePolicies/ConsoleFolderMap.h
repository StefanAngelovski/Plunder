#pragma once
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cstdio>

// Only include consoles that are actually supported (present in your supported consoles list)
static const std::unordered_map<std::string, std::string> SCRAPED_TO_FOLDER = {
    // Hexrom mappings
    {"Amstrad CPC (CPC)", "CPC"},
    {"Atari 2600 (A2600)", "ATARI2600"},
    {"Atari 5200 SuperSystem (A5200)", "ATARI5200"},
    {"Atari 7800 ProSystem (A7800)", "ATARI7800"},
    {"MAME ROMs", "MAME"},
    {"Atari 800 ROMs", "ATARI800"},
    {"C64 Preservation ROMs", "C64"},
    {"SAM Coup ROMs", "SAMCOUPE"},
    {"C64 Tapes ROMs", "C64"},
    {"Pokemon Mini ROMs", "POKEMINI"},
    {"Commodore VIC-20 ROMs", "VIC20"},
    {"Turbo Duo ROMs", "TURBODUO"},
    {"SG-1000 ROMs", "SG1000"},
    {"Enterprise ROMs", "ENTERPRISE"},
    {"Channel Fun ROMs", "CHANNELF"},
    {"SuFami Turbo ROMs", "SUFAMI"},
    {"Commodore PET ROMs", "CPET"},
    {"Atari 800 (A800)", "ATARI800"},
    {"Atari Lynx (ALYNX)", "LYNX"},
    {"Atari ST (AST)", "ATARIST"},
    {"ColecoVision (CV)", "COLECO"},
    {"Commodore 64 (C64)", "C64"},
    {"Fairchild Channel F", "CHANNELF"},
    {"Game Gear (GG)", "GG"},
    {"Gameboy (GB)", "GB"},
    {"Gameboy Color (GBC)", "GBC"},
    {"GBA ROMs", "GBA"},
    {"GCE Vectrex", "VECTREX"},
    {"Intellivision", "INTELLIVISION"},
    {"Magnavox Odyssey 2", "VIDEOPAC"},
    {"MSX Computer (MSXC)", "MSX"},
    {"MSX-2 (MSX2)", "MSX2"},
    {"Neo Geo", "NEOGEO"},
    {"Neo Geo Pocket (NGP)", "NGP"},
    {"Neo Geo Pocket Color (NPC)", "NGC"},
    {"Nintendo (NES)", "FC"},
    {"Nintendo 64", "N64"},
    {"Nintendo DS (NDS)", "NDS"},
    {"Nintendo Famicom Disk System (NFDS)", "FDS"},
    {"Nintendo Pokemon Mini", "POKEMINI"},
    {"Nintendo Virtual Boy (NVB)", "VB"},
    {"Philips Videopac", "VIDEOPAC"},
    {"Playstation 1 (PSX) Roms", "PS"},
    {"Playstation Portable (PSP ISOs)", "PSP"},
    {"Sega 32X (S32X)", "SEGA32X"},
    {"Sega Dreamcast (DC)", "DC"},
    {"SEGA Genesis (SG)", "MD"},
    {"Sega Master System (SMS)", "MS"},
    {"Sega SG1000 (SG1000)", "SG1000"},
    {"SNES Super Nintendo", "SFC"},
    {"SuFami Turbo (ST)", "SUFAMI"},
    {"Super Grafx (SGFX)", "SFX"},
    {"TurboGrafx-16 (T16)", "PCE"},
    {"Watara Supervision", "SUPERVISION"},
    {"WonderSwan (WS)", "WS"},
    {"ZX Spectrum (TAP)", "ZXS"},
    {"Capcom Play System (CPS)", "CPS1"},
    {"Capcom Play System 2 (CPS2)", "CPS2"},
    
    // Gamulator mappings
    {"PSP", "PSP"},
    {"NDS", "NDS"},
    {"GBA", "GBA"},
    {"N64", "N64"},
    {"SNES", "SFC"},
    {"Mame", "MAME"},
    {"PSX", "PS"},
    {"NES", "FC"},
    {"GBC", "GBC"},
    {"Megadrive", "MD"},
    {"Sega Dreamcast", "DC"},
    {"GB", "GB"},
    {"Neo Geo", "NEOGEO"},
    {"Sega Saturn", "SATURN"},
    {"Sega CD", "SEGACD"},
    {"Sega NAOMI", "NAOMI"},
    {"Atari 2600", "ATARI2600"},
    {"Sega Master System", "MS"},
    {"Sega Game Gear", "GG"},
    {"CPS 2", "CPS2"},
    {"CPS 1", "CPS1"},
    {"Dos", "DOS"},
    {"ScummVM", "SCUMMVM"},
    {"Famicom", "FC"},
    {"ZX Spectrum", "ZXS"},
    {"TurboGrafx16", "PCE"},
    {"CPS 3", "CPS3"},
    {"Neo Geo Pocket", "NGP"},
    {"Amiga", "AMIGA"},
    {"Sega 32X", "SEGA32X"},
    {"X68000", "X68000"},
    {"Atari 5200", "ATARI5200"},
    {"Atari ST", "ATARIST"},
    {"Atari Lynx", "LYNX"},
    {"Virtual Boy", "VB"},
    {"Atari 7800", "ATARI7800"},
    {"3DO", "PANASONIC"},
    {"PC-FX", "PCFX"},
    {"Wonderswan", "WS"},
    {"Wonderswan Color", "WSC"},

   // Romspedia mappings (added by audit)
    {"PSP ROMs", "PSP"},
    {"PSX ROMs", "PS"},
    {"NES ROMs", "FC"},
    {"GBC ROMs", "GBC"},
    {"Megadrive ROMs", "MD"},
    {"ZX Spectrum ROMs", "ZXS"},
    {"Dreamcast ROMs", "DC"},
    {"Amstrad CPC ROMs", "CPC"},
    {"Dos ROMs", "DOS"},
    {"Amiga ROMs", "AMIGA"},
    {"Neo Geo ROMs", "NEOGEO"},
    {"Sega CD ROMs", "SEGACD"},
    {"GB ROMs", "GB"},
    {"Atari ST ROMs", "ATARIST"},
    {"X68000 ROMs", "X68000"},
    {"Sega Saturn ROMs", "SATURN"},
    {"NAOMI ROMs", "NAOMI"},
    {"Atari 2600 ROMs", "ATARI2600"},
    {"Game Gear ROMs", "GG"},
    {"Master System ROMs", "MS"},
    {"MSX ROMs", "MSX"},
    {"TurboGrafx16 ROMs", "PCE"},
    {"CPS 1 ROMs", "CPS1"},
    {"CPS 2 ROMs", "CPS2"},
    {"Satellaview ROMs", "SATELLAVIEW"},
    {"Intellivision ROMs", "INTELLIVISION"},
    {"32X ROMs", "SEGA32X"},
    {"ColecoVision ROMs", "COLECO"},
    {"MSX2 ROMs", "MSX2"},
    {"Famicom ROMs", "FC"},
    {"3DO ROMs", "PANASONIC"},
    {"ScummVM ROMs", "SCUMMVM"},
    {"Vectrex ROMs", "VECTREX"},
    {"Atari Lynx ROMs", "LYNX"},
    {"Atari 5200 ROMs", "ATARI5200"},
    {"Neo Geo Pocket ROMs", "NGP"},
    {"Atari 7800 ROMs", "ATARI7800"},
    {"Wonderswan Color ROMs", "WSC"},
    {"Wonderswan ROMs", "WS"},
};

// Helper: strip trailing bracketed numbers, e.g. "Gameboy Color (GBC) (1094)" -> "Gameboy Color (GBC)"
inline std::string stripBracketedCount(const std::string& name) {
    size_t pos = name.find_last_of('(');
    if (pos != std::string::npos && name.back() == ')') {
        // Check if the part inside the brackets is a number
        size_t numStart = pos + 1;
        size_t numLen = name.size() - numStart - 1;
        if (numLen > 0 && std::all_of(name.begin() + numStart, name.begin() + numStart + numLen, ::isdigit)) {
            return name.substr(0, pos - 1); // Remove space before (
        }
    }
    return name;
}

// Fuzzy match: try exact, then stripped, then substring/abbreviation
inline std::string getFolderForScrapedConsole(const std::string& scrapedName) {
    std::string result;
    // 1. Exact match
    auto it = SCRAPED_TO_FOLDER.find(scrapedName);
    if (it != SCRAPED_TO_FOLDER.end()) {
        result = it->second;
    } else {
        // 2. Strip trailing bracketed count
        std::string stripped = stripBracketedCount(scrapedName);
        it = SCRAPED_TO_FOLDER.find(stripped);
        if (it != SCRAPED_TO_FOLDER.end()) {
            result = it->second;
        } else {
            // 3. Substring/abbreviation fuzzy match
            for (const auto& pair : SCRAPED_TO_FOLDER) {
                if (scrapedName.find(pair.first) != std::string::npos ||
                    stripped.find(pair.first) != std::string::npos ||
                    pair.first.find(scrapedName) != std::string::npos) {
                    result = pair.second;
                    break;
                }
            }
            // 4. Try matching by abbreviation in parentheses
            if (result.empty()) {
                size_t l = scrapedName.find('('), r = scrapedName.find(')');
                if (l != std::string::npos && r != std::string::npos && r > l + 1) {
                    std::string abbr = scrapedName.substr(l + 1, r - l - 1);
                    for (const auto& pair : SCRAPED_TO_FOLDER) {
                        if (pair.first.find(abbr) != std::string::npos) { result = pair.second; break; }
                    }
                }
            }
        }
    }
    // Debug print only when the console name changes to avoid flooding
    static std::string lastLoggedName;
    if (scrapedName != lastLoggedName) {
        printf("[ConsoleMap] '%s' -> '%s'\n", scrapedName.c_str(), result.c_str());
        lastLoggedName = scrapedName;
    }
    return result;
}