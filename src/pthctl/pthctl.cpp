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

/*
 * pthctl - Windows PATH Environment Variable Manager
 * 
 * Copyright (C) 2026, Roberto J Dohnert
 * 
 *  Windows CLI utility for inspecting and modifying
 * System and User PATH environment variables.
 */

#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <shlwapi.h>
#include <cstdio>
#include <streambuf>

#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Shlwapi.lib")

// Program metadata
constexpr const wchar_t* PROGRAM_NAME = L"pthctl";
constexpr const wchar_t* VERSION      = L"1.0.0";
constexpr const wchar_t* COPYRIGHT    = L"Copyright (C) 2026, Roberto J Dohnert";
constexpr size_t PATH_HARD_LIMIT_CHARS = 32767;
constexpr size_t PATH_WARN_LIMIT_CHARS = 8191;

enum class OutputFormat { Human, Json, Csv, Table };

class PthOutputBuffer : public std::wstreambuf {
    std::wstreambuf* target_;
    OutputFormat format_;
    std::wstring pending_;
    bool first_ = true;
    void write(const std::wstring& value) { target_->sputn(value.data(), static_cast<std::streamsize>(value.size())); }
    void emit() {
        if (pending_.empty()) return;
        std::wstring formatted;
        if (format_ == OutputFormat::Json) {
            if (!first_) write(L",\n");
            first_ = false;
            formatted = L"{\"message\":\"";
            for (wchar_t ch : pending_) { if (ch == L'"') formatted += L"\\\""; else if (ch == L'\\') formatted += L"\\\\"; else if (ch == L'\r') formatted += L"\\r"; else formatted += ch; }
            formatted += L"\"}";
        } else if (format_ == OutputFormat::Csv) {
            formatted = L"\"";
            for (wchar_t ch : pending_) { if (ch == L'"') formatted += L"\"\""; else formatted += ch; }
            formatted += L"\"\n";
        } else if (format_ == OutputFormat::Table) {
            formatted = pending_ + L"\n";
        } else {
            formatted = pending_ + L"\n";
        }
        write(formatted);
        pending_.clear();
    }
protected:
    int_type overflow(int_type ch) override {
        if (ch != traits_type::eof()) { pending_.push_back(static_cast<wchar_t>(ch)); if (ch == L'\n') { pending_.pop_back(); emit(); } }
        return traits_type::not_eof(ch);
    }
    int sync() override { emit(); return target_->pubsync(); }
public:
    PthOutputBuffer(std::wstreambuf* target, OutputFormat format) : target_(target), format_(format) {}
    ~PthOutputBuffer() override = default;
    void setTarget(std::wstreambuf* target) { target_ = target; }
    void start() { if (format_ == OutputFormat::Json) write(L"[\n"); }
    void finish() { emit(); if (format_ == OutputFormat::Json) write(L"\n]\n"); }
};

class PthPipeBuffer : public std::wstreambuf {
    FILE* file_;
    wchar_t buffer_[1024];
public:
    explicit PthPipeBuffer(FILE* file) : file_(file) { setp(buffer_, buffer_ + 1024); }
    int_type overflow(int_type ch) override { if (ch != traits_type::eof()) { *pptr() = static_cast<wchar_t>(ch); pbump(1); } return sync() == 0 ? traits_type::not_eof(ch) : traits_type::eof(); }
    int sync() override { auto count = pptr() - pbase(); if (count && std::fwrite(pbase(), sizeof(wchar_t), static_cast<size_t>(count), file_) != static_cast<size_t>(count)) return -1; setp(buffer_, buffer_ + 1024); return std::fflush(file_) == 0 ? 0 : -1; }
};

class PthOutputSession {
    std::wstreambuf* old_;
    PthOutputBuffer buffer_;
    FILE* pipe_ = nullptr;
    PthPipeBuffer* pipeBuffer_ = nullptr;
public:
    PthOutputSession(OutputFormat format, const std::wstring& command) : old_(std::wcout.rdbuf()), buffer_(old_, format) {
        if (!command.empty()) {
            pipe_ = _wpopen(command.c_str(), L"w");
            if (pipe_) {
                pipeBuffer_ = new PthPipeBuffer(pipe_);
                buffer_.setTarget(pipeBuffer_);
                std::wcout.rdbuf(&buffer_);
            }
        } else std::wcout.rdbuf(&buffer_);
        buffer_.start();
    }
    ~PthOutputSession() { std::wcout.flush(); buffer_.finish(); std::wcout.rdbuf(old_); delete pipeBuffer_; if (pipe_) _pclose(pipe_); }
};

