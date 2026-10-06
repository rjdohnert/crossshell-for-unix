#include "options.hpp"

void print_version() {
    std::cout << PROGRAM_NAME << " version " << PROGRAM_VERSION << " (MSVC / GCC / Clang)\n"
              << "Dependency generator for MSVC, GCC, and Clang codebases.\n";
}

void print_help() {
    std::cout << R"(makedepend(1)           CrossShell for UNIX Reference Manual           makedepend(1)

    NAME
        makedepend - create dependencies in makefiles

    SYNOPSIS
        makedepend [OPTIONS] [--] SOURCEFILE...

    DESCRIPTION
        Parses C/C++ source code, evaluates preprocessor macros and conditional
        directives (#if, #ifdef, #elif, defined), and appends object-to-header
        dependencies to a Makefile.

    OPTIONS
        -I <dir>
            Add directory DIR to header search path.

        -D <symbol>[=<val>]
            Define preprocessor symbol (default value: 1).

        -U <symbol>
            Undefine preprocessor symbol.

        -f <file>
            Specify target Makefile (default: Makefile).

        -o <suffix>
            Set object file suffix (default: .obj).

        -p <prefix>
            Set object file prefix path.

        -s <string>
            Specify dependency delimiter string.

        -a
            Append dependencies instead of replacing existing section.

        -v, --verbose
            Enable verbose output during dependency scanning.

        --compiler <name>
            Select compiler preset: msvc (default), gcc, or clang.

        --detect-cl
            Auto-detect MSVC include paths from %INCLUDE%.

        --msvc-ver <ver>
            Set MSVC version simulation (default: 1930 for MSVC 2022).

        --detect-gcc
            Probe g++ to discover GCC system include paths.

        --gcc-ver <ver>
            Set GCC version for macro simulation (default: 13).

        --detect-clang
            Probe clang++ to discover Clang system include paths.

        --clang-ver <ver>
            Set Clang version for macro simulation (default: 17).

        -h, --help
            Display this reference manual and exit.

        -V, --version
            Display version information and exit.

    EXAMPLES
        makedepend -I include -f Makefile src/*.cpp
            Generate dependencies for all C++ source files and update Makefile.

        makedepend --compiler gcc -I /usr/include -f Makefile main.c
            Generate dependencies using GCC configuration.

    EXIT STATUS
        0
            Success.

        1
            An error occurred (e.g., missing arguments or file not found).

    CrossShell for UNIX                                                    makedepend(1)
)";
}

bool parse_options(int argc, char* argv[], MakedependOptions& opts) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help" || arg == "/?") {
            opts.showHelp = true;
            return true;
        } else if (arg == "-V" || arg == "--version") {
            opts.showVersion = true;
            return true;
        } else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        } else if (arg == "--msvc-ver" && i + 1 < argc) {
            opts.msvcVer = argv[++i];
        } else if (arg == "--compiler" && i + 1 < argc) {
            std::string compiler = argv[++i];
            if (compiler == "gcc") {
                opts.gccVer = "13";
                opts.autoDetectCL = false;
                opts.autoDetectGCC = true;
                opts.objSuffix = ".o";
            } else if (compiler == "clang") {
                opts.clangVer = "17";
                opts.autoDetectCL = false;
                opts.autoDetectClang = true;
                opts.objSuffix = ".o";
            } else if (compiler == "msvc") {
                opts.msvcVer = "1930";
                opts.autoDetectCL = true;
            } else {
                std::cerr << "makedepend: Unknown compiler '" << compiler << "'. Use msvc, gcc, or clang.\n";
                return false;
            }
        } else if (arg == "--detect-cl") {
            opts.autoDetectCL = true;
        } else if (arg == "--detect-gcc") {
            opts.autoDetectGCC = true;
            opts.autoDetectCL = false;
        } else if (arg == "--gcc-ver" && i + 1 < argc) {
            opts.gccVer = argv[++i];
            opts.autoDetectCL = false;
        } else if (arg == "--detect-clang") {
            opts.autoDetectClang = true;
            opts.autoDetectCL = false;
        } else if (arg == "--clang-ver" && i + 1 < argc) {
            opts.clangVer = argv[++i];
            opts.autoDetectCL = false;
        } else if (arg.rfind("-I", 0) == 0) {
            std::string dir = (arg.length() > 2) ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "");
            if (!dir.empty()) opts.includeDirs.push_back(dir);
        } else if (arg.rfind("-D", 0) == 0) {
            std::string def = (arg.length() > 2) ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "");
            size_t eq = def.find('=');
            if (eq != std::string::npos) {
                opts.defines.push_back({def.substr(0, eq), def.substr(eq + 1)});
            } else {
                opts.defines.push_back({def, "1"});
            }
        } else if (arg.rfind("-U", 0) == 0) {
            std::string sym = (arg.length() > 2) ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "");
            if (!sym.empty()) opts.undefines.push_back(sym);
        } else if (arg.rfind("-f", 0) == 0) {
            opts.makefilePath = (arg.length() > 2) ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "Makefile");
        } else if (arg.rfind("-o", 0) == 0) {
            opts.objSuffix = (arg.length() > 2) ? arg.substr(2) : (i + 1 < argc ? argv[++i] : ".obj");
        } else if (arg.rfind("-p", 0) == 0) {
            opts.objPrefix = (arg.length() > 2) ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "");
        } else if (arg.rfind("-s", 0) == 0) {
            opts.delimiter = (arg.length() > 2) ? arg.substr(2) : (i + 1 < argc ? argv[++i] : DEFAULT_DELIMITER);
        } else if (arg == "-a") {
            opts.appendOnly = true;
        } else if (arg == "--") {
            while (++i < argc) opts.sourceFiles.push_back(argv[i]);
        } else if (arg[0] != '-') {
            opts.sourceFiles.push_back(arg);
        } else {
            std::cerr << "makedepend: Unknown option '" << arg << "'\n";
            return false;
        }
    }
    return true;
}
