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
 * SINGLE FILE INDEX: nproc.cpp
 * ============================================================================
 * WinNproc - Object-Oriented Available Processing Unit Counter for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & CONFIGURATION] ............. NprocOptions class (CLI parsing & flags)
 * 2. [PROCESSOR COUNT RESOLVER ENGINE] ..... ProcessorCountResolver, NprocEngine classes
 * 3. [APPLICATION CONTROLLER] .............. NprocApp class and main entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>
#include <string>
#include <cstdlib>
#include <memory>

// ============================================================================
// 1. OPTIONS & CONFIGURATION
// ============================================================================

class NprocOptions {
public:
    bool includeAll{false};
    unsigned long long ignore{0};

    static void printUsage(const char* programName) {
        std::cout
            << "Usage: " << programName << " [options]\n"
            << "Print the number of available processing units.\n\n"
            << "Options:\n"
            << "  --all                 print all installed processing units\n"
            << "  --ignore=N            subtract N units from the result\n"
            << "  -h, --help            display this help and exit\n"
            << "      --version         output version information and exit\n";
    }

    static void printVersion() {
        std::cout << "nproc 1.0.0\n";
    }

    static bool parseIgnoreValue(const std::string& text, unsigned long long& outValue) {
        if (text.empty()) return false;
        char* end = nullptr;
        unsigned long long value = std::strtoull(text.c_str(), &end, 10);
        if (end == text.c_str() || *end != '\0') {
            return false;
        }
        outValue = value;
        return true;
    }

    static bool parse(int argc, char* argv[], NprocOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i] ? argv[i] : "";

            if (arg == "-h" || arg == "--help" || arg == "/?") {
                printUsage((argc > 0 && argv[0]) ? argv[0] : "nproc");
                std::exit(0);
            }
            if (arg == "--version") {
                printVersion();
                std::exit(0);
            }
            if (arg == "--all") {
                opts.includeAll = true;
                continue;
            }
            if (arg.rfind("--ignore=", 0) == 0) {
                if (!parseIgnoreValue(arg.substr(9), opts.ignore)) {
                    std::cerr << "nproc: invalid --ignore value\n";
                    return false;
                }
                continue;
            }

            std::cerr << "nproc: unknown option -- " << arg << "\n";
            return false;
        }
        return true;
    }
};

// ============================================================================
// 2. PROCESSOR COUNT RESOLVER ENGINE
// ============================================================================

class ProcessorCountResolver {
public:
    static DWORD calculateCount(const NprocOptions& opts) {
        DWORD logicalCount = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
        if (logicalCount == 0) logicalCount = 1;

        DWORD result = logicalCount;
        if (opts.ignore >= logicalCount) {
            result = 1;
        } else {
            result = logicalCount - static_cast<DWORD>(opts.ignore);
        }

        if (opts.includeAll) {
            SYSTEM_INFO si = {};
            GetSystemInfo(&si);
            DWORD allCount = si.dwNumberOfProcessors ? si.dwNumberOfProcessors : logicalCount;
            if (opts.ignore >= allCount) {
                result = 1;
            } else {
                result = allCount - static_cast<DWORD>(opts.ignore);
            }
        }

        return result;
    }
};

class NprocEngine {
private:
    NprocOptions options;

public:
    explicit NprocEngine(NprocOptions opts) : options(std::move(opts)) {}

    int execute() {
        DWORD count = ProcessorCountResolver::calculateCount(options);
        std::cout << count << "\n";
        return 0;
    }
};

// ============================================================================
// 3. APPLICATION CONTROLLER
// ============================================================================

class NprocApp {
public:
    static int run(int argc, char* argv[]) {
        NprocOptions options;
        if (!NprocOptions::parse(argc, argv, options)) {
            return 1;
        }
        NprocEngine engine(std::move(options));
        return engine.execute();
    }
};

int main(int argc, char* argv[]) {
    return NprocApp::run(argc, argv);
}
