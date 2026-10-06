#include "options.hpp"
#include <iostream>
#include <cctype>

void CommandLineParser::print_version() {
    std::cout << PROG_NAME << " version " << VERSION << "\n"
              << "\n"
              << "Copyright (c) 2026, Roberto J Dohnert. Licensed under the BSD 3-Clause License.\n";
}

void CommandLineParser::print_help() {
    std::cout << R"(make(1)                  CrossShell for UNIX Reference Manual                 make(1)

    NAME
        make - maintain, update, and regenerate groups of programs

    SYNOPSIS
        make [OPTIONS] [TARGET...] [MACRO=VALUE...]

    DESCRIPTION
        Controls the generation of executables and other non-source files of a program
        from the program's source files. It allows specifying dependencies, rules,
        parallel build execution, and custom macro overrides.

    OPTIONS
        -f, --file <file>
            Read FILE as a makefile.

        -j, --jobs [<n>]
            Allow N parallel build jobs (default: number of CPU cores).

        -n, --dry-run
            Print recipes without executing them.

        -s, --silent, --quiet
            Do not echo recipes before execution.

        -k, --keep-going
            Continue execution as much as possible after encountering errors.

        -B, --always-build
            Unconditionally build all targets regardless of modification times.

        -C, --directory <dir>
            Change directory to DIR before reading the makefile.

        -H, --hash-check
            Use fast content hashing (FNV-1a) instead of file timestamps.

        --export-compile-commands
            Export compile_commands.json for Clangd and MSVC IntelliSense.

        --json
            Output structured build telemetry in JSON format.

        --no-color
            Disable ANSI terminal color output.

        -h, --help, /?
            Display this comprehensive reference manual and exit.

        -v, -V, --version
            Display version information and exit.

    EXAMPLES
        make
            Build default target from Makefile or makefile.

        make -j8 -H
            Perform parallel build with 8 jobs using content hashing.

        make -f Makefile.win CXX=clang++ --export-compile-commands
            Use custom makefile, override CXX compiler, and generate compilation database.

        make -C src clean
            Change to src directory and run clean target.

    EXIT STATUS
        0
            Successful build.
        1
            Make syntax error, build recipe failure, or target not found.

    CrossShell for UNIX                                                    make(1)
)";
}

bool CommandLineParser::parse_cli(int argc, char* argv[], Config& cfg) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help" || arg == "/?") {
            cfg.show_help = true;
            return true;
        }
        if (arg == "-v" || arg == "-V" || arg == "--version") {
            cfg.show_version = true;
            return true;
        }
        if (arg == "-n" || arg == "--dry-run") {
            cfg.dry_run = true;
        } else if (arg == "-s" || arg == "--silent" || arg == "--quiet") {
            cfg.silent = true;
        } else if (arg == "-k" || arg == "--keep-going") {
            cfg.keep_going = true;
        } else if (arg == "-B" || arg == "--always-build") {
            cfg.always_build = true;
        } else if (arg == "-H" || arg == "--hash-check") {
            cfg.hash_check = true;
        } else if (arg == "--export-compile-commands") {
            cfg.export_compile_commands = true;
        } else if (arg == "--json") {
            cfg.json_output = true;
        } else if (arg == "--no-color") {
            Color::enabled = false;
        } else if (arg == "-f" || arg == "--file") {
            if (i + 1 < argc) cfg.makefile_path = argv[++i];
        } else if (arg == "-C" || arg == "--directory") {
            if (i + 1 < argc) cfg.change_dir = argv[++i];
        } else if (arg == "-j" || arg == "--jobs") {
            if (i + 1 < argc && std::isdigit(argv[i + 1][0])) {
                cfg.jobs = std::strtoul(argv[++i], nullptr, 10);
            }
        } else if (arg.find('=') != std::string::npos) {
            size_t eq = arg.find('=');
            cfg.cli_macros[arg.substr(0, eq)] = arg.substr(eq + 1);
        } else {
            cfg.target_goals.push_back(arg);
        }
    }
    return true;
}
