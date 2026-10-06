#pragma once

#ifndef UNICODE
#ifndef UNICODE
#define UNICODE
#endif
#endif
#ifndef _UNICODE
#ifndef _UNICODE
#define _UNICODE
#endif
#endif
#ifndef NOMINMAX
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <windows.h>
#include <winsvc.h>
#include <sddl.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <array>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <cctype>
#include <cstdlib>
#include <set>
#include <deque>
#include <unordered_set>
#include <random>
#include <functional>

#pragma comment(lib, "advapi32.lib")

namespace fs = std::filesystem;
extern bool g_IsServiceMode;
