/*
 * BSD 3-Clause License
 * Copyright (c) 2026, Roberto J Dohnert
 */

#include "dskctl_helpers.hpp"

#include <knownfolders.h>
#include <shlobj.h>

#include <iomanip>
#include <iterator>
#include <limits>
#include <sstream>
#include <system_error>

std::string WideToUtf8(std::wstring_view value) {
    if (value.empty() ||
        value.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        return {};
    }

    int inputLength = static_cast<int>(value.size());
    int outputLength = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), inputLength,
        nullptr, 0, nullptr, nullptr);
    if (outputLength <= 0) return {};

    std::string output(static_cast<std::size_t>(outputLength), '\0');
    int written = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), inputLength,
        output.data(), outputLength, nullptr, nullptr);
    if (written != outputLength) return {};
    return output;
}

std::string PathToUtf8(const std::filesystem::path& path) {
    return WideToUtf8(path.native());
}

std::string FormatBytes(std::uint64_t bytes) {
    static constexpr const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    double value = static_cast<double>(bytes);
    std::size_t unit = 0;
    while (value >= 1024.0 && unit + 1 < std::size(units)) {
        value /= 1024.0;
        ++unit;
    }

    std::ostringstream output;
    output << std::fixed << std::setprecision(2) << value << ' ' << units[unit];
    return output.str();
}

std::string FormatWin32Error(DWORD errorCode) {
    return std::system_category().message(static_cast<int>(errorCode));
}

std::filesystem::path GetKnownFolderPath(REFKNOWNFOLDERID folderId,
                                         std::string& error) {
    PWSTR folder = nullptr;
    HRESULT result = SHGetKnownFolderPath(folderId, KF_FLAG_DEFAULT, nullptr, &folder);
    if (FAILED(result) || !folder) {
        error = "SHGetKnownFolderPath failed (HRESULT " +
                std::to_string(static_cast<unsigned long>(result)) + ").";
        if (folder) CoTaskMemFree(folder);
        return {};
    }

    std::filesystem::path path(folder);
    CoTaskMemFree(folder);
    return path;
}

std::filesystem::path GetWindowsDirectoryPath(std::string& error) {
    std::wstring buffer(256, L'\0');
    for (;;) {
        UINT length = GetWindowsDirectoryW(buffer.data(),
                                           static_cast<UINT>(buffer.size()));
        if (length == 0) {
            error = "GetWindowsDirectoryW failed: " +
                    FormatWin32Error(GetLastError());
            return {};
        }
        if (length < buffer.size()) {
            buffer.resize(length);
            return std::filesystem::path(buffer);
        }
        buffer.resize(static_cast<std::size_t>(length) + 1);
    }
}

bool QueryPathAttributes(const std::filesystem::path& path, DWORD& attributes,
                         std::string& error) {
    attributes = GetFileAttributesW(path.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES) return true;
    error = "GetFileAttributesW failed for " + PathToUtf8(path) + ": " +
            FormatWin32Error(GetLastError());
    return false;
}
