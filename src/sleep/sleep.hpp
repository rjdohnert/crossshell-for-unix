#ifndef SLEEP_HPP
#define SLEEP_HPP

#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <cctype>
#include <cstdlib>
#include <cwchar>
#include <cwctype>
#include <cstdio>
#include <sstream>
#include <windows.h>

enum class OutputFormat {
    None = 0,
    Json,
    Csv,
    Tsv,
    Table
};

struct SleepOptions {
    bool show_help = false;
    bool show_version = false;
    bool quiet = false;
    bool verbose = false;
    OutputFormat format = OutputFormat::None;
    std::wstring pipe_command;
    std::vector<std::wstring> raw_delays;
    double explicit_seconds = 0.0;
    bool has_explicit = false;
};

#endif // SLEEP_HPP
