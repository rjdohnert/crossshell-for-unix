#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <memory>


enum class DelimitMode {
    WHITESPACE,
    NEWLINE,
    NULL_CHAR,
    DELIMITER
};
