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

/*
Standardized Section Index
---------------------------
1. Platform includes, shared constants, and forward declarations
2. UTF translation and Win32 utility helpers
3. Shell lexer/parser, AST model, and command execution core
4. Built-in applet dispatch and BusyBox command implementations
5. Process launch, pipeline/redirection, and environment handling
6. CLI entry point, mode selection, and top-level command routing
*/

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <lmcons.h>
#include <io.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <functional>
#include <filesystem>
#include <algorithm>
#include <atomic>
#include <iomanip>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <thread>
#include <system_error>
#include <memory>
#include <regex>
#include <mutex>
#include <initializer_list>
#include <TlHelp32.h>
#include <wincrypt.h>

#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Crypt32.lib")

namespace fs = std::filesystem;

constexpr size_t MAX_AST_RECURSION_DEPTH = 128;
constexpr size_t MAX_HISTORY_ENTRIES = 4096;
constexpr size_t MAX_HISTORY_LINE_BYTES = 16 * 1024;
constexpr size_t MAX_HISTORY_FILE_BYTES = 4 * 1024 * 1024;
constexpr size_t MAX_SCRIPT_BYTES = 16 * 1024 * 1024;
constexpr size_t MAX_COMMAND_SUBSTITUTION_BYTES = 8 * 1024 * 1024;
constexpr size_t MAX_GLOB_MATCHES = 100000;
constexpr size_t MAX_DIRECTORY_SCAN_ENTRIES = 1000000;

class ShellExitSignal : public std::exception {
public:
    explicit ShellExitSignal(int code) : code_(code) {}

    int code() const noexcept { return code_; }

private:
    int code_;
};

// Forward Declarations
struct ShellContext;
class ASTNode;

// ============================================================================
// UNICODE (UTF-8 <-> UTF-16) TRANSLATION LAYER
// ============================================================================

std::wstring utf8_to_utf16(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), NULL, 0);
    if (size_needed <= 0) return L"";
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), &wstrTo[0], size_needed);
    return wstrTo;
}

std::string utf16_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), NULL, 0, NULL, NULL);
    if (size_needed <= 0) return "";
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

std::string path_to_utf8(const fs::path& path) {
    auto value = path.u8string();
    return std::string(reinterpret_cast<const char*>(value.data()), value.size());
}

// ============================================================================
// WIN32 UTILITIES & SELF-EXE RESOLUTION
// ============================================================================

std::string get_self_exe_path() {
    wchar_t buffer[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, buffer, MAX_PATH);
    return (len > 0 && len < MAX_PATH) ? utf16_to_utf8(std::wstring(buffer, len)) : "";
}

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

std::string normalize_path(std::string path) {
    if (path.length() >= 3 && path[0] == '/' && std::isalpha(static_cast<unsigned char>(path[1])) && path[2] == '/') {
        std::string drive = "";
        drive += static_cast<char>(std::toupper(path[1]));
        drive += ":\\";
        path = drive + path.substr(3);
    }
    for (char &c : path) { if (c == '/') c = '\\'; }
    return path;
}

std::string escape_win32_arg(const std::string& arg) {
    if (arg.empty()) return "\"\"";
    bool needs_quotes = false;
    for (char c : arg) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '"') {
            needs_quotes = true; break;
        }
    }
    if (!needs_quotes) return arg;

    std::string out = "\"";
    size_t backslashes = 0;
    for (char c : arg) {
        if (c == '\\') {
            backslashes++;
        } else if (c == '"') {
            out.append(backslashes * 2 + 1, '\\');
            out.push_back('"');
            backslashes = 0;
        } else {
            out.append(backslashes, '\\');
            backslashes = 0;
            out.push_back(c);
        }
    }
    out.append(backslashes * 2, '\\');
    out.push_back('"');
    return out;
}

std::string escape_cmd_metachars(const std::string& arg) {
    std::string out;
    for (char c : arg) {
        if (std::string("()<>^&|%!\"").find(c) != std::string::npos) {
            out.push_back('^');
        }
        out.push_back(c);
    }
    return out;
}

std::string resolve_executable_path(const std::string& cmd) {
    std::error_code ec;
    fs::path p(utf8_to_utf16(normalize_path(cmd)));

    wchar_t pathext_buf[32767];
    DWORD pathext_len = GetEnvironmentVariableW(L"PATHEXT", pathext_buf, 32767);
    std::vector<std::wstring> extensions = { L".exe", L".cmd", L".bat", L".com" };
    if (pathext_len > 0) {
        std::wstring pathext_wstr(pathext_buf, pathext_len);
        std::transform(pathext_wstr.begin(), pathext_wstr.end(), pathext_wstr.begin(), ::towlower);
        std::wstringstream ss(pathext_wstr);
        std::wstring ext;
        extensions.clear();
        while (std::getline(ss, ext, L';')) {
            if (!ext.empty()) extensions.push_back(ext);
        }
    }

    auto try_extensions = [&](const fs::path& base) -> std::string {
        if (fs::exists(base, ec) && !fs::is_directory(base, ec)) return utf16_to_utf8(fs::absolute(base, ec).wstring());
        for (const auto& ext : extensions) {
            fs::path candidate = base.wstring() + ext;
            if (fs::exists(candidate, ec) && !fs::is_directory(candidate, ec)) {
                return utf16_to_utf8(fs::absolute(candidate, ec).wstring());
            }
        }
        return "";
    };

    if (p.is_absolute() || cmd.find('\\') != std::string::npos || cmd.find('/') != std::string::npos) {
        std::string res = try_extensions(p);
        if (!res.empty()) return res;
    }

    std::string current_res = try_extensions(fs::current_path() / p);
    if (!current_res.empty()) return current_res;

    wchar_t path_env[32767];
    DWORD len = GetEnvironmentVariableW(L"PATH", path_env, 32767);
    if (len > 0) {
        std::wstring path_wstr(path_env, len);
        std::wstringstream ss(path_wstr);
        std::wstring dir;
        while (std::getline(ss, dir, L';')) {
            if (dir.empty()) continue;
            std::string res = try_extensions(fs::path(dir) / p);
            if (!res.empty()) return res;
        }
    }

    return "";
}

// ============================================================================
// THREAD-SAFE SHELL CONTEXT
// ============================================================================

struct JobInfo {
    int id;
    DWORD pid;
    HANDLE hProcess;
    std::string command;
    bool is_running;
};

struct ShellContext {
    std::map<std::string, std::string> variables;
    std::set<std::string> environment_touched;
    bool pipeline_stage = false;
    int last_return_code = 0;
    DWORD timeout_ms = 60000;
    std::mutex ctx_mutex;
    bool audit_logging_enabled = false;
    std::vector<std::string> history;
    std::vector<JobInfo> jobs;

    std::wstring get_history_file_path() {
        wchar_t profile[32767];
        DWORD len = GetEnvironmentVariableW(L"USERPROFILE", profile, 32767);
        if (len > 0) {
            return std::wstring(profile, len) + L"\\.busybox_history";
        }
        return L".busybox_history";
    }

    void load_history() {
        std::lock_guard<std::mutex> lock(ctx_mutex);
        std::ifstream file(get_history_file_path());
        if (!file) return;

        file.seekg(0, std::ios::end);
        std::streamoff file_size = file.tellg();
        if (file_size < 0 || static_cast<size_t>(file_size) > MAX_HISTORY_FILE_BYTES) {
            return;
        }
        file.seekg(0, std::ios::beg);

        std::string line;
        while (std::getline(file, line)) {
            if (line.size() > MAX_HISTORY_LINE_BYTES) {
                continue;
            }
            if (!line.empty()) {
                history.push_back(line);
                if (history.size() > MAX_HISTORY_ENTRIES) {
                    history.erase(history.begin());
                }
            }
        }
    }

    void save_history() {
        std::lock_guard<std::mutex> lock(ctx_mutex);
        std::ofstream file(get_history_file_path(), std::ios::trunc);
        if (!file) return;
        for (const auto& h : history) {
            file << h << "\n";
        }
    }

    void add_history(const std::string& cmd) {
        if (cmd.empty()) return;
        std::lock_guard<std::mutex> lock(ctx_mutex);
        if (history.empty() || history.back() != cmd) {
            history.push_back(cmd);
            bool rewrite = false;
            if (history.size() > MAX_HISTORY_ENTRIES) {
                history.erase(history.begin());
                rewrite = true;
            }
            if (rewrite) {
                std::ofstream file(get_history_file_path(), std::ios::trunc);
                if (file) {
                    for (const auto& entry : history) file << entry << "\n";
                }
            } else {
                std::ofstream file(get_history_file_path(), std::ios::app);
                if (file) file << cmd << "\n";
            }
        }
    }

    explicit ShellContext(bool load_history_file = true) {
        LPWCH env_strings = GetEnvironmentStringsW();
        if (env_strings) {
            LPWCH var = env_strings;
            while (*var) {
                std::wstring wentry(var);
                std::string entry = utf16_to_utf8(wentry);
                size_t eq = entry.find('=');
                if (eq != std::string::npos && eq > 0) {
                    variables[entry.substr(0, eq)] = entry.substr(eq + 1);
                }
                var += wentry.length() + 1;
            }
            FreeEnvironmentStringsW(env_strings);
        }
        variables["KSH_VERSION"] = "@(#)MIRBSD KSH 2026/07/24";
        variables["SH_VERSION"] = "BusyBox v17.0-PROD";
        if (load_history_file) {
            load_history();
        }
    }

    void set_var(const std::string& name, const std::string& val) {
        std::lock_guard<std::mutex> lock(ctx_mutex);
        variables[name] = val;
        environment_touched.insert(name);
        SetEnvironmentVariableW(utf8_to_utf16(name).c_str(), utf8_to_utf16(val).c_str());
    }

    void unset_var(const std::string& name) {
        std::lock_guard<std::mutex> lock(ctx_mutex);
        variables.erase(name);
        environment_touched.insert(name);
        SetEnvironmentVariableW(utf8_to_utf16(name).c_str(), NULL);
    }

    std::string get_var(const std::string& name) {
        std::lock_guard<std::mutex> lock(ctx_mutex);
        auto it = variables.find(name);
        return (it != variables.end()) ? it->second : "";
    }

    void audit_log(const std::string& action, const std::string& detail) {
        if (!audit_logging_enabled) return;
        std::lock_guard<std::mutex> lock(ctx_mutex);
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm tm_buf;
        localtime_s(&tm_buf, &now);
        std::clog << "[AUDIT " << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") 
                  << " PID:" << GetCurrentProcessId() << "] " 
                  << action << ": " << detail << "\n";
    }
};

bool is_valid_assignment_name(const std::string& name) {
    if (name.empty() || !(std::isalpha(static_cast<unsigned char>(name[0])) || name[0] == '_')) {
        return false;
    }
    for (size_t i = 1; i < name.size(); ++i) {
        unsigned char ch = static_cast<unsigned char>(name[i]);
        if (!(std::isalnum(ch) || ch == '_')) {
            return false;
        }
    }
    return true;
}

void restore_nested_environment(const ShellContext& parent, ShellContext& nested) {
    for (const std::string& name : nested.environment_touched) {
        auto parent_value = parent.variables.find(name);
        if (parent_value != parent.variables.end()) {
            SetEnvironmentVariableW(
                utf8_to_utf16(name).c_str(),
                utf8_to_utf16(parent_value->second).c_str());
        } else {
            SetEnvironmentVariableW(utf8_to_utf16(name).c_str(), nullptr);
        }
    }
}

class ScopedWorkingDirectory {
public:
    ScopedWorkingDirectory() {
        std::error_code ec;
        original_ = fs::current_path(ec);
        active_ = !ec;
    }

    ~ScopedWorkingDirectory() {
        if (active_) {
            std::error_code ec;
            fs::current_path(original_, ec);
        }
    }

private:
    fs::path original_;
    bool active_ = false;
};

// ============================================================================
// RAII HANDLE WRAPPER, JOB OBJECT & SIGNAL MANAGEMENT
// ============================================================================

class ScopedHandle {
    HANDLE h_ = INVALID_HANDLE_VALUE;
public:
    explicit ScopedHandle(HANDLE h = INVALID_HANDLE_VALUE) : h_(h) {}
    ~ScopedHandle() { close(); }
    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;
    ScopedHandle(ScopedHandle&& other) noexcept : h_(other.h_) { other.h_ = INVALID_HANDLE_VALUE; }
    ScopedHandle& operator=(ScopedHandle&& other) noexcept {
        if (this != &other) { close(); h_ = other.h_; other.h_ = INVALID_HANDLE_VALUE; }
        return *this;
    }
    [[nodiscard]] HANDLE get() const { return h_; }
    HANDLE release() { HANDLE tmp = h_; h_ = INVALID_HANDLE_VALUE; return tmp; }
    void close() {
        if (h_ != INVALID_HANDLE_VALUE && h_ != NULL) { CloseHandle(h_); h_ = INVALID_HANDLE_VALUE; }
    }
    [[nodiscard]] bool is_valid() const { return h_ != INVALID_HANDLE_VALUE && h_ != NULL; }
};

static HANDLE g_hJobObject = NULL;
static std::mutex g_proc_mutex;
static std::set<HANDLE> g_active_processes;

void register_active_process(HANDLE h) {
    std::lock_guard<std::mutex> lock(g_proc_mutex);
    g_active_processes.insert(h);
}

void unregister_active_process(HANDLE h) {
    std::lock_guard<std::mutex> lock(g_proc_mutex);
    g_active_processes.erase(h);
}

void init_job_object() {
    g_hJobObject = CreateJobObjectW(NULL, NULL);
    if (g_hJobObject) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK;
        SetInformationJobObject(g_hJobObject, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
    }
}

void init_vt100_and_binary_mode() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE && hOut != NULL) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
}

BOOL WINAPI console_signal_handler(DWORD dwCtrlType) {
    if (dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_BREAK_EVENT) {
        std::lock_guard<std::mutex> lock(g_proc_mutex);
        for (HANDLE h : g_active_processes) {
            TerminateProcess(h, 130);
        }
        std::cout << "\n";
        return TRUE;
    }
    return FALSE;
}

using AppletFunc = std::function<int(const std::vector<std::string>&, ShellContext&)>;
extern std::map<std::string, AppletFunc> applets;

// ============================================================================
// LEXER, TOKENIZER & CHARACTER-LEVEL GLOB EXPANSION ENGINE
// ============================================================================

enum class LexTokenType { Word, Pipe, And, Or, Semi, Ampersand, RedirectIn, RedirectOut, RedirectAppend, LParen, RParen, Newline };

struct Token {
    LexTokenType type;
    std::string value;
};

std::vector<Token> tokenize(const std::string& line) {
    std::vector<Token> tokens;
    std::string current;
    bool in_single = false, in_double = false;
    int cmd_sub_depth = 0;
    bool in_backtick = false;
    int brace_depth = 0;

    for (size_t i = 0; i < line.length(); ++i) {
        char c = line[i];
        if (c == '\'' && !in_double) { in_single = !in_single; current += c; continue; }
        if (c == '"' && !in_single) { in_double = !in_double; current += c; continue; }

        if (c == '\\' && !in_single) {
            current += c;
            if (i + 1 < line.length()) { current += line[++i]; }
            continue;
        }

        if (!in_single && !in_double) {
            if (c == '$' && i + 1 < line.length() && line[i + 1] == '(') {
                cmd_sub_depth++;
                current += "$(";
                i++;
                continue;
            }
            if (c == ')' && cmd_sub_depth > 0) {
                cmd_sub_depth--;
                current += ")";
                continue;
            }
            if (c == '$' && i + 1 < line.length() && line[i + 1] == '{') {
                brace_depth++;
                current += "${";
                i++;
                continue;
            }
            if (c == '}' && brace_depth > 0) {
                brace_depth--;
                current += "}";
                continue;
            }
            if (c == '`') {
                in_backtick = !in_backtick;
                current += "`";
                continue;
            }
        }

        bool inside_sub = (in_single || in_double || cmd_sub_depth > 0 || in_backtick || brace_depth > 0);

        if (!inside_sub) {
            if (i + 1 < line.length()) {
                std::string op2 = line.substr(i, 2);
                if (op2 == "&&" || op2 == "||" || op2 == ">>") {
                    if (!current.empty()) { tokens.push_back({LexTokenType::Word, current}); current.clear(); }
                    if (op2 == "&&") tokens.push_back({LexTokenType::And, "&&"});
                    else if (op2 == "||") tokens.push_back({LexTokenType::Or, "||"});
                    else tokens.push_back({LexTokenType::RedirectAppend, ">>"});
                    i++; continue;
                }
            }
            if (c == '|' || c == '>' || c == '<' || c == ';' || c == '&' || c == '(' || c == ')' || c == '\n') {
                if (!current.empty()) { tokens.push_back({LexTokenType::Word, current}); current.clear(); }
                if (c == '|') tokens.push_back({LexTokenType::Pipe, "|"});
                else if (c == '>') tokens.push_back({LexTokenType::RedirectOut, ">"});
                else if (c == '<') tokens.push_back({LexTokenType::RedirectIn, "<"});
                else if (c == ';') tokens.push_back({LexTokenType::Semi, ";"});
                else if (c == '&') tokens.push_back({LexTokenType::Ampersand, "&"});
                else if (c == '(') tokens.push_back({LexTokenType::LParen, "("});
                else if (c == ')') tokens.push_back({LexTokenType::RParen, ")"});
                else if (c == '\n') tokens.push_back({LexTokenType::Newline, "\n"});
                continue;
            }
            if (std::isspace(static_cast<unsigned char>(c))) {
                if (!current.empty()) { tokens.push_back({LexTokenType::Word, current}); current.clear(); }
                continue;
            }
        }
        current += c;
    }
    if (!current.empty()) tokens.push_back({LexTokenType::Word, current});
    return tokens;
}

std::string capture_cmd_output(const std::string& cmd, ShellContext& ctx);

struct ArgChar {
    char c;
    bool is_quoted;
};

using ArgString = std::vector<ArgChar>;

