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
 * SINGLE FILE INDEX: join.cpp
 * ============================================================================
 * WinJoin - Object-Oriented Relational Field Join Filter for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & CONFIGURATION] ............. JoinOptions class (CLI parsing & flags)
 * 2. [STRUCTURED OUTPUT REPORTER] .......... JoinReporter class (JSON/CSV/Table/Pipe)
 * 3. [FIELD SPLITTER & JOIN ENGINE] ........ FieldExtractor and JoinEngine classes
 * 4. [APPLICATION CONTROLLER] .............. JoinApp class and wmain entry point
 * ============================================================================
 */

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdio>
#include <memory>

// ============================================================================
// 1. OPTIONS & CONFIGURATION
// ============================================================================

class JoinOptions {
public:
    int field1{1};
    int field2{1};
    wchar_t delim{L'\0'};
    bool includeUnpaired1{false};
    bool includeUnpaired2{false};
    bool unpairedOnly1{false};
    bool unpairedOnly2{false};
    std::wstring file1;
    std::wstring file2;
    int outputFormat{0};
    std::wstring pipeCommand;

    static void printHelp() {
        std::wcout << LR"(join(1)             CrossShell for UNIX Reference Manual                 join(1)

    NAME
        join - join lines of two files on a common field

    SYNOPSIS
        join [OPTIONS] FILE1 FILE2

    DESCRIPTION
        join pairs and merges lines from two sorted input files based on identical
        data in a specified join field. If FILE1 or FILE2 is '-', standard input
        is read.

    OPTIONS
        -1 FIELD
            Join on the specified 1-based FIELD of FILE1.

        -2 FIELD
            Join on the specified 1-based FIELD of FILE2.

        -j FIELD
            Equivalent to -1 FIELD -2 FIELD.

        -t CHAR
            Use CHAR as input and output field separator.

        -a FILENO
            Print unpairable lines from file FILENO (1 or 2).

        -v FILENO
            Like -a FILENO, but suppress successfully joined output lines.

        --json
            Output joined records formatted as JSON.

        --csv
            Output joined records formatted as CSV.

        --table
            Output joined records formatted as a table.

        --pipe COMMAND
            Send output through the specified pipe command.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        join file1.txt file2.txt
            Join lines on first whitespace-delimited field.

        join -t, -1 1 -2 2 customers.csv orders.csv
            Join CSV files matching field 1 in file 1 with field 2 in file 2.

        join -a 1 employees.txt departments.txt
            Include unpairable records from employees.txt.

    CrossShell for UNIX                                                    join(1)
)";
    }

    static void printVersion() {
        std::wcout << L"join 1.0.0\n";
    }

    static bool parse(int argc, wchar_t* argv[], JoinOptions& opts) {
        std::vector<std::wstring> files;

        for (int i = 1; i < argc; ++i) {
            std::wstring a = argv[i];
            if (a == L"--help" || a == L"-h" || a == L"-help" || a == L"/?") {
                printHelp();
                std::exit(0);
            } else if (a == L"--version" || a == L"-V") {
                printVersion();
                std::exit(0);
            } else if (a == L"-1" && i + 1 < argc) {
                opts.field1 = _wtoi(argv[++i]);
            } else if (a == L"-2" && i + 1 < argc) {
                opts.field2 = _wtoi(argv[++i]);
            } else if (a == L"-j" && i + 1 < argc) {
                opts.field1 = opts.field2 = _wtoi(argv[++i]);
            } else if (a == L"-t" && i + 1 < argc) {
                std::wstring t = argv[++i];
                if (!t.empty()) opts.delim = t[0];
            } else if (a == L"-a" && i + 1 < argc) {
                int n = _wtoi(argv[++i]);
                if (n == 1) opts.includeUnpaired1 = true;
                if (n == 2) opts.includeUnpaired2 = true;
            } else if (a == L"-v" && i + 1 < argc) {
                int n = _wtoi(argv[++i]);
                if (n == 1) opts.unpairedOnly1 = true;
                if (n == 2) opts.unpairedOnly2 = true;
            } else if (a == L"--json") {
                opts.outputFormat = 1;
            } else if (a == L"--csv") {
                opts.outputFormat = 2;
            } else if (a == L"--table") {
                opts.outputFormat = 3;
            } else if (a == L"--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (a[0] == L'-' && a.size() > 1) {
                std::wcerr << L"join: unknown option " << a << L"\n";
                return false;
            } else {
                files.push_back(a);
            }
        }

        if (files.size() != 2) {
            std::wcerr << L"join: requires exactly two files\n";
            return false;
        }

        opts.file1 = files[0];
        opts.file2 = files[1];
        return true;
    }
};

// ============================================================================
// 2. STRUCTURED OUTPUT REPORTER
// ============================================================================

