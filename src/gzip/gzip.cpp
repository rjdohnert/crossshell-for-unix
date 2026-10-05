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

/**
 * ============================================================================
 * SINGLE FILE INDEX: gzip.cpp
 * ============================================================================
 * WinGzip - Object-Oriented GZip Compression and Decompression Tool for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & CONFIGURATION] ............. GzipOptions class (CLI parsing & flags)
 * 2. [STRUCTURED OUTPUT REPORTER] .......... GzipReporter class (JSON/CSV/Table/Pipe)
 * 3. [COMPRESSION DISPATCH ENGINE] ......... PowerShellGzipBridge, GzipEngine classes
 * 4. [APPLICATION CONTROLLER] .............. GzipApp class and wmain entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <fcntl.h>
#include <iostream>
#include <io.h>
#include <string>
#include <vector>
#include <cstdio>
#include <memory>

// ============================================================================
// 1. OPTIONS & CONFIGURATION
// ============================================================================

class GzipOptions {
public:
    bool keep{false};
    bool force{false};
    bool decompress{false};
    std::vector<std::wstring> files;
    int outputFormat{0};
    std::wstring pipeCommand;

    static void printUsage(const wchar_t* /*prog*/) {
        std::wcout << LR"HELP(gzip(1)                 CrossShell for UNIX Reference Manual                  gzip(1)

    NAME
        gzip - Compress or decompress gzip files.

    SYNOPSIS
        gzip [OPTIONS] FILE...

    DESCRIPTION
        Compresses each FILE to FILE.gz, or decompresses gzip files when
        decompression mode is selected. Input files are removed after a
        successful operation unless --keep is specified. Compression is
        performed through the Windows PowerShell GZipStream implementation.

    OPTIONS
        -d, --decompress, --uncompress
            Decompress each input file. A .gz suffix is removed when present;
            otherwise the output file receives an .out suffix.

        -k, --keep
            Keep input files after successful compression or decompression.

        -f, --force
            Overwrite an existing output file.

        --json
            Format the completed-operation report as JSON.

        --csv
            Format the completed-operation report as CSV.

        --table
            Format the completed-operation report as a table.

        --pipe COMMAND
            Send the formatted operation report through COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version information.

    EXAMPLES
        gzip archive.tar
            Compress archive.tar to archive.tar.gz and remove the input.

        gzip -k report.txt
            Compress report.txt while preserving the input file.

        gzip -d archive.tar.gz
            Decompress archive.tar.gz to archive.tar.

        gzip --json -k report.txt
            Keep report.txt and report the compression operation as JSON.

    CrossShell for UNIX                                                        gzip(1)
    )HELP";
    }

    static void printVersion() {
        std::wcout << L"gzip 1.0.0\n";
    }

    static bool parse(int argc, wchar_t* argv[], GzipOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
                printUsage(argv[0]);
                std::exit(0);
            } else if (arg == L"-V" || arg == L"--version") {
                printVersion();
                std::exit(0);
            } else if (arg == L"-d" || arg == L"--decompress" || arg == L"--uncompress") {
                opts.decompress = true;
            } else if (arg == L"-k" || arg == L"--keep") {
                opts.keep = true;
            } else if (arg == L"-f" || arg == L"--force") {
                opts.force = true;
            } else if (arg == L"--json") {
                opts.outputFormat = 1;
            } else if (arg == L"--csv") {
                opts.outputFormat = 2;
            } else if (arg == L"--table") {
                opts.outputFormat = 3;
            } else if (arg == L"--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (arg[0] == L'-' && arg.size() > 1) {
                for (size_t j = 1; j < arg.size(); ++j) {
                    if (arg[j] == L'd') opts.decompress = true;
                    else if (arg[j] == L'k') opts.keep = true;
                    else if (arg[j] == L'f') opts.force = true;
                    else {
                        std::wcerr << L"gzip: unknown option -" << arg[j] << L"\n";
                        return false;
                    }
                }
            } else {
                opts.files.push_back(arg);
            }
        }

        if (opts.files.empty()) {
            std::wcerr << L"gzip: no files specified\n";
            return false;
        }

        return true;
    }
};

// ============================================================================
// 2. STRUCTURED OUTPUT REPORTER
// ============================================================================

class GzipReporter {
public:
    static std::string toUtf8(const std::wstring& text) {
        int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), NULL, 0, NULL, NULL);
        if (size <= 0) return {};
        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), &result[0], size, NULL, NULL);
        return result;
    }

    static int dispatch(const std::vector<std::pair<std::wstring, std::wstring>>& operations,
                        int format, const std::wstring& pipeCommand) {
        if (format == 0 && pipeCommand.empty()) return 0;

        std::wstring text;
        if (format == 1) {
            text = L"{\"operations\":[";
            for (size_t i = 0; i < operations.size(); ++i) {
                if (i > 0) text += L",";
                text += L"{\"source\":\"" + operations[i].first + L"\",\"target\":\"" + operations[i].second + L"\"}";
            }
            text += L"]}\n";
        } else if (format == 2) {
            text = L"source,target\n";
            for (const auto& op : operations) {
                text += L"\"" + op.first + L"\",\"" + op.second + L"\"\n";
            }
        } else if (format == 3) {
            text = L"SOURCE\tTARGET\n--------------------\n";
            for (const auto& op : operations) {
                text += op.first + L"\t" + op.second + L"\n";
            }
        }

        if (!pipeCommand.empty()) {
            FILE* pipe = _wpopen(pipeCommand.c_str(), L"w");
            if (!pipe) return 1;
            std::string utf8 = toUtf8(text);
            std::fwrite(utf8.data(), 1, utf8.size(), pipe);
            _pclose(pipe);
        } else {
            std::wcout << text;
        }
        return 0;
    }
};

