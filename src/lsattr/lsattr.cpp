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

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>

// ============================================================================
// 1. RAII HANDLES & DATA MODELS
// ============================================================================

class ScopedFindHandle {
public:
    explicit ScopedFindHandle(HANDLE handle = INVALID_HANDLE_VALUE) : m_handle(handle) {}

    ~ScopedFindHandle() {
        Close();
    }

    ScopedFindHandle(const ScopedFindHandle&) = delete;
    ScopedFindHandle& operator=(const ScopedFindHandle&) = delete;

    ScopedFindHandle(ScopedFindHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = INVALID_HANDLE_VALUE;
    }

    ScopedFindHandle& operator=(ScopedFindHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    HANDLE Get() const { return m_handle; }
    bool IsValid() const { return m_handle != INVALID_HANDLE_VALUE && m_handle != NULL; }

    void Close() {
        if (IsValid()) {
            FindClose(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }
    }

private:
    HANDLE m_handle;
};

struct AttrRow {
    std::wstring flags;
    std::wstring path;
};

enum class OutputFormat {
    Table,
    Csv,
    Json
};

// ============================================================================
// 2. ATTRIBUTE INSPECTOR & TRAVERSER
// ============================================================================

class AttributeInspector {
public:
    static std::wstring GetFlags(DWORD attrs) {
        std::wstring flags;
        flags += (attrs & FILE_ATTRIBUTE_ARCHIVE) ? L'a' : L'-';
        flags += (attrs & FILE_ATTRIBUTE_HIDDEN) ? L'h' : L'-';
        flags += (attrs & FILE_ATTRIBUTE_SYSTEM) ? L's' : L'-';
        flags += (attrs & FILE_ATTRIBUTE_READONLY) ? L'i' : L'-';
        return flags;
    }

    static std::wstring GetFlagsForPath(const std::wstring& path) {
        DWORD attrs = GetFileAttributesW(path.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            return L"";
        }
        return GetFlags(attrs);
    }
};

class LsattrTraverser {
public:
    static bool Collect(const std::wstring& path, bool recursive, std::vector<AttrRow>& rows, std::wostream& err) {
        DWORD attrs = GetFileAttributesW(path.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            err << L"lsattr: " << path << L": No such file or directory\n";
            return false;
        }

        rows.push_back({ AttributeInspector::GetFlags(attrs), path });

        if (recursive && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            std::wstring pattern = path;
            if (!pattern.empty() && pattern.back() != L'\\') {
                pattern.push_back(L'\\');
            }
            pattern.push_back(L'*');

            WIN32_FIND_DATAW findData = {};
            ScopedFindHandle hFind(FindFirstFileW(pattern.c_str(), &findData));
            if (hFind.IsValid()) {
                do {
                    if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
                        continue;
                    }
                    std::wstring child = path;
                    if (!child.empty() && child.back() != L'\\') {
                        child.push_back(L'\\');
                    }
                    child += findData.cFileName;

                    DWORD childAttrs = GetFileAttributesW(child.c_str());
                    if (childAttrs != INVALID_FILE_ATTRIBUTES) {
                        rows.push_back({ AttributeInspector::GetFlags(childAttrs), child });
                    }
                } while (FindNextFileW(hFind.Get(), &findData));
            }
        }

        return true;
    }
};

// ============================================================================
// 3. OPTIONS & REPORTER
// ============================================================================

class LsattrOptions {
public:
    bool recursive = false;
    bool showHelp = false;
    bool showVersion = false;
    OutputFormat format = OutputFormat::Table;
    std::vector<std::wstring> paths;

    bool Parse(int argc, wchar_t* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] ? argv[i] : L"";
            if (arg == L"--") {
                for (++i; i < argc; ++i) {
                    paths.push_back(argv[i] ? argv[i] : L"");
                }
                break;
            }
            if (arg == L"-h" || arg == L"--help") {
                showHelp = true;
                return true;
            }
            if (arg == L"-V" || arg == L"--version") {
                showVersion = true;
                return true;
            }
            if (arg == L"-R" || arg == L"--recursive") {
                recursive = true;
                continue;
            }
            if (arg == L"--table") {
                format = OutputFormat::Table;
                continue;
            }
            if (arg == L"--csv") {
                format = OutputFormat::Csv;
                continue;
            }
            if (arg == L"--json") {
                format = OutputFormat::Json;
                continue;
            }
            if (arg == L"-") {
                std::wstring token;
                while (std::wcin >> token) {
                    paths.push_back(token);
                }
                continue;
            }
            if (!arg.empty() && arg[0] == L'-') {
                std::wcerr << L"lsattr: unknown option -- " << arg << L"\n";
                return false;
            }

