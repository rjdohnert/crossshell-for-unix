/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <thread>
#include <chrono>
#include <cwctype>
#include <limits>
#include <fstream>
#include <cstdio>
#include <deque>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

namespace fs = std::filesystem;

// ============================================================================
// 1. CONFIGURATION & STATE
// ============================================================================

struct TailOptions {
    bool count_lines = true;
    bool from_start = false;
    long long count = 10;
    bool follow = false;
    bool retry = false;
    bool quiet = false;
    bool verbose = false;
    double sleep_interval_sec = 1.0;
    int max_unchanged_stats = 5;
    std::vector<std::wstring> files;
};

class TailOutput {
public:
    static void WriteBytes(const char* data, size_t size) {
        if (size == 0) return;
        DWORD written = 0;
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE && hOut != nullptr) {
            WriteFile(hOut, data, static_cast<DWORD>(size), &written, nullptr);
        } else {
            std::cout.write(data, static_cast<std::streamsize>(size));
        }
    }

    static std::string Utf8(const std::wstring& value) {
        int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0) return {};
        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
        return result;
    }

    static void PrintHeader(const std::wstring& filename, bool& first_header) {
        if (!first_header) {
            WriteBytes("\n", 1);
        }
        first_header = false;
        std::string header = "==> " + (filename == L"-" ? "standard input" : Utf8(filename)) + " <==\n";
        WriteBytes(header.data(), header.size());
    }
};

// ============================================================================
// 2. STREAM & FILE TAILERS
// ============================================================================

class StreamTailer {
public:
    static bool TailLines(std::istream& in, long long count, bool from_start) {
        if (from_start) {
            long long line_no = 1;
            std::string line;
            while (std::getline(in, line)) {
                if (line_no >= count) {
                    std::string out = line + "\n";
                    TailOutput::WriteBytes(out.data(), out.size());
                }
                line_no++;
            }
            return true;
        }

        if (count <= 0) return true;

        std::deque<std::string> ring;
        std::string line;
        while (std::getline(in, line)) {
            ring.push_back(line);
            if (static_cast<long long>(ring.size()) > count) {
                ring.pop_front();
            }
        }

        for (const auto& item : ring) {
            std::string out = item + "\n";
            TailOutput::WriteBytes(out.data(), out.size());
        }
        return true;
    }

    static bool TailBytes(std::istream& in, long long count, bool from_start) {
        if (from_start) {
            long long byte_no = 1;
            char ch = 0;
            while (in.get(ch)) {
                if (byte_no >= count) {
                    TailOutput::WriteBytes(&ch, 1);
                }
                byte_no++;
            }
            return true;
        }

        if (count <= 0) return true;

        std::deque<char> ring;
        char ch = 0;
        while (in.get(ch)) {
            ring.push_back(ch);
            if (static_cast<long long>(ring.size()) > count) {
                ring.pop_front();
            }
        }

        std::vector<char> buffer(ring.begin(), ring.end());
        TailOutput::WriteBytes(buffer.data(), buffer.size());
        return true;
    }
};

