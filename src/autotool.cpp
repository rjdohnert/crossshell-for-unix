/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
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
 * SINGLE FILE INDEX: autotool.cpp
 * ============================================================================
 * WinAutotool - Object-Oriented Multi-Platform Build System & GNU Autotools Port Engine
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64), UNIX, AIX
 *
 * TABLE OF CONTENTS:
 * 1. [INCLUDES & PLATFORM CONSTANTS] ........ Platform macros, extensions, metadata
 * 2. [UTILITY HELPERS] ...................... Path normalization, string splitting, escaping
 * 3. [BUILD MODEL & PROJECT CONFIG] ......... ProjectConfig, TargetSpec, DependencyGraph
 * 4. [TOOLCHAIN DETECTION & DRIVERS] ........ MSVC/GCC/Clang/XLC toolchain detectors & drivers
 * 5. [AUTOTOOLS GENERATORS & MODULES] ....... AutomakeModule, AutoconfModule, LibtoolModule
 * 6. [APPLICATION CONTROLLER & DISPATCHER] .. AutotoolApp class and main router
 * ============================================================================
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <thread>
#include <future>
#include <mutex>
#include <atomic>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <chrono>
#include <utility>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif
#if !defined(_WIN32) && !defined(_WIN64)
#include <sys/wait.h>
#endif

using std::cerr;
using std::cout;
using std::endl;
using std::ifstream;
using std::ios;
using std::lock_guard;
using std::map;
using std::mutex;
using std::ofstream;
using std::pair;
using std::set;
using std::size_t;
using std::string;
using std::thread;
using std::to_string;
using std::transform;
using std::vector;
namespace fs = std::filesystem;

// Tool Metadata
const string TOOL_NAME = "autotool";
const string VERSION = "4.3.0";

// Platform Detection
#if defined(_WIN32) || defined(_WIN64)
    #define PLATFORM_WINDOWS 1
    const string OS_NAME = "Windows";
    const string EXE_EXT = ".exe";
    const string OBJ_EXT = ".obj";
    const string LIB_EXT = ".lib";
    const string DLL_EXT = ".dll";
#elif defined(__aix) || defined(_AIX)
    #define PLATFORM_AIX 1
    const string OS_NAME = "AIX";
    const string EXE_EXT = "";
    const string OBJ_EXT = ".o";
    const string LIB_EXT = ".a";
    const string DLL_EXT = ".so";
#else
    #define PLATFORM_LINUX 1
    const string OS_NAME = "Linux/POSIX";
    const string EXE_EXT = "";
    const string OBJ_EXT = ".o";
    const string LIB_EXT = ".a";
    const string DLL_EXT = ".so";
#endif

// Toolchain Types
enum class CompilerType { MSVC, GCC, CLANG, XLC, UNKNOWN };
enum class TargetType { EXECUTABLE, STATIC_LIB, SHARED_LIB };

mutex global_log_mutex;

// String and System Utilities
string trim(const string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

bool starts_with(const string& value, const string& prefix) {
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

string to_upper_identifier(string str) {
    transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
        return isalnum(c) ? toupper(c) : '_';
    });
    return str;
}

string unquote_m4(const string& str) {
    string s = trim(str);
    while (s.length() >= 2 && ((s.front() == '[' && s.back() == ']') || (s.front() == '"' && s.back() == '"'))) {
        s = s.substr(1, s.length() - 2);
        s = trim(s);
    }
    return s;
}

vector<string> split_words(const string& text) {
    vector<string> tokens;
    std::stringstream ss(text);
    string token;
    while (ss >> token) {
        tokens.push_back(unquote_m4(token));
    }
    return tokens;
}

vector<string> split_m4_args(const string& text) {
    vector<string> tokens;
    string current;
    int bracket_depth = 0;
    bool in_single_quote = false;
    bool in_double_quote = false;

    for (char ch : text) {
        if (ch == '[' && !in_single_quote && !in_double_quote) {
            ++bracket_depth;
            current.push_back(ch);
            continue;
        }
        if (ch == ']' && bracket_depth > 0 && !in_single_quote && !in_double_quote) {
            --bracket_depth;
            current.push_back(ch);
            continue;
        }
        if (ch == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
            current.push_back(ch);
            continue;
        }
        if (ch == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
            current.push_back(ch);
            continue;
        }
        if (ch == ',' && bracket_depth == 0 && !in_single_quote && !in_double_quote) {
            string trimmed = trim(current);
            if (!trimmed.empty()) {
                tokens.push_back(unquote_m4(trimmed));
            }
            current.clear();
            continue;
        }
        current.push_back(ch);
    }

    string trimmed = trim(current);
    if (!trimmed.empty()) {
        tokens.push_back(unquote_m4(trimmed));
    }
    return tokens;
}

string shell_quote(const string& value) {
    string quoted = "\"";
    for (char ch : value) {
        if (ch == '"' || ch == '$' || ch == '`') {
            quoted.push_back('\\');
        }
        quoted.push_back(ch);
    }
    quoted.push_back('"');
    return quoted;
}

string win_to_posix_path(const string& win_path) {
    fs::path p = fs::absolute(win_path);
    string str = p.string();
    replace(str.begin(), str.end(), '\\', '/');
    if (str.length() >= 2 && str[1] == ':') {
        char drive = tolower(str[0]);
        return "/" + string(1, drive) + str.substr(2);
    }
    return str;
}