// Registry Subkeys
constexpr const wchar_t* USER_ENV_KEY   = L"Environment";
constexpr const wchar_t* SYSTEM_ENV_KEY = L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment";

enum class Scope {
    User,
    System
};

// --- Helper Functions ---

// Trims quotes and whitespace, standardizes slashes to backslashes, removes trailing slashes
std::wstring NormalizePath(const std::wstring& path) {
    size_t start = path.find_first_not_of(L" \t\n\r\"");
    if (start == std::wstring::npos) return L"";
    size_t end = path.find_last_not_of(L" \t\n\r\"");
    std::wstring result = path.substr(start, end - start + 1);

    for (auto& ch : result) {
        if (ch == L'/') ch = L'\\';
    }

    // Preserve root slash like "C:\" but remove trailing slash for subdirs ("C:\Tools\" -> "C:\Tools")
    if (result.length() > 3 && result.back() == L'\\') {
        result.pop_back();
    }

    return result;
}

// Case-insensitive comparison suitable for Windows file paths
bool PathEquals(const std::wstring& p1, const std::wstring& p2) {
    std::wstring n1 = NormalizePath(p1);
    std::wstring n2 = NormalizePath(p2);
    if (n1.length() != n2.length()) return false;
    return _wcsicmp(n1.c_str(), n2.c_str()) == 0;
}

// Split PATH string by semicolon delimiter
std::vector<std::wstring> SplitPath(const std::wstring& rawPath) {
    std::vector<std::wstring> list;
    std::wstringstream ss(rawPath);
    std::wstring item;

    while (std::getline(ss, item, L';')) {
        std::wstring trimmed = NormalizePath(item);
        if (!trimmed.empty()) {
            list.push_back(trimmed);
        }
    }
    return list;
}

// Split PATH string by semicolon delimiter while preserving original entry text
std::vector<std::wstring> SplitPathRaw(const std::wstring& rawPath) {
    std::vector<std::wstring> list;
    std::wstringstream ss(rawPath);
    std::wstring item;

    while (std::getline(ss, item, L';')) {
        list.push_back(item);
    }
    return list;
}

// Join vector of paths back into semicolon-delimited string
std::wstring JoinPath(const std::vector<std::wstring>& list) {
    std::wstring joined;
    for (size_t i = 0; i < list.size(); ++i) {
        joined += list[i];
        if (i + 1 < list.size()) {
            joined += L";";
        }
    }
    return joined;
}

// Join raw path entries back into a semicolon-delimited string
std::wstring JoinRawPath(const std::vector<std::wstring>& list) {
    std::wstring joined;
    for (size_t i = 0; i < list.size(); ++i) {
        joined += list[i];
        if (i + 1 < list.size()) {
            joined += L";";
        }
    }
    return joined;
}

bool ValidatePathLength(const std::wstring& updatedPath, bool dryRun) {
    const size_t updatedChars = updatedPath.length();
    if (updatedChars >= PATH_HARD_LIMIT_CHARS) {
        std::wcerr << L"pthctl: error: resulting PATH length (" << updatedChars
                  << L" chars) exceeds Windows limit (" << (PATH_HARD_LIMIT_CHARS - 1)
                  << L" chars).\n";
        return false;
    }

    if (updatedChars > PATH_WARN_LIMIT_CHARS) {
        std::wcerr << L"pthctl: warning: resulting PATH length is " << updatedChars
                  << L" chars; some older tools may fail beyond " << PATH_WARN_LIMIT_CHARS
                  << L" chars.";
        if (dryRun) {
            std::wcerr << L"\n";
        } else {
            std::wcerr << L"\n";
        }
    }

    return true;
}