class FileTailer {
public:
    static bool TailLines(HANDLE hFile, long long count, bool from_start) {
        LARGE_INTEGER file_size;
        if (!GetFileSizeEx(hFile, &file_size)) return false;

        if (from_start) {
            LARGE_INTEGER zero = {};
            SetFilePointerEx(hFile, zero, nullptr, FILE_BEGIN);
            long long line_no = 1;
            std::vector<char> buf(64 * 1024);
            DWORD read_bytes = 0;
            std::string rem;
            while (ReadFile(hFile, buf.data(), static_cast<DWORD>(buf.size()), &read_bytes, nullptr) && read_bytes > 0) {
                size_t start = 0;
                for (size_t i = 0; i < read_bytes; ++i) {
                    if (buf[i] == '\n') {
                        if (line_no >= count) {
                            if (!rem.empty()) {
                                TailOutput::WriteBytes(rem.data(), rem.size());
                                rem.clear();
                            }
                            TailOutput::WriteBytes(buf.data() + start, i - start + 1);
                        }
                        line_no++;
                        start = i + 1;
                    }
                }
                if (start < read_bytes) {
                    if (line_no >= count) {
                        TailOutput::WriteBytes(buf.data() + start, read_bytes - start);
                    } else {
                        rem.append(buf.data() + start, read_bytes - start);
                    }
                }
            }
            return true;
        }

        if (count <= 0) {
            LARGE_INTEGER end_pos = {};
            SetFilePointerEx(hFile, end_pos, nullptr, FILE_END);
            return true;
        }

        const DWORD CHUNK_SIZE = 64 * 1024;
        std::vector<char> chunk(CHUNK_SIZE);
        long long lines_found = 0;
        long long curr_offset = file_size.QuadPart;
        LARGE_INTEGER target_pos = {};

        while (curr_offset > 0 && lines_found <= count) {
            DWORD to_read = static_cast<DWORD>((std::min)(static_cast<long long>(CHUNK_SIZE), curr_offset));
            curr_offset -= to_read;
            target_pos.QuadPart = curr_offset;
            SetFilePointerEx(hFile, target_pos, nullptr, FILE_BEGIN);

            DWORD read_bytes = 0;
            if (!ReadFile(hFile, chunk.data(), to_read, &read_bytes, nullptr) || read_bytes == 0) break;

            for (long long i = static_cast<long long>(read_bytes) - 1; i >= 0; --i) {
                if (chunk[static_cast<size_t>(i)] == '\n') {
                    if (curr_offset + i + 1 == file_size.QuadPart) continue;
                    lines_found++;
                    if (lines_found == count) {
                        target_pos.QuadPart = curr_offset + i + 1;
                        SetFilePointerEx(hFile, target_pos, nullptr, FILE_BEGIN);
                        goto read_forward;
                    }
                }
            }
        }

        target_pos.QuadPart = 0;
        SetFilePointerEx(hFile, target_pos, nullptr, FILE_BEGIN);

    read_forward:
        DWORD bytes_read = 0;
        while (ReadFile(hFile, chunk.data(), CHUNK_SIZE, &bytes_read, nullptr) && bytes_read > 0) {
            TailOutput::WriteBytes(chunk.data(), bytes_read);
        }
        return true;
    }

    static bool TailBytes(HANDLE hFile, long long count, bool from_start) {
        LARGE_INTEGER file_size;
        if (!GetFileSizeEx(hFile, &file_size)) return false;

        LARGE_INTEGER target_pos = {};
        if (from_start) {
            target_pos.QuadPart = (std::max)(0LL, count - 1);
        } else {
            target_pos.QuadPart = (std::max)(0LL, file_size.QuadPart - count);
        }
        SetFilePointerEx(hFile, target_pos, nullptr, FILE_BEGIN);

        const DWORD CHUNK_SIZE = 64 * 1024;
        std::vector<char> chunk(CHUNK_SIZE);
        DWORD bytes_read = 0;
        while (ReadFile(hFile, chunk.data(), CHUNK_SIZE, &bytes_read, nullptr) && bytes_read > 0) {
            TailOutput::WriteBytes(chunk.data(), bytes_read);
        }
        return true;
    }
};

// ============================================================================
// 3. FOLLOW CONTROLLER
// ============================================================================

