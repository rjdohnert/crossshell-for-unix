#pragma once

#include "strace.hpp"
#include "trace_options.hpp"

class TraceReporter {
public:
    static std::wstring FormatIpPrefix(const TraceOptions& options, HANDLE hThread, DWORD64 defaultIp);
    static void EmitTraceLine(TraceOptions& options, const std::wstring& line, HANDLE hThread = NULL, DWORD64 defaultIp = 0, bool isEvent = true);
    static bool ShouldPrintEvent(const TraceOptions& options, const std::wstring& category);
    static void PrintSummary(const std::unordered_map<std::wstring, SummaryStats>& summaryData);
};