// Reads PATH variable from Registry
bool ReadRegistryPath(Scope scope, std::wstring& outPath, DWORD& outType) {
    HKEY hKey;
    HKEY rootKey = (scope == Scope::User) ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
    const wchar_t* subKey = (scope == Scope::User) ? USER_ENV_KEY : SYSTEM_ENV_KEY;

    LONG status = RegOpenKeyExW(rootKey, subKey, 0, KEY_READ, &hKey);
    if (status != ERROR_SUCCESS) {
        std::wcerr << L"pthctl: error opening registry key for reading (code " << status << L")\n";
        return false;
    }

    DWORD bytesNeeded = 0;
    DWORD type = 0;
    
    // Try reading "Path" or "PATH"
    status = RegQueryValueExW(hKey, L"Path", NULL, &type, NULL, &bytesNeeded);
    const wchar_t* valueName = L"Path";
    if (status != ERROR_SUCCESS) {
        status = RegQueryValueExW(hKey, L"PATH", NULL, &type, NULL, &bytesNeeded);
        valueName = L"PATH";
    }

    if (status != ERROR_SUCCESS && status != ERROR_MORE_DATA) {
        RegCloseKey(hKey);
        outPath = L"";
        outType = REG_EXPAND_SZ;
        return true; // Key exists, but PATH string is empty
    }

    std::vector<wchar_t> buffer((bytesNeeded / sizeof(wchar_t)) + 1, 0);
    status = RegQueryValueExW(hKey, valueName, NULL, &type, reinterpret_cast<LPBYTE>(buffer.data()), &bytesNeeded);
    RegCloseKey(hKey);

    if (status == ERROR_SUCCESS) {
        outPath = buffer.data();
        outType = type;
        return true;
    }

    return false;
}

// Writes updated PATH to Registry and notifies running applications
bool WriteRegistryPath(Scope scope, const std::wstring& newPath, DWORD type) {
    HKEY hKey;
    HKEY rootKey = (scope == Scope::User) ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
    const wchar_t* subKey = (scope == Scope::User) ? USER_ENV_KEY : SYSTEM_ENV_KEY;

    LONG status = RegOpenKeyExW(rootKey, subKey, 0, KEY_SET_VALUE, &hKey);
    if (status != ERROR_SUCCESS) {
        if (status == ERROR_ACCESS_DENIED) {
            std::wcerr << L"pthctl: error: access denied. Modifying "
                       << (scope == Scope::User ? L"user" : L"system")
                       << L" PATH requires elevated (Administrator) privileges.\n";
        } else {
            std::wcerr << L"pthctl: error opening registry key for writing (code " << status << L")\n";
        }
        return false;
    }

    // Default to REG_EXPAND_SZ for environment paths
    if (type != REG_SZ && type != REG_EXPAND_SZ) {
        type = REG_EXPAND_SZ;
    }

    size_t byteCount = (newPath.length() + 1) * sizeof(wchar_t);
    status = RegSetValueExW(hKey, L"Path", 0, type,
                            reinterpret_cast<const BYTE*>(newPath.c_str()),
                            static_cast<DWORD>(byteCount));
    RegCloseKey(hKey);

    if (status != ERROR_SUCCESS) {
        std::wcerr << L"pthctl: error writing to registry (code " << status << L")\n";
        return false;
    }

    // Notify top-level windows of environment variable changes
    DWORD_PTR dwResult = 0;
    if (!SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                             reinterpret_cast<LPARAM>(L"Environment"),
                             SMTO_ABORTIFHUNG, 5000, &dwResult)) {
        DWORD notifyError = GetLastError();
        std::wcerr << L"pthctl: warning: PATH updated, but environment change notification failed";
        if (notifyError != ERROR_SUCCESS) {
            std::wcerr << L" (code " << notifyError << L")";
        }
        std::wcerr << L". New shells may be required to see the change.\n";
    }

    return true;
}

// --- Displays Help & Version ---

void ShowVersion() {
    std::wcout << PROGRAM_NAME << L" version " << VERSION << L"\n"
              << COPYRIGHT << L"\n"
              << L"License: BSD-3 Clause License\n"
              << L"Written by Roberto J Dohnert.\n";
}