#if defined(PLATFORM_WINDOWS)
bool initialize_msvc_environment(const string& vcvars_bat, string& error) {
    if (vcvars_bat.empty()) {
        return true;
    }

    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE read_pipe = nullptr;
    HANDLE write_pipe = nullptr;
    if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
        error = "CreatePipe failed for vcvars initialization";
        return false;
    }
    SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);
    si.hStdError = write_pipe;
    si.hStdOutput = write_pipe;
    si.dwFlags |= STARTF_USESTDHANDLES;

    string command = "cmd.exe /C \"call \"" + shell_quote(vcvars_bat) + "\" >nul 2>&1 && set\"";
    if (!CreateProcessA(nullptr, const_cast<char*>(command.c_str()), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(read_pipe);
        CloseHandle(write_pipe);
        error = "CreateProcess failed for vcvars initialization";
        return false;
    }

    CloseHandle(write_pipe);
    char buffer[4096];
    DWORD bytes_read = 0;
    string env_output;
    for (;;) {
        if (!ReadFile(read_pipe, buffer, sizeof(buffer), &bytes_read, nullptr) || bytes_read == 0) {
            break;
        }
        env_output.append(buffer, bytes_read);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(read_pipe);

    if (exit_code != 0) {
        error = "vcvars initialization exited with non-zero status";
        return false;
    }

    std::stringstream env_stream(env_output);
    string line;
    while (std::getline(env_stream, line)) {
        if (line.empty()) continue;
        size_t sep = line.find('=');
        if (sep == string::npos) continue;
        string name = line.substr(0, sep);
        string value = line.substr(sep + 1);
        if (!name.empty()) {
            SetEnvironmentVariableA(name.c_str(), value.c_str());
        }
    }

    return true;
}

int run_cmd(const string& cmd, string& output) {
    output.clear();
    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE read_pipe = nullptr;
    HANDLE write_pipe = nullptr;
    if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
        return -1;
    }
    SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);
    si.hStdError = write_pipe;
    si.hStdOutput = write_pipe;
    si.dwFlags |= STARTF_USESTDHANDLES;

    string command = cmd;
    if (!CreateProcessA(nullptr, const_cast<char*>(command.c_str()), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(read_pipe);
        CloseHandle(write_pipe);
        return -1;
    }

    CloseHandle(write_pipe);
    char buffer[4096];
    DWORD bytes_read = 0;
    for (;;) {
        if (!ReadFile(read_pipe, buffer, sizeof(buffer), &bytes_read, nullptr) || bytes_read == 0) {
            break;
        }
        output.append(buffer, bytes_read);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(read_pipe);
    return static_cast<int>(exit_code);
}
#else
int run_cmd(const string& cmd, string& output) {
    output.clear();
    char buffer[256];
    FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");
    if (!pipe) return -1;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    int status = pclose(pipe);
    return WIFEXITED(status) ? WEXITSTATUS(status) : status;
}
#endif

string get_env_var(const char* name) {
    const char* value = getenv(name);
    return value ? string(value) : "";
}

string trim_path(const string& path) {
    string value = trim(path);
    if (value.size() >= 2 && value[0] == '"' && value.back() == '"') {
        value = value.substr(1, value.size() - 2);
    }
    return value;
}

bool path_exists(const string& path) {
    return !path.empty() && fs::exists(path);
}

string find_executable_on_path(const vector<string>& names) {
    if (names.empty()) {
        return "";
    }

    for (const string& name : names) {
        if (name.empty()) continue;
        if (name.find('/') != string::npos || name.find('\\') != string::npos || name.find(':') != string::npos) {
            if (path_exists(name)) {
                return name;
            }
            continue;
        }

        string out;
#if defined(PLATFORM_WINDOWS)
        if (run_cmd("where " + name, out) == 0) {
            string trimmed = trim_path(out);
            if (!trimmed.empty()) {
                return trimmed;
            }
        }
#else
        if (run_cmd("which " + name, out) == 0) {
            string trimmed = trim_path(out);
            if (!trimmed.empty()) {
                return trimmed;
            }
        }
#endif
    }
    return "";
}

string find_vcvars_bat_path() {
#if defined(PLATFORM_WINDOWS)
    vector<string> candidates;
    string program_files_x86 = get_env_var("ProgramFiles(x86)");
    string program_files = get_env_var("ProgramFiles");
    string vs_install_dir = get_env_var("VSINSTALLDIR");
    string vc_install_dir = get_env_var("VCINSTALLDIR");

    if (!vs_install_dir.empty()) {
        candidates.push_back(vs_install_dir + "\\VC\\Auxiliary\\Build\\vcvars64.bat");
        candidates.push_back(vs_install_dir + "\\Common7\\Tools\\VsDevCmd.bat");
    }
    if (!vc_install_dir.empty()) {
        candidates.push_back(vc_install_dir + "\\..\\..\\Auxiliary\\Build\\vcvars64.bat");
        candidates.push_back(vc_install_dir + "\\..\\..\\Common7\\Tools\\VsDevCmd.bat");
    }
    if (!program_files_x86.empty()) {
        candidates.push_back(program_files_x86 + "\\Microsoft Visual Studio\\Installer\\vswhere.exe");
    }
    if (!program_files.empty()) {
        candidates.push_back(program_files + "\\Microsoft Visual Studio\\Installer\\vswhere.exe");
    }

    for (const auto& candidate : candidates) {
        if (candidate.find("vswhere.exe") != string::npos) {
            if (!path_exists(candidate)) continue;
            string command = "\"" + candidate + "\" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath";
            string output;
            if (run_cmd(command, output) != 0) continue;
            string install_path = trim(output);
            if (install_path.empty()) continue;
            vector<string> vcvars_candidates = {
                install_path + "\\VC\\Auxiliary\\Build\\vcvars64.bat",
                install_path + "\\Common7\\Tools\\VsDevCmd.bat"
            };
            for (const auto& vcvars_candidate : vcvars_candidates) {
                if (path_exists(vcvars_candidate)) {
                    return vcvars_candidate;
                }
            }
        }
    }

    for (const auto& candidate : candidates) {
        if (!candidate.empty() && path_exists(candidate) && (candidate.find(".bat") != string::npos || candidate.find(".cmd") != string::npos)) {
            return candidate;
        }
    }
#endif
    return "";
}

string find_msys2_root() {
#if defined(PLATFORM_WINDOWS)
    vector<string> roots;
    string env_root = get_env_var("MSYS2_ROOT");
    if (!env_root.empty()) roots.push_back(env_root);
    env_root = get_env_var("MSYS_ROOT");
    if (!env_root.empty()) roots.push_back(env_root);
    roots.push_back("C:\\msys64");
    roots.push_back("C:\\msys2");
    string program_files = get_env_var("ProgramFiles");
    if (!program_files.empty()) {
        roots.push_back(program_files + "\\msys64");
        roots.push_back(program_files + "\\msys2");
    }

    for (const auto& root : roots) {
        if (root.empty()) continue;
        fs::path bash_path = fs::path(root) / "usr" / "bin" / "bash.exe";
        if (path_exists(bash_path.string())) {
            return root;
        }
    }
#endif
    return "";
}

string find_compiler_path(const string& compiler_name) {
    if (compiler_name.empty()) return "";
    vector<string> candidates;
    if (compiler_name == "cl" || compiler_name == "cl.exe") {
        candidates = {"cl.exe", "cl"};
    } else if (compiler_name == "clang" || compiler_name == "clang.exe") {
        candidates = {"clang.exe", "clang"};
    } else if (compiler_name == "clang++" || compiler_name == "clang++.exe") {
        candidates = {"clang++.exe", "clang++"};
    } else if (compiler_name == "gcc" || compiler_name == "gcc.exe") {
        candidates = {"gcc.exe", "gcc"};
    } else if (compiler_name == "g++" || compiler_name == "g++.exe") {
        candidates = {"g++.exe", "g++"};
    } else {
        candidates = {compiler_name};
    }

    string found = find_executable_on_path(candidates);
    if (!found.empty()) {
        return found;
    }

#if defined(PLATFORM_WINDOWS)
    if (compiler_name == "gcc" || compiler_name == "g++") {
        string msys2_root = find_msys2_root();
        if (!msys2_root.empty()) {
            vector<string> msys2_candidates = {
                (fs::path(msys2_root) / "mingw64" / "bin" / "gcc.exe").string(),
                (fs::path(msys2_root) / "ucrt64" / "bin" / "gcc.exe").string(),
                (fs::path(msys2_root) / "mingw64" / "bin" / "g++.exe").string(),
                (fs::path(msys2_root) / "ucrt64" / "bin" / "g++.exe").string()
            };
            for (const auto& candidate : msys2_candidates) {
                if (path_exists(candidate)) return candidate;
            }
        }
    } else if (compiler_name == "clang" || compiler_name == "clang++") {
        string msys2_root = find_msys2_root();
        if (!msys2_root.empty()) {
            vector<string> msys2_candidates = {
                (fs::path(msys2_root) / "clang64" / "bin" / "clang.exe").string(),
                (fs::path(msys2_root) / "clang64" / "bin" / "clang++.exe").string(),
                (fs::path(msys2_root) / "mingw64" / "bin" / "clang.exe").string(),
                (fs::path(msys2_root) / "ucrt64" / "bin" / "clang.exe").string()
            };
            for (const auto& candidate : msys2_candidates) {
                if (path_exists(candidate)) return candidate;
            }
        }
    }
#endif
    return "";
}

CompilerType detect_compiler() {
    string out;
#if defined(PLATFORM_WINDOWS)
    string cc_env = get_env_var("CC");
    string cxx_env = get_env_var("CXX");
    auto detect_from_cmd = [](const string& cmd) -> CompilerType {
        if (cmd.empty()) return CompilerType::UNKNOWN;
        string lower = cmd;
        transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
            return static_cast<char>(tolower(c));
        });
        if (lower.find("clang-cl") != string::npos || lower.find("clang") != string::npos) return CompilerType::CLANG;
        if (lower.find("cl") != string::npos) return CompilerType::MSVC;
        if (lower.find("g++") != string::npos || lower.find("gcc") != string::npos) return CompilerType::GCC;
        return CompilerType::UNKNOWN;
    };

    CompilerType from_cc = detect_from_cmd(cc_env);
    if (from_cc != CompilerType::UNKNOWN) return from_cc;
    CompilerType from_cxx = detect_from_cmd(cxx_env);
    if (from_cxx != CompilerType::UNKNOWN) return from_cxx;

    if (!find_vcvars_bat_path().empty() || !find_compiler_path("cl").empty()) {
        return CompilerType::MSVC;
    }
    if (!find_compiler_path("clang").empty() || !find_compiler_path("clang++").empty()) {
        return CompilerType::CLANG;
    }
    if (!find_compiler_path("gcc").empty() || !find_compiler_path("g++").empty()) {
        return CompilerType::GCC;
    }
#else
    if (run_cmd("xlc++ -qversion", out) == 0) return CompilerType::XLC;
    if (run_cmd("g++ --version", out) == 0) return CompilerType::GCC;
    if (run_cmd("clang++ --version", out) == 0) return CompilerType::CLANG;
#endif
    return CompilerType::UNKNOWN;
}

string replace_all(const string& input, const string& from, const string& to) {
    string result = input;
    if (from.empty()) return result;
    size_t pos = 0;
    while ((pos = result.find(from, pos)) != string::npos) {
        result.replace(pos, from.size(), to);
        pos += to.size();
    }
    return result;
}

bool parse_m4_macro_call(const string& line, string& name, vector<string>& args) {
    string trimmed = trim(line);
    if (trimmed.empty() || trimmed[0] == '#' || trimmed.rfind("dnl", 0) == 0) {
        return false;
    }

    size_t open = trimmed.find('(');
    if (open == string::npos) {
        name = trimmed;
        args.clear();
        return !name.empty();
    }

    name = trim(trimmed.substr(0, open));
    size_t close = string::npos;
    int paren_depth = 0;
    int bracket_depth = 0;
    bool in_single = false;
    bool in_double = false;
    for (size_t i = open + 1; i < trimmed.size(); ++i) {
        char ch = trimmed[i];
        if (ch == '\'' && !in_double) {
            in_single = !in_single;
        } else if (ch == '"' && !in_single) {
            in_double = !in_double;
        } else if (!in_single && !in_double) {
            if (ch == '[') {
                ++bracket_depth;
            } else if (ch == ']') {
                if (bracket_depth > 0) --bracket_depth;
            } else if (ch == '(' && bracket_depth == 0) {
                ++paren_depth;
            } else if (ch == ')' && bracket_depth == 0) {
                if (paren_depth == 0) {
                    close = i;
                    break;
                }
                --paren_depth;
            }
        }
    }

    if (close == string::npos) {
        args.clear();
        return !name.empty();
    }

    string inner = trimmed.substr(open + 1, close - open - 1);
    args = split_m4_args(inner);
    return !name.empty();
}

string expand_m4_template(const string& text, const map<string, string>& substitutions) {
    string result = text;
    for (const auto& [key, value] : substitutions) {
        string token1 = "@" + key + "@";
        string token2 = "$" + string("{") + key + "}";
        result = replace_all(result, token1, value);
        result = replace_all(result, token2, value);
    }
    return result;
}

// ============================================================================
// COMPREHENSIVE HELP SYSTEM
// ============================================================================
void print_main_help() {
    cout << R"(autotool(1)              CrossShell for UNIX Reference Manual               autotool(1)

    NAME
        autotool - native build and Autotools compatibility engine

    SYNOPSIS
        autotool COMMAND [OPTIONS] [ARGUMENTS...]

    DESCRIPTION
        'autotool' is a native, zero-dependency C++ engine combining Makefile
        generation, live compiler probing, libtool-style archive and shared-library
        orchestration, multi-threaded parallel compilation, and Linux-to-Windows
        software porting.

    COMMANDS
        automake
            Generate 'Makefile.in' templates from 'Makefile.am'.

        autoconf
            Parse 'configure.ac', execute compiler probes, and write 'config.h'.

        libtool
            Compile, link, install, and clean native libraries and executables.

        build
            Multi-threaded native compilation engine for Windows, Linux, and AIX.

        port
            Automate Linux Autotools software builds on Windows via MSYS2.

        help
            Display comprehensive manual for specific subcommands.

    OPTIONS
        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        autotool automake -a Makefile.am
            Generate template with missing helpers.

        autotool autoconf -r --prefix="C:\Program Files\MyApp" CC="cl.exe"
            Configure with live compiler probing.

        autotool build -j8 -v
            Run 8 parallel compilation threads with verbose output.

        autotool port --msys2="C:\msys64" --subsystem="MINGW64" C:\src\tarball
            Port Linux autotools package to Windows.

    CrossShell for UNIX                                                    autotool(1)
)";
}

