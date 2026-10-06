#pragma once

#include "sync.hpp"

struct CliOptions {
    bool file_system = false;   // -f, --file-system
    bool data_only = false;     // -d, --data
    bool removable_only = false;// -r, --removable
    bool verbose = false;       // -v, --verbose
    bool quiet = false;         // -q, --quiet
    bool show_help = false;     // -h, --help
    bool show_version = false;  // -V, --version
    std::vector<std::string> targets;
};
