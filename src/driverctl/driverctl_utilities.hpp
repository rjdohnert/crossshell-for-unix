#ifndef DRIVERCTL_UTILITIES_HPP
#define DRIVERCTL_UTILITIES_HPP

#include <string>
#include <string_view>

/* Convert a Windows UTF-16 string to UTF-8, returning empty on invalid input. */
std::string ToUtf8(std::wstring_view value);

struct ElevationStatus {
    bool querySucceeded;
    bool elevated;
    unsigned long errorCode;
};

/* Query the current process token's UAC elevation state and preserve failures. */
ElevationStatus GetElevationStatus();

/* Format a Win32 error code for user-facing diagnostics. */
std::string FormatWin32Error(unsigned long errorCode);

#endif