void ShowHelp() {
    std::wcout << LR"(pthctl(1)               CrossShell for UNIX Reference Manual                 pthctl(1)

    NAME
        pthctl - query, modify, and deduplicate Windows PATH environment variables

    SYNOPSIS
        pthctl COMMAND [OPTIONS] [PATH]

    DESCRIPTION
        Manages user and system-level PATH environment variables directly in the
        Windows Registry. Supports inspecting, appending, prepending, verifying,
        and deduplicating PATH directories with real-time environment broadcasts
        and structured reporting.

    COMMANDS
        list, ls
            Display current PATH entries with indices and existence status.

        add PATH
            Append or prepend a directory entry to the PATH variable.

        remove, rm PATH
            Remove a specified directory entry from the PATH variable.

        check PATH
            Verify whether a directory entry is present in the PATH variable.

        clean
            Remove duplicate and non-existent directory entries from PATH.

    OPTIONS
        -u, --user
            Target the current User environment PATH variable (default).

        -s, --system
            Target the machine-wide System PATH variable (requires Admin).

        -p, --prepend
            Insert new path entries at the beginning of PATH rather than appending.

        -d, --dry-run
            Simulate modifications without committing changes to the registry.

        --json, --csv, --table
            Format output as JSON objects, CSV records, or an aligned table.

        --pipe COMMAND
            Stream formatted output directly to another command or utility.

        -h, --help
            Display this reference manual.

        -v, --version
            Display version and license information.

    EXAMPLES
        pthctl list --user
            List all entries in the user PATH variable.

        pthctl list --system --table
            Display system PATH entries in an aligned tabular grid.

        pthctl add C:\Tools\bin --user
            Append C:\Tools\bin to the user PATH variable.

        pthctl add C:\Tools\bin --system --prepend
            Prepend C:\Tools\bin to the system-wide PATH variable.

        pthctl remove C:\OldTools\bin --user
            Remove an obsolete path entry from the user PATH.

        pthctl clean --user --dry-run
            Preview deduplication and cleanup of the user PATH.

        pthctl list --json
            Export PATH entries as structured JSON.

    CrossShell for UNIX                                                     pthctl(1)
)";
}
int CommandList(Scope scope) {
    std::wstring rawPath;
    DWORD type = 0;
    if (!ReadRegistryPath(scope, rawPath, type)) return 1;

    auto entries = SplitPath(rawPath);
    std::wcout << L"[" << (scope == Scope::User ? L"User" : L"System") << L" PATH (" << entries.size() << L" entries)]\n";
    for (size_t i = 0; i < entries.size(); ++i) {
        std::wcout << L"  " << (i + 1) << L". " << entries[i] << L"\n";
    }
    return 0;
}

int CommandAdd(Scope scope, const std::wstring& targetPath, bool prepend, bool dryRun) {
    std::wstring norm = NormalizePath(targetPath);
    if (norm.empty()) {
        std::wcerr << L"pthctl: error: invalid or empty path specified.\n";
        return 1;
    }

    std::wstring rawPath;
    DWORD type = 0;
    if (!ReadRegistryPath(scope, rawPath, type)) return 1;

    auto entries = SplitPathRaw(rawPath);

    for (const auto& entry : entries) {
        if (PathEquals(entry, norm)) {
            std::wcout << L"pthctl: path is already in " 
                      << (scope == Scope::User ? L"User" : L"System") 
                      << L" PATH: " << norm << L"\n";
            return 0;
        }
    }

    if (prepend) {
        entries.insert(entries.begin(), norm);
    } else {
        entries.push_back(norm);
    }

    std::wstring updatedPath = JoinRawPath(entries);
    if (!ValidatePathLength(updatedPath, dryRun)) {
        return 1;
    }

    if (dryRun) {
        std::wcout << L"pthctl: dry-run: would add '" << norm << L"' to "
                  << (scope == Scope::User ? L"User" : L"System") << L" PATH.\n";
        return 0;
    }

    if (WriteRegistryPath(scope, updatedPath, type)) {
        std::wcout << L"pthctl: successfully added '" << norm << L"' to " 
                  << (scope == Scope::User ? L"User" : L"System") << L" PATH.\n";
        return 0;
    }

    return 1;
}

int CommandRemove(Scope scope, const std::wstring& targetPath, bool dryRun) {
    std::wstring norm = NormalizePath(targetPath);
    if (norm.empty()) {
        std::wcerr << L"pthctl: error: invalid or empty path specified.\n";
        return 1;
    }

    std::wstring rawPath;
    DWORD type = 0;
    if (!ReadRegistryPath(scope, rawPath, type)) return 1;

    auto entries = SplitPathRaw(rawPath);
    size_t initialSize = entries.size();

    entries.erase(
        std::remove_if(entries.begin(), entries.end(), [&](const std::wstring& e) {
            return PathEquals(e, norm);
        }),
        entries.end()
    );

    if (entries.size() == initialSize) {
        std::wcout << L"pthctl: path not found in " 
                  << (scope == Scope::User ? L"User" : L"System") 
                  << L" PATH: " << norm << L"\n";
        return 0;
    }

    std::wstring updatedPath = JoinRawPath(entries);
    if (!ValidatePathLength(updatedPath, dryRun)) {
        return 1;
    }

    if (dryRun) {
        std::wcout << L"pthctl: dry-run: would remove '" << norm << L"' from "
                  << (scope == Scope::User ? L"User" : L"System") << L" PATH.\n";
        return 0;
    }

    if (WriteRegistryPath(scope, updatedPath, type)) {
        std::wcout << L"pthctl: successfully removed '" << norm << L"' from " 
                  << (scope == Scope::User ? L"User" : L"System") << L" PATH.\n";
        return 0;
    }

    return 1;
}

