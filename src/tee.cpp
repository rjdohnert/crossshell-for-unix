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
#include <fstream>
#include <cwchar>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

namespace fs = std::filesystem;

// ============================================================================
// 1. CONFIGURATION & OUTPUT FORMATTER
// ============================================================================

struct TeeOptions {
    bool append = false;
    bool ignore_interrupts = false;
    int output_format = 0;
    std::wstring pipe_command;
    std::vector<std::wstring> file_paths;
};

class OutputFormatter {
public:
    static void Emit(int format, FILE* outputPipe, const std::string& data) {
        if (format == 0 && !outputPipe) return;

        std::string text;
        if (format == 1) {
            text = "{\"data\":\"" + data + "\"}\n";
        } else if (format == 2) {
            text = "\"data\"\n\"" + data + "\"\n";
        } else {
            text = "DATA\n----\n" + data;
        }

        if (outputPipe) {
            fwrite(text.data(), 1, text.size(), outputPipe);
        } else {
            std::cout.write(text.data(), static_cast<std::streamsize>(text.size()));
        }
    }
};

// ============================================================================
// 2. MULTIPLEXING TEE ENGINE
// ============================================================================

class TeeEngine {
public:
    static bool Process(const TeeOptions& opts) {
        FILE* outputPipe = nullptr;
        if (!opts.pipe_command.empty()) {
            outputPipe = _wpopen(opts.pipe_command.c_str(), L"w");
        }
        std::string structuredInput;

        if (opts.ignore_interrupts) {
#ifdef _WIN32
            SetConsoleCtrlHandler(nullptr, TRUE);
#endif
        }

        std::vector<std::ofstream> streams;
        std::ios_base::openmode mode = std::ios::out | std::ios::binary;
        if (opts.append) {
            mode |= std::ios::app;
        }

        bool had_error = false;
        for (const auto& path_str : opts.file_paths) {
            fs::path p(path_str);
            std::ofstream stream(p, mode);
            if (!stream) {
                std::wcerr << L"tee: " << path_str << L": Failed to open file\n";
                had_error = true;
            } else {
                streams.push_back(std::move(stream));
            }
        }

        char buffer[16384];
        while (std::cin) {
            std::cin.read(buffer, sizeof(buffer));
            std::streamsize bytes_read = std::cin.gcount();
            if (bytes_read > 0) {
                if (opts.output_format == 0 && !outputPipe && !std::cout.write(buffer, bytes_read)) {
                    had_error = true;
                }
                if (opts.output_format != 0 || outputPipe) {
                    structuredInput.append(buffer, static_cast<size_t>(bytes_read));
                }
                std::cout.flush();

                for (auto& fs_stream : streams) {
                    if (fs_stream.is_open()) {
                        if (!fs_stream.write(buffer, bytes_read)) {
                            had_error = true;
                        }
                        fs_stream.flush();
                    }
                }
            }
        }

        OutputFormatter::Emit(opts.output_format, outputPipe, structuredInput);

        if (outputPipe) {
            _pclose(outputPipe);
        }

        return !had_error;
    }
};

// ============================================================================
// 3. OPTION PARSER & APPLICATION CONTROLLER
// ============================================================================

class OptionParser {
public:
    static void PrintHelp() {
        std::wcout << LR"(tee(1)                  CrossShell for UNIX Reference Manual                  tee(1)

    NAME
        tee - duplicate standard input to files and standard output

    SYNOPSIS
        tee [OPTIONS] [FILE]...

    DESCRIPTION
        Copy standard input to each FILE, and also to standard output.
        Supports continuous binary stream multiplexing across multiple file targets
        and console pipes.

    OPTIONS
        -a, --append
            Append to the given files, do not overwrite.

        -i, --ignore-interrupts
            Ignore interrupt signals (SIGINT / Ctrl+C).

        --json
            Emit input capture telemetry in JSON format.

        --csv
            Emit input capture telemetry in CSV format.

        --table
            Emit input capture telemetry in tabular format.

        --pipe COMMAND
            Stream captured output into COMMAND.

        -h, --help
            Display this reference manual.

        --version
            Display version and license information.

    EXAMPLES
        ls -la | tee output.txt
            Display directory listing and write it to output.txt.

        make 2>&1 | tee -a build.log
            Append build output to build.log while viewing in console.

        cat data.csv | tee file1.csv file2.csv file3.csv
            Duplicate stream across multiple destination files simultaneously.

    CrossShell for UNIX                                                     tee(1)
    )";
    }

    static void PrintVersion() {
        std::wcout << L"tee\n";
    }

    bool Parse(int argc, wchar_t* argv[], TeeOptions& opts, bool& exitEarly) const {
        exitEarly = false;
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"--help" || arg == L"-h" || arg == L"-?" || arg == L"/?") {
                PrintHelp();
                exitEarly = true;
                return true;
            }
            if (arg == L"--version") {
                PrintVersion();
                exitEarly = true;
                return true;
            }
            if (arg == L"--append") {
                opts.append = true;
                continue;
            }
            if (arg == L"--ignore-interrupts") {
                opts.ignore_interrupts = true;
                continue;
            }
            if (arg == L"--json") { opts.output_format = 1; continue; }
            if (arg == L"--csv") { opts.output_format = 2; continue; }
            if (arg == L"--table") { opts.output_format = 3; continue; }
            if (arg == L"--pipe" && i + 1 < argc) { opts.pipe_command = argv[++i]; continue; }
            if (arg[0] == L'-' && arg.size() > 1) {
                for (size_t j = 1; j < arg.size(); ++j) {
                    switch (arg[j]) {
                        case L'a':
                            opts.append = true;
                            break;
                        case L'i':
                            opts.ignore_interrupts = true;
                            break;
                        default:
                            std::wcerr << L"tee: unknown option -- " << arg[j] << std::endl;
                            std::wcerr << L"usage: tee [-ai] [file ...]\n";
                            return false;
                    }
                }
            } else {
                opts.file_paths.push_back(arg);
            }
        }
        return true;
    }
};

class TeeApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]) const {
        std::ios_base::sync_with_stdio(false);
        std::cin.tie(nullptr);

        TeeOptions opts;
        bool exitEarly = false;
        if (!m_parser.Parse(argc, argv, opts, exitEarly)) {
            return 1;
        }
        if (exitEarly) {
            return 0;
        }

#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
#endif

        return TeeEngine::Process(opts) ? 0 : 1;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    TeeApplication app;
    return app.Run(argc, argv);
}