void print_automake_help() {
    cout << R"(autotool-automake(1)     CrossShell for UNIX Reference Manual       autotool-automake(1)

    NAME
        autotool-automake - generate Makefile.in templates

    SYNOPSIS
        autotool automake [OPTIONS] Makefile.am

    DESCRIPTION
        Parses 'Makefile.am' specifications and produces portable 'Makefile.in' files
        configured for MSVC, MinGW, or POSIX make utilities.

    OPTIONS
        -a, --add-missing
            Automatically provision required helper scripts ('install-sh', 'missing',
            'compile', 'depcomp').

        -c, --copy
            Copy missing files instead of symlinking (Windows default).

        -f, --force-missing
            Overwrite existing standard bootstrap scripts.

        -v, --verbose
            List all files processed and directives parsed.

        -h, --help
            Display this reference manual.

    EXAMPLES
        autotool automake -a -v Makefile.am
            Provision missing scripts and generate Makefile.in.

    CrossShell for UNIX                                             autotool-automake(1)
)";
}

void print_autoconf_help() {
    cout << R"(autotool-autoconf(1)     CrossShell for UNIX Reference Manual       autotool-autoconf(1)

    NAME
        autotool-autoconf - process configure.ac and compiler probes

    SYNOPSIS
        autotool autoconf [OPTIONS] configure.ac

    DESCRIPTION
        Parses M4/Autoconf macros in 'configure.ac', executes live compiler probes,
        writes 'config.h' headers, and performs @VAR@ template substitution.

    OPTIONS
        -r, --run
            Execute live compiler checks immediately on Windows.

        -C, --config-cache
            Enable probe caching via 'config.cache'.

        -d, --debug
            Retain temporary 'conftest.c' diagnostic files.

        --prefix=DIR
            Set installation prefix (default: 'C:\Program Files\Package').

        --bindir=DIR
            User executable directory [PREFIX/bin].

        --libdir=DIR
            Object library directory [PREFIX/lib].

        CC=COMPILER
            Specify C compiler (e.g. 'cl.exe' or 'gcc').

        CXX=COMPILER
            Specify C++ compiler (e.g. 'cl.exe' or 'g++').

        CFLAGS=FLAGS
            Pass additional C compiler flags.

        CXXFLAGS=FLAGS
            Pass additional C++ compiler flags.

        LDFLAGS=FLAGS
            Pass linker flags.

        LIBS=LIBRARIES
            Pass default libraries.

        -h, --help
            Display this reference manual.

    EXAMPLES
        autotool autoconf -r --prefix="C:\Libs" CC="cl.exe" CFLAGS="/O2"
            Run live probes and generate headers.

    CrossShell for UNIX                                             autotool-autoconf(1)
)";
}

void print_build_help() {
    cout << R"(autotool-build(1)        CrossShell for UNIX Reference Manual         autotool-build(1)

    NAME
        autotool-build - run parallel incremental native builds

    SYNOPSIS
        autotool build [OPTIONS]

    DESCRIPTION
        High-performance, multi-threaded native build engine. Auto-detects toolchains
        (MSVC, GCC, Clang, XLC) and performs parallel incremental compilation.

    OPTIONS
        -j, --jobs=N
            Number of parallel compilation threads (default: CPU cores).

        -c, --clean
            Clean build artifacts before compiling.

        -v, --verbose
            Print full compilation and link command lines.

        -f, --file=SPEC
            Specify custom build specification file.

        -h, --help
            Display this reference manual.

    EXAMPLES
        autotool build -j8 -v
            Run build with 8 parallel worker threads and verbose output.

    CrossShell for UNIX                                                   autotool-build(1)
)";
}

void print_port_help() {
    cout << R"(autotool-port(1)         CrossShell for UNIX Reference Manual          autotool-port(1)

    NAME
        autotool-port - port GNU Autotools projects through MSYS2

    SYNOPSIS
        autotool port [OPTIONS] PROJECT

    DESCRIPTION
        Automates porting Linux/GNU Autotools projects to Windows using MSYS2 / MinGW-w64.
        Executes build pipelines and dynamically inspects PE headers to bundle DLLs.

    OPTIONS
        --msys2=PATH
            Path to MSYS2 installation (default: 'C:\msys64').

        --subsystem=NAME
            MSYS2 subsystem: MINGW64, CLANG64, UCRT64 (default: 'MINGW64').

        --no-autoreconf
            Skip running 'autoreconf -fiv' before configuration.

        --no-dll-bundle
            Skip dynamic PE header DLL import resolution.

        -v, --verbose
            Print detailed MSYS2 bash commands and outputs.

        -h, --help
            Display this reference manual.

    EXAMPLES
        autotool port --msys2="C:\msys64" --subsystem="MINGW64" C:\src\project
            Port project using MINGW64 subsystem.

    CrossShell for UNIX                                                    autotool-port(1)
)";
}

void print_libtool_help() {
    cout << R"(autotool-libtool(1)      CrossShell for UNIX Reference Manual       autotool-libtool(1)

    NAME
        autotool-libtool - compile, link, install, and clean libraries

    SYNOPSIS
        autotool libtool --mode=MODE [OPTIONS] INPUT...

    DESCRIPTION
        Provides a native libtool-style engine for compile, link, install, clean, and
        finish workflows. It focuses on practical archive/shared-library orchestration
        for MSVC, GCC, Clang, and XLC without requiring shell scripts.

    OPTIONS
        --mode=compile
            Compile a source file into a native object.

        --mode=link
            Link objects/sources into an executable, static archive, or shared library.

        --mode=install
            Copy artifacts into a destination directory or explicit path.

        --mode=clean
            Remove generated objects and libraries.

        --mode=finish
            Validate staged runtime output directory (compatibility mode).

        --tag=CC|CXX
            Select C or C++ driver semantics (default: CXX).

        -o FILE
            Output path for compile/link mode.

        -shared, --shared
            Build a shared library / DLL.

        -static, --static
            Build a static archive / import library set.

        -module, --module
            Alias shared-library style module build.

        -n, --dry-run
            Print the native command without executing it.

        -v, --verbose
            Print resolved command lines and compatibility notes.

        --compiler=CMD
            Override the detected compiler or driver.

        --cflags=FLAGS
            Extra compile flags used by --mode=compile and source promotion in link mode.

        --ldflags=FLAGS
            Extra linker flags for --mode=link.

        --libs=FLAGS
            Extra libraries for --mode=link.

        --install-dir=DIR
            Explicit install destination for --mode=install.

        -rpath DIR
            Accepted for libtool compatibility; stored as runtime/install intent.

        -release NAME
            Accepted for compatibility; currently metadata only.

        -version-info X:Y:Z
            Accepted for compatibility; currently metadata only.

        -avoid-version
            Accepted for compatibility.

        -no-install
            Accepted for compatibility.

        -export-dynamic
            Forwarded to linker flags when meaningful.

        -h, --help
            Display this reference manual.

    EXAMPLES
        autotool libtool --mode=compile --tag=CXX -o build\hello.obj hello.cpp
            Compile source file into object.

        autotool libtool --mode=link -static -o build\libhello.lib build\hello.obj
            Link static library.

        autotool libtool --mode=link -shared -o build\hello.dll hello.cpp
            Link shared library DLL directly from source.

        autotool libtool --mode=install build\hello.dll stage\bin
            Install DLL to target directory.

        autotool libtool --mode=clean build\hello.obj build\libhello.lib
            Clean generated object and library files.

    CrossShell for UNIX                                             autotool-libtool(1)
)";
}

