#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <sddl.h>

#pragma comment(lib, "advapi32.lib")

#include <iostream>
#include <vector>
#include <string>
#include <string_view>
#include <memory>
#include <optional>
#include <iomanip>
#include <algorithm>
#include <sstream>
