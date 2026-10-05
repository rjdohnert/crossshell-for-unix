/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <cctype>
#include <system_error>

// ============================================================================
// Size Operator & Calculator
// ============================================================================

enum class SizeOp {
    Exact,      // -s <size>  : Set exact size
    Add,        // -s +<size> : Increase size by <size>
    Subtract,   // -s -<size> : Decrease size by at most <size> (min 0)
    RoundDown,  // -s %<size> : Round size down to multiple of <size>
    RoundUp     // -s /<size> : Round size up to multiple of <size>
};

class SizeCalculator {
private:
    SizeOp op = SizeOp::Exact;
    uint64_t operand = 0;

public:
    SizeCalculator() = default;
    SizeCalculator(SizeOp operation, uint64_t val) : op(operation), operand(val) {}

    uint64_t compute_new_size(uint64_t current_size) const {
        switch (op) {
            case SizeOp::Exact:
                return operand;
            case SizeOp::Add:
                return current_size + operand;
            case SizeOp::Subtract:
                return (current_size > operand) ? (current_size - operand) : 0ULL;
            case SizeOp::RoundDown:
                if (operand == 0) return current_size;
                return current_size - (current_size % operand);
            case SizeOp::RoundUp: {
                if (operand == 0) return current_size;
                uint64_t remainder = current_size % operand;
                return (remainder == 0) ? current_size : (current_size + (operand - remainder));
            }
        }
        return current_size;
    }

    static bool parse(const std::string& spec, SizeCalculator& out_calc, std::string& err) {
        if (spec.empty()) {
            err = "empty size specification";
            return false;
        }

        SizeOp op = SizeOp::Exact;
        size_t idx = 0;

        char first = spec[0];
        if (first == '+') { op = SizeOp::Add; idx++; }
        else if (first == '-') { op = SizeOp::Subtract; idx++; }
        else if (first == '%') { op = SizeOp::RoundDown; idx++; }
        else if (first == '/') { op = SizeOp::RoundUp; idx++; }

        if (idx >= spec.length()) {
            err = "missing numeric size after operator '" + std::string(1, first) + "'";
            return false;
        }

        // Parse numerical part
        size_t num_start = idx;
        while (idx < spec.length() && std::isdigit(static_cast<unsigned char>(spec[idx]))) {
            idx++;
        }

        if (num_start == idx) {
            err = "invalid number format in size '" + std::string(spec) + "'";
            return false;
        }

        uint64_t base_val = 0;
        try {
            base_val = std::stoull(std::string(spec.substr(num_start, idx - num_start)));
        } catch (...) {
            err = "size value out of range";
            return false;
        }

        // Parse BSD suffixes (case-insensitive)
        uint64_t multiplier = 1;
        if (idx < spec.length()) {
            char suffix = static_cast<char>(std::tolower(static_cast<unsigned char>(spec[idx])));
            switch (suffix) {
                case 'b': multiplier = 512ULL; break;                      // BSD 512-byte blocks
                case 'k': multiplier = 1024ULL; break;                     // Kilobytes
                case 'm': multiplier = 1024ULL * 1024ULL; break;           // Megabytes
                case 'g': multiplier = 1024ULL * 1024ULL * 1024ULL; break;  // Gigabytes
                case 't': multiplier = 1024ULL * 1024ULL * 1024ULL * 1024ULL; break; // Terabytes
                case 'p': multiplier = 1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL; break; // Petabytes
                case 'e': multiplier = 1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL; break; // Exabytes
                default:
                    err = "invalid size suffix '" + std::string(1, spec[idx]) + "'";
                    return false;
            }
            idx++;
        }

        if (idx < spec.length()) {
            err = "trailing characters in size specification '" + std::string(spec) + "'";
            return false;
        }

        if ((op == SizeOp::RoundDown || op == SizeOp::RoundUp) && (base_val * multiplier == 0)) {
            err = "rounding scale cannot be zero";
            return false;
        }

        out_calc = SizeCalculator(op, base_val * multiplier);
        return true;
    }
};

// ============================================================================
// Truncate File Engine (Win32 Native API)
// ============================================================================

class FileTruncator {
private:
    bool no_create = false;
    std::unique_ptr<SizeCalculator> size_calc;
    std::wstring ref_file;

public:
    void set_no_create(bool flag) { no_create = flag; }
    void set_size_calculator(SizeCalculator calc) {
        size_calc = std::make_unique<SizeCalculator>(calc);
    }
    void set_reference_file(std::wstring path) {
        ref_file = std::move(path);
    }

