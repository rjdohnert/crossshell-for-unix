#pragma once

#include "pskill.hpp"

class PskillOptions {
public:
    std::vector<std::wstring> targets;
    SignalType signal = SIG_TERM;
    bool force = false;
    bool tree = false;
    bool exact = false;
    bool caseSensitive = false;
    bool dryRun = false;
    bool verbose = false;
    bool quiet = false;
    std::wstring userFilter;
    bool showHelp = false;

    static std::wstring ToLower(std::wstring str);
    static bool TryParsePID(const std::wstring& str, DWORD& pid);
    int Parse(int argc, wchar_t* argv[]);
    void PrintHelp() const;
};
