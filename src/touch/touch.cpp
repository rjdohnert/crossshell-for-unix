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
#include <algorithm>
#include <cwctype>
#include <windows.h>
#include <cstdio>

// ============================================================================
// 1. PIPELINE BUFFER & OUTPUT FORMATTING
// ============================================================================
class TouchPipeBuffer : public std::wstreambuf {
private:
    FILE* m_file;
    wchar_t m_buffer[1024];

public:
    explicit TouchPipeBuffer(FILE* file) : m_file(file) {
        setp(m_buffer, m_buffer + 1024);
    }

    int_type overflow(int_type ch) override {
        if (ch != traits_type::eof()) {
            *pptr() = static_cast<wchar_t>(ch);
            pbump(1);
        }
        return sync() == 0 ? traits_type::not_eof(ch) : traits_type::eof();
    }

    int sync() override {
        auto count = pptr() - pbase();
        if (count > 0) {
            std::wstring value(pbase(), static_cast<size_t>(count));
            int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
            if (size > 0) {
                std::string utf8(static_cast<size_t>(size), '\0');
                WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), utf8.data(), size, nullptr, nullptr);
                std::fwrite(utf8.data(), 1, utf8.size(), m_file);
            }
        }
        setp(m_buffer, m_buffer + 1024);
        return std::fflush(m_file);
    }
};

class OutputFormatter {
public:
    static std::wstring Quote(const std::wstring& value) {
        std::wstring out = L"\"";
        for (wchar_t c : value) {
            if (c == L'"' || c == L'\\') out += L'\\';
            out += c;
        }
        return out + L"\"";
    }

    static void EmitHeader(int format, std::wostream& out) {
        if (format == 2) out << L"status,path\n";
        else if (format == 3) out << L"STATUS\tPATH\n";
    }

    static void EmitRecord(const std::wstring& target, const std::wstring& status, int format, std::wostream& out) {
        if (format == 1) {
            out << L"{\"status\":\"" << status << L"\",\"path\":" << Quote(target) << L"}\n";
        } else if (format == 2) {
            out << status << L"," << Quote(target) << L"\n";
        } else if (format == 3) {
            out << status << L"\t" << target << L"\n";
        }
    }
};

// ============================================================================
// 2. TIME PARSER & EXTRACTION UTILITIES
// ============================================================================
class TimeParser {
public:
    static bool IsDigits(const std::wstring& s) {
        return std::all_of(s.begin(), s.end(), [](wchar_t c) { return std::iswdigit(c); });
    }

    static bool ParseTimeSpec(const std::wstring& t_str, FILETIME& out_ft) {
        std::wstring clean_str = t_str;
        std::wstring ss_str = L"00";

        size_t dot_pos = t_str.find(L'.');
        if (dot_pos != std::wstring::npos) {
            ss_str = t_str.substr(dot_pos + 1);
            clean_str = t_str.substr(0, dot_pos);
        }

        if (ss_str.length() != 2 || !IsDigits(ss_str) || !IsDigits(clean_str)) {
            return false;
        }

        int year = 0;
        int month = 0;
        int day = 0;
        int hour = 0;
        int minute = 0;
        int second = std::stoi(ss_str);

        SYSTEMTIME current_st;
        GetLocalTime(&current_st);

        size_t len = clean_str.length();
        if (len == 8) { // MMDDhhmm
            year = current_st.wYear;
            month = std::stoi(clean_str.substr(0, 2));
            day = std::stoi(clean_str.substr(2, 2));
            hour = std::stoi(clean_str.substr(4, 2));
            minute = std::stoi(clean_str.substr(6, 2));
        } else if (len == 10) { // YYMMDDhhmm
            int yy = std::stoi(clean_str.substr(0, 2));
            year = (yy < 69) ? (2000 + yy) : (1900 + yy);
            month = std::stoi(clean_str.substr(2, 2));
            day = std::stoi(clean_str.substr(4, 2));
            hour = std::stoi(clean_str.substr(6, 2));
            minute = std::stoi(clean_str.substr(8, 2));
        } else if (len == 12) { // CCYYMMDDhhmm
            year = std::stoi(clean_str.substr(0, 4));
            month = std::stoi(clean_str.substr(4, 2));
            day = std::stoi(clean_str.substr(6, 2));
            hour = std::stoi(clean_str.substr(8, 2));
            minute = std::stoi(clean_str.substr(10, 2));
        } else {
            return false;
        }

        SYSTEMTIME local_st = { 0 };
        local_st.wYear = static_cast<WORD>(year);
        local_st.wMonth = static_cast<WORD>(month);
        local_st.wDay = static_cast<WORD>(day);
        local_st.wHour = static_cast<WORD>(hour);
        local_st.wMinute = static_cast<WORD>(minute);
        local_st.wSecond = static_cast<WORD>(second);

        SYSTEMTIME utc_st;
        if (!TzSpecificLocalTimeToSystemTime(nullptr, &local_st, &utc_st)) {
            return false;
        }

        return SystemTimeToFileTime(&utc_st, &out_ft) != 0;
    }

