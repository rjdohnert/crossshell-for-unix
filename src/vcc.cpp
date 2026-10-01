/*
BSD 3-Clause License

CrossShell for UNIX
Copyright (c) 2026, Roberto J Dohnert
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <process.h>
#include <memory>
#include <fstream>
#include <ctime>

#define VCC_VERSION "1.2.0"

// ============================================================================
// 1. COMMAND LINE FORMATTER & QUOTING UTILITIES
// ============================================================================
class CommandLineFormatter {
public:
    static std::string Quote(const std::string& arg) {
        if (arg.empty()) return "\"\"";
        if (arg.find_first_of(" \t\n\v\"") == std::string::npos) return arg;

        std::string out = "\"";
        int backslashes = 0;

        for (size_t i = 0; i < arg.length(); ++i) {
            if (arg[i] == '\\') {
                backslashes++;
            } else if (arg[i] == '\"') {
                out.append(backslashes * 2 + 1, '\\');
                out.push_back('\"');
                backslashes = 0;
            } else {
                out.append(backslashes, '\\');
                backslashes = 0;
                out.push_back(arg[i]);
            }
        }
        out.append(backslashes * 2, '\\');
        out += "\"";
        return out;
    }

    static std::string FormatArgs(const std::vector<std::string>& args) {
        std::ostringstream ss;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) ss << " ";
            ss << Quote(args[i]);
        }
        return ss.str();
    }
};

// ============================================================================
// 2. TOOLCHAIN DISCOVERY & ENVIRONMENT MANAGEMENT
// ============================================================================
enum class TargetArch {
    X64,
    X86,
    ARM64
};

class ToolchainLocator {
public:
    bool IsClInPath() const {
        return SearchPathA(NULL, "cl.exe", NULL, 0, NULL, NULL) > 0;
    }

    std::string FindVsInstallation() const {
        std::string vswherePath = "vswhere.exe";
        bool hasVswhereInPath = (SearchPathA(NULL, "vswhere.exe", NULL, 0, NULL, NULL) > 0);

        if (!hasVswhereInPath) {
            char pf[MAX_PATH] = {};
            if (GetEnvironmentVariableA("ProgramFiles(x86)", pf, MAX_PATH) > 0) {
                std::string p = std::string(pf) + "\\Microsoft Visual Studio\\Installer\\vswhere.exe";
                if (GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    vswherePath = p;
                }
            }
            if (vswherePath == "vswhere.exe" && GetEnvironmentVariableA("ProgramFiles", pf, MAX_PATH) > 0) {
                std::string p = std::string(pf) + "\\Microsoft Visual Studio\\Installer\\vswhere.exe";
                if (GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    vswherePath = p;
                }
            }
        }

        std::string command = "\"" + vswherePath + "\" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath";

        FILE* pipe = _popen(command.c_str(), "r");
        if (!pipe) return "";

        char buffer[512];
        std::string result = "";
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            result += buffer;
        }
        _pclose(pipe);

        size_t end = result.find_last_not_of(" \n\r\t");
        return (end == std::string::npos) ? "" : result.substr(0, end + 1);
    }

    std::string GetVcvarsBatchPath(const std::string& vsPath, TargetArch arch) const {
        std::string scriptName = "vcvars64.bat";
        if (arch == TargetArch::X86) {
            scriptName = "vcvars32.bat";
        } else if (arch == TargetArch::ARM64) {
            scriptName = "vcvarsarm64.bat";
        }
        return vsPath + "\\VC\\Auxiliary\\Build\\" + scriptName;
    }
};

// ============================================================================
// 3. BUILD CONFIGURATION & TRANSLATOR
// ============================================================================
struct BuildOptions {
    bool showHelp = false;
    bool showVersion = false;
    bool compileOnly = false;
    bool sharedLibrary = false;
    TargetArch targetArch = TargetArch::X64;
    std::string programName;
    std::vector<std::string> compilerArgs;
    std::vector<std::string> linkerArgs;
    std::vector<std::string> sourceFiles;
};

class FlagTranslator {
public:
    static void PrintHelp(const std::string& exeName) {
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

    static void PrintVersion() {
        std::cout << "vcc version " << VCC_VERSION << "\n";
    }

    bool Parse(int argc, char* argv[], BuildOptions& opts) const {
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
};

// ============================================================================
// 4. COMPILER DRIVER ENGINE
// ============================================================================
class CompileLogger {
public:
    static std::string GetLogPath() {
        char buf[MAX_PATH] = {};
        if (GetEnvironmentVariableA("USERPROFILE", buf, MAX_PATH) > 0 && buf[0] != '\0') {
            return std::string(buf) + "\\.vcc_log";
        }
        char drive[16] = {};
        char path[MAX_PATH] = {};
        if (GetEnvironmentVariableA("HOMEDRIVE", drive, sizeof(drive)) > 0 &&
            GetEnvironmentVariableA("HOMEPATH", path, sizeof(path)) > 0) {
            return std::string(drive) + std::string(path) + "\\.vcc_log";
        }
        if (GetEnvironmentVariableA("HOME", buf, MAX_PATH) > 0 && buf[0] != '\0') {
            return std::string(buf) + "\\.vcc_log";
        }
        return std::string();
    }

    static std::string GetTimestamp() {
        SYSTEMTIME st;
        GetLocalTime(&st);
        char ts[64] = {};
        snprintf(ts, sizeof(ts), "%04d-%02d-%02d %02d:%02d:%02d",
                 st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        return std::string(ts);
    }

    static void LogInvocation(const std::string& commandLine) {
        std::string logPath = GetLogPath();
        if (logPath.empty()) return;

        std::ofstream log(logPath, std::ios::app);
        if (!log.is_open()) return;

        char cwd[MAX_PATH] = {};
        GetCurrentDirectoryA(MAX_PATH, cwd);

        log << "[" << GetTimestamp() << "]"
            << " cwd=\"" << cwd << "\""
            << " cmd=" << commandLine << "\n";
    }

    static void LogResult(int exitCode) {
        std::string logPath = GetLogPath();
        if (logPath.empty()) return;

        std::ofstream log(logPath, std::ios::app);
        if (!log.is_open()) return;

        log << "[" << GetTimestamp() << "]"
            << " exit=" << exitCode << "\n";
    }
};

class CompilerDriver {
private:
    ToolchainLocator m_locator;

public:
    int Run(const BuildOptions& opts) {
        if (opts.showHelp) {
            FlagTranslator::PrintHelp(opts.programName);
            return 0;
        }
        if (opts.showVersion) {
            FlagTranslator::PrintVersion();
            return 0;
        }

        bool clInPath = m_locator.IsClInPath();
        std::ostringstream cmd;

        if (clInPath) {
            cmd << "cl.exe";
        } else {
            std::string vsPath = m_locator.FindVsInstallation();
            if (vsPath.empty()) {
                std::cerr << "[vcc] Error: Visual Studio could not be found via vswhere.exe.\n";
                return 1;
            }
            std::string vcvars = m_locator.GetVcvarsBatchPath(vsPath, opts.targetArch);
            cmd << "cmd.exe /S /C \"call \"" << vcvars << "\" >nul 2>&1 && cl.exe";
        }

        for (const auto& cArg : opts.compilerArgs) {
            cmd << " " << CommandLineFormatter::Quote(cArg);
        }

        if (!opts.linkerArgs.empty()) {
            cmd << " /link";
            for (const auto& lArg : opts.linkerArgs) {
                cmd << " " << CommandLineFormatter::Quote(lArg);
            }
        }

        if (!clInPath) {
            cmd << "\"";
        }

        CompileLogger::LogInvocation(cmd.str());
        int result = system(cmd.str().c_str());
        CompileLogger::LogResult(result);
        return result;
    }
};

// ============================================================================
// 5. MAIN ENTRY POINT
// ============================================================================
int main(int argc, char* argv[]) {
    BuildOptions opts;
    FlagTranslator translator;

    if (!translator.Parse(argc, argv, opts)) {
        return 1;
    }

    CompilerDriver driver;
    return driver.Run(opts);
}