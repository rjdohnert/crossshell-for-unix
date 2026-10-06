#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <memory>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")


enum class SignalType {
    SIGHUP = 1,
    SIGINT = 2,
    SIGQUIT = 3,
    SIGstop = 9,
    SIGTERM = 15
};

enum class OutputFormat {
    None = 0,
    Json = 1,
    Csv = 2,
    Table = 3
};