    static bool GetReferenceTimes(const std::wstring& ref_path, FILETIME& out_at, FILETIME& out_mt) {
        HANDLE hFile = CreateFileW(
            ref_path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS,
            nullptr
        );
        if (hFile == INVALID_HANDLE_VALUE) {
            return false;
        }
        FILETIME ct;
        bool success = GetFileTime(hFile, &ct, &out_at, &out_mt) != 0;
        CloseHandle(hFile);
        return success;
    }
};

// ============================================================================
// 3. OPTIONS & CLI PARSER
// ============================================================================
struct TouchOptions {
    bool change_access = false;     // -a
    bool no_create = false;         // -c
    bool change_mod = false;        // -m
    std::wstring ref_file = L"";    // -r
    std::wstring time_str = L"";    // -t
    std::wstring date_str = L"";    // --date
    int output_format = 0;
    std::wstring pipe_command;
    std::vector<std::wstring> targets;
};

class OptionParser {
public:
    static void PrintUsage() {
        std::wcout << LR"(touch(1)                CrossShell for UNIX Reference Manual                 touch(1)

    NAME
        touch - change file timestamps or create empty files

    SYNOPSIS
        touch [OPTIONS] FILE...

    DESCRIPTION
        Update the access and modification times of each FILE to the current
        time. A FILE argument that does not exist is created empty, unless -c
        is supplied.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -a
            Change only the access time.

        -c, --no-create
            Do not create any files.

        -m
            Change only the modification time.

        -r FILE, --reference=FILE
            Use this file's times instead of current time.

        -t TIME
            Use [[CC]YY]MMDDhhmm[.ss] instead of current time.

        --date=STRING
            Parse STRING and use it as the date.

        --json, --csv, --table
            Select structured output format.

        --pipe COMMAND
            Send output through pipeline COMMAND.

        -h, --help
            Display this reference manual and exit.

        -v, --version
            Display version information and exit.

    EXAMPLES
        touch file.txt
            Update timestamp of file.txt, creating it if it does not exist.

        touch -c non_existing.txt
            Do not create file if it does not exist.

        touch -r source.txt target.txt
            Set target.txt timestamps to match source.txt.

    CrossShell for UNIX                                                      touch(1)
)";
    }

    bool Parse(int argc, wchar_t* argv[], TouchOptions& opts) const {
        int i = 1;
        while (i < argc) {
            std::wstring arg = argv[i];
            if (arg == L"--json") { opts.output_format = 1; i++; continue; }
            if (arg == L"--csv") { opts.output_format = 2; i++; continue; }
            if (arg == L"--table") { opts.output_format = 3; i++; continue; }
            if (arg == L"--pipe" && i + 1 < argc) { opts.pipe_command = argv[++i]; i++; continue; }
            if (arg == L"-h" || arg == L"--help" || arg == L"/?" || arg == L"-?") {
                PrintUsage();
                return false;
            }
            if (arg == L"-v" || arg == L"--version") {
                std::wcout << L"touch version 1.0.0\n";
                return false;
            }
            if (arg.rfind(L"--date=", 0) == 0) {
                opts.date_str = arg.substr(7);
            } else if (arg.rfind(L"--reference=", 0) == 0) {
                opts.ref_file = arg.substr(12);
            } else if (arg.rfind(L"--time=", 0) == 0) {
                opts.time_str = arg.substr(7);
            } else if (arg[0] == L'-' && arg.length() > 1) {
                for (size_t j = 1; j < arg.length(); ++j) {
                    wchar_t flag = arg[j];
                    if (flag == L'a') {
                        opts.change_access = true;
                    } else if (flag == L'c') {
                        opts.no_create = true;
                    } else if (flag == L'm') {
                        opts.change_mod = true;
                    } else if (flag == L'r') {
                        if (j + 1 == arg.length()) {
                            if (i + 1 < argc) {
                                opts.ref_file = argv[++i];
                                break;
                            } else {
                                std::wcerr << L"touch: option requires an argument -- r\n";
                                return false;
                            }
                        } else {
                            opts.ref_file = arg.substr(j + 1);
                            break;
                        }
                    } else if (flag == L't') {
                        if (j + 1 == arg.length()) {
                            if (i + 1 < argc) {
                                opts.time_str = argv[++i];
                                break;
                            } else {
                                std::wcerr << L"touch: option requires an argument -- t\n";
                                return false;
                            }
                        } else {
                            opts.time_str = arg.substr(j + 1);
                            break;
                        }
                    } else {
                        std::wcerr << L"touch: unknown option -- " << flag << std::endl;
                        std::wcerr << L"usage: touch [-acm] [-r file] [-t time] file ...\n";
                        return false;
                    }
                }
            } else {
                opts.targets.push_back(arg);
            }
            i++;
        }

        if (opts.targets.empty()) {
            std::wcerr << L"usage: touch [-acm] [-r file] [-t time] file ...\n";
            return false;
        }

        return true;
    }
};

