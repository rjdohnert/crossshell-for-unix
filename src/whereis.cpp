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
 * SINGLE FILE INDEX: whereis.cpp
 * ============================================================================
 * WinWhereis - Object-Oriented Binary, Manual & Source Searcher for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & SEARCH CONFIGURATION] ...... SearchPathConfig and WhereisOptions classes
 * 2. [STRUCTURED OUTPUT REPORTER] .......... WhereisReporter class (JSON/CSV/Table/Pipe)
 * 3. [LOCATION SEARCH ENGINE] .............. LocationSearcher and WhereisEngine classes
 * 4. [APPLICATION CONTROLLER] .............. WhereisApp class and wmain entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <set>
#include <sstream>
#include <algorithm>
#include <fcntl.h>
#include <io.h>
#include <cstdio>
#include <memory>

namespace fs = std::filesystem;

// ============================================================================
// 1. OPTIONS & SEARCH CONFIGURATION
// ============================================================================

class SearchPathConfig {
public:
    bool searchBin{true};
    bool searchMan{true};
    bool searchSrc{true};

    std::vector<std::wstring> binDirs;
    std::vector<std::wstring> manDirs;
    std::vector<std::wstring> srcDirs;

    std::vector<std::wstring> binExts;
    std::vector<std::wstring> manExts = { L"", L".chm", L".hlp", L".html", L".htm", L".pdf", L".txt", L".md", L".1" };
    std::vector<std::wstring> srcExts = { L"", L".cpp", L".c", L".h", L".hpp", L".cs", L".py", L".rs", L".go", L".java", L".js", L".ts" };

    static std::wstring getEnvVar(const wchar_t* varName) {
        DWORD size = GetEnvironmentVariableW(varName, NULL, 0);
        if (size == 0) return L"";
        std::vector<wchar_t> buffer(size);
        GetEnvironmentVariableW(varName, buffer.data(), size);
        return std::wstring(buffer.data());
    }

    static std::vector<std::wstring> split(const std::wstring& str, wchar_t delim) {
        std::vector<std::wstring> tokens;
        std::wstringstream ss(str);
        std::wstring item;
        while (std::getline(ss, item, delim)) {
            if (!item.empty()) tokens.push_back(item);
        }
        return tokens;
    }

    void initializeDefaults() {
        binDirs.push_back(L".");
        std::wstring pathEnv = getEnvVar(L"PATH");
        if (!pathEnv.empty()) {
            std::vector<std::wstring> paths = split(pathEnv, L';');
            binDirs.insert(binDirs.end(), paths.begin(), paths.end());
        }

        binExts.push_back(L"");
        std::wstring pathextEnv = getEnvVar(L"PATHEXT");
        if (!pathextEnv.empty()) {
            std::vector<std::wstring> exts = split(pathextEnv, L';');
            for (auto& ext : exts) {
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                binExts.push_back(ext);
            }
        } else {
            binExts.insert(binExts.end(), { L".exe", L".cmd", L".bat", L".com", L".ps1" });
        }

        manDirs = { L".", L"doc", L"docs", L"man", L"help" };
        srcDirs = { L".", L"src", L"source", L"sources" };
    }
};

class WhereisOptions {
public:
    SearchPathConfig config;
    std::vector<std::wstring> targets;
    int outputFormat{0};
    std::wstring pipeCommand;

    static void printUsage(const wchar_t* progName) {
        std::wcout << L"Usage: " << progName << L" [-bms] [-u] [-BMS directory... -f] name...\n\n"
                   << L"Locate the binary, source, and manual-page files for a command.\n\n"
                   << L"Options:\n"
                   << L"  -b          Search only for binaries\n"
                   << L"  -m          Search only for manual/documentation sections\n"
                   << L"  -s          Search only for sources\n"
                   << L"  -u          Search only for unusual entries (entries without all requested parts)\n"
                   << L"  -B <dir>    Set/change search directory list for binaries\n"
                   << L"  -M <dir>    Set/change search directory list for manuals\n"
                   << L"  -S <dir>    Set/change search directory list for sources\n"
                   << L"  -f          Terminates directory lists and begins file list\n"
                   << L"      --json, --csv, --table  structured output\n"
                   << L"      --pipe CMD  send output through CMD\n"
                   << L"  -h, --help  Display this help message and exit\n";
    }

    static bool parse(int argc, wchar_t* argv[], WhereisOptions& opts) {
        opts.config.initializeDefaults();
        enum TargetList { NONE, BIN, MAN, SRC } currentDirList = NONE;

        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];

