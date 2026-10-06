#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cstdio>
#include <sstream>
#include <io.h>
#include <streambuf>

namespace fs = std::filesystem;

enum class OutputFormat { Human, Json, Csv, Table };
