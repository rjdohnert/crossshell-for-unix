#pragma once

#include "rm.hpp"

enum class InteractiveMode {
    Never,
    Once,
    Always
};

struct RmOptions {
    bool force{false};
    bool recursive{false};
    bool removeEmptyDirs{false};
    bool verbose{false};
    bool dryRun{false};
    bool preserveRoot{true};
    bool nullDelimited{false};
    bool readStdin{false};
    bool showHelp{false};
    bool showVersion{false};
    InteractiveMode interactive{InteractiveMode::Never};

    std::vector<std::string> targets;
};