            if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
                printUsage(argv[0]);
                std::exit(0);
            }
            if (arg == L"--json") { opts.outputFormat = 1; continue; }
            if (arg == L"--csv") { opts.outputFormat = 2; continue; }
            if (arg == L"--table") { opts.outputFormat = 3; continue; }
            if (arg == L"--pipe" && i + 1 < argc) { opts.pipeCommand = argv[++i]; continue; }

            if (arg == L"-b") { opts.config.searchBin = true; opts.config.searchMan = false; opts.config.searchSrc = false; currentDirList = NONE; }
            else if (arg == L"-m") { opts.config.searchBin = false; opts.config.searchMan = true; opts.config.searchSrc = false; currentDirList = NONE; }
            else if (arg == L"-s") { opts.config.searchBin = false; opts.config.searchMan = false; opts.config.searchSrc = true; currentDirList = NONE; }
            else if (arg == L"-B") { opts.config.binDirs.clear(); currentDirList = BIN; }
            else if (arg == L"-M") { opts.config.manDirs.clear(); currentDirList = MAN; }
            else if (arg == L"-S") { opts.config.srcDirs.clear(); currentDirList = SRC; }
            else if (arg == L"-f") { currentDirList = NONE; }
            else {
                if (currentDirList == BIN) opts.config.binDirs.push_back(arg);
                else if (currentDirList == MAN) opts.config.manDirs.push_back(arg);
                else if (currentDirList == SRC) opts.config.srcDirs.push_back(arg);
                else opts.targets.push_back(arg);
            }
        }

        if (opts.targets.empty()) {
            printUsage(argv[0]);
            return false;
        }

        return true;
    }
};

// ============================================================================
// 2. STRUCTURED OUTPUT REPORTER
// ============================================================================

struct TargetResult {
    std::wstring target;
    std::vector<std::wstring> bins;
    std::vector<std::wstring> mans;
    std::vector<std::wstring> srcs;
};

class WhereisReporter {
public:
    static std::string toUtf8(const std::wstring& text) {
        int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), NULL, 0, NULL, NULL);
        if (size <= 0) return {};
        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, NULL, NULL);
        return result;
    }

    static int dispatch(const std::vector<TargetResult>& results, int format, const std::wstring& pipeCommand) {
        std::wstring text;
        if (format == 1) {
            text = L"{\"whereis\":[";
            for (size_t i = 0; i < results.size(); ++i) {
                if (i > 0) text += L",";
                text += L"{\"target\":\"" + results[i].target + L"\"";
                text += L",\"bin\":[";
                for (size_t b = 0; b < results[i].bins.size(); ++b) {
                    if (b > 0) text += L",";
                    text += L"\"" + results[i].bins[b] + L"\"";
                }
                text += L"]}";
            }
            text += L"]}\n";
        } else if (format == 2) {
            text = L"target,path\n";
            for (const auto& r : results) {
                for (const auto& b : r.bins) text += L"\"" + r.target + L"\",\"" + b + L"\"\n";
                for (const auto& m : r.mans) text += L"\"" + r.target + L"\",\"" + m + L"\"\n";
                for (const auto& s : r.srcs) text += L"\"" + r.target + L"\",\"" + s + L"\"\n";
            }
        } else if (format == 3) {
            text = L"TARGET\tPATH\n--------------------\n";
            for (const auto& r : results) {
                for (const auto& b : r.bins) text += r.target + L"\t" + b + L"\n";
                for (const auto& m : r.mans) text += r.target + L"\t" + m + L"\n";
                for (const auto& s : r.srcs) text += r.target + L"\t" + s + L"\n";
            }
        } else {
            for (const auto& r : results) {
                text += r.target + L":";
                for (const auto& b : r.bins) text += L" " + b;
                for (const auto& m : r.mans) text += L" " + m;
                for (const auto& s : r.srcs) text += L" " + s;
                text += L"\n";
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
// 3. LOCATION SEARCH ENGINE
// ============================================================================

class LocationSearcher {
public:
    static std::vector<std::wstring> searchCategory(const std::wstring& target,
                                                    const std::vector<std::wstring>& dirs,
                                                    const std::vector<std::wstring>& exts) {
        std::vector<std::wstring> matches;
        std::set<std::wstring> visited;

        for (const auto& dir : dirs) {
            if (dir.empty()) continue;
            for (const auto& ext : exts) {
                fs::path candidate = fs::path(dir) / (target + ext);
                std::error_code ec;
                if (fs::exists(candidate, ec) && fs::is_regular_file(candidate, ec)) {
                    std::wstring fullPath = fs::absolute(candidate, ec).wstring();
                    std::wstring lowerKey = fullPath;
                    std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
                    if (visited.find(lowerKey) == visited.end()) {
                        visited.insert(lowerKey);
                        matches.push_back(fullPath);
                    }
                }
            }
        }
        return matches;
    }
};

class WhereisEngine {
private:
    WhereisOptions options;

public:
    explicit WhereisEngine(WhereisOptions opts) : options(std::move(opts)) {}

    int execute() {
        std::vector<TargetResult> results;

        for (const auto& target : options.targets) {
            TargetResult r;
            r.target = target;

            if (options.config.searchBin) {
                r.bins = LocationSearcher::searchCategory(target, options.config.binDirs, options.config.binExts);
            }
            if (options.config.searchMan) {
                r.mans = LocationSearcher::searchCategory(target, options.config.manDirs, options.config.manExts);
            }
            if (options.config.searchSrc) {
                r.srcs = LocationSearcher::searchCategory(target, options.config.srcDirs, options.config.srcExts);
            }

            results.push_back(r);
        }

        return WhereisReporter::dispatch(results, options.outputFormat, options.pipeCommand);
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class WhereisApp {
public:
    static int run(int argc, wchar_t* argv[]) {
        WhereisOptions options;
        if (!WhereisOptions::parse(argc, argv, options)) {
            return 1;
        }
        WhereisEngine engine(std::move(options));
        return engine.execute();
    }
};

int wmain(int argc, wchar_t* argv[]) {
    return WhereisApp::run(argc, argv);
}