            paths.push_back(arg);
        }

        if (paths.empty() && !showHelp && !showVersion) {
            return false;
        }

        return true;
    }

    void PrintUsage(const wchar_t* progName) const {
           std::wcout << LR"(lsattr(1)               CrossShell for UNIX Reference Manual                lsattr(1)

    NAME
        lsattr - display Windows file attributes as UNIX-style flags

    SYNOPSIS
        lsattr [OPTIONS] FILE...

    DESCRIPTION
        Reports Windows file attributes using four UNIX-style flags: a for archive,
        h for hidden, s for system, and i for read-only (immutable). A dash means
        that an attribute is not present.

    OPTIONS
        -R, --recursive
            Enumerate the immediate contents of directories.

        --table
            Use aligned table output (default).

        --csv
            Emit CSV output.

        --json
            Emit a JSON array.

        -
            Read paths from standard input.

        -h, --help
            Display this comprehensive reference manual and exit.

        -V, --version
            Display version information and exit.

        --
            End options; remaining values are paths.

    OUTPUT
        Table output contains Flags and Path columns. CSV fields are flags and path;
        JSON objects contain flags and path properties.

    EXAMPLES
        lsattr app.exe
            Display attributes for one file.

        lsattr -R --json build
            Enumerate a directory and emit JSON.

        Get-ChildItem -Name | lsattr - --csv
            Read paths from a PowerShell pipeline and emit CSV.

    EXIT STATUS
        0          Help, version, or successful processing.
        1          Invalid paths or failed option parsing; valid rows may still print.

    CrossShell for UNIX                                                      lsattr(1)
    )";
    }

    void PrintVersion() const {
        std::wcout << L"lsattr 1.0.0\n";
    }
};

class LsattrReporter {
public:
    static std::string ToUtf8(const std::wstring& value) {
        int n = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        std::string out(static_cast<size_t>(n), '\0');
        if (n) {
            WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), n, nullptr, nullptr);
        }
        return out;
    }

    static std::string EscapeCsv(const std::string& value) {
        std::string out = "\"";
        for (char c : value) {
            out += (c == '"') ? "\"\"" : std::string(1, c);
        }
        return out + '"';
    }

    static std::string EscapeJson(const std::string& value) {
        std::string out;
        for (char c : value) {
            if (c == '"' || c == '\\') out += '\\';
            if (c == '\n') out += "\\n";
            else out += c;
        }
        return out;
    }

    static void PrintRows(const std::vector<AttrRow>& rows, OutputFormat format) {
        if (format == OutputFormat::Csv) {
            std::cout << "flags,path\n";
            for (const auto& row : rows) {
                std::cout << EscapeCsv(ToUtf8(row.flags)) << ","
                          << EscapeCsv(ToUtf8(row.path)) << "\n";
            }
        } else if (format == OutputFormat::Json) {
            std::cout << "[\n";
            for (size_t i = 0; i < rows.size(); ++i) {
                std::cout << "  {\"flags\": \"" << EscapeJson(ToUtf8(rows[i].flags))
                          << "\", \"path\": \"" << EscapeJson(ToUtf8(rows[i].path)) << "\"}";
                if (i + 1 < rows.size()) std::cout << ",";
                std::cout << "\n";
            }
            std::cout << "]\n";
        } else {
            std::wcout << L"Flags  Path\n------ ------------------------------------------------------------\n";
            for (const auto& row : rows) {
                std::wcout << std::left << std::setw(6) << row.flags << L" " << row.path << L"\n";
            }
        }
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class LsattrApplication {
public:
    int Run(int argc, wchar_t* argv[]) const {
        LsattrOptions options;
        if (!options.Parse(argc, argv)) {
            options.PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"lsattr");
            return 1;
        }

        if (options.showHelp) {
            options.PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"lsattr");
            return 0;
        }
        if (options.showVersion) {
            options.PrintVersion();
            return 0;
        }

        std::vector<AttrRow> rows;
        bool allOk = true;

        for (const auto& path : options.paths) {
            if (!LsattrTraverser::Collect(path, options.recursive, rows, std::wcerr)) {
                allOk = false;
            }
        }

        LsattrReporter::PrintRows(rows, options.format);
        return allOk ? 0 : 1;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    LsattrApplication app;
    return app.Run(argc, argv);
}
