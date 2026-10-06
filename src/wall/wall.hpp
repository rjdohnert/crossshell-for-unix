#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wtsapi32.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <lmcons.h>
#include <memory>

#pragma comment(lib, "wtsapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")
