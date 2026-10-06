#pragma once

#include "supervisord.hpp"

class Logger {
    static inline std::mutex g_logMtx;
public:
    static void Log(const std::string& prefix, const std::string& message);
};