// ============================================================================
// MODULE 1: AUTOMAKE ENGINE
// ============================================================================
namespace AutomakeModule {

struct AutomakeProject {
    map<string, string> variables;
    vector<string> bin_programs;
    map<string, vector<string>> program_sources;
    map<string, vector<string>> program_ldadd;
    vector<string> subdirs;
    vector<string> raw_rules;
};

void run_automake(const vector<string>& args) {
    bool add_missing = false;
    bool verbose = false;
    string input_file = "Makefile.am";

    for (size_t i = 0; i < args.size(); ++i) {
        string arg = args[i];
        if (arg == "-h" || arg == "--help") { print_automake_help(); return; }
        else if (arg == "-a" || arg == "--add-missing") add_missing = true;
        else if (arg == "-v" || arg == "--verbose") verbose = true;
        else if (arg[0] != '-') input_file = arg;
    }

    if (!fs::exists(input_file)) {
        cerr << "automake: error: cannot find input file '" << input_file << "'\n";
        return;
    }

    if (add_missing) {
        vector<string> helpers = {"install-sh", "missing", "compile", "depcomp"};
        for (const auto& h : helpers) {
            if (!fs::exists(h)) {
                ofstream out(h);
                out << "#!/bin/sh\n# Helper stub generated by Autotools Enterprise Suite\nexit 0\n";
                if (verbose) cout << "automake: generated helper script '" << h << "'\n";
            }
        }
    }

    ifstream file(input_file);
    AutomakeProject proj;
    string line;
    while (getline(file, line)) {
        string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        size_t eq = trimmed.find('=');
        if (eq != string::npos) {
            string key = trim(trimmed.substr(0, eq));
            string val = trim(trimmed.substr(eq + 1));
            proj.variables[key] = val;

            if (key == "bin_PROGRAMS") proj.bin_programs = split_words(val);
            else if (key == "SUBDIRS") proj.subdirs = split_words(val);
            else if (key.find("_SOURCES") != string::npos) {
                proj.program_sources[key.substr(0, key.find("_SOURCES"))] = split_words(val);
            } else if (key.find("_LDADD") != string::npos) {
                proj.program_ldadd[key.substr(0, key.find("_LDADD"))] = split_words(val);
            }
        } else {
            proj.raw_rules.push_back(trimmed);
        }
    }

    string out_file = (fs::path(input_file).stem().string() + ".in");
    ofstream out(out_file);
    CompilerType detected_compiler = detect_compiler();
    const bool use_msvc = detected_compiler == CompilerType::MSVC;
    out << "# Makefile.in generated by " << TOOL_NAME << " " << VERSION << "\n\n";
    out << "SHELL = " << (use_msvc ? "cmd.exe" : "/bin/sh") << "\n";
    out << "CC = " << (use_msvc ? "cl.exe" : (detected_compiler == CompilerType::CLANG ? "clang" : "cc")) << "\n";
    out << "CXX = " << (use_msvc ? "cl.exe" : (detected_compiler == CompilerType::CLANG ? "clang++" : "c++")) << "\n";
    out << "CFLAGS = " << (use_msvc ? "/O2" : "-O2") << "\n";
    out << "CXXFLAGS = " << (use_msvc ? "/O2 /EHsc" : "-O2 -fPIC") << "\n";
    out << "EXEEXT = " << (use_msvc ? ".exe" : "") << "\n";
    out << "OBJEXT = " << (use_msvc ? ".obj" : ".o") << "\n\n";

    for (const auto& [k, v] : proj.variables) out << k << " = " << v << "\n";
    out << "\n.PHONY: all clean install\n\nall:";
    for (const auto& p : proj.bin_programs) out << " " << p << "$(EXEEXT)";
    out << "\n\n";

    const string link_flag = use_msvc ? "/Fe$@" : "-o $@";
    const string compile_flag = use_msvc ? "/c $< /Fo$@" : "-c $< -o $@";
    const string clean_prefix = use_msvc ? "\t@if exist " : "\t@rm -f ";
    const string clean_suffix = use_msvc ? " del /F /Q " : "";

    for (const auto& p : proj.bin_programs) {
        out << p << "$(EXEEXT):";
        vector<string> objs;
        if (proj.program_sources.count(p)) {
            for (const auto& s : proj.program_sources[p]) {
                string obj = fs::path(s).stem().string() + "$(OBJEXT)";
                objs.push_back(obj);
                out << " " << obj;
            }
        }
        out << "\n\t$(CXX) $(CXXFLAGS) " << link_flag;
        for (const auto& o : objs) out << " " << o;
        if (proj.program_ldadd.count(p)) {
            for (const auto& ld : proj.program_ldadd[p]) out << " " << ld;
        }
        out << "\n\n";
    }

    out << ".cpp$(OBJEXT):\n\t$(CXX) $(CXXFLAGS) " << compile_flag << "\n\n";
    out << "clean:\n";
    for (const auto& p : proj.bin_programs) {
        if (use_msvc) {
            out << clean_prefix << p << "$(EXEEXT)" << clean_suffix << p << "$(EXEEXT)\n";
        } else {
            out << clean_prefix << p << "$(EXEEXT)\n";
        }
    }

    cout << "automake: successfully created " << out_file << "\n";
}
}

// ============================================================================
// MODULE 2: AUTOCONF ENGINE
// ============================================================================
namespace AutoconfModule {

struct AutoconfProject {
    string package_name = "package";
    string package_version = "1.0";
    bool check_cc = false;
    bool check_cxx = false;
    vector<string> config_headers;
    vector<string> check_headers;
    vector<string> check_funcs;
    vector<string> check_types;
    vector<string> check_sizeof;
    vector<string> check_decls;
    vector<string> check_members;
    vector<pair<string, vector<string>>> search_libs;
    vector<string> aux_dirs;
    vector<string> src_dirs;
    vector<string> subst_files;
    vector<string> config_files;
    vector<string> output_files;
    vector<string> subst_names;
    map<string, string> subst_vars;
    map<string, string> subst_values;
    map<string, string> defines;
};

class ProbeEngine {
    string compiler;
    string cflags;
    ofstream log;

    static bool is_msvc_like(const string& value) {
        string lower = value;
        transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
            return static_cast<char>(tolower(c));
        });
        return lower.find("cl") != string::npos || lower.find("clang-cl") != string::npos;
    }

    string probe_obj_path() const {
        return is_msvc_like(compiler) ? "conftest.obj" : "conftest.o";
    }

    string build_probe_command(const string& obj_path) const {
        string cmd = shell_quote(compiler);
        if (!cflags.empty()) {
            cmd += " " + cflags;
        }
        if (is_msvc_like(compiler)) {
            cmd += " /c conftest.c /Fo" + shell_quote(obj_path);
        } else {
            cmd += " -c conftest.c -o " + shell_quote(obj_path);
        }
        return cmd + " >conftest.out 2>&1";
    }

public:
    ProbeEngine(const string& comp, const string& flags, const string& log_file) : compiler(comp), cflags(flags) {
        log.open(log_file, ios::app);
    }

    bool compile_check(const string& src) {
        ofstream out("conftest.c");
        out << src;
        out.close();

        string obj_path = probe_obj_path();
        string cmd = build_probe_command(obj_path);
        string output;
        int res = run_cmd(cmd, output);

        ifstream err("conftest.out");
        log << "Compiler Output:\n" << err.rdbuf() << "\nExit Code: " << res << "\n";

        fs::remove("conftest.c");
        fs::remove(obj_path);
        fs::remove("conftest.out");
        return (res == 0);
    }

    bool check_header(const string& hdr) {
        return compile_check("#include <" + hdr + ">\nint main() { return 0; }\n");
    }

    bool check_func(const string& func) {
        return compile_check("char " + func + "();\nint main() { return " + func + "(); }\n");
    }
};

