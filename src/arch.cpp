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
 * SINGLE FILE INDEX: arch.cpp
 * ============================================================================
 * WinArch - Object-Oriented Processor & Machine Architecture Query Utility
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & CONFIGURATION] ............. ArchOptions class (CLI parsing & flags)
 * 2. [STRUCTURED OUTPUT REPORTER] .......... ArchReporter class (JSON/CSV/Table/Pipe)
 * 3. [SYSTEM ARCHITECTURE ENGINE] .......... SystemArchResolver, ArchEngine classes
 * 4. [APPLICATION CONTROLLER] .............. ArchApp class and main entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>
#include <string>
#include <algorithm>
#include <cstdio>
#include <memory>

// ============================================================================
// 1. OPTIONS & CONFIGURATION
// ============================================================================

class ArchOptions {
public:
    bool asMachine{false};
    bool optKernel{false};
    bool optShort{false};
    bool optApp{false};
    int outputFormat{0};
    std::string pipeCommand;

    static bool isRunningAsMachine(std::string argv0) {
        size_t lastSlash = argv0.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            argv0 = argv0.substr(lastSlash + 1);
        }
        size_t lastDot = argv0.find_last_of('.');
        if (lastDot != std::string::npos) {
            argv0 = argv0.substr(0, lastDot);
        }
        std::transform(argv0.begin(), argv0.end(), argv0.begin(), ::tolower);
        return argv0 == "machine";
    }

    static void printHelpArch() {
        std::cout << R"(arch(1)                 CrossShell for UNIX Reference Manual                  arch(1)

    NAME
        arch - print machine hardware and application architecture

    SYNOPSIS
        arch [OPTIONS]

    DESCRIPTION
        Display the architecture name of the current machine and host operating
        environment.

    OPTIONS
        -k, --kernel
            Display the kernel architecture instead of the application architecture.

        -s, --short
            Accepted for compatibility; output is already short.

        --json, --csv, --table
            Output architecture details in JSON, CSV, or tabular format.

        --pipe COMMAND
            Pipe output directly to COMMAND.

        -h, --help
            Display this reference manual.

        --version
            Output version information and exit.

    EXAMPLES
        arch
            Display standard architecture name.

        arch -k --json
            Display kernel architecture in JSON format.

    CrossShell for UNIX                                                    arch(1)
)";
    }

    static void printHelpMachine() {
        std::cout << R"(machine(1)              CrossShell for UNIX Reference Manual               machine(1)

    NAME
        machine - print machine hardware and kernel architecture

    SYNOPSIS
        machine [OPTIONS]

    DESCRIPTION
        Display the system's machine hardware and kernel architecture.

    OPTIONS
        -a, --app
            Display application architecture instead of kernel architecture.

        --json, --csv, --table
            Output architecture details in JSON, CSV, or tabular format.

        --pipe COMMAND
            Pipe output directly to COMMAND.

        -h, --help
            Display this reference manual.

        --version
            Output version information and exit.

    EXAMPLES
        machine
            Display host kernel architecture.

        machine -a
            Display user application architecture.

    CrossShell for UNIX                                                 machine(1)
)";
    }

    static void printVersion() {
        std::cout << "arch 1.0.0\n";
    }

    static bool parse(int argc, char* argv[], ArchOptions& opts) {
        if (argc > 0 && argv[0] != nullptr) {
            opts.asMachine = isRunningAsMachine(argv[0]);
        }

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--version") {
                printVersion();
                std::exit(0);
            }
            if (arg == "--json") { opts.outputFormat = 1; continue; }
            if (arg == "--csv") { opts.outputFormat = 2; continue; }
            if (arg == "--table") { opts.outputFormat = 3; continue; }
            if (arg == "--pipe" && i + 1 < argc) { opts.pipeCommand = argv[++i]; continue; }

            if (arg == "-h" || arg == "--help" || arg == "/?") {
                if (opts.asMachine) printHelpMachine();
                else printHelpArch();
                std::exit(0);
            }

            if (arg == "--") {
                break;
            }

            if (arg.rfind("--", 0) == 0) {
                if (opts.asMachine && arg == "--app") {
                    opts.optApp = true;
                    continue;
                }
                if (!opts.asMachine) {
                    if (arg == "--kernel") { opts.optKernel = true; continue; }
                    if (arg == "--short") { opts.optShort = true; continue; }
                }

                std::cerr << (opts.asMachine ? "machine" : "arch") << ": unknown option -- " << arg << "\n";
                return false;
            }

            if (arg.front() == '-' && arg.length() > 1) {
                for (size_t j = 1; j < arg.length(); ++j) {
                    char c = arg[j];
                    if (opts.asMachine) {
                        if (c == 'a') opts.optApp = true;
                        else {
                            std::cerr << "machine: unknown option -- " << c << "\n";
                            return false;
                        }
                    } else {
                        if (c == 'k') opts.optKernel = true;
                        else if (c == 's') opts.optShort = true;
                        else {
                            std::cerr << "arch: unknown option -- " << c << "\n";
                            return false;
                        }
                    }
                }
            } else {
                std::cerr << (opts.asMachine ? "machine" : "arch") << ": extra operand '" << arg << "'\n";
                return false;
            }
        }

        return true;
    }
};

