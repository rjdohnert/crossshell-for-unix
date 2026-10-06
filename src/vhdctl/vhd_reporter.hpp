#pragma once

#include "vhdctl.hpp"

class VhdReporter {
public:
    static void EmitResult(OutputFormat format, const std::wstring& action, const std::wstring& path, const std::wstring& detail);

    static void PrintUsage();
    static void PrintVersion();
};
