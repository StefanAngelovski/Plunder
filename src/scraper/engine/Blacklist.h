#pragma once
#include <string>

// Consoles hidden from each site (res/blacklist.txt).
//
// The file has one section per site ("# Hexrom unsupported consoles", ...).
// Hexrom has always been checked against every line of the file; other sites
// only against their own section. Names are compared after Normalize().
namespace Blacklist {
    // "GBA ROMs (1963)" -> "gba": bracketed parts, ROM/ROMs/ISO/& removed, lower-case.
    std::string Normalize(const std::string& consoleLabel);

    bool Contains(const std::string& site, const std::string& consoleLabel, bool wholeFile);
}
