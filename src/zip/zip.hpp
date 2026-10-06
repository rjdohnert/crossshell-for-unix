#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <cstdlib>
#include <cstdio>
#include <streambuf>

namespace fs = std::filesystem;

enum class OutputFormat { Human, Json, Csv, Table };