class FollowController {
public:
    static void FollowFiles(const std::vector<std::wstring>& files, const TailOptions& opts, bool show_headers) {
        struct MonitoredFile {
            std::wstring path;
            HANDLE handle = INVALID_HANDLE_VALUE;
            LARGE_INTEGER last_size = {};
        };

        std::vector<MonitoredFile> targets;
        for (const auto& f : files) {
            if (f == L"-") continue;
            HANDLE h = CreateFileW(f.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                   nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            MonitoredFile mf;
            mf.path = f;
            mf.handle = h;
            if (h != INVALID_HANDLE_VALUE) {
                GetFileSizeEx(h, &mf.last_size);
            }
            targets.push_back(mf);
        }

        std::vector<char> read_buf(64 * 1024);
        std::wstring last_printed_file = L"";

        DWORD sleep_ms = static_cast<DWORD>(opts.sleep_interval_sec * 1000.0);
        if (sleep_ms == 0) sleep_ms = 100;

        while (true) {
            Sleep(sleep_ms);

            for (auto& tf : targets) {
                if (tf.handle == INVALID_HANDLE_VALUE && opts.retry) {
                    tf.handle = CreateFileW(tf.path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                    if (tf.handle != INVALID_HANDLE_VALUE) {
                        tf.last_size.QuadPart = 0;
                    }
                }

                if (tf.handle == INVALID_HANDLE_VALUE) continue;

                LARGE_INTEGER current_size = {};
                if (!GetFileSizeEx(tf.handle, &current_size)) {
                    CloseHandle(tf.handle);
                    tf.handle = INVALID_HANDLE_VALUE;
                    continue;
                }

                if (current_size.QuadPart < tf.last_size.QuadPart) {
                    std::string note = "tail: " + TailOutput::Utf8(tf.path) + ": file truncated\n";
                    TailOutput::WriteBytes(note.data(), note.size());
                    LARGE_INTEGER zero = {};
                    SetFilePointerEx(tf.handle, zero, nullptr, FILE_BEGIN);
                    tf.last_size.QuadPart = 0;
                }

                if (current_size.QuadPart > tf.last_size.QuadPart) {
                    if (show_headers && last_printed_file != tf.path) {
                        bool dummy = false;
                        TailOutput::PrintHeader(tf.path, dummy);
                        last_printed_file = tf.path;
                    }

                    DWORD bytes_read = 0;
                    while (ReadFile(tf.handle, read_buf.data(), static_cast<DWORD>(read_buf.size()), &bytes_read, nullptr) && bytes_read > 0) {
                        TailOutput::WriteBytes(read_buf.data(), bytes_read);
                    }
                    GetFileSizeEx(tf.handle, &tf.last_size);
                }
            }
        }
    }
};

// ============================================================================
// 4. OPTION PARSER & APPLICATION CONTROLLER
// ============================================================================

class OptionParser {
public:
    static void PrintUsage() {
        std::wcout << LR"(tail(1)                 CrossShell for UNIX Reference Manual                  tail(1)

    NAME
        tail - output the last part of files

    SYNOPSIS
        tail [OPTIONS] [FILE]...

    DESCRIPTION
        Print the last 10 lines of each FILE to standard output. With more than
        one FILE, precede each with a header giving the file name. With no FILE,
        or when FILE is '-', read standard input.

    OPTIONS
        -c, --bytes=[+]NUM
            Output the last NUM bytes; or use +NUM to output starting with byte NUM.

        -f, --follow
            Output appended data as the file grows.

        -n, --lines=[+]NUM
            Output the last NUM lines, instead of the last 10; or use +NUM to start at line NUM.

        --max-unchanged-stats=N
            Reopen file after N unchanged checks with -f (default: 5).

        -q, --quiet, --silent
            Never output headers giving file names.

        --retry
            Keep trying to open a file if it is inaccessible.

        -s, --sleep-interval=N
            With -f, sleep approximately N seconds (default: 1.0) between iterations.

        -v, --verbose
            Always output headers giving file names.

        -h, --help
            Display this reference manual.

        --version
            Display version and license information.

    EXAMPLES
        tail app.log
            Print the last 10 lines of app.log.

        tail -n 25 error.log
            Print the last 25 lines of error.log.

        tail -f -n 50 /var/log/syslog
            Follow syslog in real-time starting from last 50 lines.

        tail -n +100 dump.sql
            Output dump.sql starting from line 100 to end of file.

    CrossShell for UNIX                                                     tail(1)
    )";
    }

    static void PrintVersion() {
        std::wcout << L"tail (cmd-extended) 2.0.0\n"
                   << L"Copyright (C) 2026 Free Software Foundation, Inc.\n";
    }

    static bool ParseCount(const std::wstring& str, long long& count, bool& from_start) {
        if (str.empty()) return false;
        size_t idx = 0;
        if (str[0] == L'+') {
            from_start = true;
            idx = 1;
        } else if (str[0] == L'-') {
            from_start = false;
            idx = 1;
        } else {
            from_start = false;
        }

        try {
            count = std::stoll(str.substr(idx));
            return true;
        } catch (...) {
            return false;
        }
    }

    bool Parse(int argc, wchar_t* argv[], TailOptions& opts, bool& exitEarly) const {
        exitEarly = false;
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];

            if (arg == L"--help" || arg == L"-h" || arg == L"-?" || arg == L"/?") {
                PrintUsage();
                exitEarly = true;
                return true;
            } else if (arg == L"--version") {
                PrintVersion();
                exitEarly = true;
                return true;
            } else if (arg == L"-f" || arg == L"--follow") {
                opts.follow = true;
            } else if (arg == L"--retry") {
                opts.retry = true;
            } else if (arg == L"-q" || arg == L"--quiet" || arg == L"--silent") {
                opts.quiet = true;
                opts.verbose = false;
            } else if (arg == L"-v" || arg == L"--verbose") {
                opts.verbose = true;
                opts.quiet = false;
            } else if (arg.rfind(L"-n", 0) == 0) {
                opts.count_lines = true;
                if (arg == L"-n") {
                    if (i + 1 < argc) {
                        if (!ParseCount(argv[++i], opts.count, opts.from_start)) {
                            std::wcerr << L"tail: invalid number of lines: '" << argv[i] << L"'\n";
                            return false;
                        }
                    }
                } else {
                    if (!ParseCount(arg.substr(2), opts.count, opts.from_start)) {
                        std::wcerr << L"tail: invalid number of lines: '" << arg.substr(2) << L"'\n";
                        return false;
                    }
                }
            } else if (arg.rfind(L"-c", 0) == 0) {
                opts.count_lines = false;
                if (arg == L"-c") {
                    if (i + 1 < argc) {
                        if (!ParseCount(argv[++i], opts.count, opts.from_start)) {
                            std::wcerr << L"tail: invalid number of bytes: '" << argv[i] << L"'\n";
                            return false;
                        }
                    }
                } else {
                    if (!ParseCount(arg.substr(2), opts.count, opts.from_start)) {
                        std::wcerr << L"tail: invalid number of bytes: '" << arg.substr(2) << L"'\n";
                        return false;
                    }
                }
            } else if (arg.rfind(L"-s", 0) == 0) {
                if (arg == L"-s") {
                    if (i + 1 < argc) opts.sleep_interval_sec = std::stod(argv[++i]);
                } else {
                    opts.sleep_interval_sec = std::stod(arg.substr(2));
                }
            } else if (arg.size() > 1 && (arg[0] == L'+' || (arg[0] == L'-' && std::isdigit(static_cast<unsigned char>(arg[1]))))) {
                opts.count_lines = true;
                ParseCount(arg, opts.count, opts.from_start);
            } else if (!arg.empty() && arg[0] == L'-' && arg != L"-") {
                std::wcerr << L"tail: unrecognized option '" << arg << L"'\n";
                return false;
            } else {
                opts.files.push_back(arg);
            }
        }

