#pragma once

#include "tail.hpp"

class FileTailer {
public:
    static bool TailLines(HANDLE hFile, long long count, bool from_start);

    static bool TailBytes(HANDLE hFile, long long count, bool from_start);
};
