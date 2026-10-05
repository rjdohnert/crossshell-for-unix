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
 * SINGLE FILE INDEX: pr.cpp
 * ============================================================================
 * WinPr - Object-Oriented File Pagination and Header Formatter for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & CONFIGURATION] ............. PrOptions class (CLI parsing & flags)
 * 2. [STRUCTURED OUTPUT REPORTER] .......... PrReporter class (JSON/CSV/Table/Pipe)
 * 3. [PAGINATION ENGINE] ................... PaginationEngine and PrEngine classes
 * 4. [APPLICATION CONTROLLER] .............. PrApp class and wmain entry point
 * ============================================================================
 */

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <memory>

// ============================================================================
// 1. OPTIONS & CONFIGURATION
// ============================================================================

class PrOptions {
public:
    int pageLength{66};
    int width{72};
    bool omitHeader{false};
    std::wstring header;
    int outputFormat{0};
    std::wstring pipeCommand;
    std::vector<std::wstring> files;

    static void printHelp() {
        std::wcout << LR"(pr(1)                      CrossShell for UNIX Reference Manual                     pr(1)

    NAME
        pr - paginate or columnate files for printing

    SYNOPSIS
        pr [OPTIONS] [FILE...]

    DESCRIPTION
        pr formats text files into paginated output with headers and page
        numbers for printing or viewing. If no FILE is specified or if FILE
        is '-', pr reads from standard input.

    OPTIONS
        -l <length>
            Set the page length to <length> lines (default: 66).

        -w <width>
            Set the page width to <width> columns (default: 72).

        -h <header>
            Use <header> in place of the filename in page headers.

        -t
            Omit page headers and trailers (suppress 5-line header/trailer).

        --json
            Output paginated data as JSON.

        --csv
            Output paginated data as CSV.

        --table
            Output paginated data formatted as an ASCII table.

        --pipe <command>
            Pipe paginated output through the specified shell command.

        --help
            Display this reference manual and exit.

        --version
            Display version information and exit.

    EXAMPLES
        pr file.txt
            Paginate file.txt with default 66-line pages and headers.

        pr -l 50 -h "Monthly Report" report.txt
            Format report.txt with 50-line pages and a custom header title.

        pr -t document.txt
            Print document.txt suppressing top and bottom headers.

    CrossShell for UNIX                                                          pr(1)
)";
    }

    static void printVersion() {
        std::wcout << L"pr 1.0.0\n";
    }

    static bool parse(int argc, wchar_t* argv[], PrOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::wstring a = argv[i];
            if (a == L"--help" || a == L"-help" || a == L"/?") {
                printHelp();
                std::exit(0);
            } else if (a == L"--version") {
                printVersion();
                std::exit(0);
            } else if (a == L"-t") {
                opts.omitHeader = true;
            } else if (a == L"-l" && i + 1 < argc) {
                opts.pageLength = _wtoi(argv[++i]);
            } else if (a == L"-w" && i + 1 < argc) {
                opts.width = _wtoi(argv[++i]);
            } else if (a == L"-h" && i + 1 < argc) {
                opts.header = argv[++i];
            } else if (a == L"--json") {
                opts.outputFormat = 1;
            } else if (a == L"--csv") {
                opts.outputFormat = 2;
            } else if (a == L"--table") {
                opts.outputFormat = 3;
            } else if (a == L"--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (a[0] == L'-' && a.size() > 1) {
                std::wcerr << L"pr: unknown option " << a << L"\n";
                return false;
            } else {
                opts.files.push_back(a);
            }
        }

        if (opts.files.empty()) {
            opts.files.push_back(L"-");
        }

        return true;
    }
};

// ============================================================================
// 2. STRUCTURED OUTPUT REPORTER
// ============================================================================

class PrReporter {
public:
    static std::string toUtf8(const std::wstring& text) {
        int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), NULL, 0, NULL, NULL);
        if (size <= 0) return {};
        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, NULL, NULL);
        return result;
    }

    static int dispatch(const std::wstring& content, int format, const std::wstring& pipeCommand) {
        std::wstring text;
        if (format == 1) {
            text = L"{\"paginated\":\"" + content + L"\"}\n";
        } else if (format == 2) {
            text = L"paginated\n\"" + content + L"\"\n";
        } else if (format == 3) {
            text = L"PAGINATED\n---------\n" + content + L"\n";
        } else {
            text = content;
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
// 3. PAGINATION ENGINE
// ============================================================================

class PaginationEngine {
public:
    static std::wstring getTimestampString() {
        std::time_t t = std::time(nullptr);
        std::tm tmv{};
        localtime_s(&tmv, &t);
        std::wstringstream ss;
        ss << std::put_time(&tmv, L"%b %d %H:%M %Y");
        return ss.str();
    }

    static void paginateStream(std::wistream& in, const std::wstring& title, const PrOptions& opt, std::wostream& out) {
        std::vector<std::wstring> lines;
        for (std::wstring l; std::getline(in, l); ) {
            if (static_cast<int>(l.size()) > opt.width) {
                l = l.substr(0, opt.width);
            }
            lines.push_back(l);
        }

        int headerLines = opt.omitHeader ? 0 : 5;
        int contentLines = (std::max)(1, opt.pageLength - headerLines);
        int page = 1;

        for (size_t i = 0; i < lines.size(); i += contentLines, ++page) {
            if (!opt.omitHeader) {
                std::wstring docTitle = opt.header.empty() ? title : opt.header;
                out << L"\n" << getTimestampString() << L"  " << docTitle << L"  Page " << page << L"\n\n";
            }
            for (size_t j = i; j < (std::min)(lines.size(), i + static_cast<size_t>(contentLines)); ++j) {
                out << lines[j] << L"\n";
            }
            if (!opt.omitHeader) {
                out << L"\n\n\n";
            }
        }
    }
};

class PrEngine {
private:
    PrOptions options;

public:
    explicit PrEngine(PrOptions opts) : options(std::move(opts)) {}

    int execute() {
        std::wostringstream captured;
        std::wostream* outStream = (options.outputFormat || !options.pipeCommand.empty()) ? &captured : &std::wcout;

        for (const auto& f : options.files) {
            if (f == L"-") {
                PaginationEngine::paginateStream(std::wcin, L"standard input", options, *outStream);
            } else {
                std::wifstream in(f);
                if (!in) {
                    std::wcerr << L"pr: cannot open " << f << L"\n";
                    continue;
                }
                PaginationEngine::paginateStream(in, f, options, *outStream);
            }
        }

        if (options.outputFormat || !options.pipeCommand.empty()) {
            PrReporter::dispatch(captured.str(), options.outputFormat, options.pipeCommand);
        }

        return 0;
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class PrApp {
public:
    static int run(int argc, wchar_t* argv[]) {
        PrOptions options;
        if (!PrOptions::parse(argc, argv, options)) {
            return 1;
        }
        PrEngine engine(std::move(options));
        return engine.execute();
    }
};

int wmain(int argc, wchar_t* argv[]) {
    return PrApp::run(argc, argv);
}
