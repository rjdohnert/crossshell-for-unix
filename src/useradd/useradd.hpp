#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <lm.h>
#include <lmaccess.h>
#include <io.h>
#include <fcntl.h>

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <memory>
#include <optional>
#include <filesystem>
#include <iomanip>
#include <algorithm>

#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "advapi32.lib")

namespace fs = std::filesystem;
