#pragma once

#include "ptime.hpp"
#include "options.hpp"

class ProcessTimerEngine {
public:
    static std::wstring BuildCommandLine(const std::vector<std::wstring>& args);
    static int ExecuteAndMeasure(int argc, wchar_t* argv[], const TimeOptions& options, ProcessTimeMetrics& metrics);
};