std::string strip_pattern(const std::string& val, std::string pattern, bool is_prefix, bool longest) {
    if (pattern.empty()) return val;
    std::string regex_pattern;
    for (char c : pattern) {
        if (c == '*') regex_pattern += ".*";
        else if (c == '?') regex_pattern += ".";
        else if (std::string(".+^$()[]{}|\\").find(c) != std::string::npos) regex_pattern += "\\" + std::string(1, c);
        else regex_pattern += c;
    }
    if (is_prefix) {
        regex_pattern = "^(" + regex_pattern + ")";
    } else {
        regex_pattern = "(" + regex_pattern + ")$";
    }
    
    std::regex re;
    try {
        re = std::regex(regex_pattern, std::regex_constants::ECMAScript);
    } catch (...) {
        return val;
    }
    
    if (is_prefix) {
        if (longest) {
            for (size_t len = val.length(); len > 0; --len) {
                std::string sub = val.substr(0, len);
                if (std::regex_match(sub, re)) {
                    return val.substr(len);
                }
            }
        } else {
            for (size_t len = 1; len <= val.length(); ++len) {
                std::string sub = val.substr(0, len);
                if (std::regex_match(sub, re)) {
                    return val.substr(len);
                }
            }
        }
    } else {
        if (longest) {
            for (size_t len = val.length(); len > 0; --len) {
                std::string sub = val.substr(val.length() - len);
                if (std::regex_match(sub, re)) {
                    return val.substr(0, val.length() - len);
                }
            }
        } else {
            for (size_t len = 1; len <= val.length(); ++len) {
                std::string sub = val.substr(val.length() - len);
                if (std::regex_match(sub, re)) {
                    return val.substr(0, val.length() - len);
                }
            }
        }
    }
    return val;
}

ArgString expand_arg_variables(const std::string& input, ShellContext& ctx) {
    ArgString result;
    size_t i = 0, len = input.length();
    bool in_double = false;

    auto append_str = [&](const std::string& s, bool quoted) {
        for (char ch : s) result.push_back({ch, quoted});
    };

    while (i < len) {
        if (input[i] == '\'' && !in_double) {
            i++;
            while (i < len && input[i] != '\'') {
                result.push_back({input[i++], true});
            }
            if (i < len) i++;
            continue;
        }

        if (input[i] == '"') {
            in_double = !in_double;
            i++; continue;
        }

        if (input[i] == '\\' && i + 1 < len) {
            if (in_double) {
                char next_c = input[i + 1];
                if (next_c == '"' || next_c == '\\' || next_c == '$') {
                    result.push_back({next_c, true}); i += 2; continue;
                }
            } else {
                result.push_back({input[i + 1], false}); i += 2; continue;
            }
        }

        if (i + 1 < len && input[i] == '$' && input[i + 1] == '(') {
            size_t depth = 1, start = i + 2, pos = start;
            while (pos < len && depth > 0) {
                if (input[pos] == '(') depth++;
                else if (input[pos] == ')') depth--;
                if (depth > 0) pos++;
            }
            if (depth == 0) {
                append_str(capture_cmd_output(input.substr(start, pos - start), ctx), in_double);
                i = pos + 1; continue;
            }
        }

        if (input[i] == '`') {
            size_t start = i + 1, pos = start;
            while (pos < len && input[pos] != '`') {
                if (input[pos] == '\\' && pos + 1 < len) pos += 2;
                else pos++;
            }
            if (pos < len && input[pos] == '`') {
                std::string inner_cmd;
                for (size_t k = start; k < pos; ++k) {
                    if (input[k] == '\\' && k + 1 < pos && (input[k + 1] == '`' || input[k + 1] == '\\' || input[k + 1] == '$')) {
                        inner_cmd += input[k + 1];
                        k++;
                    } else {
                        inner_cmd += input[k];
                    }
                }
                append_str(capture_cmd_output(inner_cmd, ctx), in_double);
                i = pos + 1;
                continue;
            }
        }

        if (input[i] == '$') {
            i++;
            if (i >= len) { result.push_back({'$', in_double}); break; }

            if (input[i] == '{') {
                size_t close_pos = input.find('}', i);
                if (close_pos != std::string::npos) {
                    std::string var_expr = input.substr(i + 1, close_pos - i - 1);
                    i = close_pos + 1;
                     size_t cd = var_expr.find(":-");
                     size_t ce = var_expr.find(":=");
                     size_t cp = var_expr.find(":+");
                     size_t colon = var_expr.find(':');
                     bool is_slicing = false;
                     if (colon != std::string::npos && colon + 1 < var_expr.length() && 
                         var_expr[colon + 1] != '-' && var_expr[colon + 1] != '=' && var_expr[colon + 1] != '+') {
                         is_slicing = true;
                     }

                     if (is_slicing) {
                         std::string var_name = var_expr.substr(0, colon);
                         std::string slice_expr = var_expr.substr(colon + 1);
                         size_t colon2 = slice_expr.find(':');
                         int offset = 0;
                         int length = -1;
                         try {
                             if (colon2 != std::string::npos) {
                                 offset = std::stoi(slice_expr.substr(0, colon2));
                                 length = std::stoi(slice_expr.substr(colon2 + 1));
                             } else {
                                 offset = std::stoi(slice_expr);
                             }
                         } catch (...) {
                             offset = 0;
                             length = -1;
                         }

                         std::string val = ctx.get_var(var_name);
                         int val_len = static_cast<int>(val.length());
                         if (offset < 0) {
                             offset = val_len + offset;
                         }
                         if (offset < 0) offset = 0;
                         if (offset > val_len) offset = val_len;

                         std::string sliced;
                         if (length == -1) {
                             sliced = val.substr(offset);
                         } else {
                             if (length < 0) {
                                 length = (val_len + length) - offset;
                             }
                             if (length < 0) length = 0;
                             sliced = val.substr(offset, length);
                         }
                         append_str(sliced, in_double);
                     } else if (cd != std::string::npos) {
                         std::string val = ctx.get_var(var_expr.substr(0, cd));
                         append_str(val.empty() ? var_expr.substr(cd + 2) : val, in_double);
                     } else if (ce != std::string::npos) {
                         std::string var_name = var_expr.substr(0, ce);
                         std::string val = ctx.get_var(var_name);
                         if (val.empty()) { val = var_expr.substr(ce + 2); ctx.set_var(var_name, val); }
                         append_str(val, in_double);
                     } else if (cp != std::string::npos) {
                         std::string val = ctx.get_var(var_expr.substr(0, cp));
                         if (!val.empty()) append_str(var_expr.substr(cp + 2), in_double);
                     } else if (!var_expr.empty() && var_expr[0] == '#') {
                         append_str(std::to_string(ctx.get_var(var_expr.substr(1)).length()), in_double);
                     } else if (var_expr.find('#') != std::string::npos && var_expr.find('#') > 0) {
                         size_t hash_pos = var_expr.find('#');
                         bool longest = (hash_pos + 1 < var_expr.length() && var_expr[hash_pos + 1] == '#');
                         std::string var_name = var_expr.substr(0, hash_pos);
                         std::string pattern = var_expr.substr(hash_pos + (longest ? 2 : 1));
                         std::string val = ctx.get_var(var_name);
                         append_str(strip_pattern(val, pattern, true, longest), in_double);
                     } else if (var_expr.find('%') != std::string::npos && var_expr.find('%') > 0) {
                         size_t percent_pos = var_expr.find('%');
                         bool longest = (percent_pos + 1 < var_expr.length() && var_expr[percent_pos + 1] == '%');
                         std::string var_name = var_expr.substr(0, percent_pos);
                         std::string pattern = var_expr.substr(percent_pos + (longest ? 2 : 1));
                         std::string val = ctx.get_var(var_name);
                         append_str(strip_pattern(val, pattern, false, longest), in_double);
                     } else {
                         append_str(ctx.get_var(var_expr), in_double);
                     }
                    continue;
                }
            } else if (input[i] == '?') { append_str(std::to_string(ctx.last_return_code), in_double); i++; continue; }
            else if (input[i] == '$') { append_str(std::to_string(GetCurrentProcessId()), in_double); i++; continue; }
            else if (input[i] == '0') {
                std::string v0 = ctx.get_var("0");
                append_str(v0.empty() ? "busybox" : v0, in_double);
                i++;
                continue;
            }
            else if (input[i] == '#') {
                append_str(ctx.get_var("#"), in_double);
                i++;
                continue;
            }
            else if (std::isalnum(static_cast<unsigned char>(input[i])) || input[i] == '_') {
                size_t start = i;
                while (i < len && (std::isalnum(static_cast<unsigned char>(input[i])) || input[i] == '_')) i++;
                append_str(ctx.get_var(input.substr(start, i - start)), in_double);
                continue;
            }
        }
        result.push_back({input[i++], in_double});
    }
    return result;
}

std::string argstring_to_string(const ArgString& arg) {
    std::string s;
    for (const auto& ac : arg) s += ac.c;
    return s;
}

std::vector<std::string> expand_globs(const std::vector<ArgString>& args) {
    std::vector<std::string> result;
    for (const auto& arg : args) {
        bool has_unquoted_wildcard = false;
        for (const auto& ac : arg) {
            if (!ac.is_quoted && (ac.c == '*' || ac.c == '?')) {
                has_unquoted_wildcard = true; break;
            }
        }

        if (!has_unquoted_wildcard) {
            result.push_back(argstring_to_string(arg));
            continue;
        }

        std::string raw_path = argstring_to_string(arg);
        bool is_recursive = (raw_path.find("**") != std::string::npos);

        fs::path base_dir = ".";
        size_t double_star_pos = raw_path.find("**");
        if (double_star_pos != std::string::npos) {
            size_t last_slash = raw_path.substr(0, double_star_pos).find_last_of("/\\");
            if (last_slash != std::string::npos) {
                base_dir = raw_path.substr(0, last_slash);
                if (base_dir.empty()) base_dir = "/";
            }
        } else {
            fs::path p(utf8_to_utf16(normalize_path(raw_path)));
            base_dir = p.has_parent_path() ? p.parent_path() : fs::path(".");
        }

        std::string regex_pattern;
        for (size_t k = 0; k < arg.size(); ++k) {
            if (!arg[k].is_quoted && arg[k].c == '*') {
                if (k + 1 < arg.size() && !arg[k+1].is_quoted && arg[k+1].c == '*') {
                    if (k + 2 < arg.size() && (arg[k+2].c == '/' || arg[k+2].c == '\\')) {
                        regex_pattern += "(?:.*[/\\\\])?";
                        k += 2;
                    } else {
                        regex_pattern += ".*";
                        k++;
                    }
                } else {
                    regex_pattern += "[^/\\\\]*";
                }
            } else if (!arg[k].is_quoted && arg[k].c == '?') {
                regex_pattern += "[^/\\\\]";
            } else if (std::string(".+^$()[]{}|\\").find(arg[k].c) != std::string::npos) {
                regex_pattern += "\\" + std::string(1, arg[k].c);
            } else if (arg[k].c == '/' || arg[k].c == '\\') {
                regex_pattern += "[/\\\\]";
            } else {
                regex_pattern += arg[k].c;
            }
        }

        std::regex re;
        try {
            re = std::regex(regex_pattern, std::regex_constants::ECMAScript | std::regex_constants::icase | std::regex_constants::optimize);
        } catch (const std::regex_error& e) {
            std::cerr << "busybox: glob regex error: " << e.what() << " for pattern: " << regex_pattern << "\n";
            result.push_back(raw_path);
            continue;
        }

        std::error_code ec;
        std::vector<std::string> matches;
        size_t scanned_entries = 0;
        if (fs::exists(base_dir, ec) && fs::is_directory(base_dir, ec)) {
            if (is_recursive) {
                for (const auto& entry : fs::recursive_directory_iterator(base_dir, ec)) {
                    if (++scanned_entries > MAX_DIRECTORY_SCAN_ENTRIES) break;
                    std::string fname = path_to_utf8(entry.path().filename());
                    if (fname == "." || fname == "..") continue;
                    std::string path_str = path_to_utf8(entry.path());
                    std::string rel_path = path_str;
                    if (rel_path.length() >= 2 && rel_path[0] == '.' && (rel_path[1] == '/' || rel_path[1] == '\\')) {
                        rel_path = rel_path.substr(2);
                    }
                    if (std::regex_match(path_str, re) || std::regex_match(rel_path, re)) {
                        matches.push_back(path_str);
                        if (matches.size() >= MAX_GLOB_MATCHES) break;
                    }
                }
            } else {
                for (const auto& entry : fs::directory_iterator(base_dir, ec)) {
                    if (++scanned_entries > MAX_DIRECTORY_SCAN_ENTRIES) break;
                    std::string fname = path_to_utf8(entry.path().filename());
                    if (fname == "." || fname == "..") continue;
                    std::string path_str = path_to_utf8(entry.path());
                    std::string rel_path = path_str;
                    if (rel_path.length() >= 2 && rel_path[0] == '.' && (rel_path[1] == '/' || rel_path[1] == '\\')) {
                        rel_path = rel_path.substr(2);
                    }
                    if (std::regex_match(fname, re) || std::regex_match(path_str, re) || std::regex_match(rel_path, re)) {
                        matches.push_back(path_str);
                        if (matches.size() >= MAX_GLOB_MATCHES) break;
                    }
                }
            }
        }

        if (!matches.empty()) {
            std::sort(matches.begin(), matches.end());
            for (const auto& m : matches) result.push_back(m);
        } else {
            result.push_back(raw_path);
        }
    }
    return result;
}

// ============================================================================
// ABSTRACT SYNTAX TREE (AST) & SECURE WIN32 EXECUTION ENGINE
// ============================================================================

class ASTNode {
public:
    std::string redirect_in;
    std::string redirect_out;
    bool append_out = false;
    bool is_background = false;

    virtual ~ASTNode() = default;
    virtual int execute_impl(ShellContext& ctx, HANDLE hIn, HANDLE hOut, size_t depth) = 0;

    int execute(ShellContext& ctx, HANDLE hIn, HANDLE hOut, size_t depth = 0) {
        if (depth > MAX_AST_RECURSION_DEPTH) {
            std::cerr << "busybox: fatal: AST recursion depth limit exceeded\n";
            return 1;
        }

        HANDLE effective_in = hIn ? hIn : GetStdHandle(STD_INPUT_HANDLE);
        HANDLE effective_out = hOut ? hOut : GetStdHandle(STD_OUTPUT_HANDLE);
        ScopedHandle hFileIn, hFileOut;

        if (!redirect_in.empty()) {
            std::string exp_in = argstring_to_string(expand_arg_variables(redirect_in, ctx));
            HANDLE h = CreateFileW(utf8_to_utf16(normalize_path(exp_in)).c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (h == INVALID_HANDLE_VALUE) { std::cerr << "busybox: cannot open " << exp_in << "\n"; return 1; }
            hFileIn = ScopedHandle(h); effective_in = hFileIn.get();
        }
        if (!redirect_out.empty()) {
            std::string exp_out = argstring_to_string(expand_arg_variables(redirect_out, ctx));
            DWORD creation = append_out ? OPEN_ALWAYS : CREATE_ALWAYS;
            HANDLE h = CreateFileW(utf8_to_utf16(normalize_path(exp_out)).c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, creation, FILE_ATTRIBUTE_NORMAL, NULL);
            if (h == INVALID_HANDLE_VALUE) { std::cerr << "busybox: cannot open " << exp_out << "\n"; return 1; }
            hFileOut = ScopedHandle(h);
            if (append_out) SetFilePointer(hFileOut.get(), 0, NULL, FILE_END);
            effective_out = hFileOut.get();
        }

        return execute_impl(ctx, effective_in, effective_out, depth);
    }
};

class CommandNode : public ASTNode {
public:
    std::vector<std::string> args;

