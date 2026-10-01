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

#include <windows.h>
#include <io.h>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <cwchar>
#include <cstdio>

// ============================================================================
// 1. FILE SYSTEM & PLATFORM INSPECTOR
// ============================================================================

class FileSystemInspector {
public:
    static bool FileExists(const std::wstring& path) {
        DWORD attr = GetFileAttributesW(path.c_str());
        return (attr != INVALID_FILE_ATTRIBUTES);
    }

    static bool IsDirectory(const std::wstring& path) {
        DWORD attr = GetFileAttributesW(path.c_str());
        return (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);
    }

    static bool IsRegularFile(const std::wstring& path) {
        DWORD attr = GetFileAttributesW(path.c_str());
        return (attr != INVALID_FILE_ATTRIBUTES) && !(attr & FILE_ATTRIBUTE_DIRECTORY);
    }

    static bool IsSymlink(const std::wstring& path) {
        DWORD attr = GetFileAttributesW(path.c_str());
        return (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_REPARSE_POINT);
    }

    static bool IsNonEmpty(const std::wstring& path) {
        WIN32_FILE_ATTRIBUTE_DATA data;
        if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) {
            uint64_t size = (static_cast<uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
            return size > 0;
        }
        return false;
    }

    static bool IsReadable(const std::wstring& path) {
        return (_waccess(path.c_str(), 4) == 0);
    }

    static bool IsWritable(const std::wstring& path) {
        return (_waccess(path.c_str(), 2) == 0);
    }

    static bool IsExecutable(const std::wstring& path) {
        DWORD attr = GetFileAttributesW(path.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES) return false;
        if (attr & FILE_ATTRIBUTE_DIRECTORY) return true;

        size_t dot = path.find_last_of(L".");
        if (dot != std::wstring::npos) {
            std::wstring ext = path.substr(dot);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
            if (ext == L".exe" || ext == L".cmd" || ext == L".bat" || ext == L".com" ||
                ext == L".ps1" || ext == L".vbs" || ext == L".js" || ext == L".msc") {
                return true;
            }
        }
        return (_waccess(path.c_str(), 4) == 0);
    }

    static bool IsTerminal(const std::wstring& fd_str) {
        try {
            int fd = std::stoi(fd_str);
            DWORD handle_id = (fd == 0) ? STD_INPUT_HANDLE :
                              (fd == 1) ? STD_OUTPUT_HANDLE :
                              (fd == 2) ? STD_ERROR_HANDLE : 0;
            if (handle_id == 0) return false;

            HANDLE h = GetStdHandle(handle_id);
            if (h == INVALID_HANDLE_VALUE || h == NULL) return false;
            DWORD mode;
            return GetConsoleMode(h, &mode) != 0;
        } catch (...) {
            return false;
        }
    }