        if (opts.files.empty()) {
            opts.files.push_back(L"-");
        }

        return true;
    }
};

class TailApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]) const {
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);

        TailOptions opts;
        bool exitEarly = false;
        if (!m_parser.Parse(argc, argv, opts, exitEarly)) {
            return 1;
        }
        if (exitEarly) {
            return 0;
        }

        bool show_headers = (opts.files.size() > 1 || opts.verbose) && !opts.quiet;
        bool first_header = true;

        for (const auto& file_path : opts.files) {
            if (file_path == L"-") {
                if (show_headers) TailOutput::PrintHeader(L"-", first_header);
                if (opts.count_lines) {
                    StreamTailer::TailLines(std::cin, opts.count, opts.from_start);
                } else {
                    StreamTailer::TailBytes(std::cin, opts.count, opts.from_start);
                }
                continue;
            }

            HANDLE hFile = CreateFileW(file_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                       nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

            if (hFile == INVALID_HANDLE_VALUE) {
                std::wcerr << L"tail: cannot open '" << file_path << L"' for reading: No such file or directory\n";
                continue;
            }

            if (show_headers) TailOutput::PrintHeader(file_path, first_header);

            if (opts.count_lines) {
                FileTailer::TailLines(hFile, opts.count, opts.from_start);
            } else {
                FileTailer::TailBytes(hFile, opts.count, opts.from_start);
            }

            CloseHandle(hFile);
        }

        if (opts.follow) {
            FollowController::FollowFiles(opts.files, opts, show_headers);
        }

        return 0;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    TailApplication app;
    return app.Run(argc, argv);
}
