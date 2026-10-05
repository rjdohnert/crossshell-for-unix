/*
 * Copyright (c) 2025, R. J. Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "path_helper.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

#include <vector>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace dirtree {

bool ConsoleEnvironment::InitConsole() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return false;

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return false;
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    return SetConsoleMode(hOut, dwMode) != 0;
#else
    return true;
#endif
}

bool ConsoleEnvironment::ShouldUseColorByDefault() {
#ifdef _WIN32
    if (!_isatty(_fileno(stdout))) return false;

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return false;

    DWORD mode = 0;
    return GetConsoleMode(hOut, &mode) != 0;
#else
    return isatty(fileno(stdout));
#endif
}

std::string PathHelper::WideToUtf8(const std::wstring& ws) {
    if (ws.empty()) return std::string();
#ifdef _WIN32
    int count = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (count <= 0) return std::string();

    std::vector<char> out(static_cast<size_t>(count));
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, out.data(), count, nullptr, nullptr);
    return std::string(out.data());
#else
    std::string s(ws.begin(), ws.end());
    return s;
#endif
}

std::string PathHelper::PathToUtf8(const fs::path& p) {
    return WideToUtf8(p.wstring());
}

std::string PathHelper::FormatSize(std::uintmax_t size) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double s = static_cast<double>(size);
    int idx = 0;
    while (s >= 1024.0 && idx < 4) {
        s /= 1024.0;
        idx++;
    }
    std::ostringstream oss;
    if (idx == 0) {
        oss << size << " B";
    } else {
        oss << std::fixed << std::setprecision(1) << s << " " << units[idx];
    }
    return oss.str();
}

bool PathHelper::IsHidden(const fs::directory_entry& entry) {
#ifdef _WIN32
    std::wstring wpath = entry.path().wstring();
    DWORD attrs = GetFileAttributesW(wpath.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_HIDDEN)) {
        return true;
    }
#endif
    std::wstring filename = entry.path().filename().wstring();
    return (!filename.empty() && filename[0] == L'.');
}

std::string PathHelper::GetFileColor(const fs::directory_entry& entry) {
    if (entry.is_directory()) return Color::CYAN;
    if (entry.is_symlink()) return Color::MAGENTA;

    std::string ext = entry.path().extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".exe" || ext == ".bat" || ext == ".cmd" || ext == ".ps1" || ext == ".com") {
        return Color::GREEN;
    }
    if (ext == ".zip" || ext == ".tar" || ext == ".gz" || ext == ".7z" || ext == ".rar" || ext == ".iso") {
        return Color::RED;
    }
    if (ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".py" || ext == ".js" || ext == ".cs" || ext == ".rs" || ext == ".go") {
        return Color::YELLOW;
    }
    return Color::RESET;
}

} // namespace dirtree