    static bool IsCharDevice(const std::wstring& path) {
        std::wstring u = path;
        std::transform(u.begin(), u.end(), u.begin(), ::towupper);
        if (u == L"CON" || u == L"NUL" || u == L"PRN" || u == L"AUX" ||
            u.rfind(L"COM", 0) == 0 || u.rfind(L"LPT", 0) == 0) {
            return true;
        }
        HANDLE h = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            DWORD type = GetFileType(h);
            CloseHandle(h);
            return type == FILE_TYPE_CHAR;
        }
        return false;
    }

    static bool IsNamedPipe(const std::wstring& path) {
        if (path.rfind(L"\\\\.\\pipe\\", 0) == 0) return true;
        HANDLE h = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            DWORD type = GetFileType(h);
            CloseHandle(h);
            return type == FILE_TYPE_PIPE;
        }
        return false;
    }

    static bool FileNewerThan(const std::wstring& p1, const std::wstring& p2) {
        WIN32_FILE_ATTRIBUTE_DATA d1, d2;
        if (!GetFileAttributesExW(p1.c_str(), GetFileExInfoStandard, &d1)) return false;
        if (!GetFileAttributesExW(p2.c_str(), GetFileExInfoStandard, &d2)) return false;

        ULARGE_INTEGER t1, t2;
        t1.LowPart = d1.ftLastWriteTime.dwLowDateTime;
        t1.HighPart = d1.ftLastWriteTime.dwHighDateTime;
        t2.LowPart = d2.ftLastWriteTime.dwLowDateTime;
        t2.HighPart = d2.ftLastWriteTime.dwHighDateTime;

        return t1.QuadPart > t2.QuadPart;
    }

    static bool FileOlderThan(const std::wstring& p1, const std::wstring& p2) {
        WIN32_FILE_ATTRIBUTE_DATA d1, d2;
        if (!GetFileAttributesExW(p1.c_str(), GetFileExInfoStandard, &d1)) return false;
        if (!GetFileAttributesExW(p2.c_str(), GetFileExInfoStandard, &d2)) return false;

        ULARGE_INTEGER t1, t2;
        t1.LowPart = d1.ftLastWriteTime.dwLowDateTime;
        t1.HighPart = d1.ftLastWriteTime.dwHighDateTime;
        t2.LowPart = d2.ftLastWriteTime.dwLowDateTime;
        t2.HighPart = d2.ftLastWriteTime.dwHighDateTime;

        return t1.QuadPart < t2.QuadPart;
    }

    static bool SameFile(const std::wstring& p1, const std::wstring& p2) {
        HANDLE h1 = CreateFileW(p1.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
        if (h1 == INVALID_HANDLE_VALUE) return false;

        HANDLE h2 = CreateFileW(p2.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
        if (h2 == INVALID_HANDLE_VALUE) {
            CloseHandle(h1);
            return false;
        }

        BY_HANDLE_FILE_INFORMATION i1, i2;
        bool res = false;
        if (GetFileInformationByHandle(h1, &i1) && GetFileInformationByHandle(h2, &i2)) {
            res = (i1.dwVolumeSerialNumber == i2.dwVolumeSerialNumber) &&
                  (i1.nFileIndexHigh == i2.nFileIndexHigh) &&
                  (i1.nFileIndexLow == i2.nFileIndexLow);
        }

        CloseHandle(h1);
        CloseHandle(h2);
        return res;
    }
};

// ============================================================================
// 2. PARSER UTILS & VALUE CONVERTERS
// ============================================================================

class TypeParser {
public:
    static long long ParseLong(const std::wstring& str) {
        size_t idx = 0;
        long long val = 0;
        std::string narrow(str.length(), '\0');
        for (size_t i = 0; i < str.length(); ++i) narrow[i] = static_cast<char>(str[i]);
        try {
            val = std::stoll(str, &idx);
        } catch (...) {
            throw std::runtime_error("integer expression expected: " + narrow);
        }
        if (idx != str.length()) {
            throw std::runtime_error("integer expression expected: " + narrow);
        }
        return val;
    }
};

// ============================================================================
// 3. EXPRESSION EVALUATOR
// ============================================================================

class Evaluator {
    std::vector<std::wstring> tokens;
    size_t idx = 0;

public:
    explicit Evaluator(const std::vector<std::wstring>& t) : tokens(t), idx(0) {}

    bool Parse() {
        if (tokens.empty()) return false;
        bool res = ParseOr();
        if (idx < tokens.size()) {
            throw std::runtime_error("too many arguments");
        }
        return res;
    }

private:
    bool ParseOr() {
        bool left = ParseAnd();
        while (idx < tokens.size() && tokens[idx] == L"-o") {
            idx++;
            bool right = ParseAnd();
            left = left || right;
        }
        return left;
    }

    bool ParseAnd() {
        bool left = ParseNot();
        while (idx < tokens.size() && tokens[idx] == L"-a") {
            idx++;
            bool right = ParseNot();
            left = left && right;
        }
        return left;
    }

    bool ParseNot() {
        if (idx < tokens.size() && tokens[idx] == L"!") {
            idx++;
            return !ParseNot();
        }
        return ParseFactor();
    }

    bool ParseFactor() {
        if (idx >= tokens.size()) {
            throw std::runtime_error("argument expected");
        }

        if (tokens[idx] == L"(") {
            idx++;
            bool val = ParseOr();
            if (idx >= tokens.size() || tokens[idx] != L")") {
                throw std::runtime_error("missing ')'");
            }
            idx++;
            return val;
        }

        return ParsePrimary();
    }

    bool ParsePrimary() {
        if (idx >= tokens.size()) return false;

        if (idx + 2 < tokens.size() && IsBinaryOp(tokens[idx + 1])) {
            std::wstring left = tokens[idx];
            std::wstring op = tokens[idx + 1];
            std::wstring right = tokens[idx + 2];
            idx += 3;
            return EvalBinary(op, left, right);
        }

        if (IsUnaryOp(tokens[idx])) {
            std::wstring op = tokens[idx];
            idx++;
            std::wstring arg = L"";
            if (idx < tokens.size()) {
                arg = tokens[idx];
                idx++;
            }
            return EvalUnary(op, arg);
        }

        std::wstring str = tokens[idx];
        idx++;
        return !str.empty();
    }

    static bool IsUnaryOp(const std::wstring& op) {
        static const std::vector<std::wstring> ops = {
            L"-b", L"-c", L"-d", L"-e", L"-a", L"-f", L"-g", L"-h", L"-L", L"-k",
            L"-p", L"-r", L"-s", L"-S", L"-t", L"-u", L"-w", L"-x", L"-O", L"-G",
            L"-z", L"-n"
        };
        return std::find(ops.begin(), ops.end(), op) != ops.end();
    }

    static bool IsBinaryOp(const std::wstring& op) {
        static const std::vector<std::wstring> ops = {
            L"=", L"==", L"!=", L"<", L">",
            L"-eq", L"-ne", L"-gt", L"-ge", L"-lt", L"-le",
            L"-nt", L"-ot", L"-ef"
        };
        return std::find(ops.begin(), ops.end(), op) != ops.end();
    }

    static bool EvalUnary(const std::wstring& op, const std::wstring& arg) {
        if (op == L"-z") return arg.empty();
        if (op == L"-n") return !arg.empty();
        if (op == L"-e" || op == L"-a") return FileSystemInspector::FileExists(arg);
        if (op == L"-d") return FileSystemInspector::IsDirectory(arg);
        if (op == L"-f") return FileSystemInspector::IsRegularFile(arg);
        if (op == L"-s") return FileSystemInspector::IsNonEmpty(arg);
        if (op == L"-h" || op == L"-L") return FileSystemInspector::IsSymlink(arg);
        if (op == L"-r") return FileSystemInspector::IsReadable(arg);
        if (op == L"-w") return FileSystemInspector::IsWritable(arg);
        if (op == L"-x") return FileSystemInspector::IsExecutable(arg);
        if (op == L"-t") return FileSystemInspector::IsTerminal(arg);
        if (op == L"-c") return FileSystemInspector::IsCharDevice(arg);
        if (op == L"-p" || op == L"-S") return FileSystemInspector::IsNamedPipe(arg);
        if (op == L"-b") return false;
        if (op == L"-g" || op == L"-u" || op == L"-k" || op == L"-O" || op == L"-G") {
            return FileSystemInspector::FileExists(arg);
        }
        return false;
    }

    static bool EvalBinary(const std::wstring& op, const std::wstring& left, const std::wstring& right) {
        if (op == L"=" || op == L"==") return left == right;
        if (op == L"!=") return left != right;
        if (op == L"<") return left < right;
        if (op == L">") return left > right;

        if (op == L"-nt") return FileSystemInspector::FileNewerThan(left, right);
        if (op == L"-ot") return FileSystemInspector::FileOlderThan(left, right);
        if (op == L"-ef") return FileSystemInspector::SameFile(left, right);

        long long n1 = TypeParser::ParseLong(left);
        long long n2 = TypeParser::ParseLong(right);

        if (op == L"-eq") return n1 == n2;
        if (op == L"-ne") return n1 != n2;
        if (op == L"-gt") return n1 > n2;
        if (op == L"-ge") return n1 >= n2;
        if (op == L"-lt") return n1 < n2;
        if (op == L"-le") return n1 <= n2;

        return false;
    }
};

// ============================================================================
// 4. OUTPUT FORMATTER
// ============================================================================

class OutputFormatter {
public:
    static void Emit(int format, const std::wstring& pipeCommand, bool result) {
        if (format == 0 && pipeCommand.empty()) return;

        std::wstring text = format == 1 ? (L"{\"result\":" + std::wstring(result ? L"true" : L"false") + L"}\n") :
                            format == 2 ? (L"result\n" + std::wstring(result ? L"true\n" : L"false\n")) :
                                          (L"RESULT\n" + std::wstring(result ? L"true\n" : L"false\n"));

        if (!pipeCommand.empty()) {
            FILE* pipe = _wpopen(pipeCommand.c_str(), L"w");
            if (pipe) {
                int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
                if (size > 0) {
                    std::string narrow(static_cast<size_t>(size), '\0');
                    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), narrow.data(), size, nullptr, nullptr);
                    fwrite(narrow.data(), 1, narrow.size(), pipe);
                }
                _pclose(pipe);
            }
        } else {
            std::wcout << text;
        }
    }
};

