#pragma once

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <regex>
#include <memory>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;
