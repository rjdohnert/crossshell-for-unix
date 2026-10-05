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
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <cstdio>
#include <streambuf>

#pragma comment(lib, "shell32.lib")

namespace fs = std::filesystem;

// ============================================================================
// 1. STREAMING PIPE BUFFER & OUTPUT FORMATTER
// ============================================================================

enum class OutputFormat { Human, Json, Csv, Table };

class WidePipeBuffer : public std::wstreambuf {
private:
    FILE* m_file{nullptr};
    wchar_t m_buffer[2048];

public:
    explicit WidePipeBuffer(FILE* file) : m_file(file) {
        setp(m_buffer, m_buffer + 2048);
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
        if (count && std::fwrite(pbase(), sizeof(wchar_t), static_cast<size_t>(count), m_file) != static_cast<size_t>(count)) {
            return -1;
        }
        setp(m_buffer, m_buffer + 2048);
        return std::fflush(m_file) == 0 ? 0 : -1;
    }
};

class OutputFormatter {
public:
    static std::wstring CsvQuote(const std::wstring& value) {
        std::wstring out = L"\"";
        for (wchar_t ch : value) {
            out += (ch == L'"' ? L"\"\"" : std::wstring(1, ch));
        }
        return out + L"\"";
    }

    static std::wstring JsonQuote(const std::wstring& value) {
        std::wstring out = L"\"";
        for (wchar_t ch : value) {
            if (ch == L'"' || ch == L'\\') out += L'\\';
            if (ch == L'\n') out += L'n';
            else if (ch == L'\r') out += L'r';
            else out += ch;
        }
        return out + L"\"";
    }

    static void EmitHeader(OutputFormat fmt) {
        if (fmt == OutputFormat::Csv) std::wcout << L"\"status\",\"path\"\n";
        if (fmt == OutputFormat::Table) std::wcout << L"STATUS\tPATH\n";
    }

    static void EmitItem(OutputFormat fmt, const std::wstring& path, bool success, bool verbose, bool quiet) {
        if (success) {
            if (verbose) {
                std::wcout << L"DONE\n";
            } else if (!quiet && fmt == OutputFormat::Human) {
                std::wcout << L"Recycled: " << path << L"\n";
            }
            if (fmt == OutputFormat::Json) {
                std::wcout << L"{\"status\":\"success\",\"path\":" << JsonQuote(path) << L"}\n";
            } else if (fmt == OutputFormat::Csv) {
                std::wcout << L"\"success\"," << CsvQuote(path) << L"\n";
            } else if (fmt == OutputFormat::Table) {
                std::wcout << L"success\t" << path << L"\n";
            }
        } else {
            if (verbose) {
                std::wcout << L"FAILED\n";
            }
        }
    }

    static void EmitSummary(int successCount, int failureCount, bool quiet, OutputFormat fmt, size_t targetCount, bool verbose) {
        if (!quiet && fmt == OutputFormat::Human && (targetCount > 1 || verbose)) {
            std::wcout << L"\nSummary: " << successCount << L" succeeded, "
                       << failureCount << L" failed.\n";
        }
    }
};

// ============================================================================
// 2. CONSOLE PROMPTER & RECYCLE ENGINE
// ============================================================================

class ConsolePrompter {
public:
    static bool PromptUser(const fs::path& path) {
        std::wcout << L"Recycle '" << path.wstring() << L"'? (y/n): ";
        std::wstring response;
        std::getline(std::wcin, response);
        return (!response.empty() && (response[0] == L'y' || response[0] == L'Y'));
    }
};

class RecycleEngine {
public:
    static bool RecycleItem(const fs::path& targetPath, bool quiet) {
        std::error_code ec;
        fs::path absPath = fs::absolute(targetPath, ec);
        if (ec) {
            if (!quiet) {
                std::wcerr << L"[ERROR] Invalid path: " << targetPath.wstring() << L"\n";
            }
            return false;
        }

        if (!fs::exists(absPath, ec)) {
            if (!quiet) {
                std::wcerr << L"[ERROR] Not found: " << absPath.wstring() << L"\n";
            }
            return false;
        }

        std::wstring pathBuffer = absPath.wstring();
        pathBuffer.push_back(L'\0');

        SHFILEOPSTRUCTW fileOp = { nullptr };
        fileOp.wFunc = FO_DELETE;
        fileOp.pFrom = pathBuffer.c_str();
        fileOp.pTo = nullptr;
        fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;

        int result = SHFileOperationW(&fileOp);

        if (result != 0 || fileOp.fAnyOperationsAborted) {
            if (!quiet) {
                std::wcerr << L"[ERROR] Failed to recycle: " << absPath.wstring()
                           << L" (Code: " << result << L")\n";
            }
            return false;
        }

        return true;
    }
};

// ============================================================================
// 3. OPTION PARSER & APPLICATION CONTROLLER
// ============================================================================

struct RecycleOptions {
    bool verbose = false;
    bool interactive = false;
    bool quiet = false;
    bool showHelp = false;
    bool showVersion = false;
    OutputFormat outputFormat = OutputFormat::Human;
    std::wstring pipeCommand;
    std::vector<std::wstring> targets;
};

