#ifndef MKBOOT_HPP
#define MKBOOT_HPP

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlwapi.h>
#include <imapi2.h>
#include <imapi2fs.h>
#include <wrl/client.h>
#include <comdef.h>

#if __has_include(<DismApi.h>) || __has_include(<dismapi.h>)
#define MKBOOT_HAS_DISM 1
#if __has_include(<DismApi.h>)
#include <DismApi.h>
#else
#include <dismapi.h>
#endif
#else
#define MKBOOT_HAS_DISM 0
#endif

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <iomanip>
#include <atomic>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "advapi32.lib")
#if MKBOOT_HAS_DISM
#pragma comment(lib, "dismapi.lib")
#endif
#pragma comment(lib, "comsuppw.lib")

using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;

extern std::atomic<bool> g_AbortRequested;

BOOL WINAPI ConsoleControlHandler(DWORD signal);

enum class LogLevel { Debug, Info, Warning, Error };

class Logger {
public:
    static void SetVerbose(bool enable);
    static void Log(LogLevel level, const std::string& message);
private:
    static bool s_Verbose;
};

std::wstring StringToWString(const std::string& str);
std::string GetHResultErrorMessage(HRESULT hr);
bool IsElevated();
bool HasSufficientDiskSpace(const fs::path& targetPath, ULONGLONG requiredBytes);

class ScopedCOM {
public:
    ScopedCOM();
    ~ScopedCOM();
    bool IsSucceeded() const;
    HRESULT GetResult() const;
private:
    HRESULT m_hr;
};

class ScopedDism {
public:
    ScopedDism();
    ~ScopedDism();
    bool IsSucceeded() const;
    HRESULT GetResult() const;
private:
    HRESULT m_hr;
};

class ScopedTempDir {
public:
    ScopedTempDir();
    ~ScopedTempDir();
    const fs::path& GetPath() const;
private:
    fs::path m_Path;
};

#endif // MKBOOT_HPP