    int execute_impl(ShellContext& ctx, HANDLE hIn, HANDLE hOut, size_t depth) override {
        if (args.empty()) return 0;

        size_t raw_command_index = 0;
        while (raw_command_index < args.size()) {
            size_t eq = args[raw_command_index].find('=');
            std::string name = eq == std::string::npos ? "" : args[raw_command_index].substr(0, eq);
            if (eq == std::string::npos || !is_valid_assignment_name(name)) {
                break;
            }
            std::string value = argstring_to_string(
                expand_arg_variables(args[raw_command_index].substr(eq + 1), ctx));
            ctx.set_var(name, value);
            raw_command_index++;
        }
        if (raw_command_index == args.size()) return 0;

        std::vector<ArgString> expanded_arg_objs;
        for (size_t i = raw_command_index; i < args.size(); ++i) {
            expanded_arg_objs.push_back(expand_arg_variables(args[i], ctx));
        }
        std::vector<std::string> expanded_args = expand_globs(expanded_arg_objs);

        if (expanded_args.empty() || expanded_args[0].empty()) return 0;

        if (expanded_args[0] == "cd") {
            if (expanded_args.size() > 1) {
                std::error_code ec;
                fs::path target(utf8_to_utf16(normalize_path(expanded_args[1])));
                if (ctx.pipeline_stage) {
                    if (!fs::is_directory(target, ec)) {
                        std::cerr << "cd: " << expanded_args[1] << ": No such directory\n";
                        return 1;
                    }
                    return 0;
                }
                fs::current_path(target, ec);
                if (ec) { std::cerr << "cd: " << expanded_args[1] << ": No such directory\n"; return 1; }
            }
            return 0;
        }
        if (expanded_args[0] == "exit") {
            int exit_code = 0;
            if (expanded_args.size() > 1) {
                try {
                    exit_code = std::stoi(expanded_args[1]);
                } catch (...) {
                    exit_code = 2;
                }
            }
            throw ShellExitSignal(exit_code);
        }

        std::string cmd = to_lower(expanded_args[0]);
        bool is_applet = (applets.count(cmd) > 0);
        bool needs_process_isolation = (hIn != GetStdHandle(STD_INPUT_HANDLE) || hOut != GetStdHandle(STD_OUTPUT_HANDLE) || !redirect_in.empty() || !redirect_out.empty() || is_background);

        if (is_applet && !needs_process_isolation) {
            try { return applets[cmd](expanded_args, ctx); }
            catch (const std::exception& e) { std::cerr << "busybox: applet exception: " << e.what() << "\n"; return 1; }
        }

        std::string resolved_exe;
        std::string cmdline;
        bool is_batch_file = false;

        if (is_applet) {
            resolved_exe = get_self_exe_path();
            cmdline = escape_win32_arg(resolved_exe);
            for (const auto& arg : expanded_args) cmdline += " " + escape_win32_arg(arg);
        } else {
            resolved_exe = resolve_executable_path(expanded_args[0]);
            if (!resolved_exe.empty()) {
                std::string ext = to_lower(fs::path(resolved_exe).extension().string());
                if (ext == ".bat" || ext == ".cmd") is_batch_file = true;
            }

            if (is_batch_file) {
                cmdline = "cmd.exe /c " + escape_win32_arg(resolved_exe);
                for (size_t i = 1; i < expanded_args.size(); ++i) {
                    cmdline += " " + escape_cmd_metachars(escape_win32_arg(expanded_args[i]));
                }
            } else {
                if (!resolved_exe.empty()) {
                    cmdline = escape_win32_arg(resolved_exe);
                } else {
                    cmdline = escape_win32_arg(expanded_args[0]);
                }
                for (size_t i = 1; i < expanded_args.size(); ++i) {
                    cmdline += " " + escape_win32_arg(expanded_args[i]);
                }
            }
        }

        std::wstring resolved_exe_w = utf8_to_utf16(resolved_exe);
        std::wstring cmdline_w = utf8_to_utf16(cmdline);

        ctx.audit_log("EXEC", (resolved_exe.empty() ? expanded_args[0] : resolved_exe));

        std::set<HANDLE> raw_handles;
        if (hIn && hIn != INVALID_HANDLE_VALUE) raw_handles.insert(hIn);
        if (hOut && hOut != INVALID_HANDLE_VALUE) raw_handles.insert(hOut);
        HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
        if (hErr && hErr != INVALID_HANDLE_VALUE) raw_handles.insert(hErr);

        std::vector<HANDLE> valid_handles;
        for (HANDLE h : raw_handles) {
            SetHandleInformation(h, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
            valid_handles.push_back(h);
        }

        SIZE_T cbAttributeListSize = 0;
        InitializeProcThreadAttributeList(NULL, 1, 0, &cbAttributeListSize);
        std::vector<BYTE> attributeListBuffer(cbAttributeListSize);
        PPROC_THREAD_ATTRIBUTE_LIST pAttributeList = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(attributeListBuffer.data());
        InitializeProcThreadAttributeList(pAttributeList, 1, 0, &cbAttributeListSize);

        if (!valid_handles.empty()) {
            UpdateProcThreadAttribute(pAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, 
                                      valid_handles.data(), valid_handles.size() * sizeof(HANDLE), NULL, NULL);
        }

        STARTUPINFOEXW siex = { 0 };
        siex.StartupInfo.cb = sizeof(STARTUPINFOEXW);
        siex.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        siex.StartupInfo.hStdInput = hIn;
        siex.StartupInfo.hStdOutput = hOut;
        siex.StartupInfo.hStdError = hErr;
        siex.lpAttributeList = pAttributeList;

        PROCESS_INFORMATION pi = { 0 };
        std::vector<wchar_t> cmd_buf(cmdline_w.begin(), cmdline_w.end()); cmd_buf.push_back(L'\0');

        LPCWSTR lpAppName = (!resolved_exe_w.empty() && !is_batch_file) ? resolved_exe_w.c_str() : NULL;

        BOOL created = CreateProcessW(lpAppName, cmd_buf.data(), NULL, NULL, TRUE, 
                                      EXTENDED_STARTUPINFO_PRESENT | CREATE_SUSPENDED, NULL, NULL, 
                                      &siex.StartupInfo, &pi);

        DeleteProcThreadAttributeList(pAttributeList);

        if (created) {
            ScopedHandle hProc(pi.hProcess), hThread(pi.hThread);
            if (g_hJobObject) AssignProcessToJobObject(g_hJobObject, hProc.get());

            if (is_background) {
                HANDLE hThreadRelease = hThread.release();
                HANDLE hProcRelease = hProc.release();

                std::lock_guard<std::mutex> lock(ctx.ctx_mutex);
                int job_id = static_cast<int>(ctx.jobs.size() + 1);

                std::string full_cmd;
                for (const auto& arg : expanded_args) {
                    if (!full_cmd.empty()) full_cmd += " ";
                    full_cmd += arg;
                }

                ctx.jobs.push_back({job_id, pi.dwProcessId, hProcRelease, full_cmd, true});
                ResumeThread(hThreadRelease);
                CloseHandle(hThreadRelease);

                std::cout << "[" << job_id << "] " << pi.dwProcessId << "\n";
                return 0;
            }

            register_active_process(hProc.get());
            ResumeThread(hThread.get());

            DWORD wait_res = WaitForSingleObject(hProc.get(), ctx.timeout_ms);
            unregister_active_process(hProc.get());

            if (wait_res == WAIT_TIMEOUT) {
                TerminateProcess(hProc.get(), 124);
                std::cerr << "busybox: execution timed out after " << ctx.timeout_ms << "ms\n";
                return 124;
            }
            DWORD dwCode = 0; GetExitCodeProcess(hProc.get(), &dwCode);
            return static_cast<int>(dwCode);
        }

        std::cerr << "busybox: " << expanded_args[0] << ": command not found or execution failed\n";
        return 127;
    }
};

class PipelineNode : public ASTNode {
public:
    std::vector<std::shared_ptr<ASTNode>> stages;

    int execute_impl(ShellContext& ctx, HANDLE hIn, HANDLE hOut, size_t depth) override {
        if (stages.empty()) return 0;
        if (stages.size() == 1) return stages[0]->execute(ctx, hIn, hOut, depth + 1);

        size_t n = stages.size();
        std::vector<ScopedHandle> hReads(n - 1), hWrites(n - 1);
        SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

        for (size_t i = 0; i < n - 1; ++i) {
            HANDLE r, w;
            if (!CreatePipe(&r, &w, &sa, 0)) return 1;
            hReads[i] = ScopedHandle(r);
            hWrites[i] = ScopedHandle(w);
        }

        std::vector<int> exit_codes(n, 0);
        std::vector<std::thread> stage_threads;

        for (size_t i = 0; i < n; ++i) {
            HANDLE stage_in = (i == 0) ? hIn : hReads[i - 1].get();
            HANDLE stage_out = (i == n - 1) ? hOut : hWrites[i].get();

            stage_threads.emplace_back([this, i, &ctx, stage_in, stage_out, depth, &exit_codes, &hReads, &hWrites, n]() {
                ShellContext stage_ctx(false);
                {
                    std::lock_guard<std::mutex> lock(ctx.ctx_mutex);
                    stage_ctx.variables = ctx.variables;
                    stage_ctx.last_return_code = ctx.last_return_code;
                    stage_ctx.timeout_ms = ctx.timeout_ms;
                    stage_ctx.audit_logging_enabled = ctx.audit_logging_enabled;
                    stage_ctx.pipeline_stage = true;
                }
                try {
                    exit_codes[i] = stages[i]->execute(stage_ctx, stage_in, stage_out, depth + 1);
                } catch (const ShellExitSignal& signal) {
                    exit_codes[i] = signal.code();
                }
                restore_nested_environment(ctx, stage_ctx);
                
                if (i < n - 1) hWrites[i].close();
                if (i > 0) hReads[i - 1].close();
            });
        }

        for (auto& th : stage_threads) {
            if (th.joinable()) th.join();
        }

        return exit_codes.back();
    }
};

class ChainNode : public ASTNode {
public:
    std::shared_ptr<ASTNode> left;
    std::shared_ptr<ASTNode> right;
    LexTokenType op;

    int execute_impl(ShellContext& ctx, HANDLE hIn, HANDLE hOut, size_t depth) override {
        int res = left->execute(ctx, hIn, hOut, depth + 1);
        ctx.last_return_code = res;
        if (this->op == LexTokenType::And && res != 0) return res;
        if (this->op == LexTokenType::Or && res == 0) return res;
        if (right) return right->execute(ctx, hIn, hOut, depth + 1);
        return res;
    }
};

class IfNode : public ASTNode {
public:
    std::shared_ptr<ASTNode> cond;
    std::shared_ptr<ASTNode> then_body;
    std::shared_ptr<ASTNode> else_body;

    int execute_impl(ShellContext& ctx, HANDLE hIn, HANDLE hOut, size_t depth) override {
        int res = cond->execute(ctx, hIn, hOut, depth + 1);
        if (res == 0) return then_body ? then_body->execute(ctx, hIn, hOut, depth + 1) : 0;
        else return else_body ? else_body->execute(ctx, hIn, hOut, depth + 1) : 0;
    }
};

class WhileNode : public ASTNode {
public:
    std::shared_ptr<ASTNode> cond;
    std::shared_ptr<ASTNode> body;

    int execute_impl(ShellContext& ctx, HANDLE hIn, HANDLE hOut, size_t depth) override {
        int last_code = 0;
        while (cond && cond->execute(ctx, hIn, hOut, depth + 1) == 0) {
            if (body) last_code = body->execute(ctx, hIn, hOut, depth + 1);
        }
        return last_code;
    }
};

class ForNode : public ASTNode {
public:
    std::string var_name;
    std::vector<std::string> raw_values;
    std::shared_ptr<ASTNode> body;

    int execute_impl(ShellContext& ctx, HANDLE hIn, HANDLE hOut, size_t depth) override {
        if (!body) return 0;

        std::vector<ArgString> expanded_objs;
        for (const auto& val : raw_values) {
            expanded_objs.push_back(expand_arg_variables(val, ctx));
        }
        std::vector<std::string> expanded_values = expand_globs(expanded_objs);

        int last_code = 0;
        if (expanded_values.empty()) {
            ctx.set_var(var_name, "");
            return body->execute(ctx, hIn, hOut, depth + 1);
        }

        for (const auto& value : expanded_values) {
            ctx.set_var(var_name, value);
            last_code = body->execute(ctx, hIn, hOut, depth + 1);
        }
        return last_code;
    }
};

class SubshellNode : public ASTNode {
public:
    std::shared_ptr<ASTNode> child;

    int execute_impl(ShellContext& ctx, HANDLE hIn, HANDLE hOut, size_t depth) override {
        if (!child) return 0;
        ShellContext sub_ctx(false);
        {
            std::lock_guard<std::mutex> lock(ctx.ctx_mutex);
            sub_ctx.variables = ctx.variables;
            sub_ctx.last_return_code = ctx.last_return_code;
            sub_ctx.timeout_ms = ctx.timeout_ms;
            sub_ctx.audit_logging_enabled = ctx.audit_logging_enabled;
        }
        ScopedWorkingDirectory cwd_guard;
        int res = 0;
        try {
            res = child->execute(sub_ctx, hIn, hOut, depth + 1);
        } catch (const ShellExitSignal& signal) {
            res = signal.code();
        } catch (...) {
            restore_nested_environment(ctx, sub_ctx);
            throw;
        }
        restore_nested_environment(ctx, sub_ctx);
        ctx.last_return_code = res;
        return res;
    }
};

class Parser {
    std::vector<Token> tokens_;
    size_t pos_ = 0;

    Token peek() const { return pos_ < tokens_.size() ? tokens_[pos_] : Token{LexTokenType::Newline, ""}; }
    Token get() { return pos_ < tokens_.size() ? tokens_[pos_++] : Token{LexTokenType::Newline, ""}; }

public:
    explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

    bool is_block_stop_word(const std::string& word) const {
        static const std::set<std::string> stop_words = {"then", "else", "elif", "fi", "do", "done", "in", "esac"};
        return !word.empty() && stop_words.count(to_lower(word)) > 0;
    }

    bool should_stop(const std::initializer_list<std::string>& stop_words) const {
        if (peek().type != LexTokenType::Word) return false;
        std::string word = to_lower(peek().value);
        for (const auto& stop : stop_words) {
            if (word == stop) return true;
        }
        return false;
    }

    std::shared_ptr<ASTNode> parse_command(std::initializer_list<std::string> stop_words = {}) {
        std::shared_ptr<ASTNode> node;

        if (peek().type == LexTokenType::Word && should_stop(stop_words)) {
            return nullptr;
        }

        if (peek().type == LexTokenType::LParen) {
            get();
            auto sub_node = std::make_shared<SubshellNode>();
            sub_node->child = parse_chain();
            if (peek().type == LexTokenType::RParen) {
                get();
            }
            node = sub_node;
        } else if (peek().type == LexTokenType::Word && peek().value == "if") {
            get();
            auto if_node = std::make_shared<IfNode>();
            if_node->cond = parse_chain({"then", "else", "elif", "fi"});
            if (peek().type == LexTokenType::Word && to_lower(peek().value) == "then") get();
            if_node->then_body = parse_chain({"else", "elif", "fi"});
            if (peek().type == LexTokenType::Word && to_lower(peek().value) == "else") {
                get(); if_node->else_body = parse_chain({"fi"});
            }
            if (peek().type == LexTokenType::Word && to_lower(peek().value) == "fi") get();
            node = if_node;
        } else if (peek().type == LexTokenType::Word && peek().value == "while") {
            get();
            auto while_node = std::make_shared<WhileNode>();
            while_node->cond = parse_chain({"do", "done"});
            if (peek().type == LexTokenType::Word && to_lower(peek().value) == "do") get();
            while_node->body = parse_chain({"done"});
            if (peek().type == LexTokenType::Word && to_lower(peek().value) == "done") get();
            node = while_node;
        } else if (peek().type == LexTokenType::Word && peek().value == "for") {
            get();
            auto for_node = std::make_shared<ForNode>();
            if (peek().type == LexTokenType::Word) {
                for_node->var_name = get().value;
            }
            if (peek().type == LexTokenType::Word && to_lower(peek().value) == "in") {
                get();
                while (pos_ < tokens_.size()) {
                    Token t = peek();
                    if (t.type == LexTokenType::Word && !is_block_stop_word(t.value)) {
                        for_node->raw_values.push_back(get().value);
                    } else {
                        break;
                    }
                }
            }
            if (peek().type == LexTokenType::Word && to_lower(peek().value) == "do") get();
            for_node->body = parse_chain({"done"});
            if (peek().type == LexTokenType::Word && to_lower(peek().value) == "done") get();
            node = for_node;
        } else {
            auto cmd = std::make_shared<CommandNode>();
            while (pos_ < tokens_.size()) {
                Token t = peek();
                if (t.type == LexTokenType::Word) { cmd->args.push_back(get().value); }
                else if (t.type == LexTokenType::RedirectOut) {
                    get(); if (peek().type == LexTokenType::Word) cmd->redirect_out = get().value; cmd->append_out = false;
                }
                else if (t.type == LexTokenType::RedirectAppend) {
                    get(); if (peek().type == LexTokenType::Word) cmd->redirect_out = get().value; cmd->append_out = true;
                }
                else if (t.type == LexTokenType::RedirectIn) {
                    get(); if (peek().type == LexTokenType::Word) cmd->redirect_in = get().value;
                }
                else break;
            }
            node = cmd;
        }

        while (pos_ < tokens_.size()) {
            Token t = peek();
            if (t.type == LexTokenType::RedirectOut) {
                get(); if (peek().type == LexTokenType::Word) node->redirect_out = get().value; node->append_out = false;
            } else if (t.type == LexTokenType::RedirectAppend) {
                get(); if (peek().type == LexTokenType::Word) node->redirect_out = get().value; node->append_out = true;
            } else if (t.type == LexTokenType::RedirectIn) {
                get(); if (peek().type == LexTokenType::Word) node->redirect_in = get().value;
            } else if (t.type == LexTokenType::Ampersand) {
                get(); node->is_background = true;
            } else break;
        }

        return node;
    }

    std::shared_ptr<ASTNode> parse_pipeline(std::initializer_list<std::string> stop_words = {}) {
        auto pipe_node = std::make_shared<PipelineNode>();
        auto first = parse_command(stop_words);
        if (!first) return nullptr;
        pipe_node->stages.push_back(first);
        while (peek().type == LexTokenType::Pipe) {
            get();
            auto next = parse_command(stop_words);
            if (!next) break;
            pipe_node->stages.push_back(next);
        }
        return pipe_node;
    }

