/**
 * BSD 3 Clause License
 * --------------------
 *
 * CrossShell for UNIX
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this list of conditions, and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice, this list of conditions, and the following disclaimer
 * in the documentation and/or other materials provided with the distribution. Neither the name of [project] nor the names of its
 * contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 *
 * Disclaimer:
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAG
 *
 * mktemp - High-performance, secure temporary file/directory creation for Windows.
 * Engineered for Windows NT architecture (C++20).
 *
 * Compilation (MSVC):
 *   cl /std:c++20 /O2 /W4 /EHsc /utf-8 mktemp.cpp
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <sddl.h>

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <filesystem>
#include <optional>
#include <algorithm>
#include <cstdint>

template <typename T>
class Span {
public:
    using value_type = T;
    using pointer = T*;
    using reference = T&;
    using iterator = pointer;
    using const_iterator = const T*;

    constexpr Span() noexcept : data_(nullptr), size_(0) {}
    constexpr Span(pointer data, size_t count) noexcept : data_(data), size_(count) {}

    constexpr pointer data() const noexcept { return data_; }
    constexpr size_t size() const noexcept { return size_; }
    constexpr bool empty() const noexcept { return size_ == 0; }
    constexpr reference operator[](size_t index) const noexcept { return data_[index]; }
    constexpr const_iterator begin() const noexcept { return data_; }
    constexpr const_iterator end() const noexcept { return data_ + size_; }

private:
    pointer data_;
    size_t size_;
};

constexpr bool StartsWith(std::wstring_view text, std::wstring_view prefix) noexcept {
    if (prefix.empty()) return true;
    if (text.size() < prefix.size()) return false;
    return text.compare(0, prefix.size(), prefix) == 0;
}

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Constants & Engine Configuration
// ---------------------------------------------------------------------------
constexpr std::wstring_view VERSION_STR = L"mktemp 2.0.0";
constexpr std::wstring_view DEFAULT_TEMPLATE = L"tmp.XXXXXXXXXX";
constexpr wchar_t ALPHANUM[] = L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
constexpr size_t ALPHANUM_LEN = 63;
constexpr uint8_t REJECT_LIMIT = static_cast<uint8_t>(256 - (256 % ALPHANUM_LEN)); // 252 (strips modulo bias)
constexpr int MAX_ATTEMPTS = 10000;
constexpr size_t STACK_RNG_BUFFER_SIZE = 256;
constexpr size_t STACK_OUT_BUFFER_SIZE = 1024;

// ---------------------------------------------------------------------------
// Program Options Structure
// ---------------------------------------------------------------------------
struct Options {
    bool make_dir = false;              // -d, --directory
    bool dry_run = false;               // -u, --dry-run
    bool quiet = false;                 // -q, --quiet
    bool treat_as_template = false;     // -t
    std::optional<std::wstring> tmpdir; // -p, --tmpdir[=DIR]
    std::wstring suffix;                // --suffix=SUFFIX
    bool explicit_suffix = false;
    std::wstring user_template;         // [TEMPLATE]
};

// ---------------------------------------------------------------------------
// Native NT Output Dispatcher (Zero Heap Allocation on Common Paths)
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Native NT Output Dispatcher (Zero Heap Allocation on Common Paths)
// ---------------------------------------------------------------------------
class NativeConsole {
public:
    static void Print(HANDLE hOut, std::wstring_view text, bool append_newline = true) noexcept {
        if (hOut == INVALID_HANDLE_VALUE || hOut == nullptr) return;

        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode)) {
            DWORD written = 0;
            WriteConsoleW(hOut, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
            if (append_newline) {
                WriteConsoleW(hOut, L"\n", 1, &written, nullptr);
            }
        } else {
            int utf8_len = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                               nullptr, 0, nullptr, nullptr);
            if (utf8_len > 0) {
                char stack_buf[STACK_OUT_BUFFER_SIZE];
                if (static_cast<size_t>(utf8_len) < sizeof(stack_buf)) {
                    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                        stack_buf, utf8_len, nullptr, nullptr);
                    DWORD written = 0;
                    WriteFile(hOut, stack_buf, static_cast<DWORD>(utf8_len), &written, nullptr);
                } else {
                    std::vector<char> heap_buf(static_cast<size_t>(utf8_len));
                    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                        heap_buf.data(), utf8_len, nullptr, nullptr);
                    DWORD written = 0;
                    WriteFile(hOut, heap_buf.data(), static_cast<DWORD>(utf8_len), &written, nullptr);
                }
            }
            if (append_newline) {
                DWORD written = 0;
                WriteFile(hOut, "\n", 1, &written, nullptr);
            }
        }
    }

    static void PrintOut(std::wstring_view text) noexcept {
        Print(GetStdHandle(STD_OUTPUT_HANDLE), text, true);
    }

    static void PrintErr(std::wstring_view text) noexcept {
        Print(GetStdHandle(STD_ERROR_HANDLE), text, true);
    }

    static void ReportError(bool quiet, const std::wstring& msg) {
        if (!quiet) {
            PrintErr(L"mktemp: " + msg);
        }
    }

    static void ReportWin32Error(bool quiet, const std::wstring& context, DWORD error_code) {
        if (quiet) return;

        wchar_t* msg_buf = nullptr;
        DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
        FormatMessageW(flags, nullptr, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       reinterpret_cast<LPWSTR>(&msg_buf), 0, nullptr);

        std::wstring err_str = msg_buf ? msg_buf : L"Unknown error";
        if (msg_buf) LocalFree(msg_buf);

        while (!err_str.empty() && (err_str.back() == L'\r' || err_str.back() == L'\n' || err_str.back() == L' ')) {
            err_str.pop_back();
        }

        PrintErr(L"mktemp: " + context + L": " + err_str);
    }
};

// ---------------------------------------------------------------------------
// NT Security: Explicit Protected Inheritable DACL (Fail-Closed Architecture)
// D:P              = Protected DACL (blocks inheritance from permissive parent)
// (A;OICI;GA;;;OW) = Object Inherit + Container Inherit + Generic All to Owner
// (A;OICI;GA;;;SY) = Object Inherit + Container Inherit + Generic All to System
// (A;OICI;GA;;;BA) = Object Inherit + Container Inherit + Generic All to Admins
// ---------------------------------------------------------------------------
class ScopedSecurityDescriptor {
public:
    ScopedSecurityDescriptor() noexcept {
        sa_.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa_.bInheritHandle = FALSE;
        sa_.lpSecurityDescriptor = nullptr;

        if (ConvertStringSecurityDescriptorToSecurityDescriptorW(
                L"D:P(A;OICI;GA;;;OW)(A;OICI;GA;;;SY)(A;OICI;GA;;;BA)",
                SDDL_REVISION_1,
                &sa_.lpSecurityDescriptor,
                nullptr)) {
            valid_ = true;
        }
    }

    ~ScopedSecurityDescriptor() noexcept {
        if (sa_.lpSecurityDescriptor) {
            LocalFree(sa_.lpSecurityDescriptor);
        }
    }

    [[nodiscard]] bool IsValid() const noexcept {
        return valid_;
    }

    [[nodiscard]] SECURITY_ATTRIBUTES* GetSA() noexcept {
        return valid_ ? &sa_ : nullptr;
    }

private:
    SECURITY_ATTRIBUTES sa_{};
    bool valid_ = false;
};

// ---------------------------------------------------------------------------
// Cryptographically Secure Character Generator (Zero Heap Allocations)
// ---------------------------------------------------------------------------
class FastSecureRng {
public:
    static bool FillBuffer(Span<uint8_t> buffer) noexcept {
        NTSTATUS status = BCryptGenRandom(
            nullptr,
            buffer.data(),
            static_cast<ULONG>(buffer.size()),
            BCRYPT_USE_SYSTEM_PREFERRED_RNG
        );
        return BCRYPT_SUCCESS(status);
    }

    static bool GenerateChars(Span<wchar_t> out_chars, Span<uint8_t> entropy_scratch) noexcept {
        const size_t needed = out_chars.size();
        size_t generated = 0;

        while (generated < needed) {
            if (!FillBuffer(entropy_scratch)) {
                return false;
            }
            for (uint8_t b : entropy_scratch) {
                if (b < REJECT_LIMIT) {
                    out_chars[generated++] = ALPHANUM[b % ALPHANUM_LEN];
                    if (generated == needed) break;
                }
            }
        }
        return true;
    }
};

// ---------------------------------------------------------------------------
// Path Sanitization & Extended NT Long-Path Resolver
// ---------------------------------------------------------------------------
class PathResolver {
public:
    static std::wstring StripQuotes(std::wstring str) {
        while (!str.empty() && (str.front() == L'"' || str.front() == L'\'')) {
            str.erase(0, 1);
        }
        while (!str.empty() && (str.back() == L'"' || str.back() == L'\'')) {
            str.pop_back();
        }
        return str;
    }

    static std::wstring ToExtendedKernelPath(const std::wstring& path) {
        wchar_t full_buf[32768];
        DWORD len = GetFullPathNameW(path.c_str(), static_cast<DWORD>(std::size(full_buf)), full_buf, nullptr);
        if (len == 0 || len >= std::size(full_buf)) {
            return path;
        }

        std::wstring_view full(full_buf, len);
        if (StartsWith(full, L"\\\\?\\")) {
            return std::wstring(full);
        }

        if (len >= 240) {
            if (StartsWith(full, L"\\\\")) {
                return L"\\\\?\\UNC\\" + std::wstring(full.substr(2));
            }
            return L"\\\\?\\" + std::wstring(full);
        }

        return path;
    }

    static std::wstring GetSystemTempDir() {
        wchar_t buf[32768];
        const wchar_t* env_vars[] = { L"TMPDIR", L"TMP", L"TEMP", L"USERPROFILE", L"HOME" };

        for (const wchar_t* var : env_vars) {
            DWORD len = GetEnvironmentVariableW(var, buf, static_cast<DWORD>(std::size(buf)));
            if (len > 0 && len < std::size(buf)) {
                std::wstring res = StripQuotes(std::wstring(buf, len));
                if (!res.empty()) return res;
            }
        }

        wchar_t home_drive[128] = L"";
        wchar_t home_path[32768] = L"";
        DWORD drive_len = GetEnvironmentVariableW(L"HOMEDRIVE", home_drive, static_cast<DWORD>(std::size(home_drive)));
        DWORD path_len = GetEnvironmentVariableW(L"HOMEPATH", home_path, static_cast<DWORD>(std::size(home_path)));
        if (drive_len > 0 && path_len > 0 && drive_len < std::size(home_drive) && path_len < std::size(home_path)) {
            std::wstring home = std::wstring(home_drive) + std::wstring(home_path);
            if (!home.empty()) return home;
        }

        DWORD len = GetTempPathW(static_cast<DWORD>(std::size(buf)), buf);
        if (len > 0 && len < std::size(buf)) {
            return StripQuotes(std::wstring(buf, len));
        }
        return L".";
    }
};

// ---------------------------------------------------------------------------
// Help, Version & Option Parsing
// ---------------------------------------------------------------------------
class OptionParser {
public:
    static void PrintVersion() noexcept {
        NativeConsole::PrintOut(VERSION_STR);
        NativeConsole::PrintOut(L"License: BSD-3 Clause license\n"
                                L"Copyright (C) 2026 Roberto J Dohnert All Rights Reserved\n");
    }

    static void PrintHelp(const wchar_t* prog_name) noexcept {
        std::wcout << LR"(mktemp(1)               CrossShell for UNIX Reference Manual                mktemp(1)

    NAME
        mktemp - create a temporary file or directory safely

    SYNOPSIS
        mktemp [OPTIONS] [TEMPLATE]

    DESCRIPTION
        Create a temporary file or directory safely and print its path. TEMPLATE
        must contain at least 3 consecutive 'X's in the last component. If
        TEMPLATE is not specified, use tmp.XXXXXXXXXX.

    OPTIONS
        -d, --directory
            Create a directory, not a file.

        -u, --dry-run
            Do not create anything; merely print a name (unsafe).

        -q, --quiet
            Suppress diagnostics about file/dir creation failure.

        -p DIR, --tmpdir[=DIR]
            Interpret TEMPLATE relative to DIR (%TEMP% fallback).

        --suffix=SUFFIX
            Append SUFFIX to TEMPLATE.

        -t
            Interpret TEMPLATE as a single file name component.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        mktemp -d
            Create temporary directory and print its path.

        mktemp --suffix=.txt tmp.XXXXXX
            Create temporary text file.

    CrossShell for UNIX                                                 mktemp(1)
)";
    }

    bool Parse(int argc, wchar_t* argv[], Options& opts, bool& showHelp, bool& showVersion) const {
        bool explicit_template = false;
        bool end_of_options = false;

        for (int i = 1; i < argc; ++i) {
            std::wstring_view arg = argv[i];

            if (!end_of_options && arg == L"--") {
                end_of_options = true;
                continue;
            }

            if (!end_of_options && StartsWith(arg, L"-") && arg != L"-") {
                if (arg == L"--help" || arg == L"-h" || arg == L"-?") {
                    showHelp = true;
                    return true;
                }
                if (arg == L"--version" || arg == L"-V") {
                    showVersion = true;
                    return true;
                }
                if (arg == L"-d" || arg == L"--directory") {
                    opts.make_dir = true;
                } else if (arg == L"-u" || arg == L"--dry-run") {
                    opts.dry_run = true;
                } else if (arg == L"-q" || arg == L"--quiet") {
                    opts.quiet = true;
                } else if (arg == L"-t") {
                    opts.treat_as_template = true;
                } else if (StartsWith(arg, L"--suffix=")) {
                    opts.suffix = arg.substr(9);
                    opts.explicit_suffix = true;
                } else if (arg == L"--suffix") {
                    if (i + 1 < argc) {
                        opts.suffix = argv[++i];
                        opts.explicit_suffix = true;
                    } else {
                        NativeConsole::PrintErr(L"mktemp: option '--suffix' requires an argument");
                        return false;
                    }
                } else if (StartsWith(arg, L"--tmpdir=")) {
                    opts.tmpdir = std::wstring(arg.substr(9));
                } else if (arg == L"--tmpdir") {
                    opts.tmpdir = L"";
                } else if (StartsWith(arg, L"-p")) {
                    if (arg.length() > 2) {
                        opts.tmpdir = std::wstring(arg.substr(2));
                    } else if (i + 1 < argc) {
                        opts.tmpdir = argv[++i];
                    } else {
                        NativeConsole::PrintErr(L"mktemp: option '-p' requires an argument");
                        return false;
                    }
                } else {
                    for (size_t c = 1; c < arg.length(); ++c) {
                        switch (arg[c]) {
                            case L'd': opts.make_dir = true; break;
                            case L'u': opts.dry_run = true; break;
                            case L'q': opts.quiet = true; break;
                            case L't': opts.treat_as_template = true; break;
                            case L'h': showHelp = true; return true;
                            case L'V': showVersion = true; return true;
                            case L'p':
                                if (c + 1 < arg.length()) {
                                    opts.tmpdir = std::wstring(arg.substr(c + 1));
                                    c = arg.length();
                                } else if (i + 1 < argc) {
                                    opts.tmpdir = argv[++i];
                                } else {
                                    NativeConsole::PrintErr(L"mktemp: option requires an argument -- 'p'");
                                    return false;
                                }
                                break;
                            default:
                                NativeConsole::PrintErr(std::wstring(L"mktemp: invalid option -- '") + arg[c] + L"'");
                                NativeConsole::PrintErr(L"Try 'mktemp --help' for more information.");
                                return false;
                        }
                    }
                }
            } else {
                if (!explicit_template) {
                    opts.user_template = arg;
                    explicit_template = true;
                } else {
                    NativeConsole::PrintErr(L"mktemp: too many templates provided");
                    NativeConsole::PrintErr(L"Try 'mktemp --help' for more information.");
                    return false;
                }
            }
        }

        if (!explicit_template) {
            opts.user_template = DEFAULT_TEMPLATE;
            if (!opts.tmpdir.has_value()) {
                opts.tmpdir = L"";
            }
        }

        return true;
    }
};

// ---------------------------------------------------------------------------
// Win32 Kernel I/O Creation Engine
// ---------------------------------------------------------------------------
class TempCreationEngine {
public:
    static bool TryCreateFile(const wchar_t* path, LPSECURITY_ATTRIBUTES sa, DWORD& out_err) noexcept {
        HANDLE hFile = CreateFileW(
            path,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            sa,
            CREATE_NEW,
            FILE_ATTRIBUTE_TEMPORARY,
            nullptr
        );

        if (hFile != INVALID_HANDLE_VALUE) {
            CloseHandle(hFile);
            return true;
        }
        out_err = GetLastError();
        return false;
    }

    static bool TryCreateDir(const wchar_t* path, LPSECURITY_ATTRIBUTES sa, DWORD& out_err) noexcept {
        if (CreateDirectoryW(path, sa)) {
            return true;
        }
        out_err = GetLastError();
        return false;
    }

    static int CreateTemp(Options& opts) {
        if (opts.treat_as_template) {
            size_t count = 0;
            for (wchar_t ch : opts.user_template) {
                if (ch == L'X') count++;
            }
            if (count < 3) {
                opts.user_template += L".XXXXXXXXXX";
            }
        }

        std::wstring full_template = opts.user_template;

        if (opts.explicit_suffix) {
            if (opts.suffix.find(L'/') != std::wstring::npos || opts.suffix.find(L'\\') != std::wstring::npos) {
                NativeConsole::ReportError(opts.quiet, L"invalid suffix: cannot contain path separators");
                return 1;
            }
            full_template += opts.suffix;
        }

        fs::path tpath(full_template);
        std::wstring filename = tpath.filename().wstring();

        size_t x_count = 0;
        size_t suffix_len = 0;

        if (opts.explicit_suffix) {
            suffix_len = opts.suffix.length();
            if (filename.length() < suffix_len) {
                NativeConsole::ReportError(opts.quiet, L"template is too short for given suffix");
                return 1;
            }
            size_t scan_end = filename.length() - suffix_len;
            while (scan_end > 0 && filename[scan_end - 1] == L'X') {
                x_count++;
                scan_end--;
            }
        } else {
            size_t idx = filename.length();
            while (idx > 0 && filename[idx - 1] != L'X') {
                idx--;
            }
            size_t x_end = idx;
            while (idx > 0 && filename[idx - 1] == L'X') {
                x_count++;
                idx--;
            }
            suffix_len = filename.length() - x_end;
        }

        if (x_count < 3) {
            NativeConsole::ReportError(opts.quiet, L"too few X's in template '" + full_template + L"' (minimum 3 required)");
            return 1;
        }

        fs::path final_parent_dir;
        if (opts.treat_as_template) {
            if (tpath.has_parent_path()) {
                NativeConsole::ReportError(opts.quiet, L"with -t, template must not contain directory separators");
                return 1;
            }
            final_parent_dir = (opts.tmpdir && !opts.tmpdir->empty()) ? *opts.tmpdir : PathResolver::GetSystemTempDir();
        } else if (opts.tmpdir.has_value()) {
            if (tpath.is_absolute()) {
                NativeConsole::ReportError(opts.quiet, L"template cannot be absolute when --tmpdir is specified");
                return 1;
            }
            std::wstring base_dir = opts.tmpdir->empty() ? PathResolver::GetSystemTempDir() : *opts.tmpdir;
            final_parent_dir = fs::path(base_dir) / tpath.parent_path();
        } else {
            final_parent_dir = tpath.parent_path();
        }

        fs::path target_path = final_parent_dir.empty() ? fs::path(filename) : (final_parent_dir / filename);
        std::wstring display_buf = target_path.wstring();
        std::wstring kernel_buf = PathResolver::ToExtendedKernelPath(display_buf);

        const size_t display_x_offset = display_buf.length() - suffix_len - x_count;
        const size_t kernel_x_offset = kernel_buf.length() - suffix_len - x_count;

        Span<wchar_t> display_x_slice(&display_buf[display_x_offset], x_count);
        Span<wchar_t> kernel_x_slice(&kernel_buf[kernel_x_offset], x_count);

        ScopedSecurityDescriptor sec_desc;
        if (!opts.dry_run && !sec_desc.IsValid()) {
            NativeConsole::ReportError(opts.quiet, L"failed to allocate secure security descriptor");
            return 1;
        }
        LPSECURITY_ATTRIBUTES sa = sec_desc.GetSA();

        const size_t needed_bytes = x_count * 2;
        std::array<uint8_t, STACK_RNG_BUFFER_SIZE> stack_entropy;
        std::vector<uint8_t> heap_entropy;
        Span<uint8_t> entropy_span;

        if (needed_bytes <= STACK_RNG_BUFFER_SIZE) {
            entropy_span = Span<uint8_t>(stack_entropy.data(), needed_bytes);
        } else {
            heap_entropy.resize(needed_bytes);
            entropy_span = Span<uint8_t>(heap_entropy.data(), needed_bytes);
        }

        DWORD last_error = 0;

        for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
            if (!FastSecureRng::GenerateChars(display_x_slice, entropy_span)) {
                NativeConsole::ReportError(opts.quiet, L"failed to acquire cryptographic entropy");
                return 1;
            }

            if (opts.dry_run) {
                NativeConsole::PrintOut(display_buf);
                return 0;
            }

            std::copy_n(display_x_slice.data(), x_count, kernel_x_slice.data());

            if (opts.make_dir) {
                if (TryCreateDir(kernel_buf.c_str(), sa, last_error)) {
                    NativeConsole::PrintOut(display_buf);
                    return 0;
                }
            } else {
                if (TryCreateFile(kernel_buf.c_str(), sa, last_error)) {
                    NativeConsole::PrintOut(display_buf);
                    return 0;
                }
            }

            if (last_error != ERROR_FILE_EXISTS && last_error != ERROR_ALREADY_EXISTS) {
                NativeConsole::ReportWin32Error(opts.quiet, L"failed to create " + std::wstring(opts.make_dir ? L"directory" : L"file") +
                                                            L" via template '" + full_template + L"'", last_error);
                return 1;
            }
        }

        NativeConsole::ReportError(opts.quiet, L"failed to create temporary " + std::wstring(opts.make_dir ? L"directory" : L"file") +
                                               L": all unique attempts exhausted");
        return 1;
    }
};

// ---------------------------------------------------------------------------
// Application Controller & Entry Point
// ---------------------------------------------------------------------------
class MktempApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]) {
        Options opts;
        bool showHelp = false;
        bool showVersion = false;

        if (!m_parser.Parse(argc, argv, opts, showHelp, showVersion)) {
            return 1;
        }

        if (showHelp) {
            OptionParser::PrintHelp(argv[0]);
            return 0;
        }

        if (showVersion) {
            OptionParser::PrintVersion();
            return 0;
        }

        return TempCreationEngine::CreateTemp(opts);
    }
};

int wmain(int argc, wchar_t* argv[]) {
    MktempApplication app;
    return app.Run(argc, argv);
}