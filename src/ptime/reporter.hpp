#pragma once

#include "ptime.hpp"
#include "options.hpp"

class PtimeReporter {
public:
    static std::wstring FormatSeconds(double sec);
    static std::wstring FormatHuman(double sec);
    static void Report(const TimeOptions& opts, const ProcessTimeMetrics& metrics);

private:
    static void WriteOutput(const TimeOptions& opts, const std::wstring& text);
};
