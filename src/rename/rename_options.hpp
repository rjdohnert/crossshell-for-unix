#pragma once

#include "rename.hpp"

struct RenameOptions {
    bool verbose       = false; // -v, --verbose
    bool dryRun        = false; // -n, --no-act
    bool interactive   = false; // -i, --interactive
    bool noOverwrite   = false; // -o, --no-overwrite
    bool replaceAll    = false; // -a, --all
    bool replaceLast   = false; // -l, --last
    bool useRegex      = false; // -e, --regex
    bool ignoreCase    = false; // -c, --ignore-case
    bool showHelp      = false; // -h, --help
    bool showVersion   = false; // -V, --version

    std::string searchPattern;
    std::string replacement;
    std::vector<fs::path> targetFiles;
};