class JoinReporter {
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
            text = L"{\"joined\":\"" + content + L"\"}\n";
        } else if (format == 2) {
            text = L"joined\n\"" + content + L"\"\n";
        } else if (format == 3) {
            text = L"JOINED\n------\n" + content + L"\n";
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
// 3. FIELD SPLITTER & JOIN ENGINE
// ============================================================================

class FieldExtractor {
public:
    static std::vector<std::wstring> splitFields(const std::wstring& line, wchar_t delim) {
        std::vector<std::wstring> out;
        if (delim == L'\0') {
            std::wstringstream ss(line);
            std::wstring tok;
            while (ss >> tok) out.push_back(tok);
            return out;
        }

        std::wstring cur;
        for (wchar_t ch : line) {
            if (ch == delim) {
                out.push_back(cur);
                cur.clear();
            } else {
                cur.push_back(ch);
            }
        }
        out.push_back(cur);
        return out;
    }

    static std::wstring joinLine(const std::vector<std::wstring>& f1, int k1,
                                const std::vector<std::wstring>& f2, int k2, wchar_t delim) {
        wchar_t d = (delim == L'\0') ? L' ' : delim;
        std::wstring out;
        auto append = [&](const std::wstring& s) {
            if (!out.empty()) out.push_back(d);
            out += s;
        };

        append((k1 >= 0 && k1 < static_cast<int>(f1.size())) ? f1[k1] : L"");
        for (int i = 0; i < static_cast<int>(f1.size()); ++i) {
            if (i != k1) append(f1[i]);
        }
        for (int i = 0; i < static_cast<int>(f2.size()); ++i) {
            if (i != k2) append(f2[i]);
        }
        return out;
    }
};

class JoinEngine {
private:
    JoinOptions options;

    static std::vector<std::wstring> readLines(const std::wstring& path) {
        std::vector<std::wstring> out;
        if (path == L"-") {
            for (std::wstring l; std::getline(std::wcin, l); ) out.push_back(l);
        } else {
            std::wifstream in(path);
            for (std::wstring l; std::getline(in, l); ) out.push_back(l);
        }
        return out;
    }

public:
    explicit JoinEngine(JoinOptions opts) : options(std::move(opts)) {}

    int execute() {
        std::vector<std::wstring> l1 = readLines(options.file1);
        std::vector<std::wstring> l2 = readLines(options.file2);

        int k1 = options.field1 - 1;
        int k2 = options.field2 - 1;

        std::vector<std::vector<std::wstring>> f1, f2;
        for (const auto& s : l1) f1.push_back(FieldExtractor::splitFields(s, options.delim));
        for (const auto& s : l2) f2.push_back(FieldExtractor::splitFields(s, options.delim));

        std::vector<bool> used1(f1.size(), false);
        std::vector<bool> used2(f2.size(), false);

        std::wostringstream captured;
        std::wostream* outStream = (options.outputFormat || !options.pipeCommand.empty()) ? &captured : &std::wcout;

        bool suppressJoined = (options.unpairedOnly1 || options.unpairedOnly2);

        for (size_t i = 0; i < f1.size(); ++i) {
            std::wstring key1 = (k1 >= 0 && k1 < static_cast<int>(f1[i].size())) ? f1[i][k1] : L"";
            bool matched = false;

            for (size_t j = 0; j < f2.size(); ++j) {
                std::wstring key2 = (k2 >= 0 && k2 < static_cast<int>(f2[j].size())) ? f2[j][k2] : L"";
                if (!key1.empty() && key1 == key2) {
                    matched = true;
                    used1[i] = true;
                    used2[j] = true;
                    if (!suppressJoined) {
                        *outStream << FieldExtractor::joinLine(f1[i], k1, f2[j], k2, options.delim) << L"\n";
                    }
                }
            }
        }

        if (options.includeUnpaired1 || options.unpairedOnly1) {
            for (size_t i = 0; i < f1.size(); ++i) {
                if (!used1[i]) *outStream << l1[i] << L"\n";
            }
        }

        if (options.includeUnpaired2 || options.unpairedOnly2) {
            for (size_t j = 0; j < f2.size(); ++j) {
                if (!used2[j]) *outStream << l2[j] << L"\n";
            }
        }

        if (options.outputFormat || !options.pipeCommand.empty()) {
            JoinReporter::dispatch(captured.str(), options.outputFormat, options.pipeCommand);
        }

        return 0;
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class JoinApp {
public:
    static int run(int argc, wchar_t* argv[]) {
        JoinOptions options;
        if (!JoinOptions::parse(argc, argv, options)) {
            return 1;
        }
        JoinEngine engine(std::move(options));
        return engine.execute();
    }
};

int wmain(int argc, wchar_t* argv[]) {
    return JoinApp::run(argc, argv);
}