void run_autoconf(const vector<string>& args) {
    bool execute_now = false;
    string input_file = "configure.ac";
    string prefix = "C:\\Program Files\\Package";
    string cc = "cl.exe";
    string cflags = "/O2 /nologo";
    string cxx = "cl.exe";
    string cxxflags = "/O2 /nologo /EHsc";
    string ldflags = "";
    string libs = "";
    CompilerType detected = detect_compiler();
    if (detected == CompilerType::GCC) {
        cc = "gcc";
        cflags = "-O2";
        cxx = "g++";
        cxxflags = "-O2";
    } else if (detected == CompilerType::CLANG) {
        cc = "clang";
        cflags = "-O2";
        cxx = "clang++";
        cxxflags = "-O2";
    } else if (detected == CompilerType::XLC) {
        cc = "xlc";
        cflags = "-O2";
        cxx = "xlc++";
        cxxflags = "-O2";
    }

    for (size_t i = 0; i < args.size(); ++i) {
        string arg = args[i];
        if (arg == "-h" || arg == "--help") { print_autoconf_help(); return; }
        else if (arg == "-r" || arg == "--run") execute_now = true;
        else if (arg.rfind("--prefix=", 0) == 0) prefix = arg.substr(9);
        else if (arg.rfind("CC=", 0) == 0) cc = arg.substr(3);
        else if (arg.rfind("CXX=", 0) == 0) cxx = arg.substr(4);
        else if (arg.rfind("CFLAGS=", 0) == 0) cflags = arg.substr(7);
        else if (arg.rfind("CXXFLAGS=", 0) == 0) cxxflags = arg.substr(9);
        else if (arg.rfind("LDFLAGS=", 0) == 0) ldflags = arg.substr(8);
        else if (arg.rfind("LIBS=", 0) == 0) libs = arg.substr(5);
        else if (arg[0] != '-') input_file = arg;
    }

    if (!fs::exists(input_file)) {
        cerr << "autoconf: error: cannot find input file '" << input_file << "'\n";
        return;
    }

    AutoconfProject proj;
    ifstream file(input_file);
    string line;
    while (getline(file, line)) {
        string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        string macro_name;
        vector<string> macro_args;
        if (!parse_m4_macro_call(trimmed, macro_name, macro_args)) {
            continue;
        }

        if (macro_name == "AC_INIT") {
            if (!macro_args.empty()) proj.package_name = unquote_m4(macro_args[0]);
            if (macro_args.size() >= 2) proj.package_version = unquote_m4(macro_args[1]);
        } else if (macro_name == "AC_PROG_CC") {
            proj.check_cc = true;
        } else if (macro_name == "AC_PROG_CXX") {
            proj.check_cxx = true;
        } else if (macro_name == "AC_CHECK_HEADERS") {
            proj.check_headers = macro_args;
        } else if (macro_name == "AC_CHECK_FUNCS") {
            proj.check_funcs = macro_args;
        } else if (macro_name == "AC_CHECK_TYPES") {
            proj.check_types = macro_args;
        } else if (macro_name == "AC_CHECK_SIZEOF") {
            proj.check_sizeof = macro_args;
        } else if (macro_name == "AC_CHECK_DECLS") {
            proj.check_decls = macro_args;
        } else if (macro_name == "AC_CHECK_MEMBERS") {
            proj.check_members = macro_args;
        } else if (macro_name == "AC_SEARCH_LIBS") {
            if (macro_args.size() >= 2) {
                proj.search_libs.push_back({macro_args[0], vector<string>(macro_args.begin() + 1, macro_args.end())});
            }
        } else if (macro_name == "AC_CONFIG_SRCDIR") {
            if (!macro_args.empty()) proj.src_dirs.push_back(unquote_m4(macro_args[0]));
        } else if (macro_name == "AC_CONFIG_AUX_DIR") {
            if (!macro_args.empty()) proj.aux_dirs.push_back(unquote_m4(macro_args[0]));
        } else if (macro_name == "AC_SUBST_FILE") {
            if (!macro_args.empty()) proj.subst_files.push_back(unquote_m4(macro_args[0]));
        } else if (macro_name == "AC_SUBST") {
            if (!macro_args.empty()) {
                string name = unquote_m4(macro_args[0]);
                string value = macro_args.size() >= 2 ? unquote_m4(macro_args[1]) : "";
                proj.subst_names.push_back(name);
                proj.subst_values[name] = value;
            }
        } else if (macro_name == "AC_DEFINE") {
            if (!macro_args.empty()) {
                string name = unquote_m4(macro_args[0]);
                string value = macro_args.size() >= 2 ? unquote_m4(macro_args[1]) : "1";
                proj.defines[name] = value;
            }
        } else if (macro_name == "AC_CONFIG_FILES") {
            for (const auto& arg : macro_args) {
                auto parts = split_words(arg);
                proj.config_files.insert(proj.config_files.end(), parts.begin(), parts.end());
            }
        } else if (macro_name == "AC_OUTPUT") {
            for (const auto& arg : macro_args) {
                auto parts = split_words(arg);
                proj.output_files.insert(proj.output_files.end(), parts.begin(), parts.end());
            }
        } else if (macro_name == "AC_CONFIG_HEADERS") {
            proj.config_headers = macro_args;
        }
    }

    if (execute_now) {
        cout << "configure: configuring " << proj.package_name << " " << proj.package_version << "...\n";
        ProbeEngine tester(cc, cflags, "config.log");

        proj.subst_vars["PACKAGE_NAME"] = proj.package_name;
        proj.subst_vars["PACKAGE_VERSION"] = proj.package_version;
        proj.subst_vars["PACKAGE"] = proj.package_name;
        proj.subst_vars["VERSION"] = proj.package_version;
        proj.subst_vars["prefix"] = prefix;
        proj.subst_vars["CC"] = cc;
        proj.subst_vars["CXX"] = cxx;
        proj.subst_vars["CFLAGS"] = cflags;
        proj.subst_vars["CXXFLAGS"] = cxxflags;
        proj.subst_vars["LDFLAGS"] = ldflags;
        proj.subst_vars["LIBS"] = libs;
        proj.subst_vars["srcdir"] = ".";
        proj.subst_vars["builddir"] = ".";

        for (const auto& name : proj.subst_names) {
            if (proj.subst_vars.count(name)) continue;
            if (proj.subst_values.count(name)) {
                string value = proj.subst_values[name];
                for (const auto& [src, dst] : proj.subst_vars) {
                    value = replace_all(value, "@" + src + "@", dst);
                    value = replace_all(value, "$" + string("{") + src + "}", dst);
                }
                for (const auto& [define_name, define_value] : proj.defines) {
                    value = replace_all(value, "@" + define_name + "@", define_value);
                    value = replace_all(value, "$" + string("{") + define_name + "}", define_value);
                }
                proj.subst_vars[name] = value;
            } else if (name == "PACKAGE_NAME") proj.subst_vars[name] = proj.package_name;
            else if (name == "PACKAGE_VERSION") proj.subst_vars[name] = proj.package_version;
            else if (name == "PACKAGE") proj.subst_vars[name] = proj.package_name;
            else if (name == "VERSION") proj.subst_vars[name] = proj.package_version;
            else if (name == "prefix") proj.subst_vars[name] = prefix;
            else if (name == "CC") proj.subst_vars[name] = cc;
            else if (name == "CXX") proj.subst_vars[name] = cxx;
            else if (name == "CFLAGS") proj.subst_vars[name] = cflags;
            else if (name == "CXXFLAGS") proj.subst_vars[name] = cxxflags;
            else if (name == "LDFLAGS") proj.subst_vars[name] = ldflags;
            else if (name == "LIBS") proj.subst_vars[name] = libs;
            else proj.subst_vars[name] = "";
        }

        for (const auto& h : proj.check_headers) {
            cout << "checking for " << h << "... ";
            bool res = tester.check_header(h);
            cout << (res ? "yes" : "no") << "\n";
            if (res) proj.defines["HAVE_" + to_upper_identifier(h)] = "1";
        }

        for (auto& [name, value] : proj.subst_values) {
            if (value.find("@") != string::npos || value.find("${") != string::npos) {
                string expanded = value;
                for (const auto& [src, dst] : proj.subst_vars) {
                    expanded = replace_all(expanded, "@" + src + "@", dst);
                    expanded = replace_all(expanded, "$" + string("{") + src + "}", dst);
                }
                for (const auto& [define_name, define_value] : proj.defines) {
                    expanded = replace_all(expanded, "@" + define_name + "@", define_value);
                    expanded = replace_all(expanded, "$" + string("{") + define_name + "}", define_value);
                }
                proj.subst_vars[name] = expanded;
            }
        }

        for (const auto& f : proj.check_funcs) {
            cout << "checking for " << f << "... ";
            bool res = tester.check_func(f);
            cout << (res ? "yes" : "no") << "\n";
            if (res) proj.defines["HAVE_" + to_upper_identifier(f)] = "1";
        }

        for (const auto& type_name : proj.check_types) {
            cout << "checking for type " << type_name << "... ";
            bool res = tester.compile_check("typedef " + type_name + " test_type;\nint main() { return 0; }\n");
            cout << (res ? "yes" : "no") << "\n";
            if (res) proj.defines["HAVE_" + to_upper_identifier(type_name)] = "1";
        }

        for (const auto& size_name : proj.check_sizeof) {
            cout << "checking size of " << size_name << "... ";
            bool res = tester.compile_check("#include <stddef.h>\ntypedef " + size_name + " test_type;\nint main() { return sizeof(test_type); }\n");
            cout << (res ? "yes" : "no") << "\n";
            if (res) proj.defines["SIZEOF_" + to_upper_identifier(size_name)] = "1";
        }

        for (const auto& decl_name : proj.check_decls) {
            cout << "checking for declaration " << decl_name << "... ";
            bool res = tester.compile_check("#include <stdio.h>\nint main() { (void)" + decl_name + "; return 0; }\n");
            cout << (res ? "yes" : "no") << "\n";
            if (res) proj.defines["HAVE_DECL_" + to_upper_identifier(decl_name)] = "1";
        }

        for (const auto& member_name : proj.check_members) {
            cout << "checking for member " << member_name << "... ";
            bool res = tester.compile_check("struct test_struct { int field; };\nint main() { return sizeof(((test_struct*)0)->field); }\n");
            cout << (res ? "yes" : "no") << "\n";
            if (res) proj.defines["HAVE_" + to_upper_identifier(member_name)] = "1";
        }

        for (const auto& [symbol, libs] : proj.search_libs) {
            cout << "checking for library for " << symbol << "... ";
            bool res = false;
            for (const auto& lib : libs) {
                if (tester.compile_check("extern int " + symbol + "();\nint main() { return " + symbol + "(); }\n")) {
                    res = true;
                    break;
                }
            }
            cout << (res ? "yes" : "no") << "\n";
            if (res) proj.defines["HAVE_" + to_upper_identifier(symbol)] = "1";
        }

        for (const auto& path : proj.aux_dirs) {
            fs::create_directories(path);
        }
        for (const auto& path : proj.src_dirs) {
            fs::create_directories(path);
        }
        for (const auto& file : proj.subst_files) {
            if (fs::exists(file)) {
                std::ifstream in(file);
                std::ofstream out(file + ".out");
                std::string line;
                while (std::getline(in, line)) {
                    out << expand_m4_template(line, proj.subst_vars) << "\n";
                }
            }
        }

        for (const auto& hdr_file : proj.config_headers.empty() ? vector<string>{"config.h"} : proj.config_headers) {
            ofstream out(hdr_file);
            out << "#ifndef CONFIG_H_INCLUDED\n#define CONFIG_H_INCLUDED\n\n";
            for (const auto& [k, v] : proj.defines) out << "#define " << k << " " << v << "\n";
            out << "\n#endif\n";
            cout << "config.status: creating " << hdr_file << "\n";
        }

        auto emit_templates = [&](const vector<string>& files) {
            for (const auto& cfg : files) {
                string in_path = cfg;
                string out_path = cfg;
                size_t sep = cfg.find(':');
                if (sep != string::npos) {
                    out_path = cfg.substr(0, sep);
                    in_path = cfg.substr(sep + 1);
                }
                if (in_path.empty()) in_path = out_path + ".in";
                fs::path output_path = fs::path(out_path);
                if (output_path.has_parent_path() && !output_path.parent_path().empty()) {
                    fs::create_directories(output_path.parent_path());
                }
                if (fs::exists(in_path)) {
                    ifstream in(in_path);
                    ofstream out(out_path);
                    string l;
                    while (getline(in, l)) {
                        out << expand_m4_template(l, proj.subst_vars) << "\n";
                    }
                    cout << "config.status: creating " << out_path << "\n";
                } else if (fs::exists(out_path)) {
                    cout << "config.status: updating " << out_path << "\n";
                } else {
                    ofstream out(out_path);
                    out << "";
                    cout << "config.status: creating " << out_path << "\n";
                }
            }
        };

        emit_templates(proj.config_files);
        emit_templates(proj.output_files);
    } else {
        ofstream cmd("configure.cmd");
        cmd << "@echo off\n" << TOOL_NAME << ".exe autoconf --run %*\n";
        cout << "autoconf: created native launcher (configure.cmd)\n";
    }
}
}

