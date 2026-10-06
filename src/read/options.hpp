#pragma once

#include "read.hpp"

class ReadOptions {
public:
    std::wstring prompt{L""};
    bool silent{false};
    bool raw{false};
    int maxChars{-1};         // -1 = unlimited
    double timeoutSec{-1.0};   // -1.0 = no timeout
    wchar_t delim{L'\n'};
    ReadOutputFormat outputFormat{ReadOutputFormat::Human};
    std::wstring pipeCommand;

    static void printUsage();
    static void printVersion();
    static bool parse(int argc, wchar_t* argv[], ReadOptions& opts);
};