// ============================================================================
// 3. COMPRESSION DISPATCH ENGINE
// ============================================================================

class PowerShellGzipBridge {
private:
    static std::wstring quoteArgument(const std::wstring& arg) {
        if (arg.empty()) return L"\"\"";
        if (arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) return arg;

        std::wstring quoted = L"\"";
        int backslashes = 0;
        for (wchar_t c : arg) {
            if (c == L'\\') {
                ++backslashes;
            } else if (c == L'\"') {
                quoted.append(backslashes * 2 + 1, L'\\');
                quoted.push_back(L'\"');
                backslashes = 0;
            } else {
                quoted.append(backslashes, L'\\');
                quoted.push_back(c);
                backslashes = 0;
            }
        }
        quoted.append(backslashes * 2, L'\\');
        quoted.push_back(L'\"');
        return quoted;
    }

    static std::wstring escapeSingleQuotes(const std::wstring& s) {
        std::wstring out;
        out.reserve(s.size());
        for (wchar_t c : s) {
            if (c == L'\'') out += L"''";
            else out.push_back(c);
        }
        return out;
    }

    static int runPowerShell(const std::wstring& script) {
        std::vector<std::wstring> args = {
            L"powershell", L"-NoProfile", L"-ExecutionPolicy", L"Bypass", L"-Command", script
        };

        std::wstring cmdline;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) cmdline.push_back(L' ');
            cmdline += quoteArgument(args[i]);
        }

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};

        std::vector<wchar_t> cmdBuf(cmdline.begin(), cmdline.end());
        cmdBuf.push_back(L'\0');

        BOOL ok = CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi);
        if (!ok) return 1;

        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return static_cast<int>(exitCode);
    }

public:
    static bool compressFile(const std::wstring& src, const std::wstring& dst) {
        std::wstring script =
            L"$in = [System.IO.File]::OpenRead('" + escapeSingleQuotes(src) + L"'); "
            L"$out = [System.IO.File]::Create('" + escapeSingleQuotes(dst) + L"'); "
            L"$gz = New-Object System.IO.Compression.GZipStream($out, [System.IO.Compression.CompressionMode]::Compress); "
            L"$in.CopyTo($gz); "
            L"$gz.Dispose(); $out.Dispose(); $in.Dispose();";
        return runPowerShell(script) == 0;
    }

    static bool decompressFile(const std::wstring& src, const std::wstring& dst) {
        std::wstring script =
            L"$in = [System.IO.File]::OpenRead('" + escapeSingleQuotes(src) + L"'); "
            L"$gz = New-Object System.IO.Compression.GZipStream($in, [System.IO.Compression.CompressionMode]::Decompress); "
            L"$out = [System.IO.File]::Create('" + escapeSingleQuotes(dst) + L"'); "
            L"$gz.CopyTo($out); "
            L"$out.Dispose(); $gz.Dispose(); $in.Dispose();";
        return runPowerShell(script) == 0;
    }
};

class GzipEngine {
private:
    GzipOptions options;

public:
    explicit GzipEngine(GzipOptions opts) : options(std::move(opts)) {}

    int execute() {
        bool allOk = true;
        std::vector<std::pair<std::wstring, std::wstring>> ops;

        for (const auto& file : options.files) {
            std::wstring src = file;
            DWORD srcAttributes = GetFileAttributesW(src.c_str());

            if (srcAttributes == INVALID_FILE_ATTRIBUTES || (srcAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                std::wcerr << L"gzip: " << file << L": No such file\n";
                allOk = false;
                continue;
            }

            if (options.decompress) {
                std::wstring dst;
                if (src.size() > 3 && src.rfind(L".gz") == src.size() - 3) {
                    dst = src.substr(0, src.size() - 3);
                } else {
                    dst = src + L".out";
                }

                if (GetFileAttributesW(dst.c_str()) != INVALID_FILE_ATTRIBUTES && !options.force) {
                    std::wcerr << L"gzip: " << dst << L" already exists; use -f to overwrite\n";
                    allOk = false;
                    continue;
                }

                if (PowerShellGzipBridge::decompressFile(src, dst)) {
                    if (!options.keep) DeleteFileW(src.c_str());
                    ops.emplace_back(src, dst);
                } else {
                    std::wcerr << L"gzip: error decompressing " << file << L"\n";
                    allOk = false;
                }
            } else {
                std::wstring dst = src + L".gz";
                if (GetFileAttributesW(dst.c_str()) != INVALID_FILE_ATTRIBUTES && !options.force) {
                    std::wcerr << L"gzip: " << dst << L" already exists; use -f to overwrite\n";
                    allOk = false;
                    continue;
                }

                if (PowerShellGzipBridge::compressFile(src, dst)) {
                    if (!options.keep) DeleteFileW(src.c_str());
                    ops.emplace_back(src, dst);
                } else {
                    std::wcerr << L"gzip: error compressing " << file << L"\n";
                    allOk = false;
                }
            }
        }

        GzipReporter::dispatch(ops, options.outputFormat, options.pipeCommand);
        return allOk ? 0 : 1;
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class GzipApp {
public:
    static int run(int argc, wchar_t* argv[]) {
        GzipOptions options;
        if (!GzipOptions::parse(argc, argv, options)) {
            return 1;
        }
        GzipEngine engine(std::move(options));
        return engine.execute();
    }
};

int wmain(int argc, wchar_t* argv[]) {
    return GzipApp::run(argc, argv);
}