int CommandCheck(Scope scope, const std::wstring& targetPath) {
    std::wstring norm = NormalizePath(targetPath);
    if (norm.empty()) {
        std::wcerr << L"pthctl: error: invalid path specified.\n";
        return 1;
    }

    std::wstring rawPath;
    DWORD type = 0;
    if (!ReadRegistryPath(scope, rawPath, type)) return 1;

    auto entries = SplitPath(rawPath);
    for (size_t i = 0; i < entries.size(); ++i) {
        if (PathEquals(entries[i], norm)) {
            std::wcout << L"FOUND: '" << norm << L"' at position " << (i + 1) << L" in "
                      << (scope == Scope::User ? L"User" : L"System") << L" PATH.\n";
            return 0;
        }
    }

    std::wcout << L"NOT FOUND: '" << norm << L"' is not in "
              << (scope == Scope::User ? L"User" : L"System") << L" PATH.\n";
    return 1;
}

int CommandClean(Scope scope, bool dryRun) {
    std::wstring rawPath;
    DWORD type = 0;
    if (!ReadRegistryPath(scope, rawPath, type)) return 1;

    auto entries = SplitPathRaw(rawPath);
    std::vector<std::wstring> cleanedRaw;
    std::vector<std::wstring> cleanedNormalized;

    size_t entriesRemoved = 0;

    for (const auto& entry : entries) {
        std::wstring normalized = NormalizePath(entry);
        if (normalized.empty()) {
            entriesRemoved++;
            continue;
        }

        bool duplicate = false;
        for (const auto& existing : cleanedNormalized) {
            if (PathEquals(existing, normalized)) {
                duplicate = true;
                break;
            }
        }

        if (!duplicate) {
            cleanedRaw.push_back(entry);
            cleanedNormalized.push_back(normalized);
        } else {
            entriesRemoved++;
        }
    }

    if (entriesRemoved == 0) {
        std::wcout << L"pthctl: no duplicate or empty entries found in "
                  << (scope == Scope::User ? L"User" : L"System") << L" PATH.\n";
        return 0;
    }

    std::wstring updatedPath = JoinRawPath(cleanedRaw);
    if (!ValidatePathLength(updatedPath, dryRun)) {
        return 1;
    }

    if (dryRun) {
        std::wcout << L"pthctl: dry-run: would clean up " << entriesRemoved
                  << L" duplicate/empty entry/entries from "
                  << (scope == Scope::User ? L"User" : L"System") << L" PATH.\n";
        return 0;
    }

    if (WriteRegistryPath(scope, updatedPath, type)) {
        std::wcout << L"pthctl: cleaned up " << entriesRemoved << L" duplicate/empty entry/entries from "
                  << (scope == Scope::User ? L"User" : L"System") << L" PATH.\n";
        return 0;
    }

    return 1;
}

// --- Main Entry Point ---

