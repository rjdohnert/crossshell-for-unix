#pragma once

#include "ps.hpp"

enum class OutputMode { TABLE, CSV, JSON, XML };

struct PsConfig {
    bool showAll = false;
    bool fullFormat = false;
    bool longFormat = false;
    bool showHelp = false;
    bool parseError = false;
    OutputMode mode = OutputMode::TABLE;
    std::wstring userFilter;
    std::set<DWORD> pidFilter;
    std::vector<std::wstring> customColumns;
};

class CommandLineParser {
public:
    static PsConfig parse(int argc, wchar_t* argv[]);
};
