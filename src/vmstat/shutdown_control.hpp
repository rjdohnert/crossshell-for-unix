#pragma once

#include "vmstat.hpp"

extern volatile bool g_running;
extern std::condition_variable g_shutdownCV;
extern std::mutex g_shutdownMutex;

BOOL WINAPI GlobalConsoleHandler(DWORD signal);
