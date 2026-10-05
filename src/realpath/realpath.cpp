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
 * SINGLE FILE INDEX: realpath.cpp
 * ============================================================================
 * WinRealpath - Object-Oriented Canonical File Path Resolver for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & CONFIGURATION] ............. RealpathOptions class (CLI parsing & modes)
 * 2. [PATH CANONICALIZATION ENGINE] ........ PathResolver class (symlinks, relative-to, prefix stripping)
 * 3. [APPLICATION CONTROLLER] .............. RealpathApp class and main entry point
 * ============================================================================
 */

#include <iostream>
#include <vector>
#include <string>
#include <string_view>
#include <filesystem>
#include <optional>
#include <algorithm>
#include <system_error>
#include <memory>

namespace fs = std::filesystem;

// ============================================================================
// 1. OPTIONS & CONFIGURATION
// ============================================================================

enum class CanonicalMode {
    Existing,    // -e / --canonicalize-existing (all components must exist)
    Missing,     // -m / --canonicalize-missing  (components need not exist)
    NoSymlinks   // -s / --strip / --no-symlinks (do not expand symlinks)
};

class RealpathOptions {
public:
    static constexpr std::string_view PROGRAM_NAME = "realpath";
    static constexpr std::string_view VERSION = "7.0.0";

    CanonicalMode mode{CanonicalMode::Existing};
    bool quiet{false};
    bool zero{false};
    std::optional<fs::path> relativeTo;
    std::optional<fs::path> relativeBase;
    std::vector<std::string> files;

    static void printVersion() {
        std::cout << PROGRAM_NAME << " " << VERSION << "\n"
                  << "Copyright (C) 2026 Roberto J Dohnert\n";
    }

    static void printHelp() {
        std::cout << R"(realpath(1)                CrossShell for UNIX Reference Manual                realpath(1)

    NAME
        realpath - print the resolved absolute path

    SYNOPSIS
        realpath [OPTIONS] FILE...

    DESCRIPTION
        realpath resolves all symbolic links, junctions, and relative path
        references ('.' and '..') to print the canonical absolute path for
        each specified FILE.

    OPTIONS
        -e, --canonicalize-existing
            All components of the path must exist (default behavior).

        -m, --canonicalize-missing
            No components of the path need exist.

        -s, --strip, --no-symlinks
            Resolve '..' and '.' path components without expanding symbolic links.

        -q, --quiet
            Suppress error messages for paths that cannot be resolved.

        --relative-to=<dir>
            Print the resolved path relative to directory <dir>.

        --relative-base=<dir>
            Print the path relative to <dir> if within <dir>; otherwise print
            the canonical absolute path.

        -z, --zero
            End each output line with NUL (0 byte) instead of a newline.

        -h, --help
            Display this reference manual and exit.

        --version
            Display version information and exit.

    EXAMPLES
        realpath ../file.txt
            Print the canonical absolute path for ../file.txt.

        realpath -m non_existent/path/file.txt
            Canonicalize path without requiring intermediate directories to exist.

        realpath --relative-to=/usr /usr/bin/tool
            Print path relative to /usr (output: bin/tool).

        realpath -z *.log
            Print resolved paths separated by NUL characters.

    CrossShell for UNIX                                                          realpath(1)
)";
    }

    static bool parse(int argc, char* argv[], RealpathOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::string_view arg = argv[i];

            if (arg == "--help" || arg == "-h" || arg == "/?") {
                printHelp();
                std::exit(0);
            } else if (arg == "--version" || arg == "-V") {
                printVersion();
                std::exit(0);
            } else if (arg == "-e" || arg == "--canonicalize-existing") {
                opts.mode = CanonicalMode::Existing;
            } else if (arg == "-m" || arg == "--canonicalize-missing") {
                opts.mode = CanonicalMode::Missing;
            } else if (arg == "-s" || arg == "--strip" || arg == "--no-symlinks") {
                opts.mode = CanonicalMode::NoSymlinks;
            } else if (arg == "-q" || arg == "--quiet") {
                opts.quiet = true;
            } else if (arg == "-z" || arg == "--zero") {
                opts.zero = true;
            } else if (arg.rfind("--relative-to=", 0) == 0) {
                opts.relativeTo = fs::path(arg.substr(14));
            } else if (arg == "--relative-to") {
                if (i + 1 < argc) {
                    opts.relativeTo = fs::path(argv[++i]);
                } else {
                    std::cerr << PROGRAM_NAME << ": option '--relative-to' requires an argument\n";
                    return false;
                }
            } else if (arg.rfind("--relative-base=", 0) == 0) {
                opts.relativeBase = fs::path(arg.substr(16));
            } else if (arg == "--relative-base") {
                if (i + 1 < argc) {
                    opts.relativeBase = fs::path(argv[++i]);
                } else {
                    std::cerr << PROGRAM_NAME << ": option '--relative-base' requires an argument\n";
                    return false;
                }
            } else if (arg == "--") {
                for (++i; i < argc; ++i) {
                    opts.files.emplace_back(argv[i]);
                }
                break;
            } else if (arg.rfind("-", 0) == 0 && arg.length() > 1) {
                for (size_t c = 1; c < arg.length(); ++c) {
                    switch (arg[c]) {
                        case 'e': opts.mode = CanonicalMode::Existing; break;
                        case 'm': opts.mode = CanonicalMode::Missing; break;
                        case 's': opts.mode = CanonicalMode::NoSymlinks; break;
                        case 'q': opts.quiet = true; break;
                        case 'z': opts.zero = true; break;
                        default:
                            std::cerr << PROGRAM_NAME << ": unrecognized option '-" << arg[c] << "'\n"
                                      << "Try '" << PROGRAM_NAME << " --help' for more information.\n";
                            return false;
                    }
                }
            } else {
                opts.files.emplace_back(arg);
            }
        }

        if (opts.files.empty()) {
            std::cerr << PROGRAM_NAME << ": missing operand\n"
                      << "Try '" << PROGRAM_NAME << " --help' for more information.\n";
            return false;
        }

        return true;
    }
};

