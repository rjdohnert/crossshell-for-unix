#pragma once

#include <windows.h>

#include <cstddef>
#include <optional>
#include <string>

enum class OutputFormat {
    table,
    csv,
    json,
    tsv,
    pipe
};

enum class SortColumn {
    handles,
    pid,
    name
};

enum class HandleCountStatus {
    available,
    accessDenied,
    unavailable
};

struct ProcessHandleInfo {
    DWORD pid = 0;
    DWORD handleCount = 0;
    std::wstring name;
    HandleCountStatus status = HandleCountStatus::unavailable;
};

struct AppConfig {
    OutputFormat format = OutputFormat::table;
    SortColumn sortBy = SortColumn::handles;
    bool sortDescending = true;
    size_t topN = 0;
    std::wstring filterName;
    std::optional<DWORD> filterPid;
    std::wstring pipeCommand;
    bool showHelp = false;
    bool showVersion = false;
};
