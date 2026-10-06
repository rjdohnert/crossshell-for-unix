#include "build_options.hpp"
#include "flag_translator.hpp"
#include "toolchain_locator.hpp"

void FlagTranslator::PrintHelp(const std::string& exeName) {
        std::cout << R"(vcc(1)                  CrossShell for UNIX Reference Manual                 vcc(1)

    NAME
        vcc - Visual Studio C++ compiler driver

    SYNOPSIS
        vcc [FLAGS] [SOURCE_FILES] [/link LINKER_FLAGS]

    DESCRIPTION
        Translates common GCC and Clang-style compiler flags to MSVC options,
        discovers the Visual Studio toolchain, and invokes cl.exe.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -o FILE
            Output executable or object file name (/Fe:FILE or /Fo:FILE with -c).

        -c
            Compile only; do not link.

        -Wall, -Wextra
            Enable rich warnings (/W4).

        -Werror
            Treat all warnings as errors (/WX).

        -O2, -O3
            Maximize speed optimization (/O2).

        -O1, -Os
            Minimize size optimization (/O1).

        -O0
            Disable optimizations (/Od).

        -g
            Generate debugging information (/Zi).

        -s
            Strip symbols from output executable (/link /RELEASE /DEBUG:NO).

        -static
            Statically link C runtime (/MT).

        -I DIR
            Add DIR to the include search path.

        -D MACRO
            Define MACRO preprocessor symbol.

        -U MACRO
            Undefine MACRO preprocessor symbol.

        -l LIB
            Link against library LIB (LIB.lib).

        -L DIR
            Add DIR to the library search path (/link /LIBPATH:DIR).

        -Wl,OPTIONS
            Pass comma-separated OPTIONS directly to the linker.

        -mwindows
            Create Windows GUI subsystem application.

        -mconsole
            Create Windows console subsystem application.

        -shared
            Create a shared library or DLL (/LD).

        -std=c++XX
            Set C++ language standard (/std:c++XX).

        -m32, -m64
            Target x86 (32-bit) or x64 (64-bit) architecture.

        -h, --help
            Display this reference manual and exit.

        -v, --version
            Display version information and exit.

    NATIVE MSVC FLAGS
        Native MSVC flags such as /EHsc, /MD, and /utf-8, together with linker
        flags after /link, are forwarded unchanged.

    ENVIRONMENT DISCOVERY
        If cl.exe is already in PATH, vcc invokes it directly. Otherwise it queries
        vswhere.exe and loads the Visual Studio vcvars64.bat environment.

    EXAMPLES
        vcc -Wall -O2 -std=c++20 main.cpp -o app.exe
            Compile a C++ source file with warnings and optimization.

        vcc -Iinclude main.cpp -L./lib -lws2_32 -o app.exe
            Compile with an include path and library search path.

        vcc -O2 -shared plugin.cpp -o plugin.dll
            Build a shared library.

    CrossShell for UNIX                                                      vcc(1)
)";
    }

void FlagTranslator::PrintVersion() {
        std::cout << "vcc version " << VCC_VERSION << "\n";
    }