    std::shared_ptr<ASTNode> parse_chain(std::initializer_list<std::string> stop_words = {}) {
        auto left = parse_pipeline(stop_words);
        if (!left) return nullptr;
        while (peek().type == LexTokenType::And || peek().type == LexTokenType::Or || 
               peek().type == LexTokenType::Semi || peek().type == LexTokenType::Newline) {
            Token op = get();
            if (op.type == LexTokenType::Newline || op.type == LexTokenType::Semi) {
                if (pos_ >= tokens_.size()) break;
                if (peek().type == LexTokenType::Newline || peek().type == LexTokenType::Semi) continue;
                auto right = parse_pipeline(stop_words);
                if (!right) break;
                auto chain = std::make_shared<ChainNode>();
                chain->left = left; chain->right = right; chain->op = op.type;
                left = chain;
            } else {
                auto right = parse_pipeline(stop_words);
                if (!right) break;
                auto chain = std::make_shared<ChainNode>();
                chain->left = left; chain->right = right; chain->op = op.type;
                left = chain;
            }
        }
        return left;
    }
};

std::string capture_cmd_output(const std::string& cmd, ShellContext& ctx) {
    HANDLE hReadRaw, hWriteRaw;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    if (!CreatePipe(&hReadRaw, &hWriteRaw, &sa, 0)) return "";
    SetHandleInformation(hReadRaw, HANDLE_FLAG_INHERIT, 0);

    std::string output;
    std::thread reader([hReadRaw, &output]() {
        char buffer[2048]; DWORD bytesRead = 0;
        while (ReadFile(hReadRaw, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            if (output.size() < MAX_COMMAND_SUBSTITUTION_BYTES) {
                size_t remaining = MAX_COMMAND_SUBSTITUTION_BYTES - output.size();
                output.append(buffer, (std::min)(remaining, static_cast<size_t>(bytesRead)));
            }
        }
        CloseHandle(hReadRaw);
    });

    ShellContext sub_ctx(false);
    {
        std::lock_guard<std::mutex> lock(ctx.ctx_mutex);
        sub_ctx.variables = ctx.variables;
        sub_ctx.last_return_code = ctx.last_return_code;
        sub_ctx.timeout_ms = ctx.timeout_ms;
        sub_ctx.audit_logging_enabled = ctx.audit_logging_enabled;
    }

    ScopedWorkingDirectory cwd_guard;
    Parser parser(tokenize(cmd));
    auto ast = parser.parse_chain();
    int command_status = 0;
    if (ast) {
        try {
            command_status = ast->execute(sub_ctx, NULL, hWriteRaw);
        } catch (const ShellExitSignal& signal) {
            command_status = signal.code();
        }
    }

    restore_nested_environment(ctx, sub_ctx);
    
    CloseHandle(hWriteRaw);
    if (reader.joinable()) reader.join();

    while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) output.pop_back();
    ctx.last_return_code = command_status;
    return output;
}

// ============================================================================
// HARDENED POSIX APPLETS
// ============================================================================

int cmd_help(const std::vector<std::string>& args, ShellContext& ctx);
std::string expand_history_designators(const std::string& line, ShellContext& ctx);

int cmd_sh(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1) {
        if (args[1] == "--help" || args[1] == "-h") return cmd_help({"help", "sh"}, ctx);
        if (args[1] == "-c" && args.size() > 2) {
            if (args.size() > 3) {
                for (size_t k = 3; k < args.size(); ++k) {
                    ctx.set_var(std::to_string(k - 3), args[k]);
                }
                ctx.set_var("#", std::to_string(args.size() - 4));
            } else {
                ctx.set_var("0", "sh");
                ctx.set_var("#", "0");
            }
            Parser parser(tokenize(args[2]));
            auto ast = parser.parse_chain();
            try {
                return ast ? ast->execute(ctx, NULL, NULL) : 0;
            } catch (const ShellExitSignal& signal) {
                return signal.code();
            }
        }
        for (size_t k = 1; k < args.size(); ++k) {
            ctx.set_var(std::to_string(k - 1), args[k]);
        }
        ctx.set_var("#", std::to_string(args.size() - 2));

        std::ifstream file(utf8_to_utf16(normalize_path(args[1])));
        if (!file) { std::cerr << "sh: " << args[1] << ": No such file\n"; return 127; }
        file.seekg(0, std::ios::end);
        std::streamoff script_size = file.tellg();
        if (script_size < 0 || static_cast<size_t>(script_size) > MAX_SCRIPT_BYTES) {
            std::cerr << "sh: " << args[1] << ": script is too large\n";
            return 126;
        }
        file.seekg(0, std::ios::beg);
        std::string script((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        Parser parser(tokenize(script));
        auto ast = parser.parse_chain();
        try {
            return ast ? ast->execute(ctx, NULL, NULL) : 0;
        } catch (const ShellExitSignal& signal) {
            return signal.code();
        }
    }
    std::cout << "BusyBox v17.0\nType 'help' for manual.\n$ ";
    std::string line;
    while (std::getline(std::cin, line)) {
        std::string expanded = expand_history_designators(line, ctx);
        if (expanded != line) {
            std::cout << expanded << "\n";
        }
        ctx.add_history(expanded);
        Parser parser(tokenize(expanded));
        auto ast = parser.parse_chain();
        if (ast) ctx.last_return_code = ast->execute(ctx, NULL, NULL);
        std::cout << "$ ";
    }
    ctx.save_history();
    return 0;
}

std::string trim_copy(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) b++;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) e--;
    return s.substr(b, e - b);
}

int cmd_grep(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "grep"}, ctx);

    bool ignore_case = false, invert = false, line_num = false, count_only = false, quiet = false;
    bool recursive = false, files_with_matches = false, fixed_strings = false, extended_regex = false;
    bool force_filename = false, suppress_filename = false;
    std::vector<std::string> patterns;
    std::vector<std::string> files;

    bool stop_flags = false;
    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (!stop_flags && arg == "--") { stop_flags = true; continue; }
        if (!stop_flags && arg == "-e" && i + 1 < args.size()) { patterns.push_back(args[++i]); continue; }
        if (!stop_flags && arg == "-f" && i + 1 < args.size()) {
            std::ifstream pfile(utf8_to_utf16(normalize_path(args[++i])));
            std::string line;
            while (std::getline(pfile, line)) {
                if (!line.empty()) patterns.push_back(line);
            }
            continue;
        }
        if (!stop_flags && arg[0] == '-' && arg.length() > 1) {
            for (size_t j = 1; j < arg.length(); ++j) {
                char flag = arg[j];
                if (flag == 'i') ignore_case = true;
                else if (flag == 'v') invert = true;
                else if (flag == 'n') line_num = true;
                else if (flag == 'c') count_only = true;
                else if (flag == 'q') quiet = true;
                else if (flag == 'r' || flag == 'R') recursive = true;
                else if (flag == 'l') files_with_matches = true;
                else if (flag == 'F') fixed_strings = true;
                else if (flag == 'E') extended_regex = true;
                else if (flag == 'H') force_filename = true;
                else if (flag == 'h') suppress_filename = true;
            }
            continue;
        }
        if (patterns.empty()) patterns.push_back(arg);
        else files.push_back(normalize_path(arg));
    }
    if (patterns.empty()) return 2;

    struct PatternSpec {
        std::string literal;
        std::regex regex;
        bool use_regex = false;
        bool use_literal = false;
    };
    std::vector<PatternSpec> compiled;
    auto compile_pattern = [&](const std::string& pattern) {
        PatternSpec spec;
        spec.literal = pattern;
        spec.use_literal = fixed_strings;
        spec.use_regex = !fixed_strings;
        if (!fixed_strings) {
            std::regex_constants::syntax_option_type flags = std::regex_constants::ECMAScript | std::regex_constants::optimize;
            if (ignore_case) flags |= std::regex_constants::icase;
            try { spec.regex = std::regex(pattern, flags); } catch (const std::regex_error& e) { std::cerr << "grep: invalid regex: " << e.what() << "\n"; }
        }
        compiled.push_back(spec);
    };
    for (const auto& p : patterns) compile_pattern(p);

    bool found_any_match = false;
    auto matches_any = [&](const std::string& line) {
        bool matched = false;
        for (const auto& spec : compiled) {
            if (spec.use_literal) {
                matched = (line.find(spec.literal) != std::string::npos);
            } else {
                matched = std::regex_search(line, spec.regex);
            }
            if (matched) break;
        }
        return matched;
    };

    auto proc_stream = [&](std::istream& in, const std::string& name) {
        std::string line; size_t lineno = 0, matches = 0;
        while (std::getline(in, line)) {
            lineno++;
            bool matched = matches_any(line);
            if (matched != invert) {
                found_any_match = true;
                matches++;
                if (quiet) return;
                if (files_with_matches) { std::cout << name << "\n"; return; }
                if (!count_only) {
                    std::string prefix;
                    if (!name.empty() && !suppress_filename) prefix = name;
                    if (!prefix.empty() && force_filename) prefix = name;
                    if (!prefix.empty()) std::cout << prefix << ":";
                    if (line_num) std::cout << lineno << ":";
                    std::cout << line << "\n";
                }
            }
        }
        if (count_only && !quiet) std::cout << (name.empty() ? "" : name + ":") << matches << "\n";
    };

    std::function<void(const fs::path&)> proc_path = [&](const fs::path& p) {
        std::error_code ec;
        if (fs::is_directory(p, ec)) {
            if (!recursive) return;
            for (const auto& entry : fs::recursive_directory_iterator(p, ec)) {
                if (fs::is_regular_file(entry.path(), ec)) {
                    std::ifstream file(entry.path());
                    if (file) proc_stream(file, entry.path().string());
                }
            }
        } else {
            std::ifstream file(p);
            if (file) proc_stream(file, files.size() > 1 || recursive ? p.string() : "");
        }
    };

    if (files.empty()) proc_stream(std::cin, "");
    else for (const auto& f : files) proc_path(fs::path(f));
    return found_any_match ? 0 : 1;
}

int cmd_sed(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "sed"}, ctx);
    if (args.size() <= 1) return 1;

    bool in_place = false, quiet = false;
    size_t expr_idx = 1;
    std::vector<std::string> expressions;

    if (args[1] == "-i") { in_place = true; expr_idx = 2; }
    if (args[1] == "-n") { quiet = true; expr_idx = 2; }
    if (args[1] == "-i" && args.size() > 2 && args[2] == "-n") { in_place = true; quiet = true; expr_idx = 3; }

    if (expr_idx >= args.size()) return 1;

    auto collect_exprs = [&](const std::vector<std::string>& argv) {
        std::vector<std::string> exprs;
        for (size_t i = expr_idx; i < argv.size(); ++i) {
            if (argv[i] == "-e" && i + 1 < argv.size()) { exprs.push_back(argv[++i]); }
            else if (argv[i] == "-f" && i + 1 < argv.size()) {
                std::ifstream script_file(utf8_to_utf16(normalize_path(argv[++i])));
                std::string line;
                while (std::getline(script_file, line)) {
                    if (!trim_copy(line).empty()) exprs.push_back(trim_copy(line));
                }
            } else if (argv[i][0] != '-') {
                exprs.push_back(argv[i]);
            }
        }
        return exprs;
    };

    expressions = collect_exprs(args);
    if (expressions.empty()) return 1;

    auto transform = [&](std::istream& in, std::ostream& out) {
        std::string line;
        std::vector<std::string> lines;
        while (std::getline(in, line)) lines.push_back(line);
        for (std::string& current : lines) {
            bool delete_line = false;
            bool print_line = false;
            for (const auto& expr : expressions) {
                std::string trimmed = trim_copy(expr);
                if (trimmed.empty()) continue;
                bool has_address = false;
                bool address_is_regex = false;
                bool address_is_range = false;
                size_t start_line = 0, end_line = 0;
                std::string address_pattern;
                std::string body = trimmed;

                if (!body.empty() && body[0] == '/' && body.find_last_of('/') != std::string::npos) {
                    size_t end = body.find_last_of('/');
                    address_pattern = body.substr(1, end - 1);
                    body = body.substr(end + 1);
                    has_address = true;
                    address_is_regex = true;
                } else if (!body.empty() && std::isdigit(static_cast<unsigned char>(body[0]))) {
                    size_t pos = 0;
                    while (pos < body.size() && std::isdigit(static_cast<unsigned char>(body[pos]))) pos++;
                    start_line = std::stoul(body.substr(0, pos));
                    has_address = true;
                    if (pos < body.size() && body[pos] == ',') {
                        size_t pos2 = pos + 1;
                        while (pos2 < body.size() && std::isdigit(static_cast<unsigned char>(body[pos2]))) pos2++;
                        end_line = std::stoul(body.substr(pos + 1, pos2 - pos - 1));
                        address_is_range = true;
                        body = body.substr(pos2);
                    } else {
                        body = body.substr(pos);
                    }
                }

                if (body.empty()) continue;

                if (body[0] == 'd') {
                    if (!has_address || (address_is_range && start_line <= 0)) {
                        delete_line = true;
                        break;
                    }
                } else if (body[0] == 'p') {
                    print_line = true;
                    if (quiet) continue;
                } else if (body[0] == 's' && body.size() >= 3) {
                    char delim = body[1];
                    size_t p2 = std::string::npos;
                    for (size_t k = 2; k < body.size(); ++k) {
                        if (body[k] == '\\') { k++; }
                        else if (body[k] == delim) { p2 = k; break; }
                    }
                    if (p2 != std::string::npos) {
                        std::string find_str = body.substr(2, p2 - 2);
                        size_t p3 = std::string::npos;
                        for (size_t k = p2 + 1; k < body.size(); ++k) {
                            if (body[k] == '\\') { k++; }
                            else if (body[k] == delim) { p3 = k; break; }
                        }
                        if (p3 != std::string::npos) {
                            std::string replace_str = body.substr(p2 + 1, p3 - p2 - 1);
                            std::string unescaped;
                            for (size_t k = 0; k < replace_str.size(); ++k) {
                                if (replace_str[k] == '\\' && k + 1 < replace_str.size() && replace_str[k + 1] == delim) { unescaped += delim; k++; }
                                else unescaped += replace_str[k];
                            }
                            std::regex re(find_str, std::regex_constants::ECMAScript | std::regex_constants::optimize);
                            bool global = (body.substr(p3 + 1).find('g') != std::string::npos);
                            if (global) current = std::regex_replace(current, re, unescaped);
                            else current = std::regex_replace(current, re, unescaped, std::regex_constants::format_first_only);
                        }
                    }
                }
            }
            if (!delete_line && (!quiet || print_line)) out << current << "\n";
        }
    };

    if (args.size() > 2 && args[1] != "-i" && args[1] != "-n") {
        std::string target_file = normalize_path(args[args.size() - 1]);
        std::ifstream file(utf8_to_utf16(target_file));
        if (file) transform(file, std::cout);
    } else if (args.size() > 2 && args[1] == "-i") {
        std::string target_file = normalize_path(args.back());
        std::ifstream file(utf8_to_utf16(target_file));
        std::stringstream ss;
        transform(file, ss);
        std::ofstream out(utf8_to_utf16(target_file), std::ios::trunc);
        out << ss.str();
    } else {
        transform(std::cin, std::cout);
    }
    return 0;
}

int cmd_awk(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "awk"}, ctx);
    if (args.size() <= 1) return 1;

    char delim = ' ';
    std::string script;
    size_t file_idx = 0;

    if (args.size() > 3 && args[1] == "-F") {
        delim = args[2][0];
        script = args[3];
        file_idx = 4;
    } else {
        script = args[1];
        file_idx = 2;
    }

    auto extract_block = [&](const std::string& label) -> std::string {
        std::string lower = to_lower(label);
        size_t pos = script.find(lower);
        if (pos == std::string::npos) return "";
        size_t brace = script.find('{', pos);
        if (brace == std::string::npos) return "";
        int depth = 0;
        for (size_t i = brace; i < script.size(); ++i) {
            if (script[i] == '{') depth++;
            else if (script[i] == '}') {
                depth--;
                if (depth == 0) return script.substr(brace + 1, i - brace - 1);
            }
        }
        return "";
    };

    std::string begin_block = extract_block("BEGIN");
    std::string end_block = extract_block("END");
    std::string main_script = script;
    if (!begin_block.empty()) {
        size_t b = main_script.find("BEGIN");
        if (b != std::string::npos) {
            size_t brace = main_script.find('{', b);
            size_t end = main_script.find('}', brace);
            main_script.erase(b, end - b + 1);
        }
    }
    if (!end_block.empty()) {
        size_t e = main_script.find("END");
        if (e != std::string::npos) {
            size_t brace = main_script.find('{', e);
            size_t end = main_script.find('}', brace);
            main_script.erase(e, end - e + 1);
        }
    }

    auto eval_print = [&](const std::string& expr, const std::vector<std::string>& fields, const std::string& line, size_t lineno, const std::string& filename) {
        std::string output;
        std::string current = trim_copy(expr);
        if (current.empty()) return line;
        if (current.find("print") != std::string::npos) {
            size_t start = current.find('(');
            size_t end = current.rfind(')');
            if (start != std::string::npos && end != std::string::npos) {
                current = current.substr(start + 1, end - start - 1);
            }
        }
        std::stringstream ss(current);
        std::string token;
        bool first = true;
        while (ss >> token) {
            if (!first) output += " ";
            first = false;
            if (token.size() >= 2 && token[0] == '"' && token.back() == '"') {
                output += token.substr(1, token.size() - 2);
            } else if (token[0] == '$') {
                std::string num = token.substr(1);
                if (num == "0") output += line;
                else if (num == "NF") output += std::to_string(fields.size());
                else if (num == "NR") output += std::to_string(lineno);
                else if (num == "FILENAME") output += filename;
                else {
                    size_t idx = std::stoul(num);
                    if (idx > 0 && idx <= fields.size()) output += fields[idx - 1];
                }
            } else if (token == "NR") output += std::to_string(lineno);
            else if (token == "NF") output += std::to_string(fields.size());
            else if (token == "FILENAME") output += filename;
            else output += token;
        }
        return output;
    };

    auto proc = [&](std::istream& in, const std::string& filename) {
        std::string line; size_t lineno = 0;
        if (!begin_block.empty()) {
            std::vector<std::string> fields;
            std::cout << eval_print(begin_block, fields, "", 0, filename) << "\n";
        }
        while (std::getline(in, line)) {
            lineno++;
            std::vector<std::string> fields;
            if (delim == ' ') {
                std::stringstream ss(line);
                std::string field;
                while (ss >> field) fields.push_back(field);
            } else {
                std::stringstream ss(line);
                std::string field;
                while (std::getline(ss, field, delim)) fields.push_back(field);
            }

            std::string action = trim_copy(main_script);
            if (action.empty()) {
                std::cout << line << "\n";
                continue;
            }
            if (action.find("{") != std::string::npos && action.find("}") != std::string::npos) {
                std::string inner = action.substr(action.find('{') + 1, action.find_last_of('}') - action.find('{') - 1);
                std::cout << eval_print(inner, fields, line, lineno, filename) << "\n";
            } else {
                std::cout << eval_print(action, fields, line, lineno, filename) << "\n";
            }
        }
        if (!end_block.empty()) {
            std::vector<std::string> fields;
            std::cout << eval_print(end_block, fields, "", 0, filename) << "\n";
        }
    };

    if (file_idx < args.size()) {
        std::ifstream file(utf8_to_utf16(normalize_path(args[file_idx])));
        if (file) proc(file, args[file_idx]);
    } else proc(std::cin, "");
    return 0;
}

int cmd_cat(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "cat"}, ctx);

    bool number_nonblank = false, number_all = false, squeeze_blank = false;
    std::vector<std::string> files;
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-n") number_all = true;
        else if (args[i] == "-b") number_nonblank = true;
        else if (args[i] == "-s") squeeze_blank = true;
        else files.push_back(normalize_path(args[i]));
    }

    auto print_stream = [&](std::istream& in) {
        std::string line;
        size_t line_no = 0;
        bool prev_blank = false;
        while (std::getline(in, line)) {
            if (squeeze_blank && line.empty() && prev_blank) continue;
            prev_blank = line.empty();
            if (number_all || (number_nonblank && !line.empty())) {
                std::cout << line_no + 1 << " ";
            }
            std::cout << line << "\n";
            line_no++;
        }
    };

    if (files.empty()) { print_stream(std::cin); return 0; }
    for (const auto& file : files) {
        std::ifstream input(utf8_to_utf16(file), std::ios::binary);
        if (!input) std::cerr << "cat: " << file << ": No such file\n";
        else print_stream(input);
    }
    return 0;
}

int cmd_wc(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "wc"}, ctx);
    bool lines_only = false, words_only = false, bytes_only = false;
    std::vector<std::string> files;
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-l") lines_only = true;
        else if (args[i] == "-w") words_only = true;
        else if (args[i] == "-c") bytes_only = true;
        else files.push_back(normalize_path(args[i]));
    }
    if (!lines_only && !words_only && !bytes_only) lines_only = words_only = bytes_only = true;

    auto count_stream = [&](std::istream& in, const std::string& label) {
        size_t lines = 0, words = 0, bytes = 0;
        std::string line;
        while (std::getline(in, line)) {
            lines++;
            bytes += line.size() + 1;
            std::stringstream ss(line);
            std::string word;
            while (ss >> word) words++;
        }
        if (lines_only) std::cout << lines << " ";
        if (words_only) std::cout << words << " ";
        if (bytes_only) std::cout << bytes << " ";
        if (!label.empty()) std::cout << label;
        std::cout << "\n";
    };

    if (files.empty()) { count_stream(std::cin, ""); return 0; }
    for (const auto& file : files) {
        std::ifstream input(utf8_to_utf16(file));
        if (!input) std::cerr << "wc: " << file << ": No such file\n";
        else count_stream(input, file);
    }
    return 0;
}