    bool truncate_file(const std::wstring& wpath, std::string& err_msg) {

        // 1. Resolve Target Size
        uint64_t target_size = 0;
        bool has_target_size = false;

        if (!ref_file.empty()) {
            uint64_t r_size = 0;
            if (!get_file_size(ref_file, r_size, err_msg)) {
                err_msg = "reference file error: " + err_msg;
                return false;
            }
            target_size = r_size;
            has_target_size = true;
        }

        // 2. Open or Create File
        DWORD creation_disposition = no_create ? OPEN_EXISTING : OPEN_ALWAYS;
        HANDLE hFile = CreateFileW(
            wpath.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            creation_disposition,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (hFile == INVALID_HANDLE_VALUE) {
            DWORD dwErr = GetLastError();
            // In authentic BSD truncate, -c skips non-existent files silently without error
            if (no_create && dwErr == ERROR_FILE_NOT_FOUND) {
                return true;
            }
            err_msg = win32_error_to_string(dwErr);
            return false;
        }

        // 3. Compute Size
        if (!has_target_size && size_calc) {
            LARGE_INTEGER cur_size;
            if (!GetFileSizeEx(hFile, &cur_size)) {
                err_msg = win32_error_to_string(GetLastError());
                CloseHandle(hFile);
                return false;
            }
            target_size = size_calc->compute_new_size(static_cast<uint64_t>(cur_size.QuadPart));
        }

        // 4. Set End Of File via Win32 API
        LARGE_INTEGER pos;
        pos.QuadPart = static_cast<LONGLONG>(target_size);

        if (!SetFilePointerEx(hFile, pos, NULL, FILE_BEGIN)) {
            err_msg = win32_error_to_string(GetLastError());
            CloseHandle(hFile);
            return false;
        }

        if (!SetEndOfFile(hFile)) {
            err_msg = win32_error_to_string(GetLastError());
            CloseHandle(hFile);
            return false;
        }

        CloseHandle(hFile);
        return true;
    }

private:
    static bool get_file_size(const std::wstring& path, uint64_t& out_size, std::string& err) {
        WIN32_FILE_ATTRIBUTE_DATA data;
        if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) {
            err = win32_error_to_string(GetLastError());
            return false;
        }
        LARGE_INTEGER li;
        li.LowPart = data.nFileSizeLow;
        li.HighPart = data.nFileSizeHigh;
        out_size = static_cast<uint64_t>(li.QuadPart);
        return true;
    }

    static std::string win32_error_to_string(DWORD err) {
        char* buffer = nullptr;
        size_t size = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&buffer, 0, NULL
        );
        std::string message(buffer, size);
        LocalFree(buffer);
        while (!message.empty() && (message.back() == '\r' || message.back() == '\n' || message.back() == '.')) {
            message.pop_back();
        }
        return message.empty() ? ("Error " + std::to_string(err)) : message;
    }
};

// ============================================================================
// Comprehensive BSD Manual Page Documentation
// ============================================================================

const char* const BSD_MANUAL =
R"(NAME
     truncate -- truncate or extend the length of files

SYNOPSIS
     truncate [-c] -s [+|-|%|/]size[b|k|m|g|t|p|e] file ...
     truncate [-c] -r rfile file ...
     truncate [-h | -V]

DESCRIPTION
     The truncate utility shrinks or extends the length of each target file to
     the specified size.

     If a target file does not exist, it will be created unless the -c option
     is specified. If a file is extended, the newly allocated region reads as
     zero bytes (and creates sparse allocations on NTFS where applicable).

OPTIONS
     -c      Do not create files if they do not exist. The truncate utility
             treats non-existent files as non-errors when -c is specified.

     -r rfile
             Truncate or extend target files to match the exact size of the
             reference file rfile.

     -s [+|-|%|/]size[b|k|m|g|t|p|e]
             Set the target file size. The size argument can be prefixed with
             one of the following operational modifiers:

             +     Increase the current length of the file by size bytes.
             -     Decrease the current length of the file by at most size
                   bytes (the length will never be made negative).
             %     Round the current length down to a multiple of size bytes.
             /     Round the current length up to a multiple of size bytes.

             Without a prefix, the file size is set to exact size.

             The size argument may be followed by a case-insensitive unit suffix:
                   b      512-byte blocks
                   k, K   Kilobytes (1024 bytes)
                   m, M   Megabytes (1048576 bytes)
                   g, G   Gigabytes (1073741824 bytes)
                   t, T   Terabytes (1099511627776 bytes)
                   p, P   Petabytes (1125899906842624 bytes)
                   e, E   Exabytes  (1152921504606846976 bytes)

     -h, --help
             Display this manual help page and exit.

     -V, --version
             Display version information and exit.

