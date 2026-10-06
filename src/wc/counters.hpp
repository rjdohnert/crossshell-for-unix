#pragma once

#include "wc.hpp"

struct Counters {
    long long lines = 0;
    long long words = 0;
    long long bytes = 0;
    long long chars = 0;
    long long max_line_length = 0;
};

enum class OutputFormat { Human, Json, Csv, Table };
