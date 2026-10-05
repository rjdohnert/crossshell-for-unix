#pragma once

#include <string>

struct RuntimeConfig {
    enum class Format { Table, Json, Csv } format = Format::Table;
    bool showAdapters = true;
    bool showNetworks = true;
    bool triggerScan = false;
    bool useColor = true;
    int minSignal = 0;
    std::string ssidFilter = "";
    std::string bandFilter = "";

    bool Parse(int argc, char* argv[]);
    static void PrintHelp();
    static void PrintVersion();
};