bool FlagTranslator::Parse(int argc, char* argv[], BuildOptions& opts) const {
        opts.programName = (argc > 0) ? argv[0] : "vcc";

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-h" || arg == "--help" || arg == "/?" || arg == "-?") {
                opts.showHelp = true;
                return true;
            }
            if (arg == "-v" || arg == "--version") {
                opts.showVersion = true;
                return true;
            }
            if (arg == "-c" || arg == "/c") {
                opts.compileOnly = true;
            }
            if (arg == "-m32") {
                opts.targetArch = TargetArch::X86;
            } else if (arg == "-m64") {
                opts.targetArch = TargetArch::X64;
            }
        }

        opts.compilerArgs.push_back("/nologo");
        opts.compilerArgs.push_back("/EHsc");

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-m32" || arg == "-m64") {
                continue;
            }

            if (arg == "-mwindows") {
                opts.linkerArgs.push_back("/SUBSYSTEM:WINDOWS");
            } else if (arg == "-mconsole") {
                opts.linkerArgs.push_back("/SUBSYSTEM:CONSOLE");
            } else if (arg == "-static") {
                opts.compilerArgs.push_back("/MT");
            } else if (arg == "-o" && i + 1 < argc) {
                std::string outFile = argv[++i];
                opts.compilerArgs.push_back((opts.compileOnly ? "/Fo:" : "/Fe:") + outFile);
            } else if (arg.rfind("-o", 0) == 0 && arg.length() > 2) {
                std::string outFile = arg.substr(2);
                opts.compilerArgs.push_back((opts.compileOnly ? "/Fo:" : "/Fe:") + outFile);
            } else if (arg == "-Wall" || arg == "-Wextra") {
                opts.compilerArgs.push_back("/W4");
            } else if (arg == "-Werror") {
                opts.compilerArgs.push_back("/WX");
            } else if (arg == "-O2" || arg == "-O3") {
                opts.compilerArgs.push_back("/O2");
            } else if (arg == "-O1" || arg == "-Os") {
                opts.compilerArgs.push_back("/O1");
            } else if (arg == "-O0") {
                opts.compilerArgs.push_back("/Od");
            } else if (arg == "-g") {
                opts.compilerArgs.push_back("/Zi");
            } else if (arg == "-c") {
                opts.compilerArgs.push_back("/c");
            } else if (arg == "-shared") {
                opts.sharedLibrary = true;
                opts.compilerArgs.push_back("/LD");
            } else if (arg == "-s") {
                opts.linkerArgs.push_back("/RELEASE");
                opts.linkerArgs.push_back("/DEBUG:NO");
            } else if (arg == "-I" && i + 1 < argc) {
                opts.compilerArgs.push_back("/I" + std::string(argv[++i]));
            } else if (arg.rfind("-I", 0) == 0 && arg.length() > 2) {
                opts.compilerArgs.push_back("/I" + arg.substr(2));
            } else if (arg == "-D" && i + 1 < argc) {
                opts.compilerArgs.push_back("/D" + std::string(argv[++i]));
            } else if (arg.rfind("-D", 0) == 0 && arg.length() > 2) {
                opts.compilerArgs.push_back("/D" + arg.substr(2));
            } else if (arg == "-U" && i + 1 < argc) {
                opts.compilerArgs.push_back("/U" + std::string(argv[++i]));
            } else if (arg.rfind("-U", 0) == 0 && arg.length() > 2) {
                opts.compilerArgs.push_back("/U" + arg.substr(2));
            } else if (arg == "-L" && i + 1 < argc) {
                opts.linkerArgs.push_back("/LIBPATH:" + std::string(argv[++i]));
            } else if (arg.rfind("-L", 0) == 0 && arg.length() > 2) {
                opts.linkerArgs.push_back("/LIBPATH:" + arg.substr(2));
            } else if (arg == "-l" && i + 1 < argc) {
                opts.linkerArgs.push_back(std::string(argv[++i]) + ".lib");
            } else if (arg.rfind("-l", 0) == 0 && arg.length() > 2) {
                opts.linkerArgs.push_back(arg.substr(2) + ".lib");
            } else if (arg.rfind("-Wl,", 0) == 0) {
                std::string wlStr = arg.substr(4);
                std::istringstream ss(wlStr);
                std::string token;
                while (std::getline(ss, token, ',')) {
                    if (!token.empty()) {
                        opts.linkerArgs.push_back(token);
                    }
                }
            } else if (arg.rfind("-std=", 0) == 0) {
                std::string stdVer = arg.substr(5);
                opts.compilerArgs.push_back("/std:" + stdVer);
            } else if (arg == "/link") {
                for (++i; i < argc; ++i) {
                    opts.linkerArgs.push_back(argv[i]);
                }
            } else {
                opts.compilerArgs.push_back(arg);
            }
        }
        return true;
    }
