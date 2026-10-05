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
 * SINGLE FILE INDEX: whence.cpp
 * ============================================================================
 * WinWhence - Object-Oriented Command Classification & Path Resolver for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & CONFIGURATION] ............. WhenceOptions class (CLI parsing & flags)
 * 2. [STRUCTURED OUTPUT REPORTER] .......... WhenceReporter class (JSON/CSV/Table/Pipe)
 * 3. [COMMAND CLASSIFIER ENGINE] ........... CommandClassifier, WhenceEngine classes
 * 4. [APPLICATION CONTROLLER] .............. WhenceApp class and wmain entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <unordered_set>
#include <algorithm>
#include <filesystem>
#include <cstdlib>
#include <cctype>
#include <sstream>
#include <io.h>
#include <fcntl.h>
#include <cstdio>
#include <memory>

namespace fs = std::filesystem;

// ============================================================================
// 1. OPTIONS & CONFIGURATION
// ============================================================================

class WhenceOptions {
public:
    bool verbose{false};
    bool showAll{false};
    bool pathSearchOnly{false};
    std::vector<std::wstring> targets;
    int outputFormat{0};
    std::wstring pipeCommand;

    static void printHelp(const std::wstring& progName) {
        std::wcout << L"Usage: " << progName << L" [-v] [-a] [-p] name ...\n\n"
                   << L"Locate a command, resolve its path, and identify its type.\n\n"
                   << L"Options:\n"
                   << L"  -v, --verbose   produce verbose classification output\n"
                   << L"  -a, --all       display all occurrences in PATH\n"
                   << L"  -p, --path      force PATH lookup (ignore builtins)\n"
                   << L"      --json      output classification records as JSON\n"
                   << L"      --csv       output classification records as CSV\n"
                   << L"      --table     output classification records as a table\n"
                   << L"      --pipe CMD  send output through CMD\n"
                   << L"  -h, --help      display this help message and exit\n";
    }

    static bool parse(int argc, wchar_t* argv[], WhenceOptions& opts) {
        bool stopFlags = false;

        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];

            if (stopFlags) {
                opts.targets.push_back(arg);
                continue;
            }

            if (arg == L"--") {
                stopFlags = true;
                continue;
            }

            if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
                printHelp(argv[0]);
                std::exit(0);
            } else if (arg == L"--json") {
                opts.outputFormat = 1;
            } else if (arg == L"--csv") {
                opts.outputFormat = 2;
            } else if (arg == L"--table") {
                opts.outputFormat = 3;
            } else if (arg == L"--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (arg == L"-v" || arg == L"--verbose") {
                opts.verbose = true;
            } else if (arg == L"-a" || arg == L"--all") {
                opts.showAll = true;
            } else if (arg == L"-p" || arg == L"--path") {
                opts.pathSearchOnly = true;
            } else if (arg[0] == L'-' && arg.size() > 1) {
                for (size_t j = 1; j < arg.size(); ++j) {
                    if (arg[j] == L'v') opts.verbose = true;
                    else if (arg[j] == L'a') opts.showAll = true;
                    else if (arg[j] == L'p') opts.pathSearchOnly = true;
                    else {
                        std::wcerr << L"whence: unknown option -- " << arg[j] << L"\n";
                        return false;
                    }
                }
            } else {
                opts.targets.push_back(arg);
            }
        }

        if (opts.targets.empty()) {
            printHelp(argv[0]);
            return false;
        }

        return true;
    }
};

// ============================================================================
// 2. STRUCTURED OUTPUT REPORTER
// ============================================================================

struct WhenceResult {
    std::wstring target;
    std::wstring type;
    std::wstring path;
};