// ============================================================================
// MODULE 3: CROSS-PLATFORM NATIVE BUILD ENGINE
// ============================================================================
namespace BuildModule {

struct BuildTarget {
    string name = "app";
    TargetType type = TargetType::EXECUTABLE;
    vector<string> sources;
    string output_dir = "bin";
};

void run_native_build(const vector<string>& args) {
    int jobs = thread::hardware_concurrency();
    bool verbose = false;
    bool clean = false;

    for (size_t i = 0; i < args.size(); ++i) {
        string arg = args[i];
        if (arg == "-h" || arg == "--help") { print_build_help(); return; }
        else if (arg == "-v" || arg == "--verbose") verbose = true;
        else if (arg == "-c" || arg == "--clean") clean = true;
        else if (arg.rfind("-j", 0) == 0) {
            size_t eq = arg.find('=');
            if (eq != string::npos) jobs = stoi(arg.substr(eq + 1));
            else if (i + 1 < args.size()) jobs = stoi(args[++i]);
        }
    }

    CompilerType compiler = detect_compiler();
    if (compiler == CompilerType::UNKNOWN) {
        cerr << "build: error: no supported compiler detected.\n";
        return;
    }

    BuildTarget target;
    for (const auto& entry : fs::directory_iterator(".")) {
        if (entry.path().extension() == ".cpp" || entry.path().extension() == ".c") {
            target.sources.push_back(entry.path().string());
        }
    }

    if (target.sources.empty()) {
        cerr << "build: no source files (.cpp/.c) found in current directory.\n";
        return;
    }

    if (clean) {
        cout << "build: Cleaning build output directory '" << target.output_dir << "'...\n";
        fs::remove_all(target.output_dir);
    }

    fs::create_directories(target.output_dir);
    vector<string> objects;
    vector<pair<string, string>> tasks;

    for (const auto& src : target.sources) {
        string obj = (fs::path(target.output_dir) / (fs::path(src).stem().string() + OBJ_EXT)).string();
        objects.push_back(obj);
        if (!fs::exists(obj) || fs::last_write_time(src) > fs::last_write_time(obj)) {
            tasks.push_back({src, obj});
        }
    }

    string out_binary = (fs::path(target.output_dir) / (target.name + EXE_EXT)).string();
    bool needs_link = !fs::exists(out_binary);
    if (!needs_link) {
        for (const auto& obj : objects) {
            if (fs::exists(obj) && fs::exists(out_binary) && fs::last_write_time(obj) > fs::last_write_time(out_binary)) {
                needs_link = true;
                break;
            }
        }
    }

    if (!needs_link && tasks.empty()) {
        cout << "build: Target is up to date.\n";
        return;
    }

    string vcvars_bat = find_vcvars_bat_path();
    if (compiler == CompilerType::MSVC && !vcvars_bat.empty()) {
        string vcvars_error;
        if (!initialize_msvc_environment(vcvars_bat, vcvars_error)) {
            cerr << "build: error: failed to initialize MSVC environment from " << vcvars_bat << ": " << vcvars_error << "\n";
            return;
        }
    }

    cout << "build: Compiling " << tasks.size() << " file(s) using " << jobs << " worker thread(s)...\n";
    std::atomic<size_t> idx(0);
    std::atomic<bool> failed(false);

    auto worker = [&]() {
        while (true) {
            size_t i = idx.fetch_add(1);
            if (i >= tasks.size() || failed.load()) break;

            const auto& [src, obj] = tasks[i];
            string cmd;
            if (compiler == CompilerType::MSVC) {
                cmd = "cl.exe /nologo /EHsc /O2 /c " + shell_quote(src) + " /Fo" + shell_quote(obj);
            } else if (compiler == CompilerType::CLANG) {
                cmd = "clang++ -O2 -c " + shell_quote(src) + " -o " + shell_quote(obj);
            } else {
                cmd = "g++ -O2 -c " + shell_quote(src) + " -o " + shell_quote(obj);
            }

            {
                lock_guard<mutex> lock(global_log_mutex);
                cout << "  [" << (i + 1) << "/" << tasks.size() << "] " << src << "\n";
                if (verbose) cout << "    " << cmd << "\n";
            }

            string out;
            if (run_cmd(cmd, out) != 0) {
                lock_guard<mutex> lock(global_log_mutex);
                cerr << "Compilation Error in " << src << ":\n" << out << "\n";
                failed.store(true);
            }
        }
    };

    vector<thread> threads;
    for (int j = 0; j < jobs; ++j) threads.emplace_back(worker);
    for (auto& t : threads) t.join();

    if (failed.load()) return;

    string link_cmd;
    string object_args;
    for (const auto& obj : objects) {
        object_args += " " + shell_quote(obj);
    }
    if (compiler == CompilerType::MSVC) {
        link_cmd = "cl.exe /nologo /Fe" + shell_quote(out_binary) + object_args;
    } else if (compiler == CompilerType::CLANG) {
        link_cmd = "clang++ -o " + shell_quote(out_binary) + object_args;
    } else {
        link_cmd = "g++ -o " + shell_quote(out_binary) + object_args;
    }

    cout << "build: Linking " << out_binary << "...\n";
    string link_out;
    if (run_cmd(link_cmd, link_out) == 0) {
        cout << "build: Successfully built " << out_binary << "\n";
    } else {
        cerr << "Linker Error:\n" << link_out << "\n";
    }
}
}

// ============================================================================
// MODULE 4: GNU PORTING ENGINE WITH DYNAMIC PE DLL RESOLUTION
// ============================================================================
namespace PortModule {

vector<string> parse_pe_import_dlls(const string& exe_path) {
    vector<string> required_dlls;
#if defined(PLATFORM_WINDOWS)
    HANDLE file = CreateFileA(exe_path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return required_dlls;
    }

    HANDLE mapping = CreateFileMappingA(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!mapping) {
        CloseHandle(file);
        return required_dlls;
    }

    void* view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    if (!view) {
        CloseHandle(mapping);
        CloseHandle(file);
        return required_dlls;
    }

    const auto* dos_header = reinterpret_cast<const IMAGE_DOS_HEADER*>(view);
    if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
        UnmapViewOfFile(view);
        CloseHandle(mapping);
        CloseHandle(file);
        return required_dlls;
    }

    const auto* nt_headers = reinterpret_cast<const IMAGE_NT_HEADERS*>(reinterpret_cast<const BYTE*>(view) + dos_header->e_lfanew);
    if (nt_headers->Signature != IMAGE_NT_SIGNATURE) {
        UnmapViewOfFile(view);
        CloseHandle(mapping);
        CloseHandle(file);
        return required_dlls;
    }

    const auto* import_dir = &nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (import_dir->Size == 0 || import_dir->VirtualAddress == 0) {
        UnmapViewOfFile(view);
        CloseHandle(mapping);
        CloseHandle(file);
        return required_dlls;
    }

    const auto* import_desc = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(reinterpret_cast<const BYTE*>(view) + import_dir->VirtualAddress);
    for (; import_desc->Name != 0; ++import_desc) {
        const char* dll_name = reinterpret_cast<const char*>(reinterpret_cast<const BYTE*>(view) + import_desc->Name);
        if (dll_name) {
            required_dlls.push_back(dll_name);
        }
    }

    UnmapViewOfFile(view);
    CloseHandle(mapping);
    CloseHandle(file);
#endif
    return required_dlls;
}

void resolve_pe_dll_dependencies(const string& msys_root, const string& subsystem, const string& exe_path) {
    cout << "port: Dynamically inspecting PE imports for " << exe_path << "...\n";
    string bin_subdir = "mingw64";
    if (subsystem == "UCRT64") bin_subdir = "ucrt64";
    else if (subsystem == "CLANG64") bin_subdir = "clang64";
    string bin_dir = (fs::path(msys_root) / bin_subdir / "bin").string();
    fs::path target_dir = fs::path(exe_path).parent_path();

    set<string> required_dlls;
    for (const auto& dll : parse_pe_import_dlls(exe_path)) {
        required_dlls.insert(dll);
    }

    for (const auto& dll : required_dlls) {
        fs::path src_dll = fs::path(bin_dir) / dll;
        fs::path dst_dll = target_dir / dll;
        if (fs::exists(src_dll) && !fs::exists(dst_dll)) {
            cout << "  Bundled Dependency: " << dll << " -> " << target_dir.string() << "\n";
            fs::copy_file(src_dll, dst_dll, fs::copy_options::overwrite_existing);
        }
    }
}

void run_gnu_port(const vector<string>& args) {
    string msys_root = find_msys2_root();
    if (msys_root.empty()) msys_root = "C:\\msys64";
    string subsystem = "MINGW64";
    string source_dir = ".";
    bool run_autoreconf = true;
    bool bundle_dlls = true;

    for (size_t i = 0; i < args.size(); ++i) {
        string arg = args[i];
        if (arg == "-h" || arg == "--help") { print_port_help(); return; }
        else if (arg == "--no-autoreconf") run_autoreconf = false;
        else if (arg == "--no-dll-bundle") bundle_dlls = false;
        else if (arg.rfind("--msys2=", 0) == 0) msys_root = arg.substr(8);
        else if (arg.rfind("--subsystem=", 0) == 0) subsystem = arg.substr(12);
        else if (arg[0] != '-') source_dir = arg;
    }

    string bash_exe = (fs::path(msys_root) / "usr" / "bin" / "bash.exe").string();
    if (!fs::exists(bash_exe)) {
        cerr << "port: error: MSYS2 bash not found at " << bash_exe << "\n";
        return;
    }

    string posix_src = win_to_posix_path(source_dir);
    cout << "port: Automated build pipeline starting for " << posix_src << " (" << subsystem << ")...\n";

    std::stringstream pipeline;
    string bash_command = "export MSYSTEM=" + subsystem + " && source /etc/profile && cd " + shell_quote(posix_src) + " && " +
                          (run_autoreconf ? "(test -f configure || autoreconf -fiv) && " : "") +
                          "./configure && make -j4";
    pipeline << shell_quote(bash_exe) << " --login -c " << shell_quote(bash_command);

    string out;
    if (run_cmd(pipeline.str(), out) == 0) {
        cout << "port: Build succeeded.\n";
        if (bundle_dlls) {
            for (const auto& entry : fs::recursive_directory_iterator(source_dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".exe") {
                    resolve_pe_dll_dependencies(msys_root, subsystem, entry.path().string());
                }
            }
        }
    } else {
        cerr << "port: Build failed:\n" << out << "\n";
    }
}
}

