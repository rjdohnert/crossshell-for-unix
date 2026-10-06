#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#include <sys/stat.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <stdexcept>
#include <memory>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")


constexpr unsigned int MASK_ALL_BITS = 0777;
constexpr unsigned int DEFAULT_MASK  = 0022;
