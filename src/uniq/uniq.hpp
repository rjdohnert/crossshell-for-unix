#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#include <fcntl.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <string_view>
#include <memory>
#include <algorithm>
#include <iomanip>
#include <cstdint>
#include <sstream>
#include <cctype>
#include <optional>


enum class AllRepeatedMode {
    None,       // Default / inactive
    Separate,   // Separate groups with an empty line
    Prepend     // Prepend an empty line before each group
};

enum class GroupMode {
    None,
    Separate,   // Separate groups with an empty line (default)
    Prepend,    // Prepend empty line before every group
    Append,     // Append empty line after every group
    Both        // Prepend and append empty line
};