// ============================================================================
// 2. PATH CANONICALIZATION ENGINE
// ============================================================================

class PathResolver {
public:
    static fs::path stripExtendedPrefix(const fs::path& p) {
        std::wstring s = p.wstring();
        if (s.rfind(L"\\\\?\\UNC\\", 0) == 0) {
            return fs::path(L"\\\\" + s.substr(8));
        }
        if (s.rfind(L"\\\\?\\", 0) == 0) {
            return fs::path(s.substr(4));
        }
        return p;
    }

    static bool isSubpath(const fs::path& target, const fs::path& base) {
        auto t = target.lexically_normal();
        auto b = base.lexically_normal();
        auto mismatchPair = std::mismatch(b.begin(), b.end(), t.begin(), t.end());
        return mismatchPair.first == b.end();
    }

    static bool resolve(const fs::path& input, const RealpathOptions& opts, fs::path& outPath, std::string& errMsg) {
        std::error_code ec;
        fs::path resolved;

        switch (opts.mode) {
            case CanonicalMode::Existing:
                resolved = fs::canonical(input, ec);
                if (ec) {
                    errMsg = ec.message();
                    return false;
                }
                break;

            case CanonicalMode::Missing:
                resolved = fs::weakly_canonical(input, ec);
                if (ec) {
                    ec.clear();
                    resolved = fs::absolute(input, ec).lexically_normal();
                    if (ec) {
                        errMsg = ec.message();
                        return false;
                    }
                }
                break;

            case CanonicalMode::NoSymlinks:
                resolved = fs::absolute(input, ec).lexically_normal();
                if (ec) {
                    errMsg = ec.message();
                    return false;
                }
                break;
        }

        resolved = stripExtendedPrefix(resolved);

        if (opts.relativeBase) {
            fs::path baseResolved;
            if (!resolve(*opts.relativeBase, opts, baseResolved, errMsg)) {
                return false;
            }

            if (isSubpath(resolved, baseResolved)) {
                fs::path relTo = opts.relativeTo ? *opts.relativeTo : baseResolved;
                fs::path relResolved;
                if (!resolve(relTo, opts, relResolved, errMsg)) {
                    return false;
                }
                resolved = resolved.lexically_relative(relResolved);
            }
        } else if (opts.relativeTo) {
            fs::path relToResolved;
            if (!resolve(*opts.relativeTo, opts, relToResolved, errMsg)) {
                return false;
            }
            resolved = resolved.lexically_relative(relToResolved);
        }

        outPath = resolved.make_preferred();
        return true;
    }

    static int execute(const RealpathOptions& opts) {
        int exitCode = 0;
        const char delimiter = opts.zero ? '\0' : '\n';

        for (const auto& file : opts.files) {
            fs::path resolved;
            std::string errMsg;

            if (resolve(fs::path(file), opts, resolved, errMsg)) {
                std::cout << resolved.string() << delimiter;
            } else {
                exitCode = 1;
                if (!opts.quiet) {
                    std::cerr << RealpathOptions::PROGRAM_NAME << ": " << file << ": " << errMsg << "\n";
                }
            }
        }

        return exitCode;
    }
};

// ============================================================================
// 3. APPLICATION CONTROLLER
// ============================================================================

class RealpathApp {
public:
    static int run(int argc, char* argv[]) {
        RealpathOptions options;
        if (!RealpathOptions::parse(argc, argv, options)) {
            return 1;
        }
        return PathResolver::execute(options);
    }
};

int main(int argc, char* argv[]) {
    return RealpathApp::run(argc, argv);
}