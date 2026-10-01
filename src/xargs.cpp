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
 * SINGLE FILE INDEX: xargs.cpp
 * ============================================================================
 * WinXargs - Object-Oriented Command Line Builder and Execution Dispatcher for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & DELIMITER CONFIG] .......... DelimitMode, XargsOptions classes
 * 2. [ARGUMENT TOKENIZER & ESCAPER] ........ CommandLineEscaper, InputTokenizer classes
 * 3. [BATCH PROCESS EXECUTION ENGINE] ...... ProcessBatchRunner, XargsEngine classes
 * 4. [APPLICATION CONTROLLER] .............. XargsApp class and main entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <memory>

// ============================================================================
// 1. OPTIONS & DELIMITER CONFIG
// ============================================================================

enum class DelimitMode {
    WHITESPACE,
    NEWLINE,
    NULL_CHAR,
    DELIMITER
};

class XargsOptions {
public:
    DelimitMode mode{DelimitMode::WHITESPACE};
    int maxArgs{-1};
    int maxLines{-1};
    char delimiter{'\0'};
    bool verbose{false};
    bool noRunIfEmpty{false};
    std::string replaceStr;
    std::vector<std::string> command;

    static void printUsage(const char* progName) {
        (void)progName;
        std::cout << R"(xargs(1)                  CrossShell for UNIX Reference Manual                xargs(1)

    NAME
        xargs - build and execute command lines from standard input

    SYNOPSIS
        xargs [OPTIONS] [COMMAND [INITIAL-ARGS]...]

    DESCRIPTION
        Build and execute command lines from standard input.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -0, --null
            Input items are terminated by a null character instead of by
            whitespace.

        -d, --delimiter CHARACTER
            Input items are terminated by the specified character.

        -n, --max-args MAX-ARGS
            Use at most MAX-ARGS arguments per command line.

        -L, --max-lines MAX-LINES
            Use at most MAX-LINES non-empty input lines per command line.

        -I REPLACE-STR
            Replace occurrences of REPLACE-STR in initial arguments with names
            read from standard input.

        -t, --verbose
            Print the command line on the standard error output before
            executing it.

        -r, --no-run-if-empty
            If the standard input does not contain any nonblanks, do not run
            the command.

        -h, --help, /?, -?
            Display this help and exit.

        -v, --version
            Output version information and exit.

    EXAMPLES
        xargs echo
            Read items from standard input and echo them as arguments.

        find . -name "*.txt" | xargs -n 1 type
            Display the contents of each file one at a time.

        dir /b | xargs -I {} cmd /c echo Processing {}
            Execute a command for each line replacing {} with the item.

    CrossShell for UNIX                                                   xargs(1)
)";
    }

    static void printVersion() {
        std::cout << "xargs (CrossShell) 1.0\n";
    }

    static bool parse(int argc, char* argv[], XargsOptions& opts) {
        int i = 1;
        for (; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--help" || arg == "-h" || arg == "/?" || arg == "-?") {
                printUsage(argv[0]);
                std::exit(0);
            } else if (arg == "--version" || arg == "-v" || arg == "-V") {
                printVersion();
                std::exit(0);
            } else if (arg == "-0" || arg == "--null") {
                opts.mode = DelimitMode::NULL_CHAR;
            } else if (arg == "-t" || arg == "--verbose") {
                opts.verbose = true;
            } else if (arg == "-r" || arg == "--no-run-if-empty") {
                opts.noRunIfEmpty = true;
            } else if ((arg == "-n" || arg == "--max-args") && i + 1 < argc) {
                opts.maxArgs = std::stoi(argv[++i]);
            } else if ((arg == "-L" || arg == "--max-lines") && i + 1 < argc) {
                opts.maxLines = std::stoi(argv[++i]);
                opts.mode = DelimitMode::NEWLINE;
            } else if ((arg == "-d" || arg == "--delimiter") && i + 1 < argc) {
                opts.delimiter = argv[++i][0];
                opts.mode = DelimitMode::DELIMITER;
            } else if (arg == "-I" && i + 1 < argc) {
                opts.replaceStr = argv[++i];
            } else if (arg == "--") {
                ++i;
                break;
            } else if (arg[0] == '-') {
                std::cerr << "xargs: unrecognized option '" << arg << "'\n";
                return false;
            } else {
                break;
            }
        }

        for (; i < argc; ++i) {
            opts.command.push_back(argv[i]);
        }

        if (opts.command.empty()) {
            opts.command.push_back("echo");
        }

        return true;
    }
};

// ============================================================================
// 2. ARGUMENT TOKENIZER & ESCAPER
// ============================================================================

