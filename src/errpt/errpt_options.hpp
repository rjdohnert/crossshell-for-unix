#ifndef ERRPT_OPTIONS_HPP
#define ERRPT_OPTIONS_HPP

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <string>

enum class OutputFormat {
    Table,
    Json,
    Csv
};

struct ErrptOptions {
    std::wstring logName = L"System";
    DWORD maxEntries = 50;
    bool showAll = false;
    bool showHelp = false;
    bool showVersion = false;
    OutputFormat outputFormat = OutputFormat::Table;
    std::wstring pipeCommand;
};

struct ParseResult {
    bool success = false;
    ErrptOptions options;
    std::wstring error;
};

#endif