int wmain(int argc, wchar_t* argv[]) {
    if (argc < 2) {
        ShowHelp();
        return 0;
    }

    std::wstring command = argv[1];

    if (command == L"-h" || command == L"--help") {
        ShowHelp();
        return 0;
    }

    if (command == L"-v" || command == L"--version") {
        ShowVersion();
        return 0;
    }

    Scope scope = Scope::User; // Default scope
    bool prepend = false;
    bool prependSpecified = false;
    bool dryRun = false;
    bool dryRunSpecified = false;
    bool userScopeSpecified = false;
    bool systemScopeSpecified = false;
    std::wstring pathArg = L"";
    bool extraPositionalArg = false;
    OutputFormat outputFormat = OutputFormat::Human;
    std::wstring pipeCommand;

    // CLI Arguments Parsing
    for (int i = 2; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"-s" || arg == L"--system") {
            if (userScopeSpecified) {
                std::wcerr << L"pthctl: error: '--user' and '--system' cannot be used together.\n"
                          << L"Try 'pthctl --help' for usage.\n";
                return 1;
            }
            systemScopeSpecified = true;
            scope = Scope::System;
        } else if (arg == L"-u" || arg == L"--user") {
            if (systemScopeSpecified) {
                std::wcerr << L"pthctl: error: '--user' and '--system' cannot be used together.\n"
                          << L"Try 'pthctl --help' for usage.\n";
                return 1;
            }
            userScopeSpecified = true;
            scope = Scope::User;
        } else if (arg == L"-p" || arg == L"--prepend") {
            prepend = true;
            prependSpecified = true;
        } else if (arg == L"-d" || arg == L"--dry-run") {
            dryRun = true;
            dryRunSpecified = true;
        } else if (arg == L"--json" || arg == L"--csv" || arg == L"--table") {
            outputFormat = arg == L"--json" ? OutputFormat::Json : (arg == L"--csv" ? OutputFormat::Csv : OutputFormat::Table);
        } else if (arg == L"--pipe" && i + 1 < argc) {
            pipeCommand = argv[++i];
        } else if (arg == L"-h" || arg == L"--help") {
            ShowHelp();
            return 0;
        } else if (arg == L"-v" || arg == L"--version") {
            ShowVersion();
            return 0;
        } else if (!arg.empty() && arg[0] == L'-') {
            std::wcerr << L"pthctl: unrecognized option '" << arg << L"'\n"
                      << L"Try 'pthctl --help' for usage.\n";
            return 1;
        } else {
            if (pathArg.empty()) {
                pathArg = arg;
            } else {
                extraPositionalArg = true;
            }
        }
    }

    if (extraPositionalArg) {
        std::wcerr << L"pthctl: error: too many positional arguments.\n"
                  << L"Try 'pthctl --help' for usage.\n";
        return 1;
    }

    if (prependSpecified && command != L"add") {
        std::wcerr << L"pthctl: error: '--prepend' is only valid with the 'add' command.\n"
                  << L"Try 'pthctl --help' for usage.\n";
        return 1;
    }

    if (dryRunSpecified && command != L"add" && command != L"remove" && command != L"rm" && command != L"clean") {
        std::wcerr << L"pthctl: error: '--dry-run' is only valid with 'add', 'remove', and 'clean'.\n"
                  << L"Try 'pthctl --help' for usage.\n";
        return 1;
    }

    PthOutputSession outputSession(outputFormat, pipeCommand);

    // Command Router
    if (command == L"list" || command == L"ls") {
        if (!pathArg.empty()) {
            std::wcerr << L"pthctl: error: 'list' does not accept a path argument.\n"
                      << L"Try 'pthctl --help' for usage.\n";
            return 1;
        }
        return CommandList(scope);
    } else if (command == L"add") {
        if (pathArg.empty()) {
            std::wcerr << L"pthctl: error: missing path argument for 'add' command.\n"
                      << L"Try 'pthctl --help' for usage.\n";
            return 1;
        }
        return CommandAdd(scope, pathArg, prepend, dryRun);
    } else if (command == L"remove" || command == L"rm") {
        if (pathArg.empty()) {
            std::wcerr << L"pthctl: error: missing path argument for 'remove' command.\n"
                      << L"Try 'pthctl --help' for usage.\n";
            return 1;
        }
        return CommandRemove(scope, pathArg, dryRun);
    } else if (command == L"check") {
        if (pathArg.empty()) {
            std::wcerr << L"pthctl: error: missing path argument for 'check' command.\n"
                      << L"Try 'pthctl --help' for usage.\n";
            return 1;
        }
        return CommandCheck(scope, pathArg);
    } else if (command == L"clean") {
        if (!pathArg.empty()) {
            std::wcerr << L"pthctl: error: 'clean' does not accept a path argument.\n"
                      << L"Try 'pthctl --help' for usage.\n";
            return 1;
        }
        return CommandClean(scope, dryRun);
    } else {
        std::wcerr << L"pthctl: unknown command '" << command << L"'\n"
                  << L"Try 'pthctl --help' for usage.\n";
        return 1;
    }
}