EXIT STATUS
     The truncate utility exits 0 on success, and >0 if an error occurs.

EXAMPLES
     Set file.bin to exactly 10 Megabytes:
           $ truncate -s 10M file.bin

     Grow existing logs by 500 Kilobytes without creating if absent:
           $ truncate -c -s +500K server.log

     Round file.bin down to a multiple of 4 Kilobytes (4096-byte block alignment):
           $ truncate -s %4K file.bin

     Make target.dat match the exact size of template.dat:
           $ truncate -r template.dat target.dat

SEE ALSO
     SetEndOfFile(3), SetFilePointerEx(3), dd(1)
)";

// ============================================================================
// CLI Parser & Application Controller
// ============================================================================

struct CliOptions {
    bool no_create = false;
    bool has_size = false;
    SizeCalculator size_calc;
    std::wstring ref_file;
    std::vector<std::string> files;
    bool show_help = false;
    bool show_version = false;
};

class TruncateApp {
private:
    CliOptions options;

public:
    int parse(int argc, char* argv[]) {
        if (argc < 2) {
            std::cerr << "usage: truncate [-c] -s [+|-|%|/]size[b|k|m|g|t|p|e] file ...\n";
            std::cerr << "       truncate [-c] -r rfile file ...\n";
            return 1;
        }

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help") {
                options.show_help = true;
                return 0;
            }
            if (arg == "-V" || arg == "--version") {
                options.show_version = true;
                return 0;
            }

            if (arg == "-c") {
                options.no_create = true;
            } else if (arg == "-s") {
                if (i + 1 >= argc) {
                    std::cerr << "truncate: option requires an argument -- s\n";
                    return 1;
                }
                std::string err;
                if (!SizeCalculator::parse(argv[++i], options.size_calc, err)) {
                    std::cerr << "truncate: " << err << "\n";
                    return 1;
                }
                options.has_size = true;
            } else if (arg == "-r") {
                if (i + 1 >= argc) {
                    std::cerr << "truncate: option requires an argument -- r\n";
                    return 1;
                }
                std::string r_path = argv[++i];
                int wlen = MultiByteToWideChar(CP_UTF8, 0, r_path.c_str(), -1, NULL, 0);
                std::wstring wstr(wlen, 0);
                MultiByteToWideChar(CP_UTF8, 0, r_path.c_str(), -1, &wstr[0], wlen);
                wstr.pop_back(); // Remove null terminator from std::wstring length
                options.ref_file = wstr;
            } else if (!arg.empty() && arg[0] == '-') {
                std::cerr << "truncate: unknown option -- " << arg << "\n";
                std::cerr << "Try 'truncate --help' for more information.\n";
                return 1;
            } else {
                options.files.push_back(arg);
            }
        }

        if (!options.has_size && options.ref_file.empty()) {
            std::cerr << "truncate: must specify either -s or -r\n";
            return 1;
        }
        if (options.has_size && !options.ref_file.empty()) {
            std::cerr << "truncate: -s and -r cannot be specified together\n";
            return 1;
        }
        if (options.files.empty()) {
            std::cerr << "truncate: no files specified\n";
            return 1;
        }

        return 0;
    }

    int run() {
        if (options.show_help) {
            std::cout << BSD_MANUAL;
            return 0;
        }
        if (options.show_version) {
            std::cout << "truncate 2.0\n";
            return 0;
        }

        FileTruncator truncator;
        truncator.set_no_create(options.no_create);

        if (options.has_size) {
            truncator.set_size_calculator(options.size_calc);
        } else {
            truncator.set_reference_file(options.ref_file);
        }

        int failed_count = 0;
        for (const auto& file_str : options.files) {
            std::string err_msg;
            int wlen = MultiByteToWideChar(CP_UTF8, 0, file_str.c_str(), -1, NULL, 0);
            if (wlen == 0) {
                std::cerr << "truncate: " << file_str << ": invalid UTF-8 path\n";
                failed_count++;
                continue;
            }
            std::wstring wpath(static_cast<size_t>(wlen), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, file_str.c_str(), -1, &wpath[0], wlen);
            wpath.pop_back();
            if (!truncator.truncate_file(wpath, err_msg)) {
                std::cerr << "truncate: " << file_str << ": " << err_msg << "\n";
                failed_count++;
            }
        }

        return (failed_count == 0) ? 0 : 1;
    }
};

// ============================================================================
// Windows Console Entry Point
// ============================================================================

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    TruncateApp app;

    int parse_result = app.parse(argc, argv);
    if (parse_result != 0) {
        return parse_result;
    }

    return app.run();
}