// ============================================================================
// 2. STRUCTURED OUTPUT REPORTER
// ============================================================================

class ArchReporter {
public:
    static int dispatch(const std::string& archStr, int format, const std::string& pipeCommand) {
        std::string text;
        if (format == 1) {
            text = "{\"architecture\":\"" + archStr + "\"}\n";
        } else if (format == 2) {
            text = "architecture\n" + archStr + "\n";
        } else if (format == 3) {
            text = "ARCHITECTURE\n------------\n" + archStr + "\n";
        } else {
            text = archStr + "\n";
        }

        if (!pipeCommand.empty()) {
            FILE* p = _popen(pipeCommand.c_str(), "w");
            if (!p) return 1;
            std::fwrite(text.data(), 1, text.size(), p);
            _pclose(p);
        } else {
            std::cout << text;
        }
        return 0;
    }
};

// ============================================================================
// 3. SYSTEM ARCHITECTURE ENGINE
// ============================================================================

class SystemArchResolver {
public:
    static std::string getArchString(WORD archVal) {
        switch (archVal) {
            case PROCESSOR_ARCHITECTURE_AMD64:
                return "amd64";
            case PROCESSOR_ARCHITECTURE_INTEL:
                return "i386";
            case PROCESSOR_ARCHITECTURE_ARM64:
                return "arm64";
            case PROCESSOR_ARCHITECTURE_ARM:
                return "arm";
            case PROCESSOR_ARCHITECTURE_IA64:
                return "ia64";
            default:
                return "unknown";
        }
    }
};

class ArchEngine {
private:
    ArchOptions options;

public:
    explicit ArchEngine(ArchOptions opts) : options(std::move(opts)) {}

    int execute() {
        SYSTEM_INFO sysInfo;
        SYSTEM_INFO nativeSysInfo;
        GetSystemInfo(&sysInfo);
        GetNativeSystemInfo(&nativeSysInfo);

        std::string archStr;
        if (options.asMachine) {
            WORD targetArch = nativeSysInfo.wProcessorArchitecture;
            if (options.optApp) {
                targetArch = sysInfo.wProcessorArchitecture;
            }
            archStr = SystemArchResolver::getArchString(targetArch);
        } else {
            WORD targetArch = sysInfo.wProcessorArchitecture;
            if (options.optKernel) {
                targetArch = nativeSysInfo.wProcessorArchitecture;
            }
            archStr = SystemArchResolver::getArchString(targetArch);
        }

        return ArchReporter::dispatch(archStr, options.outputFormat, options.pipeCommand);
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class ArchApp {
public:
    static int run(int argc, char* argv[]) {
        ArchOptions options;
        if (!ArchOptions::parse(argc, argv, options)) {
            if (options.asMachine) ArchOptions::printHelpMachine();
            else ArchOptions::printHelpArch();
            return 1;
        }
        ArchEngine engine(std::move(options));
        return engine.execute();
    }
};

int main(int argc, char* argv[]) {
    return ArchApp::run(argc, argv);
}
