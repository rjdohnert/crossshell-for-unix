/*
 * BSD 3-Clause License
 * Copyright (c) 2026, Roberto J Dohnert
 */

#include "dskctl_security.hpp"

#include "dskctl_helpers.hpp"

#include <windows.h>

#include <algorithm>
#include <limits>
#include <string_view>
#include <system_error>

namespace fs = std::filesystem;

namespace {

std::wstring NormalizeSeparators(std::wstring value) {
    std::replace(value.begin(), value.end(), L'/', L'\\');
    while (value.size() > 3 && value.back() == L'\\') value.pop_back();
    return value;
}

bool OrdinalEquals(std::wstring_view left, std::wstring_view right) {
    return left.size() <= static_cast<std::size_t>((std::numeric_limits<int>::max)()) &&
           right.size() <= static_cast<std::size_t>((std::numeric_limits<int>::max)()) &&
           CompareStringOrdinal(left.data(), static_cast<int>(left.size()),
                                right.data(), static_cast<int>(right.size()),
                                TRUE) == CSTR_EQUAL;
}

bool IsWithinRoot(const fs::path& canonicalPath, const fs::path& canonicalRoot) {
    std::wstring pathText = NormalizeSeparators(canonicalPath.native());
    std::wstring rootText = NormalizeSeparators(canonicalRoot.native());
    if (pathText.size() < rootText.size() ||
        !OrdinalEquals(std::wstring_view(pathText).substr(0, rootText.size()),
                       rootText)) {
        return false;
    }
    return pathText.size() == rootText.size() ||
           (!rootText.empty() && rootText.back() == L'\\') ||
           pathText[rootText.size()] == L'\\';
}

} // namespace

bool IsProcessElevated(std::string& error) {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        error = "OpenProcessToken failed: " + FormatWin32Error(GetLastError());
        return false;
    }

    TOKEN_ELEVATION elevation{};
    DWORD returnedSize = 0;
    BOOL queried = GetTokenInformation(token, TokenElevation, &elevation,
                                       sizeof(elevation), &returnedSize);
    DWORD queryError = queried ? ERROR_SUCCESS : GetLastError();
    CloseHandle(token);
    if (!queried) {
        error = "GetTokenInformation failed: " + FormatWin32Error(queryError);
        return false;
    }
    if (returnedSize < sizeof(elevation)) {
        error = "GetTokenInformation returned an incomplete elevation result.";
        return false;
    }
    return elevation.TokenIsElevated != 0;
}

bool IsPathWithinApprovedRoots(
    const fs::path& path, const std::vector<fs::path>& approvedRoots,
    std::string& error) {
    if (path.empty()) {
        error = "Refusing an empty cleanup path.";
        return false;
    }

    std::error_code pathError;
    fs::path canonicalPath = fs::weakly_canonical(path, pathError);
    if (pathError || !canonicalPath.is_absolute()) {
        error = "Cannot safely canonicalize cleanup path " + PathToUtf8(path) +
                (pathError ? ": " + pathError.message() : ".");
        return false;
    }

    for (const fs::path& root : approvedRoots) {
        std::error_code rootError;
        fs::path canonicalRoot = fs::weakly_canonical(root, rootError);
        if (!rootError && canonicalRoot.is_absolute() &&
            IsWithinRoot(canonicalPath, canonicalRoot)) {
            return true;
        }
    }

    error = "Refusing cleanup outside a trusted cleanup root: " +
            PathToUtf8(canonicalPath);
    return false;
}
