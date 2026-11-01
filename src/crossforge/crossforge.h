#ifndef CROSSFORGE_H
#define CROSSFORGE_H

// Shared umbrella header: standard library + Win32 includes, the fs alias,
// version constants, and narrow->wide string helpers used across all modules.

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <iostream>
#include <vector>
#include <deque>
#include <string>
#include <memory>
#include <functional>
#include <map>
#include <unordered_map>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <cstring>
#include <cctype>
#include <thread>
#include <atomic>
#include <mutex>

namespace fs = std::filesystem;

extern const std::string APP_TITLE;
extern const std::string COPYRIGHT_NOTICE;

std::string ReadEnvironmentVariableString(const char* name);
std::wstring Utf8ToWide(const std::string& s);

#endif // CROSSFORGE_H
