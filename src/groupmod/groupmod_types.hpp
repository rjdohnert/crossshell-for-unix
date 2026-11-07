#pragma once

#include <optional>
#include <string>

struct CommandOptions {
    std::wstring groupName;
    std::optional<std::wstring> newName;
    std::optional<std::wstring> comment;
    bool force = false;
    bool quiet = false;
    bool verbose = false;
    bool showHelp = false;
    bool showVersion = false;
    bool readFromStdin = false;
};

struct GroupModification {
    std::wstring groupName;
    std::optional<std::wstring> newName;
    std::optional<std::wstring> comment;
};

struct GroupModResult {
    unsigned long status = 0;
    bool success = false;
    bool changed = false;
    bool notFound = false;
};
