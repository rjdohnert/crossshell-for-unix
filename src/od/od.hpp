#ifndef OD_HPP
#define OD_HPP

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <fcntl.h>
#include <io.h>
#include <cstdio>
#include <memory>

enum class FormatKind {
    ASCII_NAMED,  // -a
    CHAR_ESCAPE,  // -c
    SIGNED_DEC,   // -d / -t d
    UNSIGNED_DEC, // -u / -t u
    OCTAL,        // -o / -t o
    HEX,          // -x / -t x
    FLOAT         // -f / -t f
};

struct FormatSpec {
    FormatKind kind;
    size_t size; // Byte size: 1, 2, 4, 8
};

#endif // OD_HPP