// ============================================================================
// 4. FILE TOUCHER ENGINE
// ============================================================================
class FileToucher {
public:
    bool Touch(const std::wstring& target, const TouchOptions& opts, const FILETIME& target_at, const FILETIME& target_mt) const {
        DWORD creation_disposition = opts.no_create ? OPEN_EXISTING : OPEN_ALWAYS;
        HANDLE hFile = CreateFileW(
            target.c_str(),
            FILE_WRITE_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            creation_disposition,
            FILE_FLAG_BACKUP_SEMANTICS,
            nullptr
        );

        if (hFile == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            if (opts.no_create && (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND)) {
                return true;
            }
            std::wcerr << L"touch: " << target << L": cannot touch. Error: " << err << std::endl;
            return false;
        }

        const FILETIME* p_at = nullptr;
        const FILETIME* p_mt = nullptr;

        if (opts.change_access && !opts.change_mod) {
            p_at = &target_at;
        } else if (opts.change_mod && !opts.change_access) {
            p_mt = &target_mt;
        } else {
            p_at = &target_at;
            p_mt = &target_mt;
        }

        bool success = true;
        if (!SetFileTime(hFile, nullptr, p_at, p_mt)) {
            std::wcerr << L"touch: " << target << L": setting time failed. Error: " << GetLastError() << std::endl;
            success = false;
        }

        CloseHandle(hFile);
        return success;
    }
};

// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================
class TouchApplication {
private:
    OptionParser m_parser;
    FileToucher m_toucher;

public:
    int Run(int argc, wchar_t* argv[]) {
        if (argc <= 1) {
            OptionParser::PrintUsage();
            return 1;
        }

        TouchOptions opts;
        if (!m_parser.Parse(argc, argv, opts)) {
            if (argc > 1) {
                std::wstring firstArg = argv[1];
                if (firstArg == L"-h" || firstArg == L"--help" || firstArg == L"/?" || firstArg == L"-?" ||
                    firstArg == L"-v" || firstArg == L"--version") {
                    return 0;
                }
            }
            return 1;
        }

        FILETIME target_at, target_mt;
        if (!opts.ref_file.empty()) {
            if (!TimeParser::GetReferenceTimes(opts.ref_file, target_at, target_mt)) {
                std::wcerr << L"touch: " << opts.ref_file << L": reference file could not be read\n";
                return 1;
            }
        } else if (!opts.time_str.empty()) {
            if (!TimeParser::ParseTimeSpec(opts.time_str, target_at)) {
                std::wcerr << L"touch: out of range or bad time specification: " << opts.time_str << std::endl;
                return 1;
            }
            target_mt = target_at;
        } else if (!opts.date_str.empty()) {
            if (!TimeParser::ParseTimeSpec(opts.date_str, target_at)) {
                std::wcerr << L"touch: out of range or bad time specification: " << opts.date_str << std::endl;
                return 1;
            }
            target_mt = target_at;
        } else {
            SYSTEMTIME st;
            GetSystemTime(&st);
            SystemTimeToFileTime(&st, &target_at);
            target_mt = target_at;
        }

        FILE* outputPipe = opts.pipe_command.empty() ? nullptr : _wpopen(opts.pipe_command.c_str(), L"w");
        if (!opts.pipe_command.empty() && !outputPipe) return 1;

        std::wstreambuf* oldOutput = nullptr;
        TouchPipeBuffer* pipeBuffer = nullptr;
        if (outputPipe) {
            oldOutput = std::wcout.rdbuf();
            pipeBuffer = new TouchPipeBuffer(outputPipe);
            std::wcout.rdbuf(pipeBuffer);
        }

        OutputFormatter::EmitHeader(opts.output_format, std::wcout);

        bool overall_success = true;
        for (const auto& target : opts.targets) {
            bool touched = m_toucher.Touch(target, opts, target_at, target_mt);
            if (!touched) {
                overall_success = false;
            } else {
                OutputFormatter::EmitRecord(target, L"success", opts.output_format, std::wcout);
            }
        }

        if (pipeBuffer) {
            std::wcout.flush();
            std::wcout.rdbuf(oldOutput);
            delete pipeBuffer;
            _pclose(outputPipe);
        }

        return overall_success ? 0 : 1;
    }
};

// ============================================================================
// 6. ENTRY POINT
// ============================================================================
int wmain(int argc, wchar_t* argv[]) {
    TouchApplication app;
    return app.Run(argc, argv);
}