// ============================================================================
// MODULE 5: LIBTOOL-STYLE NATIVE LIBRARY ENGINE
// ============================================================================
namespace LibtoolModule {

enum class Mode { NONE, COMPILE, LINK, INSTALL, CLEAN, FINISH };

struct Options {
    Mode mode = Mode::NONE;
    string tag = "CXX";
    string compiler;
    string output;
    string cflags;
    string ldflags;
    string libs;
    string install_dir;
    string runtime_path;
    string release_name;
    string version_info;
    bool verbose = false;
    bool dry_run = false;
    bool build_shared = false;
    bool build_static = false;
    bool build_module = false;
    bool avoid_version = false;
    bool no_install = false;
    vector<string> inputs;
};

string normalize_tag(const string& tag) {
    string upper = tag;
    transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) {
        return static_cast<char>(toupper(c));
    });
    return upper.empty() ? "CXX" : upper;
}

bool is_source_file(const string& path) {
    string ext = fs::path(path).extension().string();
    transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(tolower(c));
    });
    return ext == ".c" || ext == ".cc" || ext == ".cpp" || ext == ".cxx";
}

bool is_object_file(const string& path) {
    string ext = fs::path(path).extension().string();
    transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(tolower(c));
    });
    return ext == ".o" || ext == ".obj" || ext == ".lo";
}

string mode_to_string(Mode mode) {
    switch (mode) {
        case Mode::COMPILE: return "compile";
        case Mode::LINK: return "link";
        case Mode::INSTALL: return "install";
        case Mode::CLEAN: return "clean";
        case Mode::FINISH: return "finish";
        default: return "none";
    }
}

CompilerType compiler_type_from_name(const string& compiler_name) {
    if (compiler_name.empty()) {
        return detect_compiler();
    }

    string lower = compiler_name;
    transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(tolower(c));
    });
    if (lower.find("clang-cl") != string::npos || lower.find("clang++") != string::npos || lower.find("clang") != string::npos) {
        return CompilerType::CLANG;
    }
    if (lower.find("g++") != string::npos || lower.find("gcc") != string::npos) {
        return CompilerType::GCC;
    }
    if (lower.find("xlc") != string::npos) {
        return CompilerType::XLC;
    }
    if (lower.find("cl") != string::npos) {
        return CompilerType::MSVC;
    }
    return detect_compiler();
}

string default_compiler_for_tag(CompilerType compiler, const string& tag) {
    const bool use_cxx = normalize_tag(tag) != "CC";
    switch (compiler) {
        case CompilerType::MSVC: return "cl.exe";
        case CompilerType::CLANG: return use_cxx ? "clang++" : "clang";
        case CompilerType::GCC: return use_cxx ? "g++" : "gcc";
        case CompilerType::XLC: return use_cxx ? "xlc++" : "xlc";
        default: return use_cxx ? "c++" : "cc";
    }
}

bool ensure_toolchain_ready(CompilerType compiler, string& error) {
#if defined(PLATFORM_WINDOWS)
    if (compiler != CompilerType::MSVC) {
        return true;
    }

    if (!find_compiler_path("cl.exe").empty()) {
        return true;
    }

    string vcvars_bat = find_vcvars_bat_path();
    if (vcvars_bat.empty()) {
        error = "unable to locate vcvars64.bat / VsDevCmd.bat";
        return false;
    }

    return initialize_msvc_environment(vcvars_bat, error);
#else
    (void)compiler;
    error.clear();
    return true;
#endif
}

bool run_native_command(const string& cmd, const Options& opts, const string& prefix) {
    if (opts.verbose || opts.dry_run) {
        cout << prefix << cmd << "\n";
    }
    if (opts.dry_run) {
        return true;
    }

    string output;
    int status = run_cmd(cmd, output);
    if (status != 0) {
        cerr << "libtool: command failed (mode=" << mode_to_string(opts.mode) << ")\n";
        if (!output.empty()) {
            cerr << output << "\n";
        }
        return false;
    }

    if (opts.verbose && !output.empty()) {
        cout << output;
        if (output.back() != '\n') {
            cout << "\n";
        }
    }
    return true;
}

string native_object_output(const string& requested_output, const string& source_path) {
    fs::path output_path = requested_output.empty() ? fs::path(source_path).replace_extension(OBJ_EXT) : fs::path(requested_output);
    if (output_path.extension() == ".lo") {
        output_path.replace_extension(OBJ_EXT);
    }
    return output_path.string();
}

bool compile_source(const Options& opts, CompilerType compiler_type, const string& compiler, const string& source_path, const string& object_path) {
    fs::path output_path = fs::path(object_path);
    if (output_path.has_parent_path() && !output_path.parent_path().empty()) {
        fs::create_directories(output_path.parent_path());
    }

    const bool use_cxx = normalize_tag(opts.tag) != "CC";
    string cmd = shell_quote(compiler);
    if (compiler_type == CompilerType::MSVC) {
        cmd += " /nologo";
        if (use_cxx) {
            cmd += " /EHsc";
        }
        if (!opts.cflags.empty()) {
            cmd += " " + opts.cflags;
        }
        cmd += " /c " + shell_quote(source_path) + " /Fo" + shell_quote(object_path);
    } else {
        if (!opts.cflags.empty()) {
            cmd += " " + opts.cflags;
        }
        cmd += " -c " + shell_quote(source_path) + " -o " + shell_quote(object_path);
    }

    return run_native_command(cmd, opts, "libtool: compile: ");
}

bool collect_link_inputs(const Options& opts, CompilerType compiler_type, const string& compiler, vector<string>& objects) {
    fs::path output_parent = opts.output.empty() ? fs::current_path() : fs::path(opts.output).parent_path();
    if (output_parent.empty()) {
        output_parent = fs::current_path();
    }

    for (const auto& input : opts.inputs) {
        if (is_source_file(input)) {
            fs::path generated = output_parent / fs::path(input).filename();
            generated.replace_extension(OBJ_EXT);
            if (!compile_source(opts, compiler_type, compiler, input, generated.string())) {
                return false;
            }
            objects.push_back(generated.string());
        } else {
            objects.push_back(input);
        }
    }
    return true;
}

bool run_compile_mode(const Options& opts) {
    if (opts.inputs.empty()) {
        cerr << "libtool: compile mode requires at least one source file\n";
        return false;
    }
    if (opts.inputs.size() > 1 && !opts.output.empty()) {
        cerr << "libtool: compile mode accepts '-o' only with a single source\n";
        return false;
    }

    CompilerType compiler_type = compiler_type_from_name(opts.compiler);
    string error;
    if (!ensure_toolchain_ready(compiler_type, error)) {
        cerr << "libtool: " << error << "\n";
        return false;
    }

    string compiler = opts.compiler.empty() ? default_compiler_for_tag(compiler_type, opts.tag) : opts.compiler;
    for (size_t i = 0; i < opts.inputs.size(); ++i) {
        const string& source_path = opts.inputs[i];
        if (!is_source_file(source_path)) {
            cerr << "libtool: compile mode expects source files, got '" << source_path << "'\n";
            return false;
        }

        string object_path = native_object_output(i == 0 ? opts.output : "", source_path);
        if (!compile_source(opts, compiler_type, compiler, source_path, object_path)) {
            return false;
        }
    }
    return true;
}

bool run_link_mode(const Options& opts) {
    if (opts.inputs.empty()) {
        cerr << "libtool: link mode requires at least one source, object, or library input\n";
        return false;
    }

    CompilerType compiler_type = compiler_type_from_name(opts.compiler);
    string error;
    if (!ensure_toolchain_ready(compiler_type, error)) {
        cerr << "libtool: " << error << "\n";
        return false;
    }

    string compiler = opts.compiler.empty() ? default_compiler_for_tag(compiler_type, opts.tag) : opts.compiler;
    vector<string> link_inputs;
    if (!collect_link_inputs(opts, compiler_type, compiler, link_inputs)) {
        return false;
    }

    string output = opts.output;
    if (output.empty()) {
        output = opts.build_static ? ("libtool" + LIB_EXT) : (opts.build_shared || opts.build_module ? ("libtool" + DLL_EXT) : ("a" + EXE_EXT));
    }

    fs::path output_path = fs::path(output);
    if (output_path.has_parent_path() && !output_path.parent_path().empty()) {
        fs::create_directories(output_path.parent_path());
    }

    string joined_inputs;
    for (const auto& input : link_inputs) {
        joined_inputs += " " + shell_quote(input);
    }

    string cmd;
    if (opts.build_static) {
#if defined(PLATFORM_WINDOWS)
        cmd = "lib.exe /nologo /OUT:" + shell_quote(output) + joined_inputs;
#else
        cmd = "ar rcs " + shell_quote(output) + joined_inputs;
#endif
    } else if (opts.build_shared || opts.build_module) {
        cmd = shell_quote(compiler);
        if (compiler_type == CompilerType::MSVC) {
            cmd += " /nologo /LD /Fe" + shell_quote(output) + joined_inputs;
        } else {
            cmd += " -shared -o " + shell_quote(output) + joined_inputs;
        }
        if (!opts.ldflags.empty()) {
            cmd += " " + opts.ldflags;
        }
        if (!opts.libs.empty()) {
            cmd += " " + opts.libs;
        }
    } else {
        cmd = shell_quote(compiler);
        if (compiler_type == CompilerType::MSVC) {
            cmd += " /nologo /Fe" + shell_quote(output) + joined_inputs;
        } else {
            cmd += " -o " + shell_quote(output) + joined_inputs;
        }
        if (!opts.ldflags.empty()) {
            cmd += " " + opts.ldflags;
        }
        if (!opts.libs.empty()) {
            cmd += " " + opts.libs;
        }
    }

    return run_native_command(cmd, opts, "libtool: link: ");
}

