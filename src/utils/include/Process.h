#pragma once
#include <string>
#include <vector>

// Runs external programs (curl, wget, 7z, unzip) directly, never through a shell.
//
// Arguments are passed as a list, so URLs and file names coming from websites
// can't be interpreted as shell syntax ($(...), backticks, quotes, ;) and run
// as commands.
namespace Process {
    // Runs args[0] (looked up in PATH) with the remaining arguments and waits.
    // If `out` is given, the program's stdout is captured into it.
    // If `quiet`, stdout/stderr that aren't captured are discarded.
    // Returns the exit code, or -1 if the program couldn't be started.
    int Run(const std::vector<std::string>& args, std::string* out = nullptr, bool quiet = false);

    // True if `program` can be found in PATH.
    bool Exists(const std::string& program);
}