int cmd_tr(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "tr"}, ctx);
    if (args.size() < 3) return 1;
    bool delete_mode = false;
    std::string from = args[1];
    std::string to = args[2];
    if (args.size() > 3 && args[1] == "-d") { delete_mode = true; from = args[2]; to = ""; }

    auto expand_range = [&](const std::string& spec) {
        std::string out;
        for (size_t i = 0; i < spec.size(); ++i) {
            if (i + 2 < spec.size() && spec[i + 1] == '-' && spec[i + 2] != '-') {
                char start = spec[i], end = spec[i + 2];
                for (char c = start; c <= end; ++c) out.push_back(c);
                i += 2;
            } else out.push_back(spec[i]);
        }
        return out;
    };

    std::string from_expanded = expand_range(from);
    std::string to_expanded = expand_range(to);
    std::string line;
    while (std::getline(std::cin, line)) {
        std::string out;
        for (char c : line) {
            auto pos = from_expanded.find(c);
            if (delete_mode) {
                if (pos == std::string::npos) out.push_back(c);
            } else if (pos != std::string::npos) {
                size_t idx = pos;
                if (idx < to_expanded.size()) out.push_back(to_expanded[idx]);
                else out.push_back(c);
            } else out.push_back(c);
        }
        std::cout << out << "\n";
    }
    return 0;
}

int cmd_cut(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "cut"}, ctx);
    char delimiter = '\t';
    bool by_chars = false;
    std::vector<size_t> fields;
    std::string path;

    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-d" && i + 1 < args.size()) { delimiter = args[++i][0]; }
        else if (args[i] == "-f" && i + 1 < args.size()) {
            std::stringstream ss(args[++i]);
            std::string token;
            while (std::getline(ss, token, ',')) {
                if (!token.empty()) fields.push_back(std::stoul(token));
            }
        } else if (args[i] == "-c") { by_chars = true; }
        else path = normalize_path(args[i]);
    }

    auto process = [&](std::istream& in) {
        std::string line;
        while (std::getline(in, line)) {
            if (by_chars) {
                for (size_t i = 0; i < line.size(); ++i) {
                    if (fields.empty() || std::find(fields.begin(), fields.end(), i + 1) != fields.end()) std::cout << line[i];
                }
                std::cout << "\n";
            } else {
                std::stringstream ss(line);
                std::string token;
                std::vector<std::string> parts;
                while (std::getline(ss, token, delimiter)) parts.push_back(token);
                for (size_t i = 0; i < parts.size(); ++i) {
                    if (fields.empty() || std::find(fields.begin(), fields.end(), i + 1) != fields.end()) {
                        if (i > 0) std::cout << delimiter;
                        std::cout << parts[i];
                    }
                }
                std::cout << "\n";
            }
        }
    };

    if (!path.empty()) {
        std::ifstream input(utf8_to_utf16(path));
        if (input) process(input);
    } else process(std::cin);
    return 0;
}

int cmd_tee(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "tee"}, ctx);
    bool append = false;
    std::vector<std::string> files;
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-a") append = true;
        else files.push_back(normalize_path(args[i]));
    }

    std::vector<std::ofstream> outs;
    for (const auto& file : files) {
        std::ios::openmode mode = append ? std::ios::app : std::ios::trunc;
        outs.emplace_back(utf8_to_utf16(file), mode);
    }

    std::string line;
    while (std::getline(std::cin, line)) {
        std::cout << line << "\n";
        for (auto& out : outs) out << line << "\n";
    }
    return 0;
}

int cmd_sort(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "sort"}, ctx);
    bool numeric = false, reverse = false, unique = false;
    std::vector<std::string> files;
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-n") numeric = true;
        else if (args[i] == "-r") reverse = true;
        else if (args[i] == "-u") unique = true;
        else files.push_back(normalize_path(args[i]));
    }

    auto load_lines = [&](std::istream& in) -> std::vector<std::string> {
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(in, line)) lines.push_back(line);
        return lines;
    };

    auto emit = [&](const std::vector<std::string>& lines) {
        std::vector<std::string> work = lines;
        std::stable_sort(work.begin(), work.end(), [&](const std::string& a, const std::string& b) {
            if (numeric) return std::stoll(a) < std::stoll(b);
            return a < b;
        });
        if (reverse) std::reverse(work.begin(), work.end());
        if (unique) {
            std::vector<std::string> uniqed;
            for (const auto& line : work) {
                if (uniqed.empty() || uniqed.back() != line) uniqed.push_back(line);
            }
            work = uniqed;
        }
        for (const auto& line : work) std::cout << line << "\n";
    };

    if (files.empty()) { emit(load_lines(std::cin)); return 0; }
    for (const auto& file : files) {
        std::ifstream input(utf8_to_utf16(file));
        if (!input) std::cerr << "sort: " << file << ": No such file\n";
        else emit(load_lines(input));
    }
    return 0;
}

int cmd_uniq(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "uniq"}, ctx);
    bool count = false, repeated_only = false, unique_only = false;
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-c") count = true;
        else if (args[i] == "-d") repeated_only = true;
        else if (args[i] == "-u") unique_only = true;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(std::cin, line)) lines.push_back(line);

    for (size_t i = 0; i < lines.size();) {
        size_t j = i + 1;
        while (j < lines.size() && lines[j] == lines[i]) j++;
        size_t count_val = j - i;
        bool is_repeated = count_val > 1;
        bool emit = (!repeated_only && !unique_only) || (repeated_only && is_repeated) || (unique_only && !is_repeated);
        if (emit) {
            if (count) std::cout << count_val << " ";
            std::cout << lines[i] << "\n";
        }
        i = j;
    }
    return 0;
}

int cmd_basename(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "basename"}, ctx);
    if (args.size() <= 1) return 1;
    std::string path = normalize_path(args[1]);
    std::cout << fs::path(path).filename().string() << "\n";
    return 0;
}

int cmd_dirname(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "dirname"}, ctx);
    if (args.size() <= 1) return 1;
    std::string path = normalize_path(args[1]);
    std::cout << fs::path(path).parent_path().string() << "\n";
    return 0;
}

int cmd_mkdir(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "mkdir"}, ctx);
    bool recursive = false;
    std::vector<std::string> paths;
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-p") recursive = true;
        else paths.push_back(normalize_path(args[i]));
    }
    std::error_code ec;
    for (const auto& p : paths) {
        if (recursive) fs::create_directories(utf8_to_utf16(p), ec);
        else fs::create_directory(utf8_to_utf16(p), ec);
        if (ec) { std::cerr << "mkdir: " << p << ": " << ec.message() << "\n"; return 1; }
    }
    return 0;
}

int cmd_rmdir(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "rmdir"}, ctx);
    std::vector<std::string> paths;
    for (size_t i = 1; i < args.size(); ++i) paths.push_back(normalize_path(args[i]));
    std::error_code ec;
    for (const auto& p : paths) {
        fs::remove(utf8_to_utf16(p), ec);
        if (ec) { std::cerr << "rmdir: " << p << ": " << ec.message() << "\n"; return 1; }
    }
    return 0;
}

static std::string strip_long_path_prefix(std::string path) {
    const std::string prefix = "\\\\?\\";
    if (path.rfind(prefix, 0) == 0) {
        path.erase(0, prefix.length());
    }
    return path;
}

static std::string format_local_filetime(const FILETIME& ft) {
    FILETIME local_ft;
    SYSTEMTIME st;
    if (!FileTimeToLocalFileTime(&ft, &local_ft)) return "";
    if (!FileTimeToSystemTime(&local_ft, &st)) return "";

    std::ostringstream out;
    out << std::setfill('0')
        << std::setw(4) << st.wYear << "-"
        << std::setw(2) << st.wMonth << "-"
        << std::setw(2) << st.wDay << " "
        << std::setw(2) << st.wHour << ":"
        << std::setw(2) << st.wMinute << ":"
        << std::setw(2) << st.wSecond;
    return out.str();
}

static std::string read_all_stdin_text() {
    std::ostringstream buffer;
    buffer << std::cin.rdbuf();
    return buffer.str();
}

static std::vector<unsigned char> read_file_bytes(const std::wstring& path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<unsigned char>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

static std::string hex_digest(const std::vector<unsigned char>& digest) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (unsigned char byte : digest) out << std::setw(2) << static_cast<int>(byte);
    return out.str();
}

static bool hash_stream_with_alg(std::istream& in, ALG_ID alg_id, std::string& out_hex) {
    HCRYPTPROV prov = 0;
    HCRYPTHASH hash = 0;
    if (!CryptAcquireContextW(&prov, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) return false;
    if (!CryptCreateHash(prov, alg_id, 0, 0, &hash)) {
        CryptReleaseContext(prov, 0);
        return false;
    }

    std::vector<char> buffer(8192);
    while (in.good()) {
        in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        std::streamsize read = in.gcount();
        if (read > 0 && !CryptHashData(hash, reinterpret_cast<const BYTE*>(buffer.data()), static_cast<DWORD>(read), 0)) {
            CryptDestroyHash(hash);
            CryptReleaseContext(prov, 0);
            return false;
        }
    }

    DWORD cb_hash = 0;
    DWORD cb_len = sizeof(cb_hash);
    if (!CryptGetHashParam(hash, HP_HASHSIZE, reinterpret_cast<BYTE*>(&cb_hash), &cb_len, 0) || cb_hash == 0) {
        CryptDestroyHash(hash);
        CryptReleaseContext(prov, 0);
        return false;
    }

    std::vector<unsigned char> digest(cb_hash);
    cb_len = cb_hash;
    bool ok = CryptGetHashParam(hash, HP_HASHVAL, digest.data(), &cb_len, 0) != 0;
    if (ok) out_hex = hex_digest(digest);

    CryptDestroyHash(hash);
    CryptReleaseContext(prov, 0);
    return ok;
}

static std::string base64_encode_bytes(const std::string& input) {
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    size_t i = 0;
    while (i < input.size()) {
        unsigned int octet_a = static_cast<unsigned char>(input[i++]);
        unsigned int octet_b = i < input.size() ? static_cast<unsigned char>(input[i++]) : 0;
        unsigned int octet_c = i < input.size() ? static_cast<unsigned char>(input[i++]) : 0;
        unsigned int triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        out.push_back(table[(triple >> 18) & 0x3F]);
        out.push_back(table[(triple >> 12) & 0x3F]);
        out.push_back(i - 1 <= input.size() ? table[(triple >> 6) & 0x3F] : '=');
        out.push_back(i <= input.size() ? table[triple & 0x3F] : '=');
    }

    size_t mod = input.size() % 3;
    if (mod) {
        out[out.size() - 1] = '=';
        if (mod == 1) out[out.size() - 2] = '=';
    }
    return out;
}

static std::string base64_decode_bytes(const std::string& input) {
    static const std::string table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string cleaned;
    for (char c : input) {
        if (!std::isspace(static_cast<unsigned char>(c))) cleaned.push_back(c);
    }

    auto value_of = [&](char c) -> int {
        if (c == '=') return -2;
        auto pos = table.find(c);
        return pos == std::string::npos ? -1 : static_cast<int>(pos);
    };

    std::string out;
    for (size_t i = 0; i < cleaned.size(); i += 4) {
        int vals[4] = { -1, -1, -1, -1 };
        size_t block_len = std::min<size_t>(4, cleaned.size() - i);
        for (size_t j = 0; j < block_len; ++j) vals[j] = value_of(cleaned[i + j]);
        if (vals[0] < 0 || vals[1] < 0) break;
        unsigned int triple = (static_cast<unsigned int>(vals[0]) << 18) |
                              (static_cast<unsigned int>(vals[1]) << 12) |
                              ((vals[2] > 0 ? static_cast<unsigned int>(vals[2]) : 0) << 6) |
                              (vals[3] > 0 ? static_cast<unsigned int>(vals[3]) : 0);
        out.push_back(static_cast<char>((triple >> 16) & 0xFF));
        if (vals[2] != -2 && vals[2] >= 0) out.push_back(static_cast<char>((triple >> 8) & 0xFF));
        if (vals[3] != -2 && vals[3] >= 0) out.push_back(static_cast<char>(triple & 0xFF));
    }
    return out;
}

static std::vector<std::string> read_all_lines_from_path(const std::string& path) {
    std::vector<std::string> lines;
    std::ifstream file(utf8_to_utf16(normalize_path(path)));
    std::string line;
    while (std::getline(file, line)) lines.push_back(line);
    return lines;
}

int cmd_seq(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "seq"}, ctx);
    if (args.size() < 2) return 1;

    long long start = 1;
    long long step = 1;
    long long end = 0;

    try {
        if (args.size() == 2) {
            end = std::stoll(args[1]);
        } else if (args.size() == 3) {
            start = std::stoll(args[1]);
            end = std::stoll(args[2]);
        } else {
            start = std::stoll(args[1]);
            step = std::stoll(args[2]);
            end = std::stoll(args[3]);
        }
    } catch (...) {
        return 1;
    }

    if (step == 0) return 1;
    if (step > 0) {
        for (long long value = start; value <= end; value += step) std::cout << value << "\n";
    } else {
        for (long long value = start; value >= end; value += step) std::cout << value << "\n";
    }
    return 0;
}

int cmd_touch(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "touch"}, ctx);
    bool no_create = false;
    std::vector<std::string> paths;
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-c" || args[i] == "--no-create") no_create = true;
        else paths.push_back(normalize_path(args[i]));
    }

    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    for (const auto& path : paths) {
        HANDLE file = CreateFileW(utf8_to_utf16(path).c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                 NULL, no_create ? OPEN_EXISTING : OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE) {
            if (no_create) continue;
            std::cerr << "touch: cannot open '" << path << "'\n";
            return 1;
        }
        SetFileTime(file, NULL, NULL, &now);
        CloseHandle(file);
    }
    return 0;
}

int cmd_date(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "date"}, ctx);
    std::time_t now = std::time(nullptr);
    std::tm local_tm = {};
    localtime_s(&local_tm, &now);

    std::string format = "%a %b %d %H:%M:%S %Y";
    if (args.size() > 1 && !args[1].empty() && args[1][0] == '+') {
        format = args[1].substr(1);
    }
    std::cout << std::put_time(&local_tm, format.c_str()) << "\n";
    return 0;
}

int cmd_mktemp(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "mktemp"}, ctx);
    bool make_dir = false;
    std::string template_name;
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-d") make_dir = true;
        else if (template_name.empty()) template_name = args[i];
    }

    wchar_t temp_dir[MAX_PATH + 1] = {};
    DWORD temp_len = GetTempPathW(MAX_PATH, temp_dir);
    if (temp_len == 0 || temp_len > MAX_PATH) return 1;

    std::wstring prefix = L"mkt";
    if (!template_name.empty()) {
        std::wstring template_w = utf8_to_utf16(template_name);
        std::wstring stem = fs::path(template_w).stem().wstring();
        if (!stem.empty()) prefix = stem.substr(0, std::min<size_t>(3, stem.size()));
    }

    wchar_t temp_file[MAX_PATH + 1] = {};
    if (!GetTempFileNameW(temp_dir, prefix.c_str(), 0, temp_file)) return 1;

    if (make_dir) {
        DeleteFileW(temp_file);
        if (!CreateDirectoryW(temp_file, NULL)) return 1;
    }

    std::cout << utf16_to_utf8(temp_file) << "\n";
    return 0;
}

int cmd_kill(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "kill"}, ctx);
    if (args.size() <= 1) return 1;
    int rc = 0;
    for (size_t i = 1; i < args.size(); ++i) {
        if (!args[i].empty() && args[i][0] == '-') continue;
        DWORD pid = static_cast<DWORD>(std::strtoul(args[i].c_str(), nullptr, 10));
        if (pid == 0) continue;
        HANDLE proc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (!proc) {
            std::cerr << "kill: " << pid << ": no such process\n";
            rc = 1;
            continue;
        }
        TerminateProcess(proc, 1);
        CloseHandle(proc);
    }
    return rc;
}

int cmd_clear(const std::vector<std::string>&, ShellContext&) {
    std::cout << "\x1b[2J\x1b[H" << std::flush;
    return 0;
}

int cmd_whoami(const std::vector<std::string>&, ShellContext&) {
    wchar_t buf[UNLEN + 1] = {};
    DWORD len = UNLEN + 1;
    if (!GetUserNameW(buf, &len)) return 1;
    std::cout << utf16_to_utf8(std::wstring(buf, len > 0 ? len - 1 : 0)) << "\n";
    return 0;
}

int cmd_uname(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "uname"}, ctx);
    bool all = false, sys = false, node = false, rel = false, ver = false, mach = false;
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-a") all = true;
        else if (args[i] == "-s") sys = true;
        else if (args[i] == "-n") node = true;
        else if (args[i] == "-r") rel = true;
        else if (args[i] == "-v") ver = true;
        else if (args[i] == "-m") mach = true;
    }

    if (!all && !sys && !node && !rel && !ver && !mach) sys = true;

    wchar_t computer[MAX_COMPUTERNAME_LENGTH + 1] = {};
    DWORD computer_len = MAX_COMPUTERNAME_LENGTH + 1;
    GetComputerNameW(computer, &computer_len);

    SYSTEM_INFO info;
    GetNativeSystemInfo(&info);
    std::string machine = "x86";
    switch (info.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: machine = "x86_64"; break;
        case PROCESSOR_ARCHITECTURE_ARM64: machine = "arm64"; break;
        case PROCESSOR_ARCHITECTURE_INTEL: machine = "x86"; break;
        default: break;
    }

    std::vector<std::string> parts;
    if (all || sys) parts.push_back("Windows_NT");
    if (all || node) parts.push_back(utf16_to_utf8(std::wstring(computer, computer_len)));
    if (all || rel) parts.push_back("10");
    if (all || ver) parts.push_back("build");
    if (all || mach) parts.push_back(machine);

    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) std::cout << ' ';
        std::cout << parts[i];
    }
    std::cout << "\n";
    return 0;
}

int cmd_base64(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "base64"}, ctx);
    bool decode = false;
    std::vector<std::string> inputs;
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-d" || args[i] == "--decode") decode = true;
        else inputs.push_back(args[i]);
    }

    std::string text = inputs.empty() ? read_all_stdin_text() : [&]() {
        std::ostringstream out;
        for (size_t i = 0; i < inputs.size(); ++i) {
            if (i) out << ' ';
            out << inputs[i];
        }
        return out.str();
    }();

    if (decode) std::cout << base64_decode_bytes(text);
    else std::cout << base64_encode_bytes(text);
    std::cout << "\n";
    return 0;
}

