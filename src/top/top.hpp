#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <conio.h>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>
#include <unordered_map>
#include <memory>
#include <cmath>
#include <thread>
#include <ctime>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "advapi32.lib")