class CommandLineEscaper {
public:
    static std::string escape(const std::string& arg) {
        if (arg.empty()) return "\"\"";
        if (arg.find_first_of(" \t\n\v\"") == std::string::npos) return arg;

        std::string escaped = "\"";
        for (size_t i = 0; i < arg.length(); ++i) {
            size_t backslashes = 0;
            while (i < arg.length() && arg[i] == '\\') {
                backslashes++;
                i++;
            }

            if (i == arg.length()) {
                escaped.append(backslashes * 2, '\\');
            } else if (arg[i] == '"') {
                escaped.append(backslashes * 2 + 1, '\\');
                escaped.push_back('"');
            } else {
                escaped.append(backslashes, '\\');
                escaped.push_back(arg[i]);
            }
        }
        escaped.push_back('"');
        return escaped;
    }

    static std::string replaceAll(std::string str, const std::string& from, const std::string& to) {
        if (from.empty()) return str;
        size_t startPos = 0;
        while ((startPos = str.find(from, startPos)) != std::string::npos) {
            str.replace(startPos, from.length(), to);
            startPos += to.length();
        }
        return str;
    }

    static std::string build(const std::vector<std::string>& args) {
        std::string cmd;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) cmd += " ";
            cmd += escape(args[i]);
        }
        return cmd;
    }
};

class InputTokenizer {
public:
    static std::vector<std::string> readAll(std::istream& in, const XargsOptions& opts) {
        std::vector<std::string> tokens;

        if (opts.mode == DelimitMode::NULL_CHAR || opts.mode == DelimitMode::DELIMITER) {
            char sep = (opts.mode == DelimitMode::NULL_CHAR) ? '\0' : opts.delimiter;
            std::string tok;
            while (std::getline(in, tok, sep)) {
                if (!tok.empty()) tokens.push_back(tok);
            }
        } else if (opts.mode == DelimitMode::NEWLINE) {
            std::string line;
            while (std::getline(in, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (!line.empty()) tokens.push_back(line);
            }
        } else { // WHITESPACE
            std::string tok;
            while (in >> tok) {
                tokens.push_back(tok);
            }
        }

        return tokens;
    }
};

// ============================================================================
// 3. BATCH PROCESS EXECUTION ENGINE
// ============================================================================

class ProcessBatchRunner {
public:
    static int runCommand(const std::vector<std::string>& cmdArgs, bool verbose) {
        std::string cmdLine = CommandLineEscaper::build(cmdArgs);

        if (verbose) {
            std::cerr << cmdLine << "\n";
        }

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};

        std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
        cmdBuf.push_back('\0');

        BOOL ok = CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
        if (!ok) {
            std::cerr << "xargs: " << cmdArgs[0] << ": cannot execute\n";
            return 127;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return static_cast<int>(exitCode);
    }
};

class XargsEngine {
private:
    XargsOptions options;

public:
    explicit XargsEngine(XargsOptions opts) : options(std::move(opts)) {}

    int execute() {
        std::vector<std::string> tokens = InputTokenizer::readAll(std::cin, options);

        if (tokens.empty()) {
            if (options.noRunIfEmpty) return 0;
            return ProcessBatchRunner::runCommand(options.command, options.verbose);
        }

        if (!options.replaceStr.empty()) {
            for (const auto& tok : tokens) {
                std::vector<std::string> replaced = options.command;
                for (auto& arg : replaced) {
                    arg = CommandLineEscaper::replaceAll(arg, options.replaceStr, tok);
                }
                int code = ProcessBatchRunner::runCommand(replaced, options.verbose);
                if (code != 0) return code;
            }
            return 0;
        }

        size_t batchSize = (options.maxArgs > 0) ? static_cast<size_t>(options.maxArgs)
                         : (options.maxLines > 0) ? static_cast<size_t>(options.maxLines)
                         : tokens.size();

        for (size_t i = 0; i < tokens.size(); i += batchSize) {
            std::vector<std::string> currentCmd = options.command;
            for (size_t j = i; j < (std::min)(tokens.size(), i + batchSize); ++j) {
                currentCmd.push_back(tokens[j]);
            }

            int code = ProcessBatchRunner::runCommand(currentCmd, options.verbose);
            if (code != 0) return code;
        }

        return 0;
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class XargsApp {
public:
    static int run(int argc, char* argv[]) {
        XargsOptions options;
        if (!XargsOptions::parse(argc, argv, options)) {
            return 1;
        }
        XargsEngine engine(std::move(options));
        return engine.execute();
    }
};

int main(int argc, char* argv[]) {
    return XargsApp::run(argc, argv);
}