class OptionParser {
public:
    static void PrintHelp() {
        std::wcout << LR"(recycle(1)                CrossShell for UNIX Reference Manual                recycle(1)

    NAME
        recycle - send files and directories to the Windows Recycle Bin

    SYNOPSIS
        recycle [OPTIONS] PATH...

    DESCRIPTION
        recycle safely moves target files and directories into the Windows
        Recycle Bin instead of permanently deleting them.

    OPTIONS
        -i, --interactive
            Prompt for confirmation before recycling each target.

        -v, --verbose
            Display detailed progress for each recycled item.

        -q, --quiet
            Suppress normal output; only display critical errors.

        --json
            Output results formatted as JSON.

        --csv
            Output results formatted as CSV.

        --table
            Output results formatted as an ASCII table.

        --pipe <command>
            Pipe formatted output through <command>.

        -h, --help
            Display this reference manual and exit.

        --version
            Display version information and exit.

    EXAMPLES
        recycle document.docx
            Send document.docx to the Recycle Bin.

        recycle -i file1.txt file2.txt
            Prompt for confirmation before recycling each file.

        recycle -v "C:\Temp\OldProject"
            Recycle a directory with verbose progress logging.

    CrossShell for UNIX                                                          recycle(1)
)";
    }

    static void PrintVersion() {
        std::wcout << L"recycle version 3.0.1\n";
        std::wcout << L"Copyright (C) 2026, Roberto J Dohnert.\n";
    }

    RecycleOptions Parse(int argc, wchar_t* argv[]) const {
        RecycleOptions opts;
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];

            if (arg == L"-h" || arg == L"--help" || arg == L"/?" || arg == L"-?") {
                opts.showHelp = true;
            } else if (arg == L"--version") {
                opts.showVersion = true;
            } else if (arg == L"-v" || arg == L"--verbose") {
                opts.verbose = true;
            } else if (arg == L"-q" || arg == L"--quiet") {
                opts.quiet = true;
            } else if (arg == L"-i" || arg == L"--interactive") {
                opts.interactive = true;
            } else if (arg == L"--json" || arg == L"--csv" || arg == L"--table") {
                opts.outputFormat = arg == L"--json" ? OutputFormat::Json : (arg == L"--csv" ? OutputFormat::Csv : OutputFormat::Table);
            } else if (arg == L"--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (!arg.empty() && arg[0] == L'-') {
                std::wcerr << L"[ERROR] Unknown flag: " << arg << L"\n";
                opts.showHelp = true;
            } else {
                opts.targets.push_back(arg);
            }
        }
        return opts;
    }
};

class RecycleApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]) {
        SetConsoleOutputCP(CP_UTF8);

        RecycleOptions opts = m_parser.Parse(argc, argv);

        FILE* outputPipe = nullptr;
        WidePipeBuffer* pipeBuffer = nullptr;
        std::wstreambuf* oldOutput = nullptr;

        if (!opts.pipeCommand.empty()) {
            outputPipe = _wpopen(opts.pipeCommand.c_str(), L"w");
            if (!outputPipe) {
                std::wcerr << L"recycle: failed to start pipe command\n";
                return 2;
            }
            oldOutput = std::wcout.rdbuf();
            pipeBuffer = new WidePipeBuffer(outputPipe);
            std::wcout.rdbuf(pipeBuffer);
        }

        auto cleanupPipe = [&]() {
            if (pipeBuffer) {
                std::wcout.rdbuf(oldOutput);
                delete pipeBuffer;
                pipeBuffer = nullptr;
                _pclose(outputPipe);
                outputPipe = nullptr;
            }
        };

        if (opts.showHelp) {
            cleanupPipe();
            OptionParser::PrintHelp();
            return 0;
        }

        if (opts.showVersion) {
            cleanupPipe();
            OptionParser::PrintVersion();
            return 0;
        }

        if (opts.targets.empty()) {
            cleanupPipe();
            std::wcerr << L"recycle: error: missing operand\n";
            std::wcerr << L"Try 'recycle --help' for more information.\n";
            return 2;
        }

        OutputFormatter::EmitHeader(opts.outputFormat);

        int successCount = 0;
        int failureCount = 0;

        for (const auto& target : opts.targets) {
            fs::path p(target);

            if (opts.interactive) {
                if (!ConsolePrompter::PromptUser(p)) {
                    if (opts.verbose) {
                        std::wcout << L"[SKIPPED] " << p.wstring() << L"\n";
                    }
                    continue;
                }
            }

            if (opts.verbose) {
                std::wcout << L"[RECYCLING] " << fs::absolute(p).wstring() << L" ... ";
            }

            bool ok = RecycleEngine::RecycleItem(p, opts.quiet);
            if (ok) {
                successCount++;
            } else {
                failureCount++;
            }

            OutputFormatter::EmitItem(opts.outputFormat, p.wstring(), ok, opts.verbose, opts.quiet);
        }

        OutputFormatter::EmitSummary(successCount, failureCount, opts.quiet, opts.outputFormat, opts.targets.size(), opts.verbose);

        int exitCode = (failureCount == 0) ? 0 : 1;
        cleanupPipe();
        return exitCode;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    RecycleApplication app;
    return app.Run(argc, argv);
}