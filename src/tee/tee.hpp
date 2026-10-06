#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <cwchar>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

namespace fs = std::filesystem;