int cmd_md5sum(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "md5sum"}, ctx);
    std::vector<std::string> paths;
    for (size_t i = 1; i < args.size(); ++i) paths.push_back(args[i]);
    if (paths.empty()) paths.push_back("-");

    for (const auto& item : paths) {
        std::string digest;
        bool ok = false;
        if (item == "-") {
            ok = hash_stream_with_alg(std::cin, CALG_MD5, digest);
        } else {
            std::ifstream file(utf8_to_utf16(normalize_path(item)), std::ios::binary);
            ok = file.good() && hash_stream_with_alg(file, CALG_MD5, digest);
        }
        if (!ok) return 1;
        std::cout << digest << "  " << item << "\n";
    }
    return 0;
}

int cmd_sha256sum(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "sha256sum"}, ctx);
    std::vector<std::string> paths;
    for (size_t i = 1; i < args.size(); ++i) paths.push_back(args[i]);
    if (paths.empty()) paths.push_back("-");

    for (const auto& item : paths) {
        std::string digest;
        bool ok = false;
        if (item == "-") {
            ok = hash_stream_with_alg(std::cin, CALG_SHA_256, digest);
        } else {
            std::ifstream file(utf8_to_utf16(normalize_path(item)), std::ios::binary);
            ok = file.good() && hash_stream_with_alg(file, CALG_SHA_256, digest);
        }
        if (!ok) return 1;
        std::cout << digest << "  " << item << "\n";
    }
    return 0;
}

int cmd_rev(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "rev"}, ctx);
    std::vector<std::string> lines;
    if (args.size() > 1) {
        lines = read_all_lines_from_path(args[1]);
    } else {
        std::string line;
        while (std::getline(std::cin, line)) lines.push_back(line);
    }
    for (auto line : lines) {
        std::reverse(line.begin(), line.end());
        std::cout << line << "\n";
    }
    return 0;
}

int cmd_paste(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "paste"}, ctx);
    std::vector<std::string> files;
    for (size_t i = 1; i < args.size(); ++i) files.push_back(args[i]);
    if (files.empty()) {
        std::string line;
        while (std::getline(std::cin, line)) std::cout << line << "\n";
        return 0;
    }

    std::vector<std::vector<std::string>> columns;
    for (const auto& file : files) columns.push_back(read_all_lines_from_path(file));
    size_t max_rows = 0;
    for (const auto& col : columns) max_rows = std::max(max_rows, col.size());

    for (size_t row = 0; row < max_rows; ++row) {
        for (size_t col = 0; col < columns.size(); ++col) {
            if (col) std::cout << '\t';
            if (row < columns[col].size()) std::cout << columns[col][row];
        }
        std::cout << "\n";
    }
    return 0;
}

int cmd_cmp(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "cmp"}, ctx);
    if (args.size() < 3) return 1;
    std::ifstream a(utf8_to_utf16(normalize_path(args[1])), std::ios::binary);
    std::ifstream b(utf8_to_utf16(normalize_path(args[2])), std::ios::binary);
    if (!a || !b) return 1;
    std::vector<char> buf_a((std::istreambuf_iterator<char>(a)), std::istreambuf_iterator<char>());
    std::vector<char> buf_b((std::istreambuf_iterator<char>(b)), std::istreambuf_iterator<char>());
    size_t limit = std::min(buf_a.size(), buf_b.size());
    for (size_t i = 0; i < limit; ++i) {
        if (buf_a[i] != buf_b[i]) {
            std::cerr << args[1] << " " << args[2] << " differ: byte " << (i + 1) << "\n";
            return 1;
        }
    }
    if (buf_a.size() != buf_b.size()) {
        std::cerr << args[1] << " " << args[2] << " differ: EOF on " << (buf_a.size() < buf_b.size() ? args[1] : args[2]) << "\n";
        return 1;
    }
    return 0;
}

int cmd_diff(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "diff"}, ctx);
    if (args.size() < 3) return 1;
    auto left = read_all_lines_from_path(args[1]);
    auto right = read_all_lines_from_path(args[2]);
    size_t max_rows = std::max(left.size(), right.size());
    bool different = false;
    for (size_t i = 0; i < max_rows; ++i) {
        std::string l = i < left.size() ? left[i] : "";
        std::string r = i < right.size() ? right[i] : "";
        if (l != r) {
            if (!different) {
                std::cout << "--- " << args[1] << "\n+++ " << args[2] << "\n";
                different = true;
            }
            std::cout << "@@ line " << (i + 1) << " @@\n";
            if (i < left.size()) std::cout << "-" << l << "\n";
            if (i < right.size()) std::cout << "+" << r << "\n";
        }
    }
    return different ? 1 : 0;
}

int cmd_stat(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "stat"}, ctx);
    if (args.size() < 2) return 1;

    for (size_t i = 1; i < args.size(); ++i) {
        std::wstring wpath = utf8_to_utf16(normalize_path(args[i]));
        WIN32_FILE_ATTRIBUTE_DATA data{};
        if (!GetFileAttributesExW(wpath.c_str(), GetFileExInfoStandard, &data)) {
            std::cerr << "stat: cannot stat '" << args[i] << "'\n";
            continue;
        }
        LARGE_INTEGER size{};
        size.HighPart = static_cast<LONG>(data.nFileSizeHigh);
        size.LowPart = data.nFileSizeLow;
        std::cout << "  File: " << args[i] << "\n"
                  << "  Size: " << size.QuadPart << " bytes\n"
                  << "Attrs: 0x" << std::hex << data.dwFileAttributes << std::dec << "\n"
                  << "Access: " << format_local_filetime(data.ftLastAccessTime) << "\n"
                  << "Modify: " << format_local_filetime(data.ftLastWriteTime) << "\n"
                  << "Create: " << format_local_filetime(data.ftCreationTime) << "\n";
    }
    return 0;
}

int cmd_du(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "du"}, ctx);
    std::vector<std::string> paths;
    for (size_t i = 1; i < args.size(); ++i) paths.push_back(args[i]);
    if (paths.empty()) paths.push_back(".");

    for (const auto& item : paths) {
        std::error_code ec;
        fs::path root(utf8_to_utf16(normalize_path(item)));
        uintmax_t total = 0;
        if (fs::is_directory(root, ec)) {
            for (const auto& entry : fs::recursive_directory_iterator(root, ec)) {
                if (entry.is_regular_file(ec)) total += entry.file_size(ec);
            }
        } else if (fs::exists(root, ec)) {
            total = fs::file_size(root, ec);
        }
        std::cout << total << "\t" << item << "\n";
    }
    return 0;
}

int cmd_bdf(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "bdf"}, ctx);
    std::wstring root = args.size() > 1 ? utf8_to_utf16(normalize_path(args[1])) : fs::current_path().wstring();
    ULARGE_INTEGER free_bytes{}, total_bytes{}, total_free{};
    if (!GetDiskFreeSpaceExW(root.c_str(), &free_bytes, &total_bytes, &total_free)) return 1;
    std::cout << "Filesystem\tSize\tUsed\tAvail\n"
              << strip_long_path_prefix(utf16_to_utf8(root)) << "\t"
              << total_bytes.QuadPart << "\t"
              << (total_bytes.QuadPart - total_free.QuadPart) << "\t"
              << free_bytes.QuadPart << "\n";
    return 0;
}

int cmd_ps(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "ps"}, ctx);
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 1;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            std::cout << pe.th32ProcessID << "\t" << utf16_to_utf8(pe.szExeFile) << "\n";
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return 0;
}

int cmd_realpath(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "realpath"}, ctx);
    if (args.size() < 2) return 1;
    std::error_code ec;
    auto resolved = fs::weakly_canonical(utf8_to_utf16(normalize_path(args[1])), ec);
    if (ec) return 1;
    std::cout << utf16_to_utf8(resolved.wstring()) << "\n";
    return 0;
}

int cmd_readlink(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "readlink"}, ctx);
    if (args.size() < 2) return 1;
    std::error_code ec;
    auto target = fs::read_symlink(utf8_to_utf16(normalize_path(args[1])), ec);
    if (ec) {
        auto resolved = fs::weakly_canonical(utf8_to_utf16(normalize_path(args[1])), ec);
        if (ec) return 1;
        target = resolved;
    }
    std::cout << utf16_to_utf8(target.wstring()) << "\n";
    return 0;
}

int cmd_ln(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "ln"}, ctx);
    bool symbolic = false;
    std::vector<std::string> positional;
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-s") symbolic = true;
        else positional.push_back(args[i]);
    }
    if (positional.size() < 2) return 1;

    std::wstring source = utf8_to_utf16(normalize_path(positional[0]));
    std::wstring link_name = utf8_to_utf16(normalize_path(positional[1]));
    if (symbolic) {
        DWORD flags = fs::is_directory(source) ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0;
        if (!CreateSymbolicLinkW(link_name.c_str(), source.c_str(), flags)) {
            std::cerr << "ln: failed to create symbolic link\n";
            return 1;
        }
    } else {
        if (!CreateHardLinkW(link_name.c_str(), source.c_str(), NULL)) {
            std::cerr << "ln: failed to create hard link\n";
            return 1;
        }
    }
    return 0;
}

int cmd_clear_or_cls(const std::vector<std::string>& args, ShellContext& ctx) {
    (void)args;
    return cmd_clear({}, ctx);
}

int cmd_test(const std::vector<std::string>& raw_args, ShellContext& ctx) {
    std::vector<std::string> args = raw_args;
    if (!args.empty() && args[0] == "[") {
        if (args.back() == "]") {
            args.pop_back();
        } else {
            std::cerr << "[: missing ']'\n";
            return 2;
        }
    }

    if (!args.empty() && (args[0] == "test" || args[0] == "[")) {
        args.erase(args.begin());
    }

    if (args.empty()) return 1;

    bool invert = false;
    if (args[0] == "!") {
        invert = true;
        args.erase(args.begin());
    }
    if (args.empty()) return invert ? 0 : 1;

    int result = 1;

    if (args.size() == 1) {
        result = args[0].empty() ? 1 : 0;
    } else if (args.size() == 2) {
        std::string op = args[0], path = normalize_path(args[1]);
        std::error_code ec;
        if (op == "-f") result = fs::is_regular_file(utf8_to_utf16(path), ec) ? 0 : 1;
        else if (op == "-d") result = fs::is_directory(utf8_to_utf16(path), ec) ? 0 : 1;
        else if (op == "-e") result = fs::exists(utf8_to_utf16(path), ec) ? 0 : 1;
        else if (op == "-s") result = (fs::exists(utf8_to_utf16(path), ec) && fs::file_size(utf8_to_utf16(path), ec) > 0) ? 0 : 1;
        else if (op == "-z") result = args[1].empty() ? 0 : 1;
        else if (op == "-n") result = !args[1].empty() ? 0 : 1;
    } else if (args.size() == 3) {
        std::string left = args[0], op = args[1], right = args[2];
        if (op == "-eq") result = (std::atoi(left.c_str()) == std::atoi(right.c_str())) ? 0 : 1;
        else if (op == "-ne") result = (std::atoi(left.c_str()) != std::atoi(right.c_str())) ? 0 : 1;
        else if (op == "-lt") result = (std::atoi(left.c_str()) < std::atoi(right.c_str())) ? 0 : 1;
        else if (op == "-le") result = (std::atoi(left.c_str()) <= std::atoi(right.c_str())) ? 0 : 1;
        else if (op == "-gt") result = (std::atoi(left.c_str()) > std::atoi(right.c_str())) ? 0 : 1;
        else if (op == "-ge") result = (std::atoi(left.c_str()) >= std::atoi(right.c_str())) ? 0 : 1;
        else if (op == "=" || op == "==") result = (left == right) ? 0 : 1;
        else if (op == "!=") result = (left != right) ? 0 : 1;
    }

    return invert ? (result == 0 ? 1 : 0) : result;
}

int cmd_echo(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "echo"}, ctx);
    bool nl = true, parse_escapes = false; size_t start = 1;
    while (start < args.size() && args[start][0] == '-') {
        if (args[start] == "-n") nl = false;
        else if (args[start] == "-e") parse_escapes = true;
        else break;
        start++;
    }

    for (size_t i = start; i < args.size(); ++i) {
        std::string text = args[i];
        if (parse_escapes) {
            std::string esc;
            for (size_t j = 0; j < text.length(); ++j) {
                if (text[j] == '\\' && j + 1 < text.length()) {
                    char next_c = text[++j];
                    if (next_c == 'n') esc += '\n';
                    else if (next_c == 't') esc += '\t';
                    else if (next_c == 'r') esc += '\r';
                    else if (next_c == '\\') esc += '\\';
                    else esc += next_c;
                } else esc += text[j];
            }
            text = esc;
        }
        std::cout << text << (i + 1 < args.size() ? " " : "");
    }
    if (nl) std::cout << "\n";
    return 0;
}

int cmd_pwd(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "pwd"}, ctx);
    std::cout << utf16_to_utf8(fs::current_path().wstring()) << "\n"; return 0;
}

int cmd_ls(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "ls"}, ctx);
    bool show_all = false, long_fmt = false, one_col = false, json_fmt = false;
    std::vector<std::string> paths;

    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--json") {
            json_fmt = true;
        } else if (args[i][0] == '-') {
            for (size_t j = 1; j < args[i].length(); ++j) {
                if (args[i][j] == 'a') show_all = true;
                if (args[i][j] == 'l') long_fmt = true;
                if (args[i][j] == '1') one_col = true;
            }
        } else {
            paths.push_back(normalize_path(args[i]));
        }
    }
    if (paths.empty()) paths.push_back(".");

    std::error_code ec;

    auto print_entry = [&](const fs::directory_entry& entry) {
        std::string filename = path_to_utf8(entry.path().filename());
        if (!show_all && !filename.empty() && filename[0] == '.') return;

        if (long_fmt) {
            auto size = entry.is_regular_file(ec) ? fs::file_size(entry.path(), ec) : 0;
            std::cout << (entry.is_directory(ec) ? "d " : "- ") 
                      << std::setw(10) << size << " " << filename << "\n";
        } else if (one_col) {
            std::cout << filename << "\n";
        } else {
            std::cout << filename << "  ";
        }
    };

    auto print_json = [&](const std::vector<fs::directory_entry>& entries) {
        std::cout << "[\n";
        bool first = true;
        for (const auto& entry : entries) {
            std::string filename = path_to_utf8(entry.path().filename());
            if (!show_all && !filename.empty() && filename[0] == '.') continue;

            if (!first) std::cout << ",\n";
            first = false;

            auto size = entry.is_regular_file(ec) ? fs::file_size(entry.path(), ec) : 0;
            std::cout << "  {\n"
                      << "    \"name\": \"" << filename << "\",\n"
                      << "    \"type\": \"" << (entry.is_directory(ec) ? "directory" : "file") << "\",\n"
                      << "    \"size\": " << size << "\n"
                      << "  }";
        }
        std::cout << "\n]\n";
    };

    bool print_header = (paths.size() > 1);
    bool first_path = true;
    std::vector<fs::directory_entry> all_json_entries;

    for (const auto& path : paths) {
        fs::path target(utf8_to_utf16(path));
        if (!fs::exists(target, ec)) {
            std::cerr << "ls: " << path << ": No such file or directory\n";
            continue;
        }

        if (json_fmt) {
            if (fs::is_directory(target, ec)) {
                for (const auto& entry : fs::directory_iterator(target, ec)) {
                    all_json_entries.push_back(entry);
                }
            } else {
                all_json_entries.push_back(fs::directory_entry(target));
            }
            continue;
        }

        if (!first_path && fs::is_directory(target, ec)) std::cout << "\n";
        first_path = false;

        if (print_header && fs::is_directory(target, ec)) {
            std::cout << path << ":\n";
        }

        if (fs::is_directory(target, ec)) {
            std::vector<fs::directory_entry> entries;
            for (const auto& entry : fs::directory_iterator(target, ec)) {
                entries.push_back(entry);
            }
            std::sort(entries.begin(), entries.end(), [](const fs::directory_entry& a, const fs::directory_entry& b) {
                std::wstring wa = a.path().filename().wstring();
                std::wstring wb = b.path().filename().wstring();
                std::transform(wa.begin(), wa.end(), wa.begin(), ::towlower);
                std::transform(wb.begin(), wb.end(), wb.begin(), ::towlower);
                return wa < wb;
            });
            for (const auto& entry : entries) print_entry(entry);
            if (!long_fmt && !one_col && !entries.empty()) std::cout << "\n";
        } else {
            print_entry(fs::directory_entry(target));
            if (!long_fmt && !one_col) std::cout << "\n";
        }
    }

    if (json_fmt) {
        print_json(all_json_entries);
    }

    return 0;
}

int cmd_cp(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "cp"}, ctx);
    if (args.size() < 3) return 1;

    bool recursive = false;
    std::vector<std::string> sources;
    std::string dest = normalize_path(args.back());

    for (size_t i = 1; i < args.size() - 1; ++i) {
        if (args[i] == "-r" || args[i] == "-R") recursive = true;
        else sources.push_back(normalize_path(args[i]));
    }

    std::error_code ec;
    fs::path dest_path(utf8_to_utf16(dest));
    bool dest_is_dir = fs::is_directory(dest_path, ec);

    for (const auto& src : sources) {
        fs::path src_path(utf8_to_utf16(src));
        fs::path target = dest_is_dir ? (dest_path / src_path.filename()) : dest_path;

        auto options = fs::copy_options::overwrite_existing;
        if (recursive) options |= fs::copy_options::recursive;

        fs::copy(src_path, target, options, ec);
        if (ec) {
            std::cerr << "cp: cannot copy '" << src << "': " << ec.message() << "\n";
            return 1;
        }
    }
    return 0;
}

int cmd_mv(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "mv"}, ctx);
    if (args.size() < 3) return 1;

    std::vector<std::string> sources;
    std::string dest = normalize_path(args.back());
    for (size_t i = 1; i < args.size() - 1; ++i) sources.push_back(normalize_path(args[i]));

    std::error_code ec;
    fs::path dest_path(utf8_to_utf16(dest));
    bool dest_is_dir = fs::is_directory(dest_path, ec);

    for (const auto& src : sources) {
        fs::path src_path(utf8_to_utf16(src));
        fs::path target = dest_is_dir ? (dest_path / src_path.filename()) : dest_path;

        fs::rename(src_path, target, ec);
        if (ec) {
            std::cerr << "mv: cannot move '" << src << "': " << ec.message() << "\n";
            return 1;
        }
    }
    return 0;
}

