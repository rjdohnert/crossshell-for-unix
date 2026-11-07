/*
 * BSD 3-Clause License
 * Copyright (c) 2026, Roberto J Dohnert
 */

#include "driverctl_utilities.hpp"

#include <windows.h>

#include <limits>
#include <system_error>

std::string ToUtf8(std::wstring_view value) {
    if (value.empty() ||
        value.size() > static_cast<size_t>((std::numeric_limits<int>::max)())) {
        return {};
    }
    int inputLength = static_cast<int>(value.size());
    int outputLength = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                           value.data(), inputLength,
                                           nullptr, 0, nullptr, nullptr);
    if (outputLength <= 0) return {};

    std::string output(static_cast<size_t>(outputLength), '\0');
    int written = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                      value.data(), inputLength,
                                      output.data(), outputLength,
                                      nullptr, nullptr);
    if (written != outputLength) return {};
    return output;
}

ElevationStatus GetElevationStatus() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return {false, false, GetLastError()};
    }

    TOKEN_ELEVATION elevation{};
    DWORD returnedSize = 0;
    if (!GetTokenInformation(token, TokenElevation, &elevation,
                             sizeof(elevation), &returnedSize)) {
        DWORD error = GetLastError();
        CloseHandle(token);
        return {false, false, error};
    }
    CloseHandle(token);
    return {true, elevation.TokenIsElevated != 0, 0};
}

std::string FormatWin32Error(unsigned long errorCode) {
    return std::system_category().message(static_cast<int>(errorCode));
}
