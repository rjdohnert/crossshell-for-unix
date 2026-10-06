#pragma once

#include "tail.hpp"

struct TailOptions {
    bool count_lines = true;
    bool from_start = false;
    long long count = 10;
    bool follow = false;
    bool retry = false;
    bool quiet = false;
    bool verbose = false;
    double sleep_interval_sec = 1.0;
    int max_unchanged_stats = 5;
    std::vector<std::wstring> files;
};