int cmd_rm(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "rm"}, ctx);
    if (args.size() < 2) return 1;
    bool recursive = false, force = false;
    std::vector<std::string> targets;

    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i][0] == '-') {
            for (size_t j = 1; j < args[i].length(); ++j) {
                if (args[i][j] == 'r' || args[i][j] == 'R') recursive = true;
                if (args[i][j] == 'f') force = true;
            }
        } else {
            targets.push_back(normalize_path(args[i]));
        }
    }

    std::error_code ec;
    for (const auto& target : targets) {
        std::wstring wtarget = utf8_to_utf16(target);
        if (fs::is_directory(wtarget, ec)) {
            if (!recursive) {
                std::cerr << "rm: " << target << ": is a directory\n";
                if (!force) return 1;
                continue;
            }
            fs::remove_all(wtarget, ec);
        } else {
            fs::remove(wtarget, ec);
        }
        if (ec && !force) {
            std::cerr << "rm: cannot remove '" << target << "': " << ec.message() << "\n";
            return 1;
        }
    }
    return 0;
}

int cmd_sleep(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "sleep"}, ctx);
    if (args.size() > 1) {
        double secs = std::atof(args[1].c_str());
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(secs * 1000)));
    }
    return 0;
}

int cmd_find(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "find"}, ctx);
    std::string root = ".";
    std::string name_pattern;
    char type_filter = 0;
    bool json_fmt = false;
    std::vector<std::string> exec_args;
    bool exec_mode = false;

    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--json") json_fmt = true;
        else if (args[i] == "-name" && i + 1 < args.size()) name_pattern = args[++i];
        else if (args[i] == "-type" && i + 1 < args.size()) type_filter = args[++i][0];
        else if (args[i] == "-exec" || args[i] == "--exec") {
            exec_mode = true;
            if (i + 1 < args.size()) {
                for (size_t j = i + 1; j < args.size(); ++j) {
                    if (args[j] == ";") { i = j; break; }
                    exec_args.push_back(args[j]);
                    if (j == args.size() - 1) i = j;
                }
            }
        }
        else if (args[i][0] != '-') root = normalize_path(args[i]);
    }

    std::string regex_pattern = ".*";
    if (!name_pattern.empty()) {
        regex_pattern.clear();
        for (char c : name_pattern) {
            if (c == '*') regex_pattern += ".*";
            else if (c == '?') regex_pattern += ".";
            else if (std::string(".+^$()[]{}|\\").find(c) != std::string::npos) regex_pattern += "\\" + std::string(1, c);
            else regex_pattern += c;
        }
    }
    std::regex re;
    try {
        re = std::regex(regex_pattern, std::regex_constants::ECMAScript | std::regex_constants::icase);
    } catch (const std::regex_error& e) {
        std::cerr << "find: invalid pattern: " << e.what() << "\n";
        return 1;
    }

    std::vector<fs::directory_entry> matches;
    std::error_code ec;
    size_t scanned_entries = 0;
    for (const auto& entry : fs::recursive_directory_iterator(utf8_to_utf16(root), ec)) {
        if (++scanned_entries > MAX_DIRECTORY_SCAN_ENTRIES) break;
        std::string filename = path_to_utf8(entry.path().filename());
        if (type_filter == 'f' && !entry.is_regular_file(ec)) continue;
        if (type_filter == 'd' && !entry.is_directory(ec)) continue;
        if (std::regex_match(filename, re)) {
            matches.push_back(entry);
        }
    }

    if (exec_mode) {
        for (const auto& entry : matches) {
            std::vector<std::string> cmd = exec_args;
            std::string path_str = path_to_utf8(entry.path());
            for (auto& arg : cmd) {
                if (arg == "{}") arg = path_str;
            }
            if (cmd.empty()) continue;
            CommandNode node;
            node.args = cmd;
            int rc = node.execute(ctx, NULL, NULL);
            if (rc != 0) return rc;
        }
        return 0;
    }

    if (json_fmt) {
        std::cout << "[\n";
        bool first = true;
        for (const auto& entry : matches) {
            if (!first) std::cout << ",\n";
            first = false;
            std::string path_str = path_to_utf8(entry.path());
            for (char &c : path_str) { if (c == '\\') c = '/'; }
            auto size = entry.is_regular_file(ec) ? fs::file_size(entry.path(), ec) : 0;
            std::cout << "  {\n"
                      << "    \"path\": \"" << path_str << "\",\n"
                      << "    \"type\": \"" << (entry.is_directory(ec) ? "directory" : "file") << "\",\n"
                      << "    \"size\": " << size << "\n"
                      << "  }";
        }
        std::cout << "\n]\n";
    } else {
        for (const auto& entry : matches) {
            std::cout << path_to_utf8(entry.path()) << "\n";
        }
    }
    return 0;
}

int cmd_which(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "which"}, ctx);
    if (args.size() <= 1) return 1;
    std::string res = resolve_executable_path(args[1]);
    if (!res.empty()) { std::cout << res << "\n"; return 0; }
    return 1;
}

int cmd_head(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "head"}, ctx);
    size_t lines = 10;
    std::string file_path;

    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-n" && i + 1 < args.size()) lines = std::atoi(args[++i].c_str());
        else file_path = normalize_path(args[i]);
    }

    auto proc = [&](std::istream& in) {
        std::string line; size_t count = 0;
        while (count < lines && std::getline(in, line)) {
            std::cout << line << "\n"; count++;
        }
    };

    if (!file_path.empty()) { std::ifstream file(utf8_to_utf16(file_path)); if (file) proc(file); }
    else proc(std::cin);
    return 0;
}

int cmd_tail(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "tail"}, ctx);
    size_t lines_to_show = 10;
    std::string file_path;
    bool follow = false;

    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-n" && i + 1 < args.size()) lines_to_show = std::atoi(args[++i].c_str());
        else if (args[i] == "-f" || args[i] == "--follow") follow = true;
        else file_path = normalize_path(args[i]);
    }

    auto proc = [&](std::istream& in) {
        std::vector<std::string> buf;
        std::string line;
        while (true) {
            if (!std::getline(in, line)) {
                if (follow) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }
                break;
            }
            if (!follow) {
                if (buf.size() >= lines_to_show) buf.erase(buf.begin());
                buf.push_back(line);
            } else {
                std::cout << line << "\n";
            }
        }
        if (!follow) {
            for (const auto& l : buf) std::cout << l << "\n";
        }
    };

    if (!file_path.empty()) { std::ifstream file(utf8_to_utf16(file_path)); if (file) proc(file); }
    else proc(std::cin);
    return 0;
}

int cmd_xargs(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && (args[1] == "--help" || args[1] == "-h")) return cmd_help({"help", "xargs"}, ctx);
    bool null_delimited = false;
    std::string replace_token;
    std::vector<std::string> base_cmd = {"echo"};
    std::vector<std::string> input_tokens;

    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-0" || args[i] == "--null") null_delimited = true;
        else if (args[i] == "-I" || args[i] == "--replace" || args[i] == "-i") {
            if (i + 1 < args.size()) replace_token = args[++i];
            else replace_token = "{}";
        }
        else if (args[i][0] != '-') base_cmd.push_back(args[i]);
    }

    if (null_delimited) {
        std::string token;
        while (std::getline(std::cin, token, '\0')) input_tokens.push_back(token);
    } else {
        std::string token;
        while (std::cin >> token) input_tokens.push_back(token);
    }

    if (input_tokens.empty()) return 0;

    if (replace_token.empty()) {
        std::vector<std::string> full_cmd = base_cmd;
        for (const auto& token : input_tokens) full_cmd.push_back(token);
        CommandNode node;
        node.args = full_cmd;
        return node.execute(ctx, NULL, NULL);
    }

    for (const auto& token : input_tokens) {
        std::vector<std::string> cmd = base_cmd;
        bool replaced = false;
        for (auto& arg : cmd) {
            if (arg == replace_token) {
                arg = token;
                replaced = true;
            }
        }
        if (!replaced) cmd.push_back(token);
        CommandNode node;
        node.args = cmd;
        int rc = node.execute(ctx, NULL, NULL);
        if (rc != 0) return rc;
    }
    return 0;
}

int cmd_export(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1) {
        size_t eq = args[1].find('=');
        if (eq != std::string::npos) ctx.set_var(args[1].substr(0, eq), args[1].substr(eq + 1));
    }
    return 0;
}

int cmd_unset(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1) ctx.unset_var(args[1]);
    return 0;
}

int cmd_env(const std::vector<std::string>&, ShellContext& ctx) {
    std::lock_guard<std::mutex> lock(ctx.ctx_mutex);
    for (const auto& [k, v] : ctx.variables) {
        std::cout << k << "=" << v << "\n";
    }
    return 0;
}

int cmd_true(const std::vector<std::string>&, ShellContext&) { return 0; }
int cmd_false(const std::vector<std::string>&, ShellContext&) { return 1; }
int cmd_jobs(const std::vector<std::string>&, ShellContext& ctx) {
    std::lock_guard<std::mutex> lock(ctx.ctx_mutex);
    for (auto& job : ctx.jobs) {
        if (job.is_running) {
            DWORD code = 0;
            if (GetExitCodeProcess(job.hProcess, &code) && code != STILL_ACTIVE) {
                job.is_running = false;
                CloseHandle(job.hProcess);
                job.hProcess = NULL;
            }
        }
        std::cout << "[" << job.id << "] " 
                  << (job.is_running ? "Running" : "Done")
                  << "          " << job.command << "\n";
    }
    return 0;
}

int cmd_fg(const std::vector<std::string>& args, ShellContext& ctx) {
    int target_id = -1;
    if (args.size() > 1) {
        std::string arg = args[1];
        if (arg[0] == '%') arg = arg.substr(1);
        try { target_id = std::stoi(arg); } catch (...) {}
    } else {
        std::lock_guard<std::mutex> lock(ctx.ctx_mutex);
        if (!ctx.jobs.empty()) target_id = ctx.jobs.back().id;
    }

    if (target_id == -1) {
        std::cerr << "fg: no current job\n";
        return 1;
    }

    HANDLE hProc = NULL;
    std::string cmd;
    {
        std::lock_guard<std::mutex> lock(ctx.ctx_mutex);
        for (auto& job : ctx.jobs) {
            if (job.id == target_id && job.is_running) {
                hProc = job.hProcess;
                cmd = job.command;
                job.is_running = false;
                job.hProcess = NULL;
                break;
            }
        }
    }

    if (!hProc) {
        std::cerr << "fg: job " << target_id << " not found or already finished\n";
        return 1;
    }

    std::cout << cmd << "\n";
    register_active_process(hProc);
    DWORD wait_res = WaitForSingleObject(hProc, ctx.timeout_ms);
    unregister_active_process(hProc);

    if (wait_res == WAIT_TIMEOUT) {
        TerminateProcess(hProc, 124);
        WaitForSingleObject(hProc, 1000);
        CloseHandle(hProc);
        std::cerr << "fg: job " << target_id << " timed out after " << ctx.timeout_ms << "ms\n";
        return 124;
    }

    DWORD code = 0;
    GetExitCodeProcess(hProc, &code);
    CloseHandle(hProc);
    return static_cast<int>(code);
}

int cmd_bg(const std::vector<std::string>& args, ShellContext& ctx) {
    int target_id = -1;
    if (args.size() > 1) {
        std::string arg = args[1];
        if (arg[0] == '%') arg = arg.substr(1);
        try { target_id = std::stoi(arg); } catch (...) {}
    } else {
        std::lock_guard<std::mutex> lock(ctx.ctx_mutex);
        if (!ctx.jobs.empty()) target_id = ctx.jobs.back().id;
    }

    if (target_id == -1) {
        std::cerr << "bg: no current job\n";
        return 1;
    }

    std::lock_guard<std::mutex> lock(ctx.ctx_mutex);
    for (auto& job : ctx.jobs) {
        if (job.id == target_id) {
            if (job.is_running) {
                std::cout << "[" << job.id << "] " << job.command << " &\n";
                return 0;
            } else {
                std::cerr << "bg: job " << target_id << " already finished\n";
                return 1;
            }
        }
    }
    std::cerr << "bg: job " << target_id << " not found\n";
    return 1;
}

int cmd_history(const std::vector<std::string>&, ShellContext& ctx) {
    std::lock_guard<std::mutex> lock(ctx.ctx_mutex);
    for (size_t i = 0; i < ctx.history.size(); ++i) {
        std::cout << "  " << (i + 1) << "  " << ctx.history[i] << "\n";
    }
    return 0;
}

std::string expand_history_designators(const std::string& line, ShellContext& ctx) {
    if (line.empty()) return line;
    std::lock_guard<std::mutex> lock(ctx.ctx_mutex);
    if (ctx.history.empty()) return line;

    std::string expanded;
    size_t i = 0, len = line.length();
    while (i < len) {
        if (line[i] == '!' && i + 1 < len && line[i + 1] != ' ' && line[i + 1] != '\t' && line[i + 1] != '=') {
            i++;
            if (line[i] == '!') {
                expanded += ctx.history.back();
                i++;
            } else if (line[i] == '-') {
                i++;
                size_t start = i;
                while (i < len && std::isdigit(static_cast<unsigned char>(line[i]))) i++;
                std::string num_str = line.substr(start, i - start);
                if (!num_str.empty()) {
                    try {
                        int offset = std::stoi(num_str);
                        if (offset > 0 && offset <= static_cast<int>(ctx.history.size())) {
                            expanded += ctx.history[ctx.history.size() - offset];
                        }
                    } catch (...) {}
                }
            } else if (std::isdigit(static_cast<unsigned char>(line[i]))) {
                size_t start = i;
                while (i < len && std::isdigit(static_cast<unsigned char>(line[i]))) i++;
                std::string num_str = line.substr(start, i - start);
                try {
                    int index = std::stoi(num_str);
                    if (index > 0 && index <= static_cast<int>(ctx.history.size())) {
                        expanded += ctx.history[index - 1];
                    }
                } catch (...) {}
            } else {
                size_t start = i;
                while (i < len && line[i] != ' ' && line[i] != '\t' && line[i] != '\n') i++;
                std::string prefix = line.substr(start, i - start);
                bool found = false;
                for (auto it = ctx.history.rbegin(); it != ctx.history.rend(); ++it) {
                    if (it->rfind(prefix, 0) == 0) {
                        expanded += *it;
                        found = true;
                        break;
                    }
                }
                if (!found) expanded += "!" + prefix;
            }
        } else {
            expanded += line[i++];
        }
    }
    return expanded;
}

int cmd_read(const std::vector<std::string>& args, ShellContext& ctx) {
    bool raw = false;
    std::string prompt;
    std::vector<std::string> var_names;

    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-r") raw = true;
        else if (args[i] == "-p" && i + 1 < args.size()) prompt = args[++i];
        else var_names.push_back(args[i]);
    }

    if (var_names.empty()) var_names.push_back("REPLY");

    if (!prompt.empty()) std::cout << prompt;
    std::string line;
    if (!std::getline(std::cin, line)) return 1;

    if (raw) {
        for (size_t i = 0; i < var_names.size(); ++i) {
            if (i == 0) ctx.set_var(var_names[i], line);
            else ctx.set_var(var_names[i], "");
        }
        return 0;
    }

    std::stringstream ss(line);
    std::vector<std::string> words;
    std::string word;
    while (ss >> word) words.push_back(word);

    for (size_t i = 0; i < var_names.size(); ++i) {
        if (i < words.size()) {
            if (i == var_names.size() - 1) {
                std::string rest = words[i];
                for (size_t j = i + 1; j < words.size(); ++j) {
                    rest += " " + words[j];
                }
                ctx.set_var(var_names[i], rest);
            } else {
                ctx.set_var(var_names[i], words[i]);
            }
        } else {
            ctx.set_var(var_names[i], "");
        }
    }
    return 0;
}