bool run_install_mode(const Options& opts) {
    if (opts.inputs.empty()) {
        cerr << "libtool: install mode requires at least one source artifact\n";
        return false;
    }

    fs::path destination = opts.install_dir.empty() ? fs::path(opts.inputs.back()) : fs::path(opts.install_dir);
    const bool explicit_destination = !opts.install_dir.empty();
    size_t source_count = explicit_destination ? opts.inputs.size() : opts.inputs.size() - 1;
    if (!explicit_destination && opts.inputs.size() < 2) {
        cerr << "libtool: install mode requires a source and destination\n";
        return false;
    }

    const bool dest_is_directory = explicit_destination || source_count > 1 || destination.extension().empty() || fs::is_directory(destination);
    if (dest_is_directory) {
        fs::create_directories(destination);
    } else if (destination.has_parent_path() && !destination.parent_path().empty()) {
        fs::create_directories(destination.parent_path());
    }

    for (size_t i = 0; i < source_count; ++i) {
        fs::path src = opts.inputs[i];
        if (!fs::exists(src)) {
            cerr << "libtool: install source missing: " << src.string() << "\n";
            return false;
        }

        fs::path target = dest_is_directory ? (destination / src.filename()) : destination;
        if (opts.verbose || opts.dry_run) {
            cout << "libtool: install: " << src.string() << " -> " << target.string() << "\n";
        }
        if (!opts.dry_run) {
            fs::copy_file(src, target, fs::copy_options::overwrite_existing);
        }
    }
    return true;
}

bool run_clean_mode(const Options& opts) {
    if (opts.inputs.empty()) {
        cerr << "libtool: clean mode requires one or more files\n";
        return false;
    }

    for (const auto& input : opts.inputs) {
        if (opts.verbose || opts.dry_run) {
            cout << "libtool: clean: " << input << "\n";
        }
        if (!opts.dry_run) {
            std::error_code ec;
            fs::remove(input, ec);
            if (ec && fs::exists(input)) {
                cerr << "libtool: failed to remove '" << input << "': " << ec.message() << "\n";
                return false;
            }
        }
    }
    return true;
}

bool run_finish_mode(const Options& opts) {
    string path = !opts.install_dir.empty() ? opts.install_dir : (!opts.runtime_path.empty() ? opts.runtime_path : (opts.inputs.empty() ? "" : opts.inputs.front()));
    if (path.empty()) {
        cerr << "libtool: finish mode requires a staging or runtime directory\n";
        return false;
    }

    if (opts.verbose || opts.dry_run) {
        cout << "libtool: finish: runtime directory '" << path << "'\n";
    }
    if (!opts.dry_run && !fs::exists(path)) {
        cerr << "libtool: finish target does not exist: " << path << "\n";
        return false;
    }
    return true;
}

int run_libtool(const vector<string>& args) {
    Options opts;

    for (size_t i = 0; i < args.size(); ++i) {
        const string& arg = args[i];
        if (arg == "-h" || arg == "--help") {
            print_libtool_help();
            return EXIT_SUCCESS;
        } else if (starts_with(arg, "--mode=")) {
            string mode = arg.substr(7);
            if (mode == "compile") opts.mode = Mode::COMPILE;
            else if (mode == "link") opts.mode = Mode::LINK;
            else if (mode == "install") opts.mode = Mode::INSTALL;
            else if (mode == "clean") opts.mode = Mode::CLEAN;
            else if (mode == "finish") opts.mode = Mode::FINISH;
            else {
                cerr << "libtool: unsupported mode '" << mode << "'\n";
                return EXIT_FAILURE;
            }
        } else if (starts_with(arg, "--tag=")) {
            opts.tag = normalize_tag(arg.substr(6));
        } else if (arg == "-o") {
            if (i + 1 >= args.size()) {
                cerr << "libtool: missing argument for -o\n";
                return EXIT_FAILURE;
            }
            opts.output = args[++i];
        } else if (starts_with(arg, "--output=")) {
            opts.output = arg.substr(9);
        } else if (arg == "-shared" || arg == "--shared") {
            opts.build_shared = true;
        } else if (arg == "-static" || arg == "--static") {
            opts.build_static = true;
        } else if (arg == "-module" || arg == "--module") {
            opts.build_module = true;
            opts.build_shared = true;
        } else if (arg == "-n" || arg == "--dry-run") {
            opts.dry_run = true;
        } else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        } else if (arg == "--silent") {
            opts.verbose = false;
        } else if (starts_with(arg, "--compiler=")) {
            opts.compiler = arg.substr(11);
        } else if (starts_with(arg, "--cflags=")) {
            opts.cflags = arg.substr(9);
        } else if (starts_with(arg, "--ldflags=")) {
            opts.ldflags = arg.substr(10);
        } else if (starts_with(arg, "--libs=")) {
            opts.libs = arg.substr(7);
        } else if (starts_with(arg, "--install-dir=")) {
            opts.install_dir = arg.substr(14);
        } else if (arg == "-rpath") {
            if (i + 1 >= args.size()) {
                cerr << "libtool: missing argument for -rpath\n";
                return EXIT_FAILURE;
            }
            opts.runtime_path = args[++i];
        } else if (starts_with(arg, "--rpath=")) {
            opts.runtime_path = arg.substr(8);
        } else if (arg == "-release") {
            if (i + 1 >= args.size()) {
                cerr << "libtool: missing argument for -release\n";
                return EXIT_FAILURE;
            }
            opts.release_name = args[++i];
        } else if (starts_with(arg, "-version-info=")) {
            opts.version_info = arg.substr(14);
        } else if (arg == "-version-info") {
            if (i + 1 >= args.size()) {
                cerr << "libtool: missing argument for -version-info\n";
                return EXIT_FAILURE;
            }
            opts.version_info = args[++i];
        } else if (arg == "-avoid-version") {
            opts.avoid_version = true;
        } else if (arg == "-no-install") {
            opts.no_install = true;
        } else if (arg == "-export-dynamic") {
            if (!opts.ldflags.empty()) {
                opts.ldflags += " ";
            }
            opts.ldflags += "-export-dynamic";
        } else if (!arg.empty() && arg[0] == '-' && opts.mode == Mode::COMPILE) {
            if (!opts.cflags.empty()) {
                opts.cflags += " ";
            }
            opts.cflags += arg;
        } else if (!arg.empty() && arg[0] == '-' && opts.mode == Mode::LINK) {
            if (starts_with(arg, "-l") || starts_with(arg, "-L") || starts_with(arg, "-Wl,")) {
                if (!opts.libs.empty()) {
                    opts.libs += " ";
                }
                opts.libs += arg;
            } else {
                if (!opts.ldflags.empty()) {
                    opts.ldflags += " ";
                }
                opts.ldflags += arg;
            }
        } else {
            opts.inputs.push_back(arg);
        }
    }

    if (opts.mode == Mode::NONE) {
        cerr << "libtool: missing required --mode switch\n";
        print_libtool_help();
        return EXIT_FAILURE;
    }

    if (opts.verbose) {
        if (!opts.release_name.empty()) {
            cout << "libtool: compatibility note: -release " << opts.release_name << " accepted as metadata\n";
        }
        if (!opts.version_info.empty()) {
            cout << "libtool: compatibility note: -version-info " << opts.version_info << " accepted as metadata\n";
        }
        if (opts.avoid_version) {
            cout << "libtool: compatibility note: -avoid-version accepted\n";
        }
        if (opts.no_install) {
            cout << "libtool: compatibility note: -no-install accepted\n";
        }
        if (!opts.runtime_path.empty()) {
            cout << "libtool: compatibility note: runtime/install path set to '" << opts.runtime_path << "'\n";
        }
    }

    bool ok = false;
    switch (opts.mode) {
        case Mode::COMPILE:
            ok = run_compile_mode(opts);
            break;
        case Mode::LINK:
            ok = run_link_mode(opts);
            break;
        case Mode::INSTALL:
            ok = run_install_mode(opts);
            break;
        case Mode::CLEAN:
            ok = run_clean_mode(opts);
            break;
        case Mode::FINISH:
            ok = run_finish_mode(opts);
            break;
        default:
            ok = false;
            break;
    }

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
}

// ============================================================================
// 6. APPLICATION CONTROLLER & DISPATCHER
// ============================================================================

class AutotoolApp {
public:
    static int run(int argc, char* argv[]) {
        if (argc < 2) {
            print_main_help();
            return EXIT_SUCCESS;
        }

        string subcommand = argv[1];
        vector<string> sub_args;
        for (int i = 2; i < argc; ++i) sub_args.push_back(argv[i]);

        if (subcommand == "-h" || subcommand == "--help") {
            print_main_help();
        } else if (subcommand == "help") {
            if (sub_args.empty()) print_main_help();
            else if (sub_args[0] == "automake") print_automake_help();
            else if (sub_args[0] == "autoconf") print_autoconf_help();
            else if (sub_args[0] == "libtool") print_libtool_help();
            else if (sub_args[0] == "build") print_build_help();
            else if (sub_args[0] == "port") print_port_help();
            else print_main_help();
        } else if (subcommand == "automake") {
            AutomakeModule::run_automake(sub_args);
        } else if (subcommand == "autoconf") {
            AutoconfModule::run_autoconf(sub_args);
        } else if (subcommand == "libtool") {
            return LibtoolModule::run_libtool(sub_args);
        } else if (subcommand == "build") {
            BuildModule::run_native_build(sub_args);
        } else if (subcommand == "port") {
            PortModule::run_gnu_port(sub_args);
        } else {
            cerr << TOOL_NAME << ": error: unknown command '" << subcommand << "'\n";
            print_main_help();
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }
};

int main(int argc, char* argv[]) {
    return AutotoolApp::run(argc, argv);
}