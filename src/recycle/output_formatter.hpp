#pragma once

#include "recycle.hpp"
#include "wide_pipe_buffer.hpp"

class OutputFormatter {
public:
    static std::wstring CsvQuote(const std::wstring& value);

    static std::wstring JsonQuote(const std::wstring& value);

    static void EmitHeader(OutputFormat fmt);

    static void EmitItem(OutputFormat fmt, const std::wstring& path, bool success, bool verbose, bool quiet);

    static void EmitSummary(int successCount, int failureCount, bool quiet, OutputFormat fmt, size_t targetCount, bool verbose);
};
