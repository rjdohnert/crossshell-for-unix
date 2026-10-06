#pragma once

#include "read.hpp"
#include "options.hpp"

class ReadReporter {
public:
    static std::wstring jsonQuote(const std::wstring& value);
    static std::wstring csvQuote(const std::wstring& value);
    static void writeHandle(HANDLE hHandle, const std::wstring& text);
    static int output(const std::wstring& result, const ReadOptions& opts, int exitCode);
};
