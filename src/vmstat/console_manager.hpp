#pragma once

#include "table_layout.hpp"
#include "vmstat.hpp"

class ConsoleManager {
public:
    static int GetConsoleWidth();

    static TableLayout BuildTableLayout(int consoleWidth, LayoutMode layoutMode);
};
