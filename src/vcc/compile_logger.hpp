#pragma once

#include "vcc.hpp"

class CompileLogger {
public:
    static std::string GetLogPath();

    static std::string GetTimestamp();

    static void LogInvocation(const std::string& commandLine);

    static void LogResult(int exitCode);
};
