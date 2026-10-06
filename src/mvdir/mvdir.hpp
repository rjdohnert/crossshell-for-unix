#ifndef MVDIR_HPP
#define MVDIR_HPP

#include <iostream>
#include <filesystem>
#include <string>
#include <system_error>
#include <windows.h>
#include <io.h>
#include <fcntl.h>

namespace fs = std::filesystem;

constexpr wchar_t PROG_NAME[] = L"mvdir";
constexpr wchar_t VERSION[]   = L"1.0.0";

class StringConverter {
public:
    static std::wstring ToWide(const std::string& str);
};

class PathValidator {
public:
    static fs::path NormalizeTrailingSeparator(const fs::path& p);
    static bool IsDotOrDotDot(const fs::path& p);
    static bool IsSubpathOrEqual(const fs::path& parent, const fs::path& child);
};

#endif // MVDIR_HPP
