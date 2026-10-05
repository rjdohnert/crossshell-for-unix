/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 */

#ifndef LOGGER_OPTIONS_HPP
#define LOGGER_OPTIONS_HPP

#include <string>
#include <vector>

class LoggerOptions {
public:
    std::wstring tag = L"logger";
    std::vector<std::wstring> messageParts;
    bool showHelp = false;
    bool showVersion = false;

    static std::wstring JoinWords(const std::vector<std::wstring>& words, size_t startIndex);

    bool Parse(int argc, wchar_t* argv[]);
    void PrintUsage(const wchar_t* progName) const;
    void PrintVersion() const;
};

#endif // LOGGER_OPTIONS_HPP
