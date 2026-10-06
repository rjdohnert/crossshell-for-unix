#pragma once

#include "vmstat.hpp"

struct TableLayout {
    int runWidth = 4;
    int blkWidth = 4;
    int thrWidth = 8;
    int memWidth = 10;
    int rateWidth = 6;
    int inWidth = 6;
    int cpuWidth = 4;
    bool compactBanner = false;
    bool splitRows = false;
};
