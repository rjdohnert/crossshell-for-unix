#pragma once

#include "realpath.hpp"

enum class CanonicalMode {
    Existing,    // -e / --canonicalize-existing (all components must exist)
    Missing,     // -m / --canonicalize-missing  (components need not exist)
    NoSymlinks   // -s / --strip / --no-symlinks (do not expand symlinks)
};

class RealpathOptions {
public:
    static constexpr std::string_view PROGRAM_NAME = "realpath";
    static constexpr std::string_view VERSION = "7.0.0";

    CanonicalMode mode{CanonicalMode::Existing};
    bool quiet{false};
    bool zero{false};
    std::optional<fs::path> relativeTo;
    std::optional<fs::path> relativeBase;
    std::vector<std::string> files;

    static void printVersion();

    static void printHelp();

    static bool parse(int argc, char* argv[], RealpathOptions& opts);
};
