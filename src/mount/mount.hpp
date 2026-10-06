#ifndef MOUNT_HPP
#define MOUNT_HPP

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <winnetwk.h>
#include <virtdisk.h>
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
#include <algorithm>

#pragma comment(lib, "mpr.lib")
#pragma comment(lib, "virtdisk.lib")
#pragma comment(lib, "advapi32.lib")

class AdminValidator {
public:
    static bool IsRunningAsAdmin();
};

class SystemErrorResolver {
public:
    static std::wstring GetSystemErrorMessage(DWORD errorCode);
};

class StringUtils {
public:
    static std::wstring TrimQuotes(const std::wstring& str);
};

#endif // MOUNT_HPP
