#pragma once

#include "threads.hpp"

enum class OutputType { Table, CSV, JSON, Pipe };
enum class SortColumn { Threads, PID, Name };

struct AppConfig {
    OutputType format = OutputType::Table;
    SortColumn sortBy = SortColumn::Threads;
    bool sortDescending = true;
    size_t topN = 0;               // 0 = all
    DWORD minThreads = 0;          // Filter out processes below this count
    std::wstring filterName = L"";
    DWORD filterPid = 0;
    bool showHelp = false;
    bool showVersion = false;
    std::wstring pipeCommand = L"";
};
