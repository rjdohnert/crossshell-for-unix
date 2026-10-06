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
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <cstdint>
#include <sstream>
#include <unordered_map>
#include <optional>

namespace fs = std::filesystem;