// ============================================================================
// 5. OPTION PARSER & APPLICATION CONTROLLER
// ============================================================================

struct TestOptions {
    bool isBracket = false;
    int format = 0;
    std::wstring pipeCommand;
    std::vector<std::wstring> tokens;
};

class OptionParser {
public:
    static void PrintHelp() {
        std::wcout << L"Usage:\n"
                   << L"  test EXPRESSION\n"
                   << L"  [ EXPRESSION ]\n"
                   << L"  test --help\n"
                   << L"  test --version\n\n"
                   << L"File tests:\n"
                   << L"  -d FILE   True if FILE exists and is a directory\n"
                   << L"  -e FILE   True if FILE exists\n"
                   << L"  -f FILE   True if FILE exists and is a regular file\n"
                   << L"  -s FILE   True if FILE exists and size > 0\n"
                   << L"  -h FILE   True if FILE exists and is a symlink/reparse point\n"
                   << L"  -r FILE   True if FILE is readable\n"
                   << L"  -w FILE   True if FILE is writable\n"
                   << L"  -x FILE   True if FILE is executable\n"
                   << L"  -t FD     True if file descriptor FD (0, 1, 2) is open on a console\n"
                   << L"  F1 -nt F2 True if F1 modification date is newer than F2\n"
                   << L"  F1 -ot F2 True if F1 modification date is older than F2\n"
                   << L"  F1 -ef F2 True if F1 and F2 point to the same physical file\n\n"
                   << L"String tests:\n"
                   << L"  -z STRING True if length of STRING is zero\n"
                   << L"  -n STRING True if length of STRING is non-zero\n"
                   << L"  S1 = S2   True if strings are equal\n"
                   << L"  S1 != S2  True if strings are not equal\n\n"
                   << L"Integer tests:\n"
                   << L"  N1 -eq N2 True if N1 equals N2\n"
                   << L"  N1 -ne N2 True if N1 is not equal to N2\n"
                   << L"  N1 -gt N2 True if N1 is greater than N2\n"
                   << L"  N1 -ge N2 True if N1 is greater than or equal to N2\n"
                   << L"  N1 -lt N2 True if N1 is less than N2\n"
                   << L"  N1 -le N2 True if N1 is less than or equal to N2\n\n"
                   << L"Logical operators:\n"
                   << L"  ! EXPR    Logical NOT\n"
                   << L"  E1 -a E2  Logical AND\n"
                   << L"  E1 -o E2  Logical OR\n"
                   << L"  ( EXPR )  Group expression\n";
        std::wcout << L"  --json, --csv, --table  Format the expression result\n  --pipe COMMAND          Send formatted result through COMMAND\n";
    }

