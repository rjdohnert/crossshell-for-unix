#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winternl.h>
#include <iphlpapi.h>
#include <winioctl.h>
#include <setupapi.h>
#include <ntddstor.h>
#include <psapi.h>

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <functional>
#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <ctime>
#include <cstring>
#include <limits>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "psapi.lib")

namespace sysctl {

void ThrowWindowsError(DWORD error, const std::string& operation);
std::string Trim(const std::string& text);
std::string WideToUtf8(const std::wstring& text);
std::wstring Utf8ToWide(const std::string& text);

} // namespace sysctl
