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
 * SINGLE FILE INDEX: tsort.cpp
 * ============================================================================
 * WinTsort - Object-Oriented Topological Sorter for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & CONFIGURATION] ............. TsortOptions class (CLI parsing & flags)
 * 2. [STRUCTURED OUTPUT REPORTER] .......... TsortReporter class (JSON/CSV/Table/Pipe)
 * 3. [GRAPH & TOPOLOGICAL SORT ENGINE] ..... GraphNodeManager, TsortEngine classes
 * 4. [APPLICATION CONTROLLER] .............. TsortApp class and main entry point
 * ============================================================================
 */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <unordered_set>
#include <queue>
#include <cstdio>
#include <memory>

#ifdef _WIN32
#include <io.h>
#endif

// ============================================================================
// 1. OPTIONS & CONFIGURATION
// ============================================================================

class TsortOptions {
public:
    bool multiple{false};
    std::vector<std::string> filenames;
    int outputFormat{0};
    std::string pipeCommand;

    static void printUsage() {
        std::cout << R"(tsort(1)                CrossShell for UNIX Reference Manual                 tsort(1)

    NAME
        tsort - perform topological sort

    SYNOPSIS
        tsort [OPTIONS] [FILE]

    DESCRIPTION
        Write totally ordered list consistent with the partial ordering in
        FILE. With no FILE, or when FILE is -, read standard input.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -m, --multiple
            Process multiple input files in order.

        --json, --csv
            Output ordered nodes as structured JSON or CSV records.

        --table
            Output ordered nodes as a formatted table.

        --pipe COMMAND
            Send formatted output through pipeline COMMAND.

        -h, --help
            Display this reference manual and exit.

        -v, --version
            Display version information and exit.

    EXAMPLES
        tsort pairs.txt
            Topologically sort dependency pairs from pairs.txt.

    CrossShell for UNIX                                                      tsort(1)
)";
    }

    static void printVersion() {
        std::cout << "tsort 1.0\n";
    }

    static bool parse(int argc, char* argv[], TsortOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-h" || arg == "--help" || arg == "/?" || arg == "-?") {
                printUsage();
                std::exit(0);
            } else if (arg == "--version" || arg == "-v") {
                printVersion();
                std::exit(0);
            } else if (arg == "-m" || arg == "--multiple") {
                opts.multiple = true;
            } else if (arg == "--json") {
                opts.outputFormat = 1;
            } else if (arg == "--csv") {
                opts.outputFormat = 2;
            } else if (arg == "--table") {
                opts.outputFormat = 3;
            } else if (arg == "--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (arg[0] == '-' && arg != "-") {
                std::cerr << "tsort: unrecognized option '" << arg << "'\n";
                return false;
            } else {
                opts.filenames.push_back(arg);
            }
        }

        if (!opts.multiple && opts.filenames.size() > 1) {
            std::cerr << "tsort: too many arguments\n";
            return false;
        }

        if (opts.filenames.empty()) {
            opts.filenames.push_back("-");
        }

        return true;
    }
};

// ============================================================================
// 2. STRUCTURED OUTPUT REPORTER
// ============================================================================

class TsortReporter {
public:
    static int dispatch(const std::vector<std::string>& nodes, int format, const std::string& pipeCommand) {
        std::string text;
        if (format == 1) {
            text = "[\n";
            for (size_t i = 0; i < nodes.size(); ++i) {
                text += (i ? ",\n" : "") + std::string("  {\"node\":\"") + nodes[i] + "\"}";
            }
            text += "\n]\n";
        } else if (format == 2) {
            text = "node\n";
            for (const auto& n : nodes) {
                text += "\"" + n + "\"\n";
            }
        } else if (format == 3) {
            text = "NODE\n----\n";
            for (const auto& n : nodes) {
                text += n + "\n";
            }
        } else {
            for (const auto& n : nodes) {
                text += n + "\n";
            }
        }

        if (!pipeCommand.empty()) {
            FILE* pipe = _popen(pipeCommand.c_str(), "w");
            if (!pipe) return 1;
            std::fwrite(text.data(), 1, text.size(), pipe);
            _pclose(pipe);
        } else {
            std::cout << text;
        }
        return 0;
    }
};

// ============================================================================
// 3. GRAPH & TOPOLOGICAL SORT ENGINE
// ============================================================================

class GraphNodeManager {
private:
    std::map<std::string, int> nameToId;
    std::vector<std::string> idToName;

public:
    int getOrCreateId(const std::string& name) {
        auto it = nameToId.find(name);
        if (it != nameToId.end()) {
            return it->second;
        }
        int newId = static_cast<int>(idToName.size());
        nameToId[name] = newId;
        idToName.push_back(name);
        return newId;
    }

    size_t size() const { return idToName.size(); }
    const std::string& getName(int id) const { return idToName[id]; }
};

class TsortEngine {
private:
    TsortOptions options;

public:
    explicit TsortEngine(TsortOptions opts) : options(std::move(opts)) {}

    int execute() {
        std::vector<std::string> allTokens;

        for (const auto& fname : options.filenames) {
            if (fname == "-") {
                std::string tok;
                while (std::cin >> tok) allTokens.push_back(tok);
            } else {
                std::ifstream f(fname);
                if (!f.is_open()) {
                    std::cerr << "tsort: cannot open " << fname << "\n";
                    return 1;
                }
                std::string tok;
                while (f >> tok) allTokens.push_back(tok);
            }
        }

        if (allTokens.size() % 2 != 0) {
            std::cerr << "tsort: odd number of tokens: input contains an incomplete pair\n";
            return 1;
        }

        GraphNodeManager nodes;
        std::vector<std::unordered_set<int>> adj;
        std::vector<int> inDegree;

        for (size_t i = 0; i < allTokens.size(); i += 2) {
            int u = nodes.getOrCreateId(allTokens[i]);
            int v = nodes.getOrCreateId(allTokens[i + 1]);

            while (adj.size() <= static_cast<size_t>((std::max)(u, v))) {
                adj.emplace_back();
                inDegree.push_back(0);
            }

            if (u != v) {
                if (adj[u].find(v) == adj[u].end()) {
                    adj[u].insert(v);
                    inDegree[v]++;
                }
            }
        }

        std::queue<int> q;
        for (size_t i = 0; i < nodes.size(); ++i) {
            if (i < inDegree.size() && inDegree[i] == 0) {
                q.push(static_cast<int>(i));
            }
        }

        std::vector<std::string> ordered;
        while (!q.empty()) {
            int u = q.front();
            q.pop();
            ordered.push_back(nodes.getName(u));

            if (static_cast<size_t>(u) < adj.size()) {
                for (int v : adj[u]) {
                    inDegree[v]--;
                    if (inDegree[v] == 0) {
                        q.push(v);
                    }
                }
            }
        }

        if (ordered.size() < nodes.size()) {
            std::cerr << "tsort: input contains a loop\n";
        }

        return TsortReporter::dispatch(ordered, options.outputFormat, options.pipeCommand);
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class TsortApp {
public:
    static int run(int argc, char* argv[]) {
        TsortOptions options;
        if (!TsortOptions::parse(argc, argv, options)) {
            return 1;
        }
        TsortEngine engine(std::move(options));
        return engine.execute();
    }
};

int main(int argc, char* argv[]) {
    return TsortApp::run(argc, argv);
}
