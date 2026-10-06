#include "console_manager.hpp"
#include "table_layout.hpp"

int ConsoleManager::GetConsoleWidth() {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut == INVALID_HANDLE_VALUE) return 120;
        CONSOLE_SCREEN_BUFFER_INFO csbi{};
        if (!GetConsoleScreenBufferInfo(hOut, &csbi)) return 120;
        return static_cast<int>(csbi.srWindow.Right - csbi.srWindow.Left + 1);
    }

TableLayout ConsoleManager::BuildTableLayout(int consoleWidth, LayoutMode layoutMode) {
        TableLayout layout{};
        if (layoutMode == LayoutMode::Compact) {
            layout.runWidth = 4;
            layout.blkWidth = 4;
            layout.thrWidth = 8;
            layout.memWidth = 10;
            layout.rateWidth = 6;
            layout.inWidth = 6;
            layout.cpuWidth = 4;
            layout.compactBanner = true;
            layout.splitRows = true;
            return layout;
        }

        if (consoleWidth < 105) {
            layout.runWidth = 3;
            layout.blkWidth = 3;
            layout.thrWidth = 6;
            layout.memWidth = 8;
            layout.rateWidth = 5;
            layout.inWidth = 4;
            layout.cpuWidth = 3;
            layout.compactBanner = true;
            layout.splitRows = true;
        } else if (consoleWidth < 130) {
            layout.runWidth = 4;
            layout.blkWidth = 4;
            layout.thrWidth = 8;
            layout.memWidth = 10;
            layout.rateWidth = 6;
            layout.inWidth = 6;
            layout.cpuWidth = 4;
            layout.compactBanner = true;
            layout.splitRows = false;
        } else {
            layout.runWidth = 6;
            layout.blkWidth = 6;
            layout.thrWidth = 10;
            layout.memWidth = 12;
            layout.rateWidth = 8;
            layout.inWidth = 8;
            layout.cpuWidth = 5;
            layout.compactBanner = (consoleWidth < 140);
            layout.splitRows = false;
        }

        if (layoutMode == LayoutMode::SingleLine) {
            layout.splitRows = false;
        }

        return layout;
    }
