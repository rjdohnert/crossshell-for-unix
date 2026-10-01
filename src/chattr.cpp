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
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstdio>
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

struct AttrFlags {
    bool archive = false;
    bool hidden = false;
    bool system = false;
    bool readonly = false;
    bool immutable = false;
};

enum class OutputFormat {
    Default,
    Json,
    Csv,
    Table
};

// ============================================================================
// 2. ATTRIBUTE ENGINE & TRAVERSER
// ============================================================================

class FileAttributeEngine {
public:
    static AttrFlags ReadFlags(DWORD attrs) {
        AttrFlags flags;
        flags.archive = (attrs & FILE_ATTRIBUTE_ARCHIVE) != 0;
        flags.hidden = (attrs & FILE_ATTRIBUTE_HIDDEN) != 0;
        flags.system = (attrs & FILE_ATTRIBUTE_SYSTEM) != 0;
        flags.readonly = (attrs & FILE_ATTRIBUTE_READONLY) != 0;
        flags.immutable = flags.readonly;
        return flags;
    }

    static bool WriteFlags(const std::wstring& path, const AttrFlags& flags) {
        DWORD attrs = GetFileAttributesW(path.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            return false;
        }

        if (flags.archive) attrs |= FILE_ATTRIBUTE_ARCHIVE;
        else attrs &= ~FILE_ATTRIBUTE_ARCHIVE;
        if (flags.hidden) attrs |= FILE_ATTRIBUTE_HIDDEN;
        else attrs &= ~FILE_ATTRIBUTE_HIDDEN;
        if (flags.system) attrs |= FILE_ATTRIBUTE_SYSTEM;
        else attrs &= ~FILE_ATTRIBUTE_SYSTEM;
        if (flags.readonly || flags.immutable) attrs |= FILE_ATTRIBUTE_READONLY;
        else attrs &= ~FILE_ATTRIBUTE_READONLY;

        return SetFileAttributesW(path.c_str(), attrs) != FALSE;
    }

    static void ApplySpec(const std::wstring& spec, AttrFlags& flags) {
        if (spec.size() < 2) return;

        wchar_t op = spec[0];
        for (size_t i = 1; i < spec.size(); ++i) {
            wchar_t c = static_cast<wchar_t>(towlower(spec[i]));
            switch (c) {
                case L'a': flags.archive = (op == L'+'); break;
                case L'h': flags.hidden = (op == L'+'); break;
                case L's': flags.system = (op == L'+'); break;
                case L'i': flags.immutable = (op == L'+'); flags.readonly = (op == L'+'); break;
                default: break;
            }
        }
    }

    static std::wstring FormatFlags(const AttrFlags& flags) {
        std::wstring out;
        out += (flags.archive ? L'a' : L'-');
        out += (flags.hidden ? L'h' : L'-');
        out += (flags.system ? L's' : L'-');
        out += (flags.immutable ? L'i' : L'-');
        return out;
    }
};

class AttributeTraverser {
public:
    static bool ProcessFile(const std::wstring& path, const std::vector<std::wstring>& specs, bool recursive, bool statusOnly, std::wostream& out, std::wostream& err) {
        DWORD attrs = GetFileAttributesW(path.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            err << L"chattr: " << path << L": No such file or directory\n";
            return false;
        }

        bool ok = true;
        if (statusOnly) {
            AttrFlags flags = FileAttributeEngine::ReadFlags(attrs);
            out << FileAttributeEngine::FormatFlags(flags) << L" " << path << L"\n";
        } else {
            AttrFlags flags = FileAttributeEngine::ReadFlags(attrs);
            for (const auto& spec : specs) {
                FileAttributeEngine::ApplySpec(spec, flags);
            }
            if (!FileAttributeEngine::WriteFlags(path, flags)) {
                err << L"chattr: failed to set attributes on " << path << L"\n";
                ok = false;
            }
        }

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

                    if (!ProcessFile(child, specs, recursive, statusOnly, out, err)) {
                        ok = false;
                    }
                } while (FindNextFileW(hFind.Get(), &findData));
            }
        }

        return ok;
    }
};

// ============================================================================
// 3. OPTIONS & REPORTER
// ============================================================================

class ChattrOptions {
public:
    bool recursive = false;
    bool statusOnly = false;
    bool showHelp = false;
    bool showVersion = false;
    OutputFormat format = OutputFormat::Default;
    std::wstring pipeCommand;
    std::vector<std::wstring> specs;
    std::vector<std::wstring> paths;

