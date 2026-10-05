#pragma once

#include <windows.h>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

struct LnOptions {
    bool symbolic = false;        // -s
    bool force = false;           // -f
    bool interactive = false;     // -i
    bool verbose = false;         // -v
    bool no_deref = false;        // -n, --no-dereference
    bool force_dir = false;       // -F
    bool relative = false;        // -r
    bool show_help = false;
    bool show_version = false;
    std::vector<fs::path> files;
};
