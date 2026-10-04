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
 *
 * CrossShell for UNIX
 */

#define _CRT_SECURE_NO_WARNINGS
#include "utils.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>

#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#if !defined(_WIN32) && !defined(_WIN64)
#include <sys/wait.h>
#endif

namespace fs = std::filesystem;
using std::string;
using std::vector;
using std::map;

std::mutex global_log_mutex;

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
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
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
    std::replace(str.begin(), str.end(), '\\', '/');
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
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
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