    bool Parse(int argc, wchar_t* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] ? argv[i] : L"";
            if (arg == L"--json") { format = OutputFormat::Json; continue; }
            if (arg == L"--csv") { format = OutputFormat::Csv; continue; }
            if (arg == L"--table") { format = OutputFormat::Table; continue; }
            if (arg == L"--pipe" && i + 1 < argc) { pipeCommand = argv[++i]; continue; }
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
            if (arg == L"-p") {
                statusOnly = true;
                continue;
            }
            if (arg == L"-f") {
                continue;
            }
            if (!arg.empty() && (arg[0] == L'+' || arg[0] == L'-')) {
                specs.push_back(arg);
                continue;
            }
            paths.push_back(arg);
        }

        if (paths.empty() && !showHelp && !showVersion) {
            return false;
        }

        return true;
    }

    void PrintUsage(const wchar_t* progName) const {
        std::wcout << LR"(chattr(1)               CrossShell for UNIX Reference Manual                chattr(1)

    NAME
        chattr - change file attributes on Windows NTFS/ReFS filesystems

    SYNOPSIS
        chattr [OPTIONS] [+/-ATTR...] FILE...

    DESCRIPTION
        chattr changes the file attributes on a Windows filesystem using Unix-style
        operator syntax (+, -, =). Supports Archive (a), Hidden (h), System (s),
        and Read-Only/Immutable (i).

    OPTIONS
        -R, --recursive
            Recursively change attributes of directories and their contents.

        -p
            Print attributes without changing them.

        --json, --csv, --table
            Output structured status as JSON, CSV, or table.

        --pipe COMMAND
            Send output through COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        chattr +r file.txt
            Make file.txt read-only.

        chattr -R +h .git
            Recursively mark directory hidden.

    CrossShell for UNIX                                                 chattr(1)
)";
    }

    void PrintVersion() const {
        std::wcout << L"chattr 1.0.0\n";
    }
};

class ChattrReporter {
public:
    static std::string ToUtf8(const std::wstring& text) {
        if (text.empty()) return {};
        int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        std::string result(size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
        return result;
    }

    static void OutputResults(const std::wstring& outputData, bool success, OutputFormat format, const std::wstring& pipeCommand) {
        std::wstring text;
        if (format == OutputFormat::Json) {
            text = L"{\"status\":\"" + std::wstring(success ? L"success" : L"failure") + L"\",\"output\":\"" + outputData + L"\"}\n";
        } else if (format == OutputFormat::Csv) {
            text = L"status,output\n" + std::wstring(success ? L"success,\"" : L"failure,\"") + outputData + L"\"\n";
        } else if (format == OutputFormat::Table) {
            text = L"STATUS\tOUTPUT\n" + std::wstring(success ? L"success\t" : L"failure\t") + outputData + L"\n";
        } else {
            text = outputData;
        }

        if (!pipeCommand.empty()) {
            FILE* pipe = _wpopen(pipeCommand.c_str(), L"w");
            if (pipe) {
                std::string narrow = ToUtf8(text);
                fwrite(narrow.data(), 1, narrow.size(), pipe);
                _pclose(pipe);
            }
        } else {
            std::wcout << text;
        }
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class ChattrApplication {
public:
    int Run(int argc, wchar_t* argv[]) const {
        ChattrOptions options;
        if (!options.Parse(argc, argv)) {
            options.PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"chattr");
            return 1;
        }

        if (options.showHelp) {
            options.PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"chattr");
            return 0;
        }
        if (options.showVersion) {
            options.PrintVersion();
            return 0;
        }

        bool allOk = true;
        std::wostringstream capturedOutput;
        std::wostream& targetOut = (options.format != OutputFormat::Default || !options.pipeCommand.empty()) ? capturedOutput : std::wcout;

        for (const auto& path : options.paths) {
            if (!AttributeTraverser::ProcessFile(path, options.specs, options.recursive, options.statusOnly, targetOut, std::wcerr)) {
                allOk = false;
            }
        }

        if (options.format != OutputFormat::Default || !options.pipeCommand.empty()) {
            ChattrReporter::OutputResults(capturedOutput.str(), allOk, options.format, options.pipeCommand);
        }

        return allOk ? 0 : 1;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    ChattrApplication app;
    return app.Run(argc, argv);
}
