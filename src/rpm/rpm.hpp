#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <aclapi.h>
#include <sddl.h>
#include <shlobj.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <iomanip>
#include <map>
#include <set>
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <system_error>
#include <cctype>

#if defined(USE_LIBARCHIVE)
#include <archive.h>
#include <archive_entry.h>
#pragma comment(lib, "archive.lib")
#endif

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

namespace fs = std::filesystem;