int cmd_help(const std::vector<std::string>& args, ShellContext& ctx) {
    if (args.size() > 1 && args[1] != "--help" && args[1] != "-h") {
        std::string applet = to_lower(args[1]);
        if (applet == "sh" || applet == "bash" || applet == "ksh") {
            std::cout << "Usage: busybox " << applet << " [-c 'command'] [script.sh] [args...]\n\n"
                      << "  POSIX-style shell engine for interactive use and script execution.\n\n"
                      << "  Invocation:\n"
                      << "    busybox sh                         Start an interactive shell.\n"
                      << "    busybox sh -c \"COMMANDS\"          Execute commands and exit.\n"
                      << "    busybox sh SCRIPT.sh [ARG...]       Run a script file.\n"
                      << "    busybox -- sh SCRIPT.sh [ARG...]    Explicitly select the shell applet.\n\n"
                      << "  Directory switching:\n"
                      << "    cd                              Show no output and keep the current directory.\n"
                      << "    cd project                      Change to a relative directory.\n"
                      << "    cd project\\src                  Windows path separators are accepted.\n"
                      << "    cd /project/src                  Slash paths are accepted on Windows.\n"
                      << "    cd C:/work/project              Drive-qualified slash paths are accepted.\n"
                      << "    pwd                             Print the directory selected by cd.\n"
                      << "  The directory change belongs to this BusyBox shell process and is used by\n"
                      << "  later commands in the same -c string or script.\n\n"
                      << "  Language features:\n"
                      << "    Variables:       NAME=value; echo $NAME; echo ${NAME:-default}\n"
                      << "    Environment:     export NAME=value; unset NAME; env\n"
                      << "    Substitution:    echo $(pwd) or echo `pwd`\n"
                      << "    Globbing:        ls *.txt and find src -name \"*.cpp\"\n"
                      << "    Pipelines:       cat input.txt | grep error | wc -l\n"
                      << "    Redirection:     command > output.txt, command >> log.txt, command < input.txt\n"
                      << "    Conditions:      if test -e file; then echo found; else echo missing; fi\n"
                      << "    Loops:           for f in *.txt; do echo $f; done\n"
                      << "    While loops:     while test -e ready.flag; do sleep 1; done\n"
                      << "    Chaining:        first && second, first || fallback, first; second\n"
                      << "    Background:      long-command &, jobs, fg [JOB], bg [JOB]\n\n"
                      << "  Script arguments:\n"
                      << "    In SCRIPT.sh, $0 is the script name, $1 and later are supplied arguments,\n"
                      << "    and $# contains the argument count. Use quoted values for paths with spaces.\n\n"
                      << "  Examples:\n"
                      << "    busybox sh\n"
                      << "    busybox sh -c \"cd src; pwd; ls -1\"\n"
                      << "    busybox sh -c \"cd C:/work/project && grep -n error logs/*.txt\"\n"
                      << "    busybox sh -c \"if test -e input.txt; then echo found; fi\"\n"
                      << "    busybox sh -c \"for i in $(seq 1 3); do echo $i; done\"\n"
                      << "    busybox sh build.sh release x64\n"
                      << "    busybox sh -c \"cat input.txt | grep error > errors.txt\"\n";
        } else if (applet == "seq") {
            std::cout << "Usage: seq [START [STEP]] END\n\n"
                      << "  Print a numeric sequence, one value per line.\n\n"
                      << "  Examples:\n"
                      << "    seq 5\n"
                      << "    seq 1 5\n"
                      << "    seq 10 2 20\n";
        } else if (applet == "touch") {
            std::cout << "Usage: touch [-c|--no-create] FILE [FILE...]\n\n"
                      << "  Create files if needed and update timestamps.\n\n"
                      << "  Examples:\n"
                      << "    touch flag.txt\n"
                      << "    touch -c existing.log\n";
        } else if (applet == "date") {
            std::cout << "Usage: date [+FORMAT]\n\n"
                      << "  Print the current local date and time.\n\n"
                      << "  Examples:\n"
                      << "    date\n"
                      << "    date +%Y-%m-%d\n"
                      << "    date \"+%Y-%m-%d %H:%M:%S\"\n";
        } else if (applet == "mktemp") {
            std::cout << "Usage: mktemp [-d] [TEMPLATE]\n\n"
                      << "  Create a unique temporary file or directory.\n\n"
                      << "  Examples:\n"
                      << "    mktemp\n"
                      << "    mktemp temp-XXX.txt\n"
                      << "    mktemp -d temp-dir-XXX\n";
        } else if (applet == "kill") {
            std::cout << "Usage: kill PID [PID...]\n\n"
                      << "  Terminate one or more processes by PID.\n\n"
                      << "  Examples:\n"
                      << "    kill 1234\n"
                      << "    kill 1234 5678\n";
        } else if (applet == "clear" || applet == "cls") {
            std::cout << "Usage: clear\n       cls\n\n"
                      << "  Clear the terminal screen and move the cursor to the top-left corner.\n\n"
                      << "  Example:\n"
                      << "    clear\n";
        } else if (applet == "whoami") {
            std::cout << "Usage: whoami\n\n"
                      << "  Print the current Windows user name.\n";
        } else if (applet == "uname") {
            std::cout << "Usage: uname [-a|-s|-n|-r|-v|-m]\n\n"
                      << "  Print system identification information.\n\n"
                      << "  Examples:\n"
                      << "    uname\n"
                      << "    uname -a\n";
        } else if (applet == "base64") {
            std::cout << "Usage: base64 [-d|--decode] [TEXT]\n\n"
                      << "  Encode or decode Base64 text. When no TEXT is supplied, read from stdin.\n\n"
                      << "  Examples:\n"
                      << "    base64 hello\n"
                      << "    base64 -d aGVsbG8=\n";
        } else if (applet == "md5sum" || applet == "sha256sum") {
            std::cout << "Usage: " << applet << " [FILE...]\n\n"
                      << "  Print a checksum for each file or for stdin when no file is supplied.\n\n"
                      << "  Examples:\n"
                      << "    " << applet << " download.zip\n"
                      << "    type package.bin | " << applet << "\n";
        } else if (applet == "cmp") {
            std::cout << "Usage: cmp FILE1 FILE2\n\n"
                      << "  Compare two files byte by byte and stop at the first difference.\n\n"
                      << "  Example:\n"
                      << "    cmp a.txt b.txt\n";
        } else if (applet == "diff") {
            std::cout << "Usage: diff FILE1 FILE2\n\n"
                      << "  Show a simple line-by-line difference between two files.\n\n"
                      << "  Example:\n"
                      << "    diff old.txt new.txt\n";
        } else if (applet == "rev") {
            std::cout << "Usage: rev [FILE]\n\n"
                      << "  Reverse each input line.\n\n"
                      << "  Examples:\n"
                      << "    rev\n"
                      << "    rev names.txt\n";
        } else if (applet == "paste") {
            std::cout << "Usage: paste FILE [FILE...]\n\n"
                      << "  Merge lines from multiple files side by side.\n\n"
                      << "  Example:\n"
                      << "    paste left.txt right.txt\n";
        } else if (applet == "stat") {
            std::cout << "Usage: stat FILE [FILE...]\n\n"
                      << "  Display file metadata such as size and timestamps.\n\n"
                      << "  Example:\n"
                      << "    stat report.txt\n";
        } else if (applet == "du") {
            std::cout << "Usage: du [PATH...]\n\n"
                      << "  Estimate disk usage for files and directories.\n\n"
                      << "  Examples:\n"
                      << "    du\n"
                      << "    du src\n";
        } else if (applet == "df") {
            std::cout << "Usage: df [PATH]\n\n"
                      << "  Show free and used space for the selected drive or path.\n\n"
                      << "  Examples:\n"
                      << "    df\n"
                      << "    df C:\\n";
        } else if (applet == "ps") {
            std::cout << "Usage: ps\n\n"
                      << "  List running processes.\n";
        } else if (applet == "realpath") {
            std::cout << "Usage: realpath PATH\n\n"
                      << "  Resolve a path to its canonical form.\n\n"
                      << "  Example:\n"
                      << "    realpath .\\src\\..\\README.md\n";
        } else if (applet == "readlink") {
            std::cout << "Usage: readlink PATH\n\n"
                      << "  Resolve a symbolic link or return the canonical path.\n\n"
                      << "  Example:\n"
                      << "    readlink link.txt\n";
        } else if (applet == "ln") {
            std::cout << "Usage: ln [-s] SOURCE LINK_NAME\n\n"
                      << "  Create a hard link by default, or a symbolic link with -s.\n\n"
                      << "  Examples:\n"
                      << "    ln source.txt source-copy.txt\n"
                      << "    ln -s source.txt source-link.txt\n";
        } else if (applet == "grep") {
            std::cout << "Usage: grep [-i] [-v] [-n] [-c] [-q] [-r] [-l] PATTERN [FILE...]\n\n"
                      << "  Search for ECMAScript regular expressions in FILE(s) or stdin.\n";
        } else if (applet == "sed") {
            std::cout << "Usage: sed [-i] 's/PATTERN/REPLACEMENT/[g]' [FILE]\n\n"
                      << "  Stream editor for filtering and transforming text using regular expressions.\n";
        } else if (applet == "awk") {
            std::cout << "Usage: awk [-F DELIM] 'script' [FILE]\n\n"
                      << "  Pattern scanning and processing language ($1, $2, $0, NF, NR).\n";
        } else if (applet == "test" || applet == "[") {
            std::cout << "Usage: test EXPRESSION  or  [ EXPRESSION ]\n\n"
                      << "  Evaluate expression and return exit code 0 (true) or 1 (false).\n";
        } else {
            std::cout << "Usage details available for applet: " << applet << "\n";
        }
        return 0;
    }

    std::cout << R"(busybox(1)             CrossShell for UNIX Reference Manual              busybox(1)

    NAME
        busybox - tiny UNIX and POSIX multi-call utility toolkit for Windows

    SYNOPSIS
        busybox [applet [arguments...]]
        busybox --help | help [applet]
        busybox --version
        busybox --self-test
        <applet> [arguments...]

    DESCRIPTION
        BusyBox combines tiny versions of many common UNIX utilities into a
        single multi-call binary.

    OPTIONS
        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    AVAILABLE APPLETS
        sh, ksh, bash, seq, touch, date, mktemp, kill, clear, cls, whoami, uname,
        base64, md5sum, sha256sum, cmp, diff, rev, paste, stat, du, bdf, ps,
        realpath, readlink, ln, grep, sed, awk, test, [, echo, cat, pwd, ls,
        cp, mv, rm, sleep, find, which, head, tail, xargs, export, unset, env,
        true, false, read, history, jobs, fg, bg, help, wc, tr, cut, tee,
        sort, uniq, basename, dirname, mkdir, rmdir

    EXAMPLES
        busybox sh -c "cd src && pwd && ls -1"
            Execute shell command script.

        busybox sha256sum busybox.exe
            Compute SHA256 checksum.

        busybox help seq
            Display help for specific applet.

    CrossShell for UNIX                                                    busybox(1)
)";
    return 0;
}

std::map<std::string, AppletFunc> applets = {
    {"sh", cmd_sh}, {"ksh", cmd_sh}, {"bash", cmd_sh},
    {"grep", cmd_grep}, {"sed", cmd_sed}, {"awk", cmd_awk},
    {"test", cmd_test}, {"[", cmd_test}, {"echo", cmd_echo},
    {"cat", cmd_cat}, {"pwd", cmd_pwd}, {"ls", cmd_ls},
    {"cp", cmd_cp}, {"mv", cmd_mv}, {"rm", cmd_rm},
    {"sleep", cmd_sleep}, {"find", cmd_find}, {"which", cmd_which},
    {"head", cmd_head}, {"tail", cmd_tail}, {"xargs", cmd_xargs},
    {"export", cmd_export}, {"unset", cmd_unset}, {"env", cmd_env},
    {"true", cmd_true}, {"false", cmd_false}, {"read", cmd_read}, {"history", cmd_history},
    {"jobs", cmd_jobs}, {"fg", cmd_fg}, {"bg", cmd_bg}, {"help", cmd_help},
    {"wc", cmd_wc}, {"tr", cmd_tr}, {"cut", cmd_cut}, {"tee", cmd_tee},
    {"sort", cmd_sort}, {"uniq", cmd_uniq}, {"basename", cmd_basename},
    {"dirname", cmd_dirname}, {"mkdir", cmd_mkdir}, {"rmdir", cmd_rmdir},
    {"seq", cmd_seq}, {"touch", cmd_touch}, {"date", cmd_date}, {"mktemp", cmd_mktemp},
    {"kill", cmd_kill}, {"clear", cmd_clear_or_cls}, {"cls", cmd_clear_or_cls},
    {"whoami", cmd_whoami}, {"uname", cmd_uname}, {"base64", cmd_base64},
    {"md5sum", cmd_md5sum}, {"sha256sum", cmd_sha256sum}, {"cmp", cmd_cmp},
    {"diff", cmd_diff}, {"rev", cmd_rev}, {"paste", cmd_paste}, {"stat", cmd_stat},
    {"du", cmd_du}, {"bdf", cmd_bdf}, {"ps", cmd_ps}, {"realpath", cmd_realpath},
    {"readlink", cmd_readlink}, {"ln", cmd_ln}
};

int run_self_test() {
    int failures = 0;
    auto check = [&](const std::string& name, bool passed) {
        std::cout << "SELF-TEST " << (passed ? "PASS" : "FAIL") << ": " << name << "\n";
        if (!passed) failures++;
    };

    std::error_code ec;
    fs::path original_directory = fs::current_path(ec);
    fs::path test_root = fs::temp_directory_path(ec) /
        ("busybox_self_test_" + std::to_string(GetCurrentProcessId()));
    fs::remove_all(test_root, ec);
    fs::create_directories(test_root / "nested", ec);
    if (ec) {
        std::cerr << "SELF-TEST ERROR: cannot create temporary test directory\n";
        return 1;
    }

    ShellContext ctx(false);
    const std::string test_variable = "BUSYBOX_SELF_TEST_VAR";
    DWORD old_variable_length = GetEnvironmentVariableW(utf8_to_utf16(test_variable).c_str(), nullptr, 0);
    std::wstring old_variable;
    bool had_old_variable = old_variable_length > 0;
    if (had_old_variable) {
        old_variable.resize(old_variable_length - 1);
        GetEnvironmentVariableW(utf8_to_utf16(test_variable).c_str(), old_variable.data(), old_variable_length);
    }
    SetEnvironmentVariableW(utf8_to_utf16(test_variable).c_str(), nullptr);

    check("applet dispatch and sequence output",
        capture_cmd_output("seq 1 3", ctx) == "1\n2\n3");
    check("assignment prefix executes the command",
        capture_cmd_output("TEST_VALUE=visible echo $TEST_VALUE", ctx) == "visible");

    Parser assignment_parser(tokenize("TEST_VALUE=stateful"));
    auto assignment_ast = assignment_parser.parse_chain();
    if (assignment_ast) assignment_ast->execute(ctx, NULL, NULL);
    check("shell assignment updates shell state", ctx.get_var("TEST_VALUE") == "stateful");

    fs::path saved_directory = fs::current_path(ec);
    Parser cwd_parser(tokenize("(cd \"" + path_to_utf8(test_root / "nested") + "\")"));
    auto cwd_ast = cwd_parser.parse_chain();
    if (cwd_ast) cwd_ast->execute(ctx, NULL, NULL);
    check("subshell restores current directory", fs::current_path(ec) == saved_directory);

    Parser pipeline_cwd_parser(tokenize("(cd \"" + path_to_utf8(test_root / "nested") + "\") | pwd"));
    auto pipeline_cwd_ast = pipeline_cwd_parser.parse_chain();
    if (pipeline_cwd_ast) pipeline_cwd_ast->execute(ctx, NULL, NULL);
    check("pipeline directory changes do not leak", fs::current_path(ec) == saved_directory);

    Parser environment_parser(tokenize("(BUSYBOX_SELF_TEST_VAR=inner)"));
    auto environment_ast = environment_parser.parse_chain();
    if (environment_ast) environment_ast->execute(ctx, NULL, NULL);
    DWORD leaked_length = GetEnvironmentVariableW(utf8_to_utf16(test_variable).c_str(), nullptr, 0);
    check("subshell restores process environment", leaked_length == 0);
    check("command substitution isolates shell variables",
        capture_cmd_output("(BUSYBOX_SELF_TEST_VAR=inner); echo $BUSYBOX_SELF_TEST_VAR", ctx).empty());

    Parser exit_parser(tokenize("exit 7"));
    auto exit_ast = exit_parser.parse_chain();
    int exit_status = 0;
    try {
        if (exit_ast) exit_ast->execute(ctx, NULL, NULL);
    } catch (const ShellExitSignal& signal) {
        exit_status = signal.code();
    }
    check("nested exit returns a shell status", exit_status == 7);

    fs::path script_path = test_root / "self_test.sh";
    fs::path script_output = test_root / "script_output.txt";
    {
        std::ofstream script(script_path, std::ios::binary);
        script << "echo $1 > \"" << path_to_utf8(script_output) << "\"\n";
    }
    int script_status = cmd_sh({"sh", path_to_utf8(script_path), "script-argument"}, ctx);
    std::ifstream script_result(script_output, std::ios::binary);
    std::string script_value((std::istreambuf_iterator<char>(script_result)), std::istreambuf_iterator<char>());
    while (!script_value.empty() && (script_value.back() == '\r' || script_value.back() == '\n')) script_value.pop_back();
    check("script execution and positional arguments", script_status == 0 && script_value == "script-argument");

    fs::path oversized_script = test_root / "oversized.sh";
    {
        std::ofstream script(oversized_script, std::ios::binary);
        script << std::string(MAX_SCRIPT_BYTES + 1, 'x');
    }
    check("oversized scripts are rejected", cmd_sh({"sh", path_to_utf8(oversized_script)}, ctx) == 126);

    std::wstring saved_userprofile;
    DWORD userprofile_length = GetEnvironmentVariableW(L"USERPROFILE", nullptr, 0);
    bool had_userprofile = userprofile_length > 0;
    if (had_userprofile) {
        saved_userprofile.resize(userprofile_length - 1);
        GetEnvironmentVariableW(L"USERPROFILE", saved_userprofile.data(), userprofile_length);
    }
    fs::path history_root = test_root / "history_home";
    fs::create_directories(history_root, ec);
    {
        std::ofstream history(history_root / ".busybox_history", std::ios::binary);
        for (size_t i = 0; i < MAX_HISTORY_ENTRIES + 100; ++i) history << "history-" << i << "\n";
    }
    SetEnvironmentVariableW(L"USERPROFILE", history_root.wstring().c_str());
    ShellContext history_ctx(true);
    check("history loading is bounded", history_ctx.history.size() == MAX_HISTORY_ENTRIES);
    if (had_userprofile) SetEnvironmentVariableW(L"USERPROFILE", saved_userprofile.c_str());
    else SetEnvironmentVariableW(L"USERPROFILE", nullptr);

    ctx.timeout_ms = 50;
    Parser timeout_parser(tokenize("sleep 2 &; fg"));
    auto timeout_ast = timeout_parser.parse_chain();
    int timeout_status = timeout_ast ? timeout_ast->execute(ctx, NULL, NULL) : 1;
    check("foreground jobs honor shell timeout", timeout_status == 124);

    for (auto& job : ctx.jobs) {
        if (job.hProcess != NULL) {
            TerminateProcess(job.hProcess, 124);
            CloseHandle(job.hProcess);
            job.hProcess = NULL;
        }
    }

    SetEnvironmentVariableW(
        utf8_to_utf16(test_variable).c_str(),
        had_old_variable ? old_variable.c_str() : nullptr);
    if (original_directory != fs::current_path(ec)) {
        fs::current_path(original_directory, ec);
    }
    fs::remove_all(test_root, ec);

    std::cout << "SELF-TEST RESULT: " << (failures == 0 ? "PASS" : "FAIL")
              << " (" << failures << " failure(s))\n";
    return failures == 0 ? 0 : 1;
}

int main(int argc, char* argv[]) {
    init_vt100_and_binary_mode();
    init_job_object();
    SetConsoleCtrlHandler(console_signal_handler, TRUE);

    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i) args.push_back(argv[i]);

    if (args.size() > 1 && to_lower(args[1]) == "--self-test") {
        return run_self_test();
    }

    ShellContext ctx;

    try {
        if (!args.empty()) {
            std::string prog_name = fs::path(utf8_to_utf16(args[0])).stem().string();
            std::transform(prog_name.begin(), prog_name.end(), prog_name.begin(), ::tolower);
            if (applets.count(prog_name)) {
                return applets[prog_name](args, ctx);
            }
        }

        if (args.size() > 1) {
            std::string first_arg = to_lower(args[1]);
            if (first_arg == "--") {
                if (args.size() > 2) {
                    std::string next = to_lower(args[2]);
                    if (applets.count(next)) {
                        std::vector<std::string> applet_args(args.begin() + 2, args.end());
                        return applets[next](applet_args, ctx);
                    }
                }
                return cmd_sh(args, ctx);
            }
            if (first_arg == "--help" || first_arg == "-h" || first_arg == "help") {
                std::vector<std::string> help_args = (args.size() > 2) ? std::vector<std::string>{"help", args[2]} : std::vector<std::string>{"help"};
                return cmd_help(help_args, ctx);
            }
            if (first_arg == "--version" || first_arg == "-v") {
                std::cout << "busybox\n";
                return 0;
            }
            if (applets.count(first_arg)) {
                std::vector<std::string> applet_args(args.begin() + 1, args.end());
                return applets[first_arg](applet_args, ctx);
            }
        }

        return cmd_sh(args, ctx);
    } catch (const std::exception& e) {
        std::cerr << "busybox: fatal error: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "busybox: fatal error: unknown exception\n";
        return 1;
    }
}