class WhenceReporter {
public:
    static std::string toUtf8(const std::wstring& text) {
        int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), NULL, 0, NULL, NULL);
        if (size <= 0) return {};
        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, NULL, NULL);
        return result;
    }

    static int dispatch(const std::vector<WhenceResult>& results, int format, bool verbose, const std::wstring& pipeCommand) {
        std::wstring text;
        if (format == 1) {
            text = L"{\"whence\":[";
            for (size_t i = 0; i < results.size(); ++i) {
                if (i > 0) text += L",";
                text += L"{\"target\":\"" + results[i].target + L"\",\"type\":\"" + results[i].type + L"\",\"path\":\"" + results[i].path + L"\"}";
            }
            text += L"]}\n";
        } else if (format == 2) {
            text = L"target,type,path\n";
            for (const auto& r : results) {
                text += L"\"" + r.target + L"\",\"" + r.type + L"\",\"" + r.path + L"\"\n";
            }
        } else if (format == 3) {
            text = L"TARGET\tTYPE\tPATH\n------------------------------------\n";
            for (const auto& r : results) {
                text += r.target + L"\t" + r.type + L"\t" + r.path + L"\n";
            }
        } else {
            for (const auto& r : results) {
                if (verbose) {
                    if (r.type == L"builtin") text += r.target + L" is a shell builtin\n";
                    else if (r.type == L"not_found") text += r.target + L" not found\n";
                    else text += r.target + L" is " + r.path + L"\n";
                } else {
                    if (r.type == L"builtin") text += r.target + L"\n";
                    else if (r.type != L"not_found") text += r.path + L"\n";
                }
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
// 3. COMMAND CLASSIFIER ENGINE
// ============================================================================

class CommandClassifier {
private:
    static inline const std::set<std::wstring> CMD_BUILTINS = {
        L"assoc", L"break", L"call", L"cd", L"chdir", L"cls", L"color", L"copy",
        L"date", L"del", L"dir", L"echo", L"endlocal", L"erase", L"exit", L"for",
        L"ftype", L"goto", L"if", L"md", L"mkdir", L"mklink", L"move", L"path",
        L"pause", L"prompt", L"rd", L"ren", L"rename", L"rmdir", L"set", L"setlocal",
        L"shift", L"start", L"time", L"title", L"type", L"ver", L"verify", L"vol"
    };

    static std::wstring toLower(std::wstring s) {
        std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
        return s;
    }

    static std::vector<std::wstring> split(const std::wstring& str, wchar_t delimiter) {
        std::vector<std::wstring> tokens;
        std::wstring token;
        std::wistringstream ss(str);
        while (std::getline(ss, token, delimiter)) {
            if (!token.empty()) tokens.push_back(token);
        }
        return tokens;
    }

public:
    static bool isBuiltin(const std::wstring& name) {
        std::wstring lower = toLower(name);
        return CMD_BUILTINS.find(lower) != CMD_BUILTINS.end();
    }

    static std::vector<std::wstring> findInPath(const std::wstring& name, bool showAll) {
        std::vector<std::wstring> matches;
        std::set<std::wstring> visited;

        DWORD pathLen = GetEnvironmentVariableW(L"PATH", nullptr, 0);
        if (pathLen == 0) return matches;
        std::wstring pathEnv(pathLen, L'\0');
        GetEnvironmentVariableW(L"PATH", &pathEnv[0], pathLen);

        std::vector<std::wstring> dirs = split(pathEnv, L';');
        dirs.insert(dirs.begin(), L".");

        DWORD extLen = GetEnvironmentVariableW(L"PATHEXT", nullptr, 0);
        std::wstring extEnv = L".COM;.EXE;.BAT;.CMD";
        if (extLen > 0) {
            extEnv.resize(extLen);
            GetEnvironmentVariableW(L"PATHEXT", &extEnv[0], extLen);
        }
        std::vector<std::wstring> exts = split(extEnv, L';');
        exts.insert(exts.begin(), L"");

        for (const auto& dir : dirs) {
            if (dir.empty()) continue;
            for (const auto& ext : exts) {
                fs::path candidate = fs::path(dir) / (name + ext);
                std::error_code ec;
                if (fs::exists(candidate, ec) && fs::is_regular_file(candidate, ec)) {
                    std::wstring full = fs::absolute(candidate, ec).wstring();
                    std::wstring lowerFull = toLower(full);
                    if (visited.find(lowerFull) == visited.end()) {
                        visited.insert(lowerFull);
                        matches.push_back(full);
                        if (!showAll) return matches;
                    }
                }
            }
        }
        return matches;
    }
};

class WhenceEngine {
private:
    WhenceOptions options;

public:
    explicit WhenceEngine(WhenceOptions opts) : options(std::move(opts)) {}

    int execute() {
        std::vector<WhenceResult> results;
        bool allFound = true;

        for (const auto& target : options.targets) {
            bool found = false;

            if (!options.pathSearchOnly && CommandClassifier::isBuiltin(target)) {
                results.push_back({ target, L"builtin", L"" });
                found = true;
                if (!options.showAll) continue;
            }

            std::vector<std::wstring> paths = CommandClassifier::findInPath(target, options.showAll);
            for (const auto& p : paths) {
                results.push_back({ target, L"executable", p });
                found = true;
            }

            if (!found) {
                results.push_back({ target, L"not_found", L"" });
                allFound = false;
            }
        }

        WhenceReporter::dispatch(results, options.outputFormat, options.verbose, options.pipeCommand);
        return allFound ? 0 : 1;
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class WhenceApp {
public:
    static int run(int argc, wchar_t* argv[]) {
        WhenceOptions options;
        if (!WhenceOptions::parse(argc, argv, options)) {
            return 1;
        }
        WhenceEngine engine(std::move(options));
        return engine.execute();
    }
};

int wmain(int argc, wchar_t* argv[]) {
    return WhenceApp::run(argc, argv);
}