    static void PrintVersion() {
        std::wcout << L"test\n";
    }

    TestOptions Parse(int argc, wchar_t* argv[]) const {
        TestOptions opts;
        std::wstring prog = argv[0];
        size_t slash = prog.find_last_of(L"\\/");
        std::wstring exec_name = (slash != std::wstring::npos) ? prog.substr(slash + 1) : prog;
        std::transform(exec_name.begin(), exec_name.end(), exec_name.begin(), ::towlower);

        opts.isBracket = (exec_name == L"[" || exec_name == L"[.exe");

        for (int i = 1; i < argc; ++i) {
            std::wstring token = argv[i];
            if (token == L"--json") opts.format = 1;
            else if (token == L"--csv") opts.format = 2;
            else if (token == L"--table") opts.format = 3;
            else if (token == L"--pipe" && i + 1 < argc) opts.pipeCommand = argv[++i];
            else opts.tokens.push_back(token);
        }
        return opts;
    }
};

class TestApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]) const {
        TestOptions opts = m_parser.Parse(argc, argv);

        if (opts.tokens.size() == 1 && opts.tokens[0] == L"--help") {
            OptionParser::PrintHelp();
            return 0;
        }

        if (opts.tokens.size() == 1 && opts.tokens[0] == L"--version") {
            OptionParser::PrintVersion();
            return 0;
        }

        if (opts.isBracket) {
            if (opts.tokens.empty() || opts.tokens.back() != L"]") {
                std::fwprintf(stderr, L"[: missing ']'\n");
                return 2;
            }
            opts.tokens.pop_back();
        }

        if (opts.tokens.empty()) {
            return 1;
        }

        try {
            Evaluator eval(opts.tokens);
            bool result = eval.Parse();
            OutputFormatter::Emit(opts.format, opts.pipeCommand, result);
            return result ? 0 : 1;
        } catch (const std::exception& ex) {
            std::fwprintf(stderr, L"test: %S\n", ex.what());
            return 2;
        }
    }
};

int wmain(int argc, wchar_t* argv[]) {
    TestApplication app;
    return app.Run(argc, argv);
}
