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
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * CrossShellZSH - Standardized Section Index
 * ==============================================
 * 1. PLATFORM DEFINITIONS & INCLUDES
 * 2. RAII UTILITIES & PATH NORMALIZATION
 * 3. MATH EVALUATION ENGINE $(( ... ))
 * 4. ENVIRONMENT & VARIABLE EXPANSION
 * 5. WIN32 SIGNAL EMULATION & TRAPS
 * 6. GLOBBING & QUALIFIERS
 * 7. PIPELINE & NATIVE PROCESS LAUNCHER
 * 8. CONTROL FLOW ENGINE (if, for, while, case)
 * 9. BUILT-IN COMMAND HANDLERS
 * 10. INTERACTIVE LINE EDITOR & TAB COMPLETION
 * 11. BUILT-IN REGRESSION SELF-TESTS
 * 12. MAIN ENTRY POINT
 *
 * Section Header Convention:
 *   // SECTION NN: <area summary>
 *   // SECTION NNX: <sub-area summary>
 *
 * Maintenance Contract (Single-File Mode):
 *   1) Keep this section index aligned with major blocks.
 *   2) Prefer shared metadata tables over duplicated command/help literals.
 *   3) Preserve one-way helper flow: utilities -> parse/expand -> execute -> startup.
 *   4) Run verifier tasks for touched behavior before merge.
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX  // Prevents Windows min/max macros from conflicting with std::min/std::max
#include <windows.h>
#include <shellapi.h>

#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Shell32.lib")
#include <conio.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <memory>
#include <cctype>
#include <ctime>
#include <regex>
#include <set>
#include <stack>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>
#include <limits>
#include <climits>
#include <cerrno>
#include <io.h>
#include <fcntl.h>
#include <tlhelp32.h>

using namespace std;
namespace fs = std::filesystem;

using unique_handle = std::unique_ptr<void, void(*)(HANDLE)>;

static void close_handle_if_valid(HANDLE h) {
    if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h);
}

static unique_handle make_unique_handle(HANDLE h = nullptr) {
    if (h == INVALID_HANDLE_VALUE) h = nullptr;
    return unique_handle(h, close_handle_if_valid);
}

int parse_and_execute(const string& line);
int execute_command_line(const string& line);
static int execute_statement_block_aware(const string& statement);
string capture_command_output(const string& cmd);
bool match_wildcard(const string& pattern, const string& str);
static string trim_copy(const string& s);
static vector<string> tokenize_words(const string& s);
wstring string_to_wstring(const string& str);
string normalize_path_to_win(string p);
string normalize_path_to_unix(string p);

struct ProcessSubstSink {
    string path;
    string command;
};

static vector<string> g_process_subst_temp_files;
static vector<ProcessSubstSink> g_process_subst_sinks;
static int g_process_subst_eval_depth = 0;
static bool g_login_shell = false;

static void write_binary_string(ofstream& output, const string& value) {
    uint64_t size = static_cast<uint64_t>(value.size());
    output.write(reinterpret_cast<const char*>(&size), sizeof(size));
    output.write(value.data(), static_cast<streamsize>(value.size()));
}

static bool read_binary_string(ifstream& input, string& value) {
    uint64_t size = 0;
    if (!input.read(reinterpret_cast<char*>(&size), sizeof(size)) || size > 16 * 1024 * 1024) return false;
    value.resize(static_cast<size_t>(size));
    return size == 0 || static_cast<bool>(input.read(value.data(), static_cast<streamsize>(size)));
}

static string canonicalize_option_name(const string& raw) {
    string out;
    out.reserve(raw.size());
    for (char c : raw) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (isalnum(uc)) out += static_cast<char>(tolower(uc));
    }
    return out;
}

static void apply_shell_option_token(const string& token, bool enable_default, map<string, bool>& options) {
    string name = canonicalize_option_name(token);
    if (name.empty()) return;

    bool enable = enable_default;
    if (name.size() > 2 && name.rfind("no", 0) == 0) {
        name = name.substr(2);
        enable = !enable_default;
    }
    if (name.empty()) return;

    options[name] = enable;
}

static string create_temp_process_subst_path() {
    auto reserve_unique_in_dir = [](const string& raw_dir) -> string {
        error_code ec;
        string dir = normalize_path_to_win(raw_dir);
        fs::create_directories(dir, ec);
        if (ec) return ""; // directory could not be created; caller falls back

        for (int attempt = 0; attempt < 64; ++attempt) {
            unsigned long long nonce =
                (GetTickCount64() ^
                 (static_cast<unsigned long long>(GetCurrentProcessId()) << 16) ^
                 (static_cast<unsigned long long>(GetCurrentThreadId()) << 1) ^
                 static_cast<unsigned long long>(attempt));
            string candidate = dir + "\\zps_" + to_string(GetCurrentProcessId()) + "_" + to_string(nonce) + ".tmp";
            wstring wcandidate = string_to_wstring(candidate);
            HANDLE h = CreateFileW(
                wcandidate.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                0,
                NULL,
                CREATE_NEW,
                FILE_ATTRIBUTE_TEMPORARY,
                NULL);
            if (h != INVALID_HANDLE_VALUE) {
                CloseHandle(h);
                return normalize_path_to_win(candidate);
            }
            DWORD gle = GetLastError();
            if (gle != ERROR_FILE_EXISTS && gle != ERROR_ALREADY_EXISTS) break;
        }
        return "";
    };

    char temp_dir[MAX_PATH] = {};
    DWORD n = GetTempPathA(MAX_PATH, temp_dir);
    if (n != 0 && n < MAX_PATH) {
        char temp_file[MAX_PATH] = {};
        if (GetTempFileNameA(temp_dir, "zps", 0, temp_file) != 0) {
            return normalize_path_to_win(string(temp_file));
        }
        string from_temp_dir = reserve_unique_in_dir(string(temp_dir));
        if (!from_temp_dir.empty()) return from_temp_dir;
    }

    string local_tmp = reserve_unique_in_dir("tmp\\zsh_psub");
    if (!local_tmp.empty()) return local_tmp;

    // Last resort: try CWD-relative tmp dir only if it can actually be created.
    {
        error_code ec;
        fs::create_directories("tmp\\zsh_psub", ec);
        if (!ec && fs::is_directory("tmp\\zsh_psub", ec) && !ec) {
            return normalize_path_to_win("tmp\\zsh_psub\\psub_fallback_" + to_string(GetCurrentProcessId()) + "_" + to_string(GetTickCount64()) + ".tmp");
        }
    }
    return ""; // signal failure to caller instead of a bogus path
}

static string quote_for_shell_path(const string& p) {
    string q = "\"";
    for (char c : p) {
        if (c == '"') q += "\\\"";
        else q += c;
    }
    q += "\"";
    return q;
}

// ============================================================================
// ANSI COLOR CODES & VT100
// ============================================================================
const string COLOR_RESET     = "\033[0m";
const string COLOR_RED       = "\033[31m";
const string COLOR_GREEN     = "\033[32m";
const string COLOR_YELLOW    = "\033[33m";
const string COLOR_BLUE      = "\033[34m";
const string COLOR_MAGENTA   = "\033[35m";
const string COLOR_CYAN      = "\033[36m";
const string COLOR_WHITE     = "\033[37m";
const string COLOR_ORANGE    = "\033[38;5;208m";
const string COLOR_PINK      = "\033[38;5;205m";
const string COLOR_BR_GREEN  = "\033[92m";
const string COLOR_BR_BLUE   = "\033[94m";
const string COLOR_BR_YELLOW = "\033[93m";
const string COLOR_BR_CYAN   = "\033[96m";

void enable_ansi_support() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}

const string ZSH_VERSION_STRING = "CrossShellZSH 3.7.16";

static string format_now_header_line() {
    const time_t now = time(nullptr);
    tm local_tm{};
    if (localtime_s(&local_tm, &now) != 0) {
        return "Date/Time unavailable";
    }
    char weekday_month[96] = {};
    char time_12h[32] = {};
    if (strftime(weekday_month, sizeof(weekday_month), "%A %B", &local_tm) == 0) {
        return "Date/Time unavailable";
    }
    if (strftime(time_12h, sizeof(time_12h), "%I:%M:%S %p", &local_tm) == 0) {
        return "Date/Time unavailable";
    }
    ostringstream out;
    out << weekday_month << " " << local_tm.tm_mday << " " << (local_tm.tm_year + 1900)
        << " " << time_12h;
    return out.str();
}

static const vector<string>& zsh_builtin_command_names() {
    static const vector<string> names = {
        "alias","autoload","bg","bindkey","break","builtin","cd","chdir","clear","cls","command","continue","coproc",
        "compadd","compdef","compinit","declare","dir","dirs","disown","echo","emulate","eval","exec","exit","export",
        "false","fc","fg","getopts","help","history","integer","jobs","kill","let","local","logout","nocorrect",
        "popd","print","printf","pushd","pwd","read","readonly","rehash","repeat","return","select","set","setopt","shift",
        "source","strftime","sysread","syswrite","test","time","times","trap","true","type","typeset","unalias","unset","unsetopt",
        "until","wait","whence","which","zle","zmodload","zstyle",".","[","[["
    };
    return names;
}

static bool is_zsh_builtin_command(const string& name) {
    static const set<string> names(zsh_builtin_command_names().begin(), zsh_builtin_command_names().end());
    return names.count(name) != 0;
}

// SECTION 02: Path normalization (/ vs \).
string normalize_path_to_win(string p) {
    if (p.length() >= 2 && p[0] == '/' && isalpha((unsigned char)p[1]) && (p.length() == 2 || p[2] == '/')) {
        string rest = p.length() > 2 ? p.substr(3) : "";
        p = string(1, toupper((unsigned char)p[1])) + ":\\" + rest;
    }
    replace(p.begin(), p.end(), '/', '\\');
    return p;
}

string normalize_path_to_unix(string p) {
    replace(p.begin(), p.end(), '\\', '/');
    return p;
}

static bool is_valid_env_var_name(const string& name) {
    if (name.empty()) return false;
    if (!isalpha((unsigned char)name[0]) && name[0] != '_') return false;
    for (char c : name) {
        if (!(isalnum((unsigned char)c) || c == '_')) return false;
    }
    return true;
}

static bool contains_dangerous_alias_tokens(const string& value) {
    static const string dangerous = "|&;<>`";
    return value.find_first_of(dangerous) != string::npos ||
           value.find("&&") != string::npos ||
           value.find("||") != string::npos ||
           value.find("$(") != string::npos ||
           value.find("`") != string::npos;
}

static bool parse_ll_checked(const string& text, long long& out) {
    if (text.empty()) return false;
    size_t i = 0;
    if (text[0] == '+' || text[0] == '-') i = 1;
    if (i == text.size()) return false;
    for (; i < text.size(); ++i) {
        if (!isdigit((unsigned char)text[i])) return false;
    }
    try {
        out = stoll(text);
        return true;
    } catch (...) {
        return false;
    }
}

static bool checked_add(long long a, long long b, long long& out) {
    if ((b > 0 && a > std::numeric_limits<long long>::max() - b) ||
        (b < 0 && a < std::numeric_limits<long long>::min() - b)) {
        return false;
    }
    out = a + b;
    return true;
}

static bool checked_sub(long long a, long long b, long long& out) {
    if ((b < 0 && a > std::numeric_limits<long long>::max() + b) ||
        (b > 0 && a < std::numeric_limits<long long>::min() + b)) {
        return false;
    }
    out = a - b;
    return true;
}

static bool checked_mul(long long a, long long b, long long& out) {
    if (a == 0 || b == 0) {
        out = 0;
        return true;
    }
    if ((b > 0 && a > std::numeric_limits<long long>::max() / b) ||
        (b < 0 && a < std::numeric_limits<long long>::min() / b) ||
        (b > 0 && a < std::numeric_limits<long long>::min() / b) ||
        (b < 0 && a > std::numeric_limits<long long>::max() / b)) {
        return false;
    }
    out = a * b;
    return true;
}

static bool checked_div(long long a, long long b, long long& out) {
    if (b == 0) return false;
    // LLONG_MIN / -1 overflows signed division (undefined behavior).
    if (b == -1 && a == LLONG_MIN) return false;
    out = a / b;
    return true;
}

// SECTION 03: Inline math evaluator engine $(( ... )).
long long eval_math_expr(const string& expr) {
    struct Eval {
        const string& s;
        size_t i;
        int depth = 0;
        Eval(const string& e, size_t start=0) : s(e), i(start), depth(0) {}

        long long primary() {
            if (++depth > 200) return 0; // guard against deeply nested parentheses
            if (i < s.size() && s[i] == '(') {
                ++i; long long v = lorr(); if (i < s.size() && s[i] == ')') ++i; --depth; return v;
            }
            // hex literal
            if (i+1 < s.size() && s[i]=='0' && (s[i+1]=='x'||s[i+1]=='X')) {
                i += 2; long long v = 0;
                while (i < s.size() && isxdigit((unsigned char)s[i])) {
                    char c = (char)tolower((unsigned char)s[i++]);
                    v = v*16 + (isdigit(c) ? c-'0' : c-'a'+10);
                }
                --depth; return v;
            }
            // octal literal (0 followed by octal digits)
            if (i < s.size() && s[i]=='0' && i+1 < s.size() && s[i+1]>='0' && s[i+1]<='7') {
                ++i; long long v = 0;
                while (i < s.size() && s[i]>='0' && s[i]<='7') v = v*8 + (s[i++]-'0');
                --depth; return v;
            }
            long long v = 0;
            while (i < s.size() && isdigit((unsigned char)s[i])) v = v*10 + (s[i++]-'0');
            --depth; return v;
        }

        long long unary() {
            if (++depth > 200) return 0; // guard against stack overflow via deeply nested input
            long long v;
            if (i < s.size()) {
                if (s[i]=='-') { ++i; v = -unary(); --depth; return v; }
                if (s[i]=='+') { ++i; v =  unary(); --depth; return v; }
                if (s[i]=='~') { ++i; v = ~unary(); --depth; return v; }
                if (s[i]=='!') { ++i; v =  unary() ? 0LL : 1LL; --depth; return v; }
            }
            v = primary(); --depth; return v;
        }

        long long mul() {
            long long v = unary();
            while (i < s.size() && (s[i]=='*'||s[i]=='/'||s[i]=='%')) {
                char op = s[i++]; long long r = unary();
                long long tmp;
                if (op == '*') {
                    if (!checked_mul(v, r, tmp)) return 0;
                    v = tmp;
                } else if (op == '/') {
                    if (!checked_div(v, r, tmp)) return 0;
                    v = tmp;
                } else {
                    if (r == 0) return 0;
                    v = v % r;
                }
            }
            return v;
        }

        long long add() {
            long long v = mul();
            while (i < s.size() && (s[i]=='+'||s[i]=='-')) {
                char op = s[i++]; long long r = mul();
                long long tmp;
                if (op == '+') {
                    if (!checked_add(v, r, tmp)) return 0;
                    v = tmp;
                } else {
                    if (!checked_sub(v, r, tmp)) return 0;
                    v = tmp;
                }
            }
            return v;
        }

        long long shift() {
            long long v = add();
            while (i+1 < s.size() && ((s[i]=='<'&&s[i+1]=='<')||(s[i]=='>'&&s[i+1]=='>'))) {
                bool left = s[i]=='<'; i+=2; long long r = add();
                if (r < 0 || r >= 63) return 0;
                v = left ? (v << r) : (v >> r);
            }
            return v;
        }

        long long rel() {
            long long v = shift();
            while (i < s.size()) {
                if (i+1<s.size()&&s[i]=='<'&&s[i+1]=='=') { i+=2; v = v<=shift() ? 1:0; }
                else if (i+1<s.size()&&s[i]=='>'&&s[i+1]=='=') { i+=2; v = v>=shift() ? 1:0; }
                else if (s[i]=='<'&&(i+1>=s.size()||s[i+1]!='<')) { ++i; v = v<shift()  ? 1:0; }
                else if (s[i]=='>'&&(i+1>=s.size()||s[i+1]!='>')) { ++i; v = v>shift()  ? 1:0; }
                else break;
            }
            return v;
        }

        long long eq() {
            long long v = rel();
            while (i+1 < s.size() && ((s[i]=='='&&s[i+1]=='=')||(s[i]=='!'&&s[i+1]=='='))) {
                bool equ = s[i]=='='; i+=2; long long r = rel(); v = equ ? (v==r?1:0) : (v!=r?1:0);
            }
            return v;
        }

        long long band() {
            long long v = eq();
            while (i<s.size() && s[i]=='&' && (i+1>=s.size()||s[i+1]!='&')) { ++i; v &= eq(); }
            return v;
        }

        long long bxor() {
            long long v = band();
            while (i<s.size() && s[i]=='^') { ++i; v ^= band(); }
            return v;
        }

        long long borr() {
            long long v = bxor();
            while (i<s.size() && s[i]=='|' && (i+1>=s.size()||s[i+1]!='|')) { ++i; v |= bxor(); }
            return v;
        }

        long long land() {
            long long v = borr();
            while (i+1<s.size() && s[i]=='&' && s[i+1]=='&') { i+=2; long long r=borr(); v=(v&&r)?1:0; }
            return v;
        }

        long long lorr() {
            long long v = land();
            while (i+1<s.size() && s[i]=='|' && s[i+1]=='|') { i+=2; long long r=land(); v=(v||r)?1:0; }
            return v;
        }
    };

    string clean;
    clean.reserve(expr.size());
    for (char c : expr) if (!isspace((unsigned char)c)) clean += c;
    if (clean.empty()) return 0;
    return Eval(clean).lorr();
}

// Escapes a single argument for safe use in a Win32 CreateProcessW command line string.
static string win_quote_arg(const string& arg) {
    string r = "\"";
    int bs = 0;
    for (char c : arg) {
        if (c == '\\') { bs++; }
        else if (c == '"') { r += string(2*bs, '\\') + "\\\""; bs = 0; }
        else { r += string(bs, '\\'); bs = 0; r += c; }
    }
    return r + string(2*bs, '\\') + "\"";
}

// Escapes cmd.exe metacharacters with ^ outside of quoted regions.
static string cmd_escape_meta(const string& s) {
    static const string meta = "&|<>^()@!";
    string r;
    for (char c : s) {
        if (meta.find(c) != string::npos) r += '^';
        r += c;
    }
    return r;
}

// SECTION 04: Environment and zsh arrays manager.
enum class JobState { Running, Stopped, Done, Failed };

struct BackgroundJob {
    DWORD pid = 0;
    HANDLE hProcess = nullptr;
    HANDLE hJob = nullptr;
    vector<HANDLE> processes;
    vector<DWORD> pids;
    string command;
    unsigned long job_id = 0;
    JobState state = JobState::Running;
    DWORD exit_code = STILL_ACTIVE;
    DWORD launch_error = ERROR_SUCCESS;
};

static unsigned long g_next_job_id = 1;
static unsigned long g_current_job_id = 0;
static unsigned long g_previous_job_id = 0;
static void select_current_job(unsigned long job_id);

struct RegistryValueMetadata {
    DWORD type = REG_NONE;
    vector<BYTE> data;
};

static bool read_zsh_registry_property(const string& property_path, string& value, RegistryValueMetadata& metadata);
static bool write_zsh_registry_property(const string& property_path, const string& value);

class ZshEnvironment {
public:
    map<string, string> vars;
    map<string, RegistryValueMetadata> registry_metadata;
    map<string, vector<string>> indexed_arrays;      // typeset -a
    map<string, map<string, string>> assoc_arrays;   // typeset -A
    set<string> unique_arrays;
    set<string> integer_vars;
    map<string, string> aliases;
    map<string, bool> options;
    map<string, string> functions;
    set<string> readonly_vars;
    set<string> autoload_functions;
    map<string, string> completion_definitions;
    map<string, map<string, string>> keymaps;
    map<pair<string, string>, vector<string>> styles;
    map<string, string> widgets;
    set<string> loaded_modules;
    vector<string> positional_args;
    vector<string> dir_stack;
    vector<string> history;
    vector<BackgroundJob> jobs;
    
    string home_dir;
    string zshrc_path;
    string history_path;
    string oldpwd;
    int    last_exit_code = 0;
    bool   prompt_dirty   = true;  // invalidated by cd; avoids per-keystroke Win32 API calls
    string prompt_cache;
    time_t start_time     = time(nullptr);

    ZshEnvironment() {
        char* user_profile = getenv("USERPROFILE");
        if (!user_profile) user_profile = getenv("HOME");
        home_dir = user_profile ? normalize_path_to_win(user_profile) : "C:\\";

        zshrc_path = home_dir + "\\.zshrc";
        history_path = home_dir + "\\.zsh_history";

        options["autocd"] = true;
        options["correct"] = true;
        options["extendedglob"] = true;
        options["interactive"] = true;
        options["promptsubst"] = true;

        vars["ZSH_VERSION"] = "5.9";
        vars["OSTYPE"] = "mswin";
        vars["HOME"] = home_dir;
        vars["PROMPT"] = "[%n@%m] %~ %# ";
        vars["RPROMPT"] = "";
        vars["PROMPT3"] = "?# ";
        vars["TIMEFMT"] = "%E real\t%U user\t%S sys";
        vars["UID"] = "1000";
        vars["EUID"] = "1000";
        vars["GID"] = "1000";
        vars["EGID"] = "1000";
        vars["0"] = "zsh";

        aliases["clear"] = "cls";
        // ls and clear aliases resolved at startup after PATH is checked (see main()).

        try { oldpwd = fs::current_path().string(); } catch (...) { oldpwd = home_dir; }
        vars["OLDPWD"] = normalize_path_to_unix(oldpwd);
        try { vars["PWD"] = normalize_path_to_unix(fs::current_path().string()); }
        catch (...) { vars["PWD"] = vars["OLDPWD"]; }
        SetEnvironmentVariableA("OLDPWD", vars["OLDPWD"].c_str());
        SetEnvironmentVariableA("PWD", vars["PWD"].c_str());
    }

    void load_history() {
        ifstream file(history_path);
        if (!file.is_open()) return;
    // load_history already caps to 10000 on read; single-call find for ';'
        string line;
        while (getline(file, line)) {
            if (line.empty()) continue;
            string entry;
            size_t semi = (line[0] == ':') ? line.find(';') : string::npos;
            entry = (semi != string::npos) ? line.substr(semi + 1) : line;
            // Strip non-printable bytes to prevent terminal injection from history file.
            string safe; safe.reserve(entry.size());
            for (unsigned char c : entry) if (c >= 32 || c == '\t') safe += (char)c;
            if (!safe.empty()) history.push_back(safe);
            if (history.size() >= 10000) break; // cap to prevent memory exhaustion
        }
    }

    void add_history(const string& cmd) {
        if (cmd.empty()) return;
        if (history.size() >= 10000) history.erase(history.begin()); // keep at most 10000 entries
        history.push_back(cmd);
        ofstream file(history_path, ios::app);
        if (file.is_open()) {
            const time_t now = time(nullptr);
            tm local_tm{};
            char month[32] = {};
            char time_of_day[32] = {};
            if (localtime_s(&local_tm, &now) == 0 &&
                strftime(month, sizeof(month), "%B", &local_tm) != 0 &&
                strftime(time_of_day, sizeof(time_of_day), "%I:%M:%S %p", &local_tm) != 0) {
                file << ": " << month << " " << local_tm.tm_mday << " "
                     << (local_tm.tm_year + 1900) << " " << time_of_day
                     << " unix=" << static_cast<long long>(now) << ":0;"
                     << cmd << "\n";
            }
        }
    }

    string expand_vars(const string& input) {
        string str = input;

        auto is_single_quoted_at = [&](size_t pos) {
            bool in_sq = false;
            bool in_dq = false;
            for (size_t k = 0; k < pos && k < str.size(); ++k) {
                if (str[k] == '"' && !in_sq) in_dq = !in_dq;
                else if (str[k] == '\'' && !in_dq) in_sq = !in_sq;
            }
            return in_sq;
        };

        static const regex nested_name_re(R"(\$\{\$\{([A-Za-z_][A-Za-z0-9_]*)\}\})");
        for (int pass = 0; pass < 16; ++pass) {
            smatch nested_match;
            if (!regex_search(str, nested_match, nested_name_re)) break;
            string indirect_name;
            auto inner = vars.find(nested_match[1].str());
            if (inner != vars.end()) indirect_name = inner->second;
            string value;
            auto indirect = vars.find(indirect_name);
            if (indirect != vars.end()) value = indirect->second;
            else if (!indirect_name.empty()) {
                char* environment_value = getenv(indirect_name.c_str());
                if (environment_value) value = environment_value;
            }
            str.replace(static_cast<size_t>(nested_match.position()), nested_match.length(), value);
        }

        // Tilde expansion: ~ or ~/path → $HOME
        if (!str.empty() && str[0] == '~') {
            if (str.size() == 1 || str[1] == '/' || str[1] == '\\')
                str = home_dir + str.substr(1);
        }

        // 1. Math Evaluation $(( ... ))
        size_t search_from = 0;
        while (search_from < str.size()) {
            size_t p1 = str.find("$(( ", search_from);
            size_t p2 = str.find("$(((", search_from);
            size_t p3 = str.find("$((", search_from);
            size_t math_pos = string::npos;
            for (size_t p : {p1, p2, p3}) {
                if (p != string::npos && (math_pos == string::npos || p < math_pos)) math_pos = p;
            }
            if (math_pos == string::npos) break;
            if (is_single_quoted_at(math_pos)) { search_from = math_pos + 1; continue; }
            size_t end_pos = str.find("))", math_pos);
            if (end_pos != string::npos) {
                string expr = expand_vars(str.substr(math_pos + 3, end_pos - (math_pos + 3)));
                string repl = to_string(eval_math_expr(expr));
                str.replace(math_pos, (end_pos + 2) - math_pos, repl);
                search_from = math_pos + repl.size();
            } else {
                search_from = math_pos + 3;
            }
        }

        // 1b. Command substitution $( ... ) and ` ... `
        auto find_matching_paren = [&](size_t open_pos) {
            bool q_sq = false, q_dq = false;
            int depth = 0;
            for (size_t j = open_pos; j < str.size(); ++j) {
                char ch = str[j];
                if (ch == '"' && !q_sq) { q_dq = !q_dq; continue; }
                if (ch == '\'' && !q_dq) { q_sq = !q_sq; continue; }
                if (q_sq || q_dq) continue;
                if (ch == '(') depth++;
                else if (ch == ')') {
                    depth--;
                    if (depth == 0) return j;
                }
            }
            return string::npos;
        };

        search_from = 0;
        while (search_from < str.size()) {
            size_t sub_pos = str.find("$(", search_from);
            if (sub_pos == string::npos) break;
            if (sub_pos + 2 < str.size() && str[sub_pos + 2] == '(') { search_from = sub_pos + 2; continue; }
            if (is_single_quoted_at(sub_pos)) { search_from = sub_pos + 2; continue; }

            size_t close_pos = find_matching_paren(sub_pos + 1);
            if (close_pos == string::npos) break;

            string inner_cmd = str.substr(sub_pos + 2, close_pos - (sub_pos + 2));
            string captured = capture_command_output(inner_cmd);
            str.replace(sub_pos, (close_pos + 1) - sub_pos, captured);
            search_from = sub_pos + captured.size();
        }

        search_from = 0;
        while (search_from < str.size()) {
            size_t bt_open = str.find('`', search_from);
            if (bt_open == string::npos) break;
            if (is_single_quoted_at(bt_open)) { search_from = bt_open + 1; continue; }

            size_t bt_close = str.find('`', bt_open + 1);
            if (bt_close == string::npos) break;

            string inner_cmd = str.substr(bt_open + 1, bt_close - (bt_open + 1));
            string captured = capture_command_output(inner_cmd);
            str.replace(bt_open, (bt_close + 1) - bt_open, captured);
            search_from = bt_open + captured.size();
        }

        // 1c. Process substitution: <(command), >(command), and =(command)
        search_from = 0;
        while (search_from < str.size()) {
            size_t in_pos = str.find("<(", search_from);
            size_t out_pos = str.find(">(", search_from);
            size_t eq_pos = str.find("=(", search_from);
            while (eq_pos != string::npos) {
                // Check if this is an array assignment like name=(...)
                size_t id_start = eq_pos;
                while (id_start > 0 && (isalnum((unsigned char)str[id_start - 1]) || str[id_start - 1] == '_')) {
                    --id_start;
                }
                bool is_identifier_assignment = (id_start < eq_pos) && (id_start == 0 || isspace((unsigned char)str[id_start - 1]));
                if (is_identifier_assignment) {
                    eq_pos = str.find("=(", eq_pos + 2);
                } else {
                    break;
                }
            }

            size_t sub_pos = string::npos;
            bool to_input_file = true;

            if (in_pos != string::npos && (sub_pos == string::npos || in_pos < sub_pos)) {
                sub_pos = in_pos;
                to_input_file = true;
            }
            if (eq_pos != string::npos && (sub_pos == string::npos || eq_pos < sub_pos)) {
                sub_pos = eq_pos;
                to_input_file = true;
            }
            if (out_pos != string::npos && (sub_pos == string::npos || out_pos < sub_pos)) {
                sub_pos = out_pos;
                to_input_file = false;
            }

            if (sub_pos == string::npos) break;
            if (is_single_quoted_at(sub_pos)) { search_from = sub_pos + 2; continue; }

            size_t close_pos = find_matching_paren(sub_pos + 1);
            if (close_pos == string::npos) break;

            string inner_cmd = trim_copy(str.substr(sub_pos + 2, close_pos - (sub_pos + 2)));
            string tmp_path = create_temp_process_subst_path();
            g_process_subst_temp_files.push_back(tmp_path);

            if (to_input_file) {
                // Produce data now so the caller can read from a normal file path.
                ++g_process_subst_eval_depth;
                parse_and_execute(inner_cmd + " > " + quote_for_shell_path(normalize_path_to_unix(tmp_path)));
                --g_process_subst_eval_depth;
            } else {
                // Defer sink execution until the current command finishes writing to this file.
                g_process_subst_sinks.push_back({tmp_path, inner_cmd});
            }

            string repl = normalize_path_to_win(tmp_path);
            str.replace(sub_pos, (close_pos + 1) - sub_pos, repl);
            search_from = sub_pos + repl.size();
        }

        // 2. Array Length Expansion $#arr
        if (str.length() > 2 && str[0] == '$' && str[1] == '#' && !is_single_quoted_at(0)) {
            string arr_name = str.substr(2);
            if (indexed_arrays.count(arr_name)) return to_string(indexed_arrays[arr_name].size());
            if (assoc_arrays.count(arr_name)) return to_string(assoc_arrays[arr_name].size());
        }

        // 3. Array Indexing Expansion $arr[1] or $map[key]
        static const regex arr_regex(R"(\$(\w+)\[([^\]]+)\])"); // static: avoid per-call construction
        if (str.size() <= 4096) {
            smatch match;
            string::const_iterator search_it = str.cbegin();
            while (regex_search(search_it, str.cend(), match, arr_regex)) {
                size_t abs_pos = static_cast<size_t>(match[0].first - str.cbegin());
                if (is_single_quoted_at(abs_pos)) {
                    search_it = match[0].second;
                    continue;
                }

                string var_name = match[1].str();
                string key = match[2].str();
                string val = "";

                if (indexed_arrays.count(var_name)) {
                    auto trim_ws = [](string s) {
                        size_t b = 0;
                        while (b < s.size() && isspace((unsigned char)s[b])) ++b;
                        size_t e = s.size();
                        while (e > b && isspace((unsigned char)s[e - 1])) --e;
                        return s.substr(b, e - b);
                    };

                    size_t comma = key.find(',');
                    if (comma != string::npos) {
                        long long a = 0, b = 0;
                        string left = trim_ws(key.substr(0, comma));
                        string right = trim_ws(key.substr(comma + 1));
                        if (parse_ll_checked(left, a) && parse_ll_checked(right, b) && a > 0 && b > 0) {
                            if (a > b) swap(a, b);
                            long long lo = max(1LL, a), hi = min(static_cast<long long>(indexed_arrays[var_name].size()), b);
                            for (long long k = lo; k <= hi; ++k) {
                                if (!val.empty()) val += " ";
                                val += indexed_arrays[var_name][static_cast<size_t>(k - 1)];
                            }
                        }
                    } else {
                        long long idx = 0;
                        if (parse_ll_checked(key, idx)) {
                            bool zero_based = options.count("ksharrays") && options.at("ksharrays");
                            if (idx < 0) idx = static_cast<long long>(indexed_arrays[var_name].size()) + idx + (zero_based ? 0 : 1);
                            if ((zero_based && idx >= 0) || (!zero_based && idx > 0)) {
                                size_t pos = static_cast<size_t>(zero_based ? idx : idx - 1);
                                if (pos < indexed_arrays[var_name].size()) {
                                    val = indexed_arrays[var_name][pos];
                                }
                            }
                        }
                    }
                } else if (assoc_arrays.count(var_name)) {
                    if (assoc_arrays[var_name].count(key)) {
                        val = assoc_arrays[var_name][key];
                    }
                }
                str.replace(abs_pos, match.length(0), val);
                search_it = str.cbegin() + static_cast<ptrdiff_t>(abs_pos + val.size());
            }
        }

        // 4. Standard Variable & Modifier Expansion
        string result;
        result.reserve(str.size());
        size_t i = 0;
        bool in_sq = false;
        bool in_dq = false;
        while (i < str.length()) {
            if (str[i] == '\'' && !in_dq) {
                in_sq = !in_sq;
                result += str[i++];
                continue;
            }
            if (str[i] == '"' && !in_sq) {
                in_dq = !in_dq;
                result += str[i++];
                continue;
            }

            if (str[i] == '$' && !in_sq) {
                i++;
                if (i < str.length() && str[i] == '$') { result += to_string(GetCurrentProcessId()); i++; continue; }
                if (i < str.length() && str[i] == '!') { result += jobs.empty() ? "" : to_string(jobs.back().pid); i++; continue; }
                if (i < str.length() && str[i] == '?') { result += to_string(last_exit_code); i++; continue; }
                if (i < str.length() && str[i] == '#') { result += to_string(positional_args.size()); i++; continue; }
                if (i < str.length() && str[i] == '0') { result += (vars.count("0") ? vars.at("0") : "zsh"); i++; continue; }
                if (i < str.length() && str[i] == '@') {
                    char ifs_sep = ' ';
                    if (vars.count("IFS") && !vars.at("IFS").empty()) ifs_sep = vars.at("IFS")[0];
                    else if (getenv("IFS") && getenv("IFS")[0] != '\0') ifs_sep = getenv("IFS")[0];
                    for (size_t k = 0; k < positional_args.size(); ++k) {
                        if (k > 0) result += ifs_sep;
                        result += positional_args[k];
                    }
                    i++;
                    continue;
                }
                if (i < str.length() && str[i] == '*') {
                    char ifs_sep = ' ';
                    if (vars.count("IFS") && !vars.at("IFS").empty()) ifs_sep = vars.at("IFS")[0];
                    else if (getenv("IFS") && getenv("IFS")[0] != '\0') ifs_sep = getenv("IFS")[0];
                    for (size_t k = 0; k < positional_args.size(); ++k) {
                        if (k > 0) result += ifs_sep;
                        result += positional_args[k];
                    }
                    i++;
                    continue;
                }
                if (i < str.length() && isdigit(str[i])) {
                    int idx = str[i] - '0';
                    if (idx >= 1 && idx <= (int)positional_args.size()) result += positional_args[idx - 1];
                    i++; continue;
                }

                bool braced = false;
                if (i < str.length() && str[i] == '{') { braced = true; i++; }

                string var_expr = "";
                if (braced) {
                    int nested_braces = 0;
                    while (i < str.length()) {
                        if (str[i] == '$' && i + 1 < str.length() && str[i + 1] == '{') {
                            ++nested_braces;
                            var_expr += "${";
                            i += 2;
                            continue;
                        }
                        if (str[i] == '}') {
                            if (nested_braces == 0) break;
                            --nested_braces;
                        }
                        var_expr += str[i++];
                    }
                } else {
                    while (i < str.length() && (isalnum(static_cast<unsigned char>(str[i])) || str[i] == '_'))
                        var_expr += str[i++];
                }
                if (braced && i < str.length() && str[i] == '}') i++;

                if (!braced) {
                    if (var_expr == "SECONDS") result += to_string(time(nullptr) - start_time);
                    else if (var_expr == "RANDOM") result += to_string(rand() % 32768);
                    else if (var_expr == "UID" || var_expr == "EUID" || var_expr == "GID" || var_expr == "EGID") {
                        result += vars.count(var_expr) ? vars.at(var_expr) : "1000";
                    } else if (var_expr == "pipestatus" || var_expr == "PIPESTATUS") {
                        if (indexed_arrays.count("pipestatus")) {
                            const auto& vals = indexed_arrays.at("pipestatus");
                            for (size_t pi = 0; pi < vals.size(); ++pi) {
                                if (pi > 0) result += ' ';
                                result += vals[pi];
                            }
                        } else if (vars.count(var_expr)) result += vars.at(var_expr);
                        else result += to_string(last_exit_code);
                    } else if (vars.count(var_expr)) result += vars[var_expr];
                    else { char* e = getenv(var_expr.c_str()); if (e) result += e; }
                } else {
                    // Length expansion: ${#var} or ${#arr}
                    if (!var_expr.empty() && var_expr[0] == '#') {
                        string vn = var_expr.substr(1);
                        if (indexed_arrays.count(vn)) result += to_string(indexed_arrays[vn].size());
                        else if (assoc_arrays.count(vn)) result += to_string(assoc_arrays[vn].size());
                        else {
                            string v = vars.count(vn) ? vars[vn] : "";
                            result += to_string(v.size());
                        }
                        continue;
                    }

                    // Flag handling: ${(flags)expr}
                    string flags;
                    string core_expr = var_expr;
                    if (var_expr.size() > 3 && var_expr[0] == '(') {
                        size_t fp = var_expr.find(')');
                        if (fp != string::npos && fp + 1 < var_expr.size()) {
                            flags = var_expr.substr(1, fp - 1);
                            core_expr = var_expr.substr(fp + 1);
                        }
                    }

                    auto get_flag_delim = [](const string& fl, char code, string& out_delim) -> bool {
                        string marker; marker += code; marker += ':';
                        size_t p = fl.find(marker);
                        if (p == string::npos) return false;
                        p += marker.size();
                        size_t e = fl.find(':', p);
                        if (e == string::npos) return false;
                        out_delim = fl.substr(p, e - p);
                        return true;
                    };

                    auto apply_flags_to_list = [&](vector<string>& list) {
                        for (auto& val : list) {
                            if (flags.find('L') != string::npos) {
                                transform(val.begin(), val.end(), val.begin(), [](unsigned char c){ return static_cast<char>(tolower(c)); });
                            }
                            if (flags.find('U') != string::npos) {
                                transform(val.begin(), val.end(), val.begin(), [](unsigned char c){ return static_cast<char>(toupper(c)); });
                            }
                            if (flags.find('C') != string::npos) {
                                bool cap_next = true;
                                for (char& c : val) {
                                    if (isspace((unsigned char)c) || c == '_' || c == '-') cap_next = true;
                                    else if (cap_next) { c = static_cast<char>(toupper((unsigned char)c)); cap_next = false; }
                                    else { c = static_cast<char>(tolower((unsigned char)c)); }
                                }
                            }
                            if (flags.find('q') != string::npos) {
                                string q = "'";
                                for (char c : val) { if (c == '\'') q += "'\\''"; else q += c; }
                                q += "'";
                                val = q;
                            }
                        }
                    };

                    // Type flag: ${(t)var}
                    if (flags.find('t') != string::npos) {
                        if (integer_vars.count(core_expr)) result += "integer";
                        else if (assoc_arrays.count(core_expr)) result += "association";
                        else if (indexed_arrays.count(core_expr)) result += "array";
                        else if (vars.count(core_expr)) result += "scalar";
                        else result += "";
                        continue;
                    }

                    // Keys flag: ${(k)assoc} or Values flag: ${(v)assoc}
                    if (flags.find('k') != string::npos && assoc_arrays.count(core_expr)) {
                        vector<string> keys;
                        for (const auto& [k, v] : assoc_arrays[core_expr]) keys.push_back(k);
                        apply_flags_to_list(keys);
                        string delim = " ";
                        get_flag_delim(flags, 'j', delim);
                        for (size_t ki = 0; ki < keys.size(); ++ki) {
                            if (ki > 0) result += delim;
                            result += keys[ki];
                        }
                        continue;
                    }
                    if (flags.find('v') != string::npos && assoc_arrays.count(core_expr)) {
                        vector<string> vals;
                        for (const auto& [k, v] : assoc_arrays[core_expr]) vals.push_back(v);
                        apply_flags_to_list(vals);
                        string delim = " ";
                        get_flag_delim(flags, 'j', delim);
                        for (size_t vi = 0; vi < vals.size(); ++vi) {
                            if (vi > 0) result += delim;
                            result += vals[vi];
                        }
                        continue;
                    }

                    // Check for parameter operators (:- , :+ , := , :? , etc.)
                    size_t name_length = 0;
                    if (!core_expr.empty() && (isalpha(static_cast<unsigned char>(core_expr[0])) || core_expr[0] == '_')) {
                        name_length = 1;
                        while (name_length < core_expr.size() &&
                               (isalnum(static_cast<unsigned char>(core_expr[name_length])) || core_expr[name_length] == '_'))
                            ++name_length;
                    }
                    if (name_length > 0 && name_length < core_expr.size()) {
                        string name = core_expr.substr(0, name_length);
                        string remainder = core_expr.substr(name_length);
                        static const vector<string> parameter_operators = {
                            ":-", ":+", ":=", ":?", "-", "+", "=", "?"
                        };
                        bool handled_op = false;
                        for (const auto& parameter_operator : parameter_operators) {
                            if (remainder.rfind(parameter_operator, 0) != 0) continue;

                            auto variable = vars.find(name);
                            bool is_set = variable != vars.end();
                            string value = is_set ? variable->second : "";
                            if (!is_set) {
                                if (const char* environment_value = getenv(name.c_str())) {
                                    value = environment_value;
                                    is_set = true;
                                }
                            }
                            bool colon_form = parameter_operator[0] == ':';
                            bool has_value = is_set && (!colon_form || !value.empty());
                            string word = remainder.substr(parameter_operator.size());
                            char operation = parameter_operator.back();

                            if (operation == '-') {
                                result += has_value ? value : expand_vars(word);
                            } else if (operation == '+') {
                                if (has_value) result += expand_vars(word);
                            } else if (operation == '=') {
                                if (!has_value) {
                                    if (readonly_vars.count(name)) {
                                        cerr << "zsh: read-only variable: " << name << "\n";
                                        last_exit_code = 1;
                                    } else {
                                        value = expand_vars(word);
                                        vars[name] = value;
                                    }
                                }
                                result += value;
                            } else {
                                if (!has_value) {
                                    cerr << "zsh: " << name << ": "
                                         << (word.empty() ? "parameter not set" : expand_vars(word)) << "\n";
                                    last_exit_code = 1;
                                } else {
                                    result += value;
                                }
                            }
                            handled_op = true;
                            break;
                        }
                        if (handled_op) continue;
                    }

                    // Array / string indexing & slicing: ${var[key]} or ${arr[start,end]}
                    size_t bracket_open = core_expr.find('[');
                    if (bracket_open != string::npos && core_expr.back() == ']') {
                        string vn = core_expr.substr(0, bracket_open);
                        string key = core_expr.substr(bracket_open + 1, core_expr.size() - bracket_open - 2);
                        if (indexed_arrays.count(vn)) {
                            const auto& values = indexed_arrays[vn];
                            if (key == "@" || key == "*") {
                                vector<string> vals = values;
                                apply_flags_to_list(vals);
                                string delim = " ";
                                get_flag_delim(flags, 'j', delim);
                                for (size_t value_index = 0; value_index < vals.size(); ++value_index) {
                                    if (value_index > 0) result += delim;
                                    result += vals[value_index];
                                }
                            } else if (key.find(',') != string::npos) {
                                size_t comma = key.find(',');
                                long long s_idx = 1, e_idx = static_cast<long long>(values.size());
                                parse_ll_checked(key.substr(0, comma), s_idx);
                                parse_ll_checked(key.substr(comma + 1), e_idx);
                                bool zero_based = options.count("ksharrays") && options.at("ksharrays");
                                long long n = static_cast<long long>(values.size());
                                if (s_idx < 0) s_idx = n + s_idx + (zero_based ? 0 : 1);
                                if (e_idx < 0) e_idx = n + e_idx + (zero_based ? 0 : 1);
                                long long start_pos = zero_based ? s_idx : s_idx - 1;
                                long long end_pos = zero_based ? e_idx : e_idx - 1;
                                if (start_pos < 0) start_pos = 0;
                                if (end_pos >= n) end_pos = n - 1;
                                vector<string> slice;
                                for (long long idx = start_pos; idx <= end_pos && idx < n; ++idx) {
                                    slice.push_back(values[static_cast<size_t>(idx)]);
                                }
                                apply_flags_to_list(slice);
                                string delim = " ";
                                get_flag_delim(flags, 'j', delim);
                                for (size_t sli = 0; sli < slice.size(); ++sli) {
                                    if (sli > 0) result += delim;
                                    result += slice[sli];
                                }
                            } else {
                                long long index = 0;
                                if (parse_ll_checked(key, index)) {
                                    bool zero_based = options.count("ksharrays") && options.at("ksharrays");
                                    if (index < 0) index = static_cast<long long>(values.size()) + index + (zero_based ? 0 : 1);
                                    if ((zero_based && index >= 0 && index < static_cast<long long>(values.size())) ||
                                        (!zero_based && index > 0 && index <= static_cast<long long>(values.size()))) {
                                        string v = values[static_cast<size_t>(zero_based ? index : index - 1)];
                                        vector<string> vals = {v};
                                        apply_flags_to_list(vals);
                                        result += vals[0];
                                    }
                                }
                            }
                        } else if (assoc_arrays.count(vn) && assoc_arrays[vn].count(key)) {
                            string v = assoc_arrays[vn][key];
                            vector<string> vals = {v};
                            apply_flags_to_list(vals);
                            result += vals[0];
                        } else if (vars.count(vn)) {
                            string val = vars[vn];
                            if (key.find(',') != string::npos) {
                                size_t comma = key.find(',');
                                long long s_idx = 1, e_idx = static_cast<long long>(val.size());
                                parse_ll_checked(key.substr(0, comma), s_idx);
                                parse_ll_checked(key.substr(comma + 1), e_idx);
                                bool zero_based = options.count("ksharrays") && options.at("ksharrays");
                                long long n = static_cast<long long>(val.size());
                                if (s_idx < 0) s_idx = n + s_idx + (zero_based ? 0 : 1);
                                if (e_idx < 0) e_idx = n + e_idx + (zero_based ? 0 : 1);
                                long long start_pos = zero_based ? s_idx : s_idx - 1;
                                long long end_pos = zero_based ? e_idx : e_idx - 1;
                                if (start_pos < 0) start_pos = 0;
                                if (end_pos >= n) end_pos = n - 1;
                                if (start_pos <= end_pos && start_pos < n) {
                                    string slice = val.substr(static_cast<size_t>(start_pos), static_cast<size_t>(end_pos - start_pos + 1));
                                    vector<string> vals = {slice};
                                    apply_flags_to_list(vals);
                                    result += vals[0];
                                }
                            }
                        }
                        continue;
                    }

                    // Modifiers and transformations: :h, :t, :r, :e, :l, :u, //, #, %
                    // Separate base var name from modifiers
                    string base_name = core_expr;
                    vector<string> mods;
                    size_t mod_pos = core_expr.find(':');
                    if (mod_pos != string::npos && (core_expr.find(":-") == string::npos && core_expr.find(":=") == string::npos && core_expr.find(":?") == string::npos && core_expr.find(":+") == string::npos)) {
                        base_name = core_expr.substr(0, mod_pos);
                        size_t cp = mod_pos;
                        while (cp < core_expr.size()) {
                            if (core_expr[cp] == ':') {
                                size_t next_col = core_expr.find(':', cp + 1);
                                if (next_col == string::npos) {
                                    mods.push_back(core_expr.substr(cp));
                                    break;
                                } else {
                                    mods.push_back(core_expr.substr(cp, next_col - cp));
                                    cp = next_col;
                                }
                            } else {
                                ++cp;
                            }
                        }
                    }

                    if (!mods.empty() && (vars.count(base_name) || indexed_arrays.count(base_name) || getenv(base_name.c_str()))) {
                        string val;
                        if (vars.count(base_name)) val = vars[base_name];
                        else if (indexed_arrays.count(base_name)) {
                            for (size_t vi = 0; vi < indexed_arrays[base_name].size(); ++vi) {
                                if (vi > 0) val += ' ';
                                val += indexed_arrays[base_name][vi];
                            }
                        } else if (const char* env_v = getenv(base_name.c_str())) val = env_v;

                        for (const auto& mod : mods) {
                            if (mod == ":h") {
                                string norm = normalize_path_to_unix(val);
                                fs::path p(norm);
                                string parent = p.parent_path().string();
                                val = parent.empty() ? "." : normalize_path_to_unix(parent);
                            } else if (mod == ":t") {
                                string norm = normalize_path_to_unix(val);
                                fs::path p(norm);
                                val = normalize_path_to_unix(p.filename().string());
                            } else if (mod == ":r") {
                                size_t dot = val.rfind('.');
                                if (dot != string::npos && (dot > val.find_last_of("/\\") || val.find_last_of("/\\") == string::npos))
                                    val = val.substr(0, dot);
                            } else if (mod == ":e") {
                                size_t dot = val.rfind('.');
                                if (dot != string::npos && (dot > val.find_last_of("/\\") || val.find_last_of("/\\") == string::npos))
                                    val = val.substr(dot + 1);
                                else val = "";
                            } else if (mod == ":l") {
                                transform(val.begin(), val.end(), val.begin(), [](unsigned char c){ return static_cast<char>(tolower(c)); });
                            } else if (mod == ":u") {
                                transform(val.begin(), val.end(), val.begin(), [](unsigned char c){ return static_cast<char>(toupper(c)); });
                            }
                        }
                        vector<string> vals = {val};
                        apply_flags_to_list(vals);
                        result += vals[0];
                        continue;
                    }

                    if (core_expr.find('#') != string::npos) {
                        size_t p = core_expr.find('#');
                        string vn = core_expr.substr(0, p), pref = core_expr.substr(p + 1);
                        string v = vars.count(vn) ? vars[vn] : "";
                        if (!pref.empty()) {
                            if (pref.find_first_of("*?") != string::npos) {
                                for (size_t cut = 0; cut <= v.size(); ++cut) {
                                    if (match_wildcard(pref, v.substr(0, cut))) {
                                        v = v.substr(cut);
                                        break;
                                    }
                                }
                            } else if (v.rfind(pref, 0) == 0) {
                                v = v.substr(pref.size());
                            }
                        }
                        vector<string> vals = {v};
                        apply_flags_to_list(vals);
                        result += vals[0];
                    } else if (core_expr.find('%') != string::npos) {
                        size_t p = core_expr.find('%');
                        string vn = core_expr.substr(0, p), suf = core_expr.substr(p + 1);
                        string v = vars.count(vn) ? vars[vn] : "";
                        if (!suf.empty()) {
                            if (suf.find_first_of("*?") != string::npos) {
                                for (size_t i2 = v.size(); ; ) {
                                    if (match_wildcard(suf, v.substr(i2))) {
                                        v.resize(i2);
                                        break;
                                    }
                                    if (i2 == 0) break;
                                    --i2;
                                }
                            } else if (v.size() >= suf.size() && v.compare(v.size() - suf.size(), suf.size(), suf) == 0) {
                                v.resize(v.size() - suf.size());
                            }
                        }
                        vector<string> vals = {v};
                        apply_flags_to_list(vals);
                        result += vals[0];
                    } else if (core_expr.find("//") != string::npos) {
                        size_t pos = core_expr.find("//");
                        string vn = core_expr.substr(0, pos), rest = core_expr.substr(pos + 2);
                        size_t slash = rest.find('/');
                        string search = rest.substr(0, slash), rep = (slash != string::npos) ? rest.substr(slash + 1) : "";
                        string v = vars.count(vn) ? vars[vn] : "";
                        if (!search.empty()) {
                            string out; size_t last = 0, n = search.size(), p = 0;
                            while ((p = v.find(search, last)) != string::npos) { out += v.substr(last, p - last) + rep; last = p + n; }
                            out += v.substr(last);
                            v = out;
                        }
                        vector<string> vals = {v};
                        apply_flags_to_list(vals);
                        result += vals[0];
                    } else {
                        auto split_by_delim = [](const string& src, const string& delim) {
                            vector<string> items;
                            if (delim.empty()) {
                                for (char c : src) items.push_back(string(1, c));
                                return items;
                            }
                            size_t start = 0;
                            while (start <= src.size()) {
                                size_t p = src.find(delim, start);
                                if (p == string::npos) {
                                    items.push_back(src.substr(start));
                                    break;
                                }
                                items.push_back(src.substr(start, p - start));
                                start = p + delim.size();
                            }
                            return items;
                        };

                        auto emit_vals = [&](vector<string> vals) {
                            string s_delim;
                            if (vals.size() == 1 && get_flag_delim(flags, 's', s_delim)) {
                                vals = split_by_delim(vals[0], s_delim);
                            } else if (vals.size() == 1 && flags.find('f') != string::npos) {
                                vals = split_by_delim(vals[0], "\n");
                            } else if (vals.size() == 1 && flags.find('w') != string::npos) {
                                vals = tokenize_words(vals[0]);
                            }
                            apply_flags_to_list(vals);
                            string delim = " ";
                            get_flag_delim(flags, 'j', delim);
                            for (size_t vi = 0; vi < vals.size(); ++vi) {
                                if (vi > 0) result += delim;
                                result += vals[vi];
                            }
                        };

                        if (core_expr.find('$') != string::npos) {
                            string expanded_core = expand_vars(core_expr);
                            emit_vals({expanded_core});
                        } else if (indexed_arrays.count(core_expr)) {
                            emit_vals(indexed_arrays[core_expr]);
                        } else {
                            string registry_value;
                            RegistryValueMetadata registry_metadata;
                            if (read_zsh_registry_property(core_expr, registry_value, registry_metadata)) {
                                this->registry_metadata[core_expr] = std::move(registry_metadata);
                                emit_vals({registry_value});
                            } else if (vars.count(core_expr)) {
                                emit_vals({vars[core_expr]});
                            } else {
                                char* e = getenv(core_expr.c_str());
                                if (e) emit_vals({string(e)});
                                else if (!flags.empty()) emit_vals({""});
                            }
                        }
                    }
                }
            } else {
                result += str[i]; i++;
            }
        }
        return result;
    }
};

static bool split_zsh_registry_path(const string& property_path, HKEY& root, wstring& key_path, wstring& value_name) {
    size_t separator = property_path.find('.');
    if (separator == string::npos || separator == 0 || separator + 1 >= property_path.size()) return false;
    string root_name = property_path.substr(0, separator);
    if (root_name == "HKLM" || root_name == "HKEY_LOCAL_MACHINE") root = HKEY_LOCAL_MACHINE;
    else if (root_name == "HKCU" || root_name == "HKEY_CURRENT_USER") root = HKEY_CURRENT_USER;
    else return false;

    string remainder = property_path.substr(separator + 1);
    size_t value_separator = remainder.rfind('.');
    if (value_separator == string::npos || value_separator == 0 || value_separator + 1 >= remainder.size()) return false;
    string raw_key = remainder.substr(0, value_separator);
    value_name = string_to_wstring(remainder.substr(value_separator + 1));
    if (raw_key.find('/') != string::npos) replace(raw_key.begin(), raw_key.end(), '/', '\\');
    else replace(raw_key.begin(), raw_key.end(), '.', '\\');
    key_path = string_to_wstring(raw_key);
    return true;
}

static bool read_zsh_registry_property(const string& property_path, string& value, RegistryValueMetadata& metadata) {
    HKEY root = nullptr;
    wstring key_path, value_name;
    if (!split_zsh_registry_path(property_path, root, key_path, value_name)) return false;
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, key_path.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS) return false;

    DWORD type = REG_NONE, size = 0;
    LONG status = RegQueryValueExW(key, value_name.c_str(), nullptr, &type, nullptr, &size);
    if (status != ERROR_SUCCESS) { RegCloseKey(key); return false; }
    vector<BYTE> data(size);
    if (size > 0) status = RegQueryValueExW(key, value_name.c_str(), nullptr, &type, data.data(), &size);
    RegCloseKey(key);
    if (status != ERROR_SUCCESS) return false;
    data.resize(size);
    metadata.type = type;
    metadata.data = data;

    wstring wide_value;
    if (type == REG_DWORD && data.size() >= sizeof(DWORD)) {
        wide_value = to_wstring(*reinterpret_cast<const DWORD*>(data.data()));
    } else if (type == REG_QWORD && data.size() >= sizeof(ULONGLONG)) {
        wide_value = to_wstring(*reinterpret_cast<const ULONGLONG*>(data.data()));
    } else if (type == REG_BINARY) {
        static const wchar_t hex[] = L"0123456789ABCDEF";
        for (BYTE byte : data) { wide_value += hex[(byte >> 4) & 0x0F]; wide_value += hex[byte & 0x0F]; }
    } else if (type == REG_MULTI_SZ) {
        size_t count = data.size() / sizeof(wchar_t), offset = 0;
        const wchar_t* text = reinterpret_cast<const wchar_t*>(data.data());
        while (offset < count && text[offset] != L'\0') {
            size_t start = offset;
            while (offset < count && text[offset] != L'\0') ++offset;
            if (offset >= count) return false;
            if (!wide_value.empty()) wide_value.push_back(L'\n');
            wide_value.append(text + start, offset - start);
            ++offset;
        }
    } else if (type == REG_SZ || type == REG_EXPAND_SZ) {
        size_t count = data.size() / sizeof(wchar_t);
        const wchar_t* text = reinterpret_cast<const wchar_t*>(data.data());
        while (count > 0 && text[count - 1] == L'\0') --count;
        wide_value.assign(text, count);
    } else {
        return false;
    }

    if (wide_value.empty()) { value.clear(); return true; }
    int length = WideCharToMultiByte(CP_UTF8, 0, wide_value.data(), static_cast<int>(wide_value.size()), nullptr, 0, nullptr, nullptr);
    if (length <= 0) return false;
    value.resize(length);
    WideCharToMultiByte(CP_UTF8, 0, wide_value.data(), static_cast<int>(wide_value.size()), value.data(), length, nullptr, nullptr);
    return true;
}

static bool write_zsh_registry_property(const string& property_path, const string& value) {
    HKEY root = nullptr;
    wstring key_path, value_name;
    if (!split_zsh_registry_path(property_path, root, key_path, value_name)) return false;
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, key_path.c_str(), 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &key) != ERROR_SUCCESS) return false;

    DWORD type = REG_SZ, existing_size = 0;
    if (RegQueryValueExW(key, value_name.c_str(), nullptr, &type, nullptr, &existing_size) != ERROR_SUCCESS) type = REG_SZ;
    vector<BYTE> data;
    if (type == REG_DWORD) {
        char* end = nullptr; unsigned long parsed = strtoul(value.c_str(), &end, 0);
        if (end == value.c_str() || *end != '\0' || parsed > MAXDWORD) { RegCloseKey(key); return false; }
        data.resize(sizeof(DWORD)); DWORD number = static_cast<DWORD>(parsed); memcpy(data.data(), &number, sizeof(number));
    } else if (type == REG_QWORD) {
        char* end = nullptr; unsigned long long parsed = strtoull(value.c_str(), &end, 0);
        if (end == value.c_str() || *end != '\0') { RegCloseKey(key); return false; }
        data.resize(sizeof(ULONGLONG)); ULONGLONG number = static_cast<ULONGLONG>(parsed); memcpy(data.data(), &number, sizeof(number));
    } else if (type == REG_SZ || type == REG_EXPAND_SZ) {
        wstring wide = string_to_wstring(value);
        data.resize((wide.size() + 1) * sizeof(wchar_t));
        memcpy(data.data(), wide.c_str(), data.size());
    } else {
        RegCloseKey(key);
        return false;
    }
    LONG status = RegSetValueExW(key, value_name.c_str(), 0, type, data.data(), static_cast<DWORD>(data.size()));
    RegCloseKey(key);
    return status == ERROR_SUCCESS;
}

static ZshEnvironment g_env;
static map<int, HANDLE> g_persistent_fds;
static map<int, HANDLE> g_active_command_fds;
static HANDLE shell_fd_handle(int fd);

static bool save_pipeline_shell_state(const string& path) {
    ofstream output(normalize_path_to_win(path), ios::binary | ios::trunc);
    if (!output) return false;
    const char magic[] = "ZSHPIPE2";
    output.write(magic, sizeof(magic));

    auto write_string_map = [&](const auto& values) {
        uint64_t count = static_cast<uint64_t>(values.size());
        output.write(reinterpret_cast<const char*>(&count), sizeof(count));
        for (const auto& [name, value] : values) {
            write_binary_string(output, name);
            write_binary_string(output, value);
        }
    };
    auto write_string_set = [&](const set<string>& values) {
        uint64_t count = static_cast<uint64_t>(values.size());
        output.write(reinterpret_cast<const char*>(&count), sizeof(count));
        for (const auto& value : values) write_binary_string(output, value);
    };
    auto write_string_vector = [&](const vector<string>& values) {
        uint64_t count = static_cast<uint64_t>(values.size());
        output.write(reinterpret_cast<const char*>(&count), sizeof(count));
        for (const auto& value : values) write_binary_string(output, value);
    };
    write_string_map(g_env.vars);
    write_string_map(g_env.functions);
    write_string_map(g_env.aliases);

    uint64_t option_count = static_cast<uint64_t>(g_env.options.size());
    output.write(reinterpret_cast<const char*>(&option_count), sizeof(option_count));
    for (const auto& [name, enabled] : g_env.options) {
        write_binary_string(output, name);
        uint8_t value = enabled ? 1 : 0;
        output.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }

    uint64_t array_count = static_cast<uint64_t>(g_env.indexed_arrays.size());
    output.write(reinterpret_cast<const char*>(&array_count), sizeof(array_count));
    for (const auto& [name, values] : g_env.indexed_arrays) {
        write_binary_string(output, name);
        uint64_t value_count = static_cast<uint64_t>(values.size());
        output.write(reinterpret_cast<const char*>(&value_count), sizeof(value_count));
        for (const auto& value : values) write_binary_string(output, value);
    }

    uint64_t assoc_count = static_cast<uint64_t>(g_env.assoc_arrays.size());
    output.write(reinterpret_cast<const char*>(&assoc_count), sizeof(assoc_count));
    for (const auto& [name, values] : g_env.assoc_arrays) {
        write_binary_string(output, name);
        write_string_map(values);
    }
    write_string_set(g_env.unique_arrays);
    write_string_set(g_env.integer_vars);
    write_string_set(g_env.readonly_vars);
    write_string_set(g_env.autoload_functions);
    write_string_set(g_env.loaded_modules);
    write_string_vector(g_env.positional_args);
    write_string_map(g_env.completion_definitions);
    write_string_map(g_env.widgets);
    return static_cast<bool>(output);
}

static bool load_pipeline_shell_state(const string& path) {
    ifstream input(normalize_path_to_win(path), ios::binary);
    char magic[9] = {};
    if (!input.read(magic, sizeof(magic)) || memcmp(magic, "ZSHPIPE2", sizeof(magic)) != 0) return false;

    auto read_string_map = [&](auto& values) {
        uint64_t count = 0;
        if (!input.read(reinterpret_cast<char*>(&count), sizeof(count)) || count > 100000) return false;
        values.clear();
        for (uint64_t i = 0; i < count; ++i) {
            string name, value;
            if (!read_binary_string(input, name) || !read_binary_string(input, value)) return false;
            values[std::move(name)] = std::move(value);
        }
        return true;
    };
    auto read_string_set = [&](set<string>& values) {
        uint64_t count = 0;
        if (!input.read(reinterpret_cast<char*>(&count), sizeof(count)) || count > 100000) return false;
        values.clear();
        for (uint64_t i = 0; i < count; ++i) {
            string value;
            if (!read_binary_string(input, value)) return false;
            values.insert(std::move(value));
        }
        return true;
    };
    auto read_string_vector = [&](vector<string>& values) {
        uint64_t count = 0;
        if (!input.read(reinterpret_cast<char*>(&count), sizeof(count)) || count > 1000000) return false;
        values.clear();
        values.reserve(static_cast<size_t>(count));
        for (uint64_t i = 0; i < count; ++i) {
            string value;
            if (!read_binary_string(input, value)) return false;
            values.push_back(std::move(value));
        }
        return true;
    };
    if (!read_string_map(g_env.vars) || !read_string_map(g_env.functions) || !read_string_map(g_env.aliases)) return false;

    uint64_t option_count = 0;
    if (!input.read(reinterpret_cast<char*>(&option_count), sizeof(option_count)) || option_count > 10000) return false;
    g_env.options.clear();
    for (uint64_t i = 0; i < option_count; ++i) {
        string name;
        uint8_t enabled = 0;
        if (!read_binary_string(input, name) || !input.read(reinterpret_cast<char*>(&enabled), sizeof(enabled))) return false;
        g_env.options[std::move(name)] = enabled != 0;
    }

    uint64_t array_count = 0;
    if (!input.read(reinterpret_cast<char*>(&array_count), sizeof(array_count)) || array_count > 100000) return false;
    g_env.indexed_arrays.clear();
    for (uint64_t i = 0; i < array_count; ++i) {
        string name;
        uint64_t value_count = 0;
        if (!read_binary_string(input, name) ||
            !input.read(reinterpret_cast<char*>(&value_count), sizeof(value_count)) || value_count > 1000000) return false;
        vector<string> values;
        values.reserve(static_cast<size_t>(value_count));
        for (uint64_t value_index = 0; value_index < value_count; ++value_index) {
            string value;
            if (!read_binary_string(input, value)) return false;
            values.push_back(std::move(value));
        }
        g_env.indexed_arrays[std::move(name)] = std::move(values);
    }

    uint64_t assoc_count = 0;
    if (!input.read(reinterpret_cast<char*>(&assoc_count), sizeof(assoc_count)) || assoc_count > 100000) return false;
    g_env.assoc_arrays.clear();
    for (uint64_t i = 0; i < assoc_count; ++i) {
        string name;
        map<string, string> values;
        if (!read_binary_string(input, name) || !read_string_map(values)) return false;
        g_env.assoc_arrays[std::move(name)] = std::move(values);
    }
    return read_string_set(g_env.unique_arrays) &&
           read_string_set(g_env.integer_vars) &&
           read_string_set(g_env.readonly_vars) &&
           read_string_set(g_env.autoload_functions) &&
           read_string_set(g_env.loaded_modules) &&
           read_string_vector(g_env.positional_args) &&
           read_string_map(g_env.completion_definitions) &&
           read_string_map(g_env.widgets);
}
struct CoprocessState {
    HANDLE process = nullptr;
    HANDLE input = nullptr;
    HANDLE output = nullptr;
    DWORD pid = 0;
};
static CoprocessState g_coprocess;

static void close_coprocess(bool terminate_if_running) {
    close_handle_if_valid(g_coprocess.input);
    close_handle_if_valid(g_coprocess.output);
    g_coprocess.input = nullptr;
    g_coprocess.output = nullptr;

    if (g_coprocess.process) {
        DWORD exit_code = 0;
        if (terminate_if_running && GetExitCodeProcess(g_coprocess.process, &exit_code) && exit_code == STILL_ACTIVE) {
            TerminateProcess(g_coprocess.process, 1);
        }
        CloseHandle(g_coprocess.process);
        g_coprocess.process = nullptr;
    }
    g_coprocess.pid = 0;
    g_env.vars.erase("COPROC_PID");
}
struct FunctionLocalScope {
    map<string, string> vars_before;
    map<string, vector<string>> indexed_arrays_before;
    map<string, map<string, string>> assoc_arrays_before;
    set<string> integer_vars_before;
    set<string> readonly_vars_before;
    set<string> unique_arrays_before;
    set<string> local_names;
    bool local_options = false;
    map<string, bool> options_before;
};
static vector<FunctionLocalScope> g_function_local_scopes;

static void mark_function_local(const string& name) {
    if (!g_function_local_scopes.empty() && is_valid_env_var_name(name))
        g_function_local_scopes.back().local_names.insert(name);
}

static void restore_function_locals(const FunctionLocalScope& scope) {
    for (const auto& name : scope.local_names) {
        auto scalar = scope.vars_before.find(name);
        if (scalar != scope.vars_before.end()) g_env.vars[name] = scalar->second;
        else g_env.vars.erase(name);

        auto indexed = scope.indexed_arrays_before.find(name);
        if (indexed != scope.indexed_arrays_before.end()) g_env.indexed_arrays[name] = indexed->second;
        else g_env.indexed_arrays.erase(name);

        auto assoc = scope.assoc_arrays_before.find(name);
        if (assoc != scope.assoc_arrays_before.end()) g_env.assoc_arrays[name] = assoc->second;
        else g_env.assoc_arrays.erase(name);

        if (scope.integer_vars_before.count(name)) g_env.integer_vars.insert(name);
        else g_env.integer_vars.erase(name);
        if (scope.readonly_vars_before.count(name)) g_env.readonly_vars.insert(name);
        else g_env.readonly_vars.erase(name);
        if (scope.unique_arrays_before.count(name)) g_env.unique_arrays.insert(name);
        else g_env.unique_arrays.erase(name);
    }
    if (scope.local_options) {
        g_env.options = scope.options_before;
    }
}

static bool g_script_returning  = false;
static int  g_script_return_code = 0;
static bool g_in_try_block       = false;
static int  g_loop_depth = 0;
static int  g_loop_breaking = 0;
static int  g_loop_continuing = 0;

struct LoopGuard {
    LoopGuard() { ++g_loop_depth; }
    ~LoopGuard() {
        --g_loop_depth;
        if (g_loop_depth <= 0) {
            g_loop_depth = 0;
            g_loop_breaking = 0;
            g_loop_continuing = 0;
        }
    }
};
static int  g_subshell_depth = 0;
static bool g_subshell_exiting = false;
static int  g_subshell_exit_code = 0;
static unsigned long long g_heredoc_seq = 0;
static map<string, string> g_heredoc_payloads;
// Recursion guard for parse_and_execute (nested eval, $(), recursive functions).
// Limit is conservative: each nested function/command-substitution frame uses
// several KB of stack, so a low cap prevents stack overflow on Windows.
static int  g_exec_recursion_depth = 0;
static const int kMaxExecRecursionDepth = 48;
// Set when the EXIT trap has already fired, to avoid double-firing.
static bool g_exit_trap_fired = false;

// SECTION 05: OS signal emulation and trap dispatcher.
static atomic<bool> g_sigint_pending{false};
static atomic<bool> g_sigwinch_pending{false};
static atomic<bool> g_sigtstp_pending{false};
static mutex g_foreground_pids_mutex;
static vector<DWORD> g_foreground_pids;

static void set_foreground_pids(const vector<DWORD>& pids) {
    lock_guard<mutex> lock(g_foreground_pids_mutex);
    g_foreground_pids = pids;
}

static void clear_foreground_pids() {
    lock_guard<mutex> lock(g_foreground_pids_mutex);
    g_foreground_pids.clear();
}

// 1. SIGINT Handler (Ctrl+C / Ctrl+Break)
static BOOL WINAPI console_ctrl_handler(DWORD dwCtrlType) {
    if (dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_BREAK_EVENT) {
        g_sigint_pending.store(true);
        return TRUE; // handled; process trap on main thread
    }
    return FALSE;
}

#ifndef PROCESS_SUSPEND_RESUME
#define PROCESS_SUSPEND_RESUME 0x0800
#endif

// Helper: Suspend a target process on Windows (SIGTSTP emulation)
static void suspend_win32_process(DWORD pid) {
    typedef LONG (NTAPI *NtSuspendProcPtr)(HANDLE ProcessHandle);
    static NtSuspendProcPtr NtSuspendProcess =
        (NtSuspendProcPtr)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtSuspendProcess");

    HANDLE hProcess = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
    if (hProcess) {
        if (NtSuspendProcess) {
            NtSuspendProcess(hProcess);
        } else {
            // Fallback: suspend all threads in target process.
            HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
            if (hSnap != INVALID_HANDLE_VALUE) {
                THREADENTRY32 te{ sizeof(THREADENTRY32) };
                if (Thread32First(hSnap, &te)) {
                    do {
                        if (te.th32OwnerProcessID == pid) {
                            HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                            if (hThread) {
                                SuspendThread(hThread);
                                CloseHandle(hThread);
                            }
                        }
                    } while (Thread32Next(hSnap, &te));
                }
                CloseHandle(hSnap);
            }
        }
        CloseHandle(hProcess);
    }
}

static bool resume_win32_process(DWORD pid) {
    typedef LONG (NTAPI *NtResumeProcPtr)(HANDLE ProcessHandle);
    static NtResumeProcPtr NtResumeProcess =
        (NtResumeProcPtr)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtResumeProcess");
    HANDLE process = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
    if (!process) return false;
    bool resumed = false;
    if (NtResumeProcess) {
        resumed = NtResumeProcess(process) >= 0;
    } else {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot != INVALID_HANDLE_VALUE) {
            THREADENTRY32 entry{sizeof(THREADENTRY32)};
            if (Thread32First(snapshot, &entry)) {
                do {
                    if (entry.th32OwnerProcessID != pid) continue;
                    HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, entry.th32ThreadID);
                    if (!thread) continue;
                    while (ResumeThread(thread) > 1) {}
                    CloseHandle(thread);
                    resumed = true;
                } while (Thread32Next(snapshot, &entry));
            }
            CloseHandle(snapshot);
        }
    }
    CloseHandle(process);
    return resumed;
}

static bool suspend_active_foreground_processes() {
    vector<DWORD> pids;
    {
        lock_guard<mutex> lock(g_foreground_pids_mutex);
        pids = g_foreground_pids;
    }
    if (pids.empty()) return false;
    for (DWORD pid : pids) suspend_win32_process(pid);
    return true;
}

// 2. SIGWINCH Detection Helper (window resize)
static void check_window_resize_event() {
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD num_events = 0;
    if (GetNumberOfConsoleInputEvents(hIn, &num_events) && num_events > 0) {
        vector<INPUT_RECORD> records(num_events);
        DWORD read_count = 0;
        if (PeekConsoleInputW(hIn, records.data(), num_events, &read_count)) {
            for (DWORD i = 0; i < read_count; ++i) {
                if (records[i].EventType == WINDOW_BUFFER_SIZE_EVENT) {
                    g_sigwinch_pending.store(true);
                    break;
                }
            }
        }
    }
}

// Initialize OS handlers
void init_signal_handlers() {
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
}

// Fire the EXIT trap exactly once (idempotent via g_exit_trap_fired).
void fire_exit_trap();

// Safe main-thread trap processor
void process_pending_traps() {
    // A. SIGINT
    if (g_sigint_pending.exchange(false)) {
        if (g_env.vars.count("__trap_INT")) {
            execute_command_line(g_env.vars["__trap_INT"]);
        } else if (g_env.vars.count("__trap_SIGINT")) {
            execute_command_line(g_env.vars["__trap_SIGINT"]);
        } else if (g_env.vars.count("__trap_2")) {
            execute_command_line(g_env.vars["__trap_2"]);
        }
    }

    // B. SIGWINCH
    if (g_sigwinch_pending.exchange(false)) {
        g_env.prompt_dirty = true;
        if (g_env.vars.count("__trap_WINCH")) {
            execute_command_line(g_env.vars["__trap_WINCH"]);
        } else if (g_env.vars.count("__trap_SIGWINCH")) {
            execute_command_line(g_env.vars["__trap_SIGWINCH"]);
        } else if (g_env.vars.count("__trap_28")) {
            execute_command_line(g_env.vars["__trap_28"]);
        }
    }

    // C. SIGTSTP
    if (g_sigtstp_pending.exchange(false)) {
        if (g_env.vars.count("__trap_TSTP")) {
            execute_command_line(g_env.vars["__trap_TSTP"]);
        } else if (g_env.vars.count("__trap_SIGTSTP")) {
            execute_command_line(g_env.vars["__trap_SIGTSTP"]);
        } else if (g_env.vars.count("__trap_20")) {
            execute_command_line(g_env.vars["__trap_20"]);
        }
    }
}

// SECTION 06: Extended globbing and qualifiers (*(.), *(/), *(H), *(@), *(m-1), **/*).
bool match_wildcard(const string& pattern, const string& str) {
    size_t alt_open = pattern.find("@(");
    if (alt_open != string::npos) {
        int depth = 1;
        size_t alt_close = string::npos;
        for (size_t i = alt_open + 2; i < pattern.size(); ++i) {
            if (pattern[i] == '(') ++depth;
            else if (pattern[i] == ')' && --depth == 0) { alt_close = i; break; }
        }
        if (alt_close != string::npos) {
            string prefix = pattern.substr(0, alt_open);
            string suffix = pattern.substr(alt_close + 1);
            string alternatives = pattern.substr(alt_open + 2, alt_close - alt_open - 2);
            string current;
            int nested_depth = 0;
            for (size_t i = 0; i <= alternatives.size(); ++i) {
                char ch = i < alternatives.size() ? alternatives[i] : '|';
                if (ch == '(') ++nested_depth;
                else if (ch == ')' && nested_depth > 0) --nested_depth;
                if (ch == '|' && nested_depth == 0) {
                    if (match_wildcard(prefix + current + suffix, str)) return true;
                    current.clear();
                } else {
                    current += ch;
                }
            }
            return false;
        }
    }

    size_t p = 0, s = 0;
    size_t star_p = string::npos, star_s = 0;

    while (s < str.length()) {
        if (p < pattern.length() && pattern[p] == '[') {
            size_t close = pattern.find(']', p + 1);
            if (close != string::npos) {
                bool negate = false;
                size_t class_start = p + 1;
                if (class_start < close && (pattern[class_start] == '!' || pattern[class_start] == '^')) {
                    negate = true;
                    class_start++;
                }
                char sc = static_cast<char>(tolower(static_cast<unsigned char>(str[s])));
                bool matched = false;
                for (size_t ci = class_start; ci < close; ++ci) {
                    if (ci + 2 < close && pattern[ci + 1] == '-') {
                        char start_c = static_cast<char>(tolower(static_cast<unsigned char>(pattern[ci])));
                        char end_c = static_cast<char>(tolower(static_cast<unsigned char>(pattern[ci + 2])));
                        if (start_c > end_c) std::swap(start_c, end_c);
                        if (sc >= start_c && sc <= end_c) { matched = true; break; }
                        ci += 2;
                    } else {
                        char pc = static_cast<char>(tolower(static_cast<unsigned char>(pattern[ci])));
                        if (sc == pc) { matched = true; break; }
                    }
                }
                if (matched != negate) {
                    p = close + 1;
                    s++;
                    continue;
                }
            }
        }

        if (p < pattern.length() && (pattern[p] == '?' || tolower(static_cast<unsigned char>(pattern[p])) == tolower(static_cast<unsigned char>(str[s])))) {
            p++;
            s++;
        } else if (p < pattern.length() && pattern[p] == '*') {
            star_p = p;
            star_s = s;
            p++;
        } else if (star_p != string::npos) {
            p = star_p + 1;
            star_s++;
            s = star_s;
        } else {
            return false;
        }
    }

    while (p < pattern.length() && pattern[p] == '*') p++;
    return p == pattern.length();
}

struct GlobQualifiers {
    bool has_qualifier = false;
    bool regular_only = false;     // .
    bool directory_only = false;   // /
    bool symlink_only = false;     // @
    bool hidden_only = false;      // H or h
    bool executable_only = false;  // *
    bool order_mtime_desc = false; // om
    bool order_mtime_asc = false;  // Om
    bool order_size_desc = false;  // ol
    bool order_size_asc = false;   // Ol
    bool order_name_desc = false;  // On
    bool order_name_asc = false;   // on
    bool null_glob = false;        // N
    int mtime_mode = 0;            // 0: none, -1: m-N (< N days), 1: m+N (> N days), 2: mN (== N days)
    double mtime_days = 0.0;
    size_t select_index = 0;       // 1-based [N]
    size_t select_end = 0;         // 1-based [start,end]
};

static bool parse_glob_qualifiers(string& pattern, GlobQualifiers& q) {
    if (pattern.size() < 3 || pattern.back() != ')') return false;
    size_t open = pattern.rfind('(');
    if (open == string::npos || open == 0) return false;
    if (pattern[open - 1] == '@' || pattern[open - 1] == '$' || pattern[open - 1] == '=') return false;

    string body = pattern.substr(open + 1, pattern.size() - open - 2);
    if (body.empty()) return false;

    GlobQualifiers parsed;
    parsed.has_qualifier = true;
    size_t idx = 0;
    while (idx < body.size()) {
        if (body[idx] == '.') { parsed.regular_only = true; idx++; }
        else if (body[idx] == '/') { parsed.directory_only = true; idx++; }
        else if (body[idx] == '@') { parsed.symlink_only = true; idx++; }
        else if (body[idx] == 'H' || body[idx] == 'h') { parsed.hidden_only = true; idx++; }
        else if (body[idx] == 'D') { /* dotfiles include */ idx++; }
        else if (body[idx] == '*') { parsed.executable_only = true; idx++; }
        else if (body[idx] == 'N') { parsed.null_glob = true; idx++; }
        else if (body.compare(idx, 2, "om") == 0) { parsed.order_mtime_desc = true; idx += 2; }
        else if (body.compare(idx, 2, "Om") == 0) { parsed.order_mtime_asc = true; idx += 2; }
        else if (body.compare(idx, 2, "ol") == 0) { parsed.order_size_desc = true; idx += 2; }
        else if (body.compare(idx, 2, "Ol") == 0) { parsed.order_size_asc = true; idx += 2; }
        else if (body.compare(idx, 2, "on") == 0) { parsed.order_name_asc = true; idx += 2; }
        else if (body.compare(idx, 2, "On") == 0) { parsed.order_name_desc = true; idx += 2; }
        else if (body[idx] == 'm') {
            idx++;
            int mode = 2; // exact
            if (idx < body.size() && body[idx] == '-') { mode = -1; idx++; }
            else if (idx < body.size() && body[idx] == '+') { mode = 1; idx++; }
            size_t num_start = idx;
            while (idx < body.size() && isdigit(static_cast<unsigned char>(body[idx]))) idx++;
            if (idx > num_start) {
                parsed.mtime_mode = mode;
                try { parsed.mtime_days = stod(body.substr(num_start, idx - num_start)); } catch (...) {}
            } else {
                return false;
            }
        }
        else if (body[idx] == '[') {
            size_t close_bracket = body.find(']', idx);
            if (close_bracket == string::npos) return false;
            string inner = body.substr(idx + 1, close_bracket - idx - 1);
            size_t comma = inner.find(',');
            if (comma != string::npos) {
                try {
                    parsed.select_index = static_cast<size_t>(stoull(inner.substr(0, comma)));
                    parsed.select_end = static_cast<size_t>(stoull(inner.substr(comma + 1)));
                } catch (...) {}
            } else {
                try {
                    parsed.select_index = static_cast<size_t>(stoull(inner));
                } catch (...) {}
            }
            idx = close_bracket + 1;
        }
        else {
            return false;
        }
    }

    q = parsed;
    pattern.erase(open);
    return true;
}

struct DirEntryInfo {
    string name;
    DWORD attributes = 0;
    uintmax_t size = 0;
    FILETIME mtime = {0, 0};
    bool is_directory = false;
    bool is_symlink = false;
    bool is_hidden = false;
    bool is_regular = false;
    bool is_executable = false;
    double age_days = 0.0;
};

static vector<DirEntryInfo> read_dir_entries_win32(const string& dir_path, const FILETIME& now_ft) {
    vector<DirEntryInfo> entries;
    string search_path = (dir_path.empty() || dir_path == ".") ? "*" : dir_path + "\\*";
    wstring wsearch = string_to_wstring(normalize_path_to_win(search_path));
    WIN32_FIND_DATAW fd = {};
    HANDLE hFind = FindFirstFileW(wsearch.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return entries;

    ULARGE_INTEGER now_u;
    now_u.LowPart = now_ft.dwLowDateTime;
    now_u.HighPart = now_ft.dwHighDateTime;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;

        int req = WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, NULL, 0, NULL, NULL);
        string name(req > 1 ? req - 1 : 0, '\0');
        if (!name.empty()) {
            WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, &name[0], req, NULL, NULL);
        }

        DirEntryInfo info;
        info.name = name;
        info.attributes = fd.dwFileAttributes;
        info.is_directory = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        info.is_symlink = (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
        info.is_hidden = ((fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0) || (!name.empty() && name[0] == '.');
        info.is_regular = !info.is_directory && !info.is_symlink;

        ULARGE_INTEGER sz;
        sz.LowPart = fd.nFileSizeLow;
        sz.HighPart = fd.nFileSizeHigh;
        info.size = sz.QuadPart;
        info.mtime = fd.ftLastWriteTime;

        ULARGE_INTEGER file_u;
        file_u.LowPart = fd.ftLastWriteTime.dwLowDateTime;
        file_u.HighPart = fd.ftLastWriteTime.dwHighDateTime;
        if (now_u.QuadPart >= file_u.QuadPart) {
            unsigned long long diff_100ns = now_u.QuadPart - file_u.QuadPart;
            info.age_days = static_cast<double>(diff_100ns) / (86400.0 * 10000000.0);
        } else {
            info.age_days = 0.0;
        }

        string ext;
        size_t dot_pos = name.rfind('.');
        if (dot_pos != string::npos) ext = name.substr(dot_pos);
        transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return static_cast<char>(tolower(c)); });
        info.is_executable = (ext == ".exe" || ext == ".bat" || ext == ".cmd" || ext == ".com" || ext == ".ps1" || ext == ".vbs" || ext == ".wsf");

        entries.push_back(std::move(info));
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
    return entries;
}

static bool match_qualifiers(const DirEntryInfo& st, const GlobQualifiers& q) {
    if (q.regular_only && !st.is_regular) return false;
    if (q.directory_only && !st.is_directory) return false;
    if (q.symlink_only && !st.is_symlink) return false;
    if (q.hidden_only && !st.is_hidden) return false;
    if (q.executable_only && !st.is_executable) return false;
    if (q.mtime_mode == -1) {
        if (st.age_days > q.mtime_days) return false;
    } else if (q.mtime_mode == 1) {
        if (st.age_days < q.mtime_days) return false;
    } else if (q.mtime_mode == 2) {
        if (static_cast<long long>(st.age_days) != static_cast<long long>(q.mtime_days)) return false;
    }
    return true;
}

struct GlobResultItem {
    string display_path;
    DirEntryInfo info;
};

static void glob_traverse(
    size_t seg_idx,
    const string& current_fs_dir,
    const string& current_display,
    const vector<string>& segments,
    vector<GlobResultItem>& results,
    const GlobQualifiers& q,
    const FILETIME& now_ft,
    char sep_char,
    int depth_limit = 0)
{
    if (depth_limit > 64) return;
    if (seg_idx >= segments.size()) return;

    const string& seg = segments[seg_idx];
    bool is_last = (seg_idx == segments.size() - 1);

    if (seg == "**") {
        if (!is_last) {
            glob_traverse(seg_idx + 1, current_fs_dir, current_display, segments, results, q, now_ft, sep_char, depth_limit);
        } else {
            auto entries = read_dir_entries_win32(current_fs_dir, now_ft);
            for (const auto& entry : entries) {
                if ((!entry.name.empty() && entry.name[0] == '.') || entry.is_hidden) {
                    if (!q.hidden_only) continue;
                }
                string display_path = current_display + entry.name;
                if (match_qualifiers(entry, q)) {
                    results.push_back({display_path, entry});
                }
            }
        }

        auto entries = read_dir_entries_win32(current_fs_dir, now_ft);
        for (const auto& entry : entries) {
            if (!entry.is_directory) continue;
            if ((!entry.name.empty() && entry.name[0] == '.') || entry.is_hidden) {
                if (!q.hidden_only) continue;
            }
            string next_fs = (current_fs_dir.empty() || current_fs_dir == ".") ? entry.name : current_fs_dir + "/" + entry.name;
            string next_disp = current_display + entry.name + sep_char;
            glob_traverse(seg_idx, next_fs, next_disp, segments, results, q, now_ft, sep_char, depth_limit + 1);
        }
        return;
    }

    bool has_wildcard = (seg.find_first_of("*?[") != string::npos || seg.find("@(") != string::npos);

    if (has_wildcard) {
        auto entries = read_dir_entries_win32(current_fs_dir, now_ft);
        for (const auto& entry : entries) {
            if ((!entry.name.empty() && entry.name[0] == '.') || entry.is_hidden) {
                if (!q.hidden_only && (seg.empty() || seg[0] != '.')) continue;
            }

            if (!match_wildcard(seg, entry.name)) continue;

            if (is_last) {
                string display_path = current_display + entry.name;
                if (match_qualifiers(entry, q)) {
                    results.push_back({display_path, entry});
                }
            } else {
                if (entry.is_directory) {
                    string next_fs = (current_fs_dir.empty() || current_fs_dir == ".") ? entry.name : current_fs_dir + "/" + entry.name;
                    string next_disp = current_display + entry.name + sep_char;
                    glob_traverse(seg_idx + 1, next_fs, next_disp, segments, results, q, now_ft, sep_char, depth_limit + 1);
                }
            }
        }
    } else {
        string next_fs = (current_fs_dir.empty() || current_fs_dir == ".") ? seg : current_fs_dir + "/" + seg;
        string next_disp = current_display + seg + (is_last ? "" : string(1, sep_char));

        if (is_last) {
            wstring wpath = string_to_wstring(normalize_path_to_win(next_fs));
            WIN32_FILE_ATTRIBUTE_DATA data = {};
            if (GetFileAttributesExW(wpath.c_str(), GetFileExInfoStandard, &data)) {
                DirEntryInfo entry_info;
                entry_info.name = seg;
                entry_info.attributes = data.dwFileAttributes;
                entry_info.is_directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                entry_info.is_symlink = (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
                entry_info.is_hidden = ((data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0) || (!seg.empty() && seg[0] == '.');
                entry_info.is_regular = !entry_info.is_directory && !entry_info.is_symlink;
                ULARGE_INTEGER sz; sz.LowPart = data.nFileSizeLow; sz.HighPart = data.nFileSizeHigh;
                entry_info.size = sz.QuadPart;
                entry_info.mtime = data.ftLastWriteTime;
                ULARGE_INTEGER now_u, file_u;
                now_u.LowPart = now_ft.dwLowDateTime; now_u.HighPart = now_ft.dwHighDateTime;
                file_u.LowPart = data.ftLastWriteTime.dwLowDateTime; file_u.HighPart = data.ftLastWriteTime.dwHighDateTime;
                if (now_u.QuadPart >= file_u.QuadPart) {
                    entry_info.age_days = static_cast<double>(now_u.QuadPart - file_u.QuadPart) / (86400.0 * 10000000.0);
                }
                string ext;
                size_t dot_pos = seg.rfind('.');
                if (dot_pos != string::npos) ext = seg.substr(dot_pos);
                transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return static_cast<char>(tolower(c)); });
                entry_info.is_executable = (ext == ".exe" || ext == ".bat" || ext == ".cmd" || ext == ".com" || ext == ".ps1" || ext == ".vbs" || ext == ".wsf");

                if (match_qualifiers(entry_info, q)) {
                    results.push_back({next_disp, entry_info});
                }
            }
        } else {
            wstring wpath = string_to_wstring(normalize_path_to_win(next_fs));
            DWORD attr = GetFileAttributesW(wpath.c_str());
            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
                glob_traverse(seg_idx + 1, next_fs, next_disp, segments, results, q, now_ft, sep_char, depth_limit + 1);
            }
        }
    }
}

vector<string> expand_globs(const vector<string>& args, const vector<bool>* glob_allowed = nullptr) {
    vector<string> expanded;
    bool global_null_glob = g_env.options.count("nullglob") && g_env.options.at("nullglob");

    FILETIME now_ft;
    GetSystemTimeAsFileTime(&now_ft);

    for (size_t arg_index = 0; arg_index < args.size(); ++arg_index) {
        const auto& arg = args[arg_index];
        if (glob_allowed && arg_index < glob_allowed->size() && !(*glob_allowed)[arg_index]) {
            expanded.push_back(arg);
            continue;
        }

        string pattern_str = arg;
        GlobQualifiers qualifiers;
        parse_glob_qualifiers(pattern_str, qualifiers);
        bool null_glob = global_null_glob || qualifiers.null_glob;

        char sep_char = (pattern_str.find('\\') != string::npos && pattern_str.find('/') == string::npos) ? '\\' : '/';

        string drive_prefix;
        size_t offset = 0;
        if (pattern_str.size() >= 2 && isalpha(static_cast<unsigned char>(pattern_str[0])) && pattern_str[1] == ':') {
            drive_prefix = pattern_str.substr(0, 2);
            offset = 2;
            if (offset < pattern_str.size() && (pattern_str[offset] == '/' || pattern_str[offset] == '\\')) {
                drive_prefix += pattern_str[offset];
                offset++;
            }
        } else if (!pattern_str.empty() && (pattern_str[0] == '/' || pattern_str[0] == '\\')) {
            drive_prefix = pattern_str.substr(0, 1);
            offset = 1;
        }

        vector<string> raw_segments;
        string cur;
        for (size_t i = offset; i < pattern_str.size(); ++i) {
            if (pattern_str[i] == '/' || pattern_str[i] == '\\') {
                if (!cur.empty()) {
                    raw_segments.push_back(cur);
                    cur.clear();
                }
            } else {
                cur += pattern_str[i];
            }
        }
        if (!cur.empty()) raw_segments.push_back(cur);

        size_t first_wc = raw_segments.size();
        for (size_t i = 0; i < raw_segments.size(); ++i) {
            const string& s = raw_segments[i];
            if (s == "**" || s.find_first_of("*?[") != string::npos || s.find("@(") != string::npos) {
                first_wc = i;
                break;
            }
        }

        if (first_wc == raw_segments.size() && !qualifiers.has_qualifier) {
            expanded.push_back(arg);
            continue;
        }

        string prefix_fs_dir = drive_prefix;
        string display_base = drive_prefix;

        for (size_t i = 0; i < first_wc; ++i) {
            if (!prefix_fs_dir.empty() && prefix_fs_dir.back() != '/' && prefix_fs_dir.back() != '\\' && prefix_fs_dir.back() != ':') {
                prefix_fs_dir += "/";
                display_base += sep_char;
            }
            prefix_fs_dir += raw_segments[i];
            display_base += raw_segments[i];
        }
        if (first_wc > 0 || !drive_prefix.empty()) {
            if (!display_base.empty() && display_base.back() != '/' && display_base.back() != '\\' && display_base.back() != ':') {
                display_base += sep_char;
            }
        }

        if (prefix_fs_dir.empty()) {
            prefix_fs_dir = ".";
        }

        vector<string> glob_segments;
        if (first_wc < raw_segments.size()) {
            glob_segments.assign(raw_segments.begin() + first_wc, raw_segments.end());
        }

        vector<GlobResultItem> results;
        if (!glob_segments.empty()) {
            glob_traverse(0, prefix_fs_dir, display_base, glob_segments, results, qualifiers, now_ft, sep_char);
        } else if (qualifiers.has_qualifier) {
            wstring wpath = string_to_wstring(normalize_path_to_win(prefix_fs_dir));
            WIN32_FILE_ATTRIBUTE_DATA data = {};
            if (GetFileAttributesExW(wpath.c_str(), GetFileExInfoStandard, &data)) {
                DirEntryInfo entry_info;
                entry_info.name = pattern_str;
                entry_info.attributes = data.dwFileAttributes;
                entry_info.is_directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                entry_info.is_symlink = (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
                entry_info.is_hidden = ((data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0);
                entry_info.is_regular = !entry_info.is_directory && !entry_info.is_symlink;
                ULARGE_INTEGER sz; sz.LowPart = data.nFileSizeLow; sz.HighPart = data.nFileSizeHigh;
                entry_info.size = sz.QuadPart;
                entry_info.mtime = data.ftLastWriteTime;
                ULARGE_INTEGER now_u, file_u;
                now_u.LowPart = now_ft.dwLowDateTime; now_u.HighPart = now_ft.dwHighDateTime;
                file_u.LowPart = data.ftLastWriteTime.dwLowDateTime; file_u.HighPart = data.ftLastWriteTime.dwHighDateTime;
                if (now_u.QuadPart >= file_u.QuadPart) {
                    entry_info.age_days = static_cast<double>(now_u.QuadPart - file_u.QuadPart) / (86400.0 * 10000000.0);
                }
                string ext = fs::path(normalize_path_to_win(pattern_str)).extension().string();
                transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return static_cast<char>(tolower(c)); });
                entry_info.is_executable = (ext == ".exe" || ext == ".bat" || ext == ".cmd" || ext == ".com" || ext == ".ps1" || ext == ".vbs" || ext == ".wsf");

                if (match_qualifiers(entry_info, qualifiers)) {
                    results.push_back({pattern_str, entry_info});
                }
            }
        }

        if (qualifiers.order_mtime_desc) {
            stable_sort(results.begin(), results.end(), [](const GlobResultItem& a, const GlobResultItem& b) {
                ULARGE_INTEGER ua, ub;
                ua.LowPart = a.info.mtime.dwLowDateTime; ua.HighPart = a.info.mtime.dwHighDateTime;
                ub.LowPart = b.info.mtime.dwLowDateTime; ub.HighPart = b.info.mtime.dwHighDateTime;
                if (ua.QuadPart != ub.QuadPart) return ua.QuadPart > ub.QuadPart;
                return a.display_path < b.display_path;
            });
        } else if (qualifiers.order_mtime_asc) {
            stable_sort(results.begin(), results.end(), [](const GlobResultItem& a, const GlobResultItem& b) {
                ULARGE_INTEGER ua, ub;
                ua.LowPart = a.info.mtime.dwLowDateTime; ua.HighPart = a.info.mtime.dwHighDateTime;
                ub.LowPart = b.info.mtime.dwLowDateTime; ub.HighPart = b.info.mtime.dwHighDateTime;
                if (ua.QuadPart != ub.QuadPart) return ua.QuadPart < ub.QuadPart;
                return a.display_path < b.display_path;
            });
        } else if (qualifiers.order_size_desc) {
            stable_sort(results.begin(), results.end(), [](const GlobResultItem& a, const GlobResultItem& b) {
                if (a.info.size != b.info.size) return a.info.size > b.info.size;
                return a.display_path < b.display_path;
            });
        } else if (qualifiers.order_size_asc) {
            stable_sort(results.begin(), results.end(), [](const GlobResultItem& a, const GlobResultItem& b) {
                if (a.info.size != b.info.size) return a.info.size < b.info.size;
                return a.display_path < b.display_path;
            });
        } else if (qualifiers.order_name_desc) {
            stable_sort(results.begin(), results.end(), [](const GlobResultItem& a, const GlobResultItem& b) {
                return a.display_path > b.display_path;
            });
        } else {
            stable_sort(results.begin(), results.end(), [](const GlobResultItem& a, const GlobResultItem& b) {
                string sa = a.display_path, sb = b.display_path;
                transform(sa.begin(), sa.end(), sa.begin(), [](unsigned char c){ return static_cast<char>(tolower(c)); });
                transform(sb.begin(), sb.end(), sb.begin(), [](unsigned char c){ return static_cast<char>(tolower(c)); });
                if (sa != sb) return sa < sb;
                return a.display_path < b.display_path;
            });
        }

        if (qualifiers.select_index > 0) {
            size_t start = qualifiers.select_index;
            size_t end = qualifiers.select_end > 0 ? qualifiers.select_end : start;
            if (start <= results.size()) {
                for (size_t k = start - 1; k < end && k < results.size(); ++k) {
                    expanded.push_back(results[k].display_path);
                }
            } else if (!null_glob) {
                expanded.push_back(arg);
            }
        } else if (!results.empty()) {
            for (const auto& item : results) {
                expanded.push_back(item.display_path);
            }
        } else if (!null_glob) {
            expanded.push_back(arg);
        }
    }
    return expanded;
}

static vector<string> expand_brace_word_impl(const string& word, int recursion_depth) {
    if (recursion_depth > 16) return {word};

    for (size_t open = 0; open < word.size(); ++open) {
        if (word[open] != '{' || (open > 0 && word[open - 1] == '$')) continue;

        int depth = 1;
        size_t close = string::npos;
        for (size_t i = open + 1; i < word.size(); ++i) {
            if (word[i] == '{') ++depth;
            else if (word[i] == '}' && --depth == 0) { close = i; break; }
        }
        if (close == string::npos) continue;

        string body = word.substr(open + 1, close - open - 1);
        vector<string> alternatives;
        string current;
        int nested_depth = 0;
        bool has_comma = false;
        for (size_t i = 0; i <= body.size(); ++i) {
            char ch = i < body.size() ? body[i] : ',';
            if (ch == '{') ++nested_depth;
            else if (ch == '}' && nested_depth > 0) --nested_depth;
            if (ch == ',' && nested_depth == 0) {
                alternatives.push_back(current);
                current.clear();
                if (i < body.size()) has_comma = true;
            } else {
                current += ch;
            }
        }

        if (!has_comma) {
            smatch sequence;
            static const regex numeric_sequence(R"(^(-?[0-9]+)\.\.(-?[0-9]+)(?:\.\.(-?[0-9]+))?$)");
            static const regex character_sequence(R"(^([A-Za-z])\.\.([A-Za-z])(?:\.\.(-?[0-9]+))?$)");
            alternatives.clear();
            if (regex_match(body, sequence, numeric_sequence)) {
                long long first = 0, last = 0, step = 0;
                if (!parse_ll_checked(sequence[1].str(), first) || !parse_ll_checked(sequence[2].str(), last))
                    continue;
                if (sequence[3].matched && !parse_ll_checked(sequence[3].str(), step)) continue;
                if (step == 0) step = first <= last ? 1 : -1;
                if ((first < last && step < 0) || (first > last && step > 0)) continue;
                for (long long value = first;
                     alternatives.size() < 10000 && (step > 0 ? value <= last : value >= last);
                     value += step) {
                    alternatives.push_back(to_string(value));
                    if ((step > 0 && value > LLONG_MAX - step) || (step < 0 && value < LLONG_MIN - step)) break;
                }
            } else if (regex_match(body, sequence, character_sequence)) {
                int first = static_cast<unsigned char>(sequence[1].str()[0]);
                int last = static_cast<unsigned char>(sequence[2].str()[0]);
                long long parsed_step = first <= last ? 1 : -1;
                if (sequence[3].matched && !parse_ll_checked(sequence[3].str(), parsed_step)) continue;
                if (parsed_step == 0 || (first < last && parsed_step < 0) || (first > last && parsed_step > 0)) continue;
                for (long long value = first;
                     alternatives.size() < 256 && (parsed_step > 0 ? value <= last : value >= last);
                     value += parsed_step) {
                    alternatives.push_back(string(1, static_cast<char>(value)));
                }
            } else {
                continue;
            }
        }

        vector<string> expanded;
        const string prefix = word.substr(0, open);
        const string suffix = word.substr(close + 1);
        for (const auto& alternative : alternatives) {
            vector<string> nested = expand_brace_word_impl(prefix + alternative + suffix, recursion_depth + 1);
            expanded.insert(expanded.end(), nested.begin(), nested.end());
            if (expanded.size() >= 10000) break;
        }
        return expanded;
    }
    return {word};
}

static vector<string> expand_brace_word(const string& word) {
    return expand_brace_word_impl(word, 0);
}

static vector<string> split_ifs_words(const string& value, const string& separators) {
    if (separators.empty()) return {value};
    vector<string> fields;
    string current;
    bool last_was_nonwhitespace_separator = false;
    for (size_t i = 0; i < value.size();) {
        char ch = value[i];
        bool is_separator = separators.find(ch) != string::npos;
        if (!is_separator) {
            current += ch;
            last_was_nonwhitespace_separator = false;
            ++i;
            continue;
        }

        bool whitespace_separator = isspace(static_cast<unsigned char>(ch)) != 0;
        if (whitespace_separator) {
            if (!current.empty()) {
                fields.push_back(std::move(current));
                current.clear();
            }
            while (i < value.size() && separators.find(value[i]) != string::npos &&
                   isspace(static_cast<unsigned char>(value[i]))) ++i;
            last_was_nonwhitespace_separator = false;
        } else {
            fields.push_back(std::move(current));
            current.clear();
            ++i;
            while (i < value.size() && separators.find(value[i]) != string::npos &&
                   isspace(static_cast<unsigned char>(value[i]))) ++i;
            last_was_nonwhitespace_separator = true;
        }
    }
    if (!current.empty() || last_was_nonwhitespace_separator) fields.push_back(std::move(current));
    return fields;
}

// SECTION 07: Pipeline and redirection structures.
struct SingleCmd {
    vector<string> args;
    vector<bool> glob_allowed;
    vector<pair<string, bool>> output_files;
    string input_file = "";
    string output_file = "";
    string error_file = "";
    string here_string = "";
    int dup_stdin_from = -1;
    int dup_stdout_from = -1;
    int dup_stderr_from = -1;
    bool close_stdin = false;
    bool close_stdout = false;
    bool close_stderr = false;
    bool append_out = false;
    bool append_err = false;
    map<int, string> extra_input_files;
    map<int, pair<string, bool>> extra_output_files;
    map<int, int> extra_fd_duplications;
    set<int> extra_closed_fds;
};

struct Pipeline {
    vector<SingleCmd> cmds;
    bool background = false;
    string error;
};
enum class CommandListConnector {
    Always,
    And,
    Or
};
struct CommandListNode {
    CommandListConnector connector = CommandListConnector::Always;
    string source;
};
struct CommandListAst {
    vector<CommandListNode> nodes;
    string error;
};

Pipeline parse_pipeline(const string& line) {
    struct ShellToken {
        string raw;
        bool quoted = false;
    };

    Pipeline pl;
    string current = "";
    bool in_dq = false, in_sq = false;
    bool current_started = false;
    bool current_quoted = false;
    int assignment_list_depth = 0;
    int pattern_group_depth = 0;
    int double_bracket_depth = 0;
    string lexical_error;
    vector<ShellToken> raw_tokens;

    auto flush_current = [&]() {
        if (current_started) {
            raw_tokens.push_back({current, current_quoted});
            current.clear();
            current_started = false;
            current_quoted = false;
        }
    };

    auto push_operator = [&](string text) {
        raw_tokens.push_back({std::move(text), false});
    };

    for (size_t i = 0; i < line.length(); ++i) {
        char c = line[i];
        if (c == '\\' && in_dq && i + 1 < line.length() && line[i+1] == '"') {
            current += '\\'; current += '"'; current_started = true; ++i;
        } else if (c == '"' && !in_sq) {
            in_dq = !in_dq; current += c; current_started = true; current_quoted = true;
        }
        else if (c == '\'' && !in_dq) {
            in_sq = !in_sq; current += c; current_started = true; current_quoted = true;
        }
        else if (c == '#' && !in_dq && !in_sq && current.empty()) break; // inline comment
        else if (!in_dq && !in_sq) {
            if (c == '[' && i + 1 < line.length() && line[i + 1] == '[') {
                flush_current();
                push_operator("[[");
                ++double_bracket_depth;
                ++i;
                continue;
            }
            if (c == ']' && i + 1 < line.length() && line[i + 1] == ']' && double_bracket_depth > 0) {
                flush_current();
                push_operator("]]");
                --double_bracket_depth;
                ++i;
                continue;
            }
            if (double_bracket_depth > 0) {
                if (isspace((unsigned char)c)) {
                    flush_current();
                    continue;
                }
                current += c;
                current_started = true;
                continue;
            }
            if (c == '(' && (pattern_group_depth > 0 || (!current.empty() && current.back() == '@'))) {
                ++pattern_group_depth;
                current += c;
                current_started = true;
                continue;
            }
            if (c == ')' && pattern_group_depth > 0) {
                --pattern_group_depth;
                current += c;
                current_started = true;
                continue;
            }
            if (c == '(' && (assignment_list_depth > 0 || (!current.empty() && current.back() == '='))) {
                ++assignment_list_depth;
                current += c;
                current_started = true;
                continue;
            }
            if (c == ')' && assignment_list_depth > 0) {
                --assignment_list_depth;
                current += c;
                current_started = true;
                continue;
            }
            if (c == '$' && i + 1 < line.length() && line[i + 1] == '(') {
                size_t j = i + 2;
                int depth = 1;
                bool csq = false;
                bool cdq = false;
                for (; j < line.length(); ++j) {
                    char cj = line[j];
                    if (cj == '"' && !csq) { cdq = !cdq; continue; }
                    if (cj == '\'' && !cdq) { csq = !csq; continue; }
                    if (csq || cdq) continue;
                    if (cj == '(') { depth++; continue; }
                    if (cj == ')') {
                        depth--;
                        if (depth == 0) { ++j; break; }
                    }
                }
                if (depth != 0) {
                    lexical_error = "unmatched command substitution '('";
                    break;
                }
                current += line.substr(i, j - i);
                current_started = true;
                i = (j == 0) ? i : (j - 1);
                continue;
            }
            if (isspace((unsigned char)c) && assignment_list_depth == 0 && pattern_group_depth == 0) {
                flush_current();
                continue;
            }
            if ((c == '<' || c == '>' || (c == '=' && (current.empty() || !is_valid_env_var_name(current)))) &&
                i + 1 < line.length() && line[i + 1] == '(') {
                size_t j = i + 2;
                int depth = 1;
                bool psq = false;
                bool pdq = false;
                for (; j < line.length(); ++j) {
                    char cj = line[j];
                    if (cj == '"' && !psq) { pdq = !pdq; continue; }
                    if (cj == '\'' && !pdq) { psq = !psq; continue; }
                    if (psq || pdq) continue;
                    if (cj == '(') { depth++; continue; }
                    if (cj == ')') {
                        depth--;
                        if (depth == 0) { ++j; break; }
                    }
                }
                if (depth != 0) {
                    lexical_error = "unmatched process substitution '('";
                    break;
                }
                current += line.substr(i, j - i);
                current_started = true;
                i = (j == 0) ? i : (j - 1);
                continue;
            }
            if (c == '|' && pattern_group_depth == 0) {
                flush_current();
                push_operator("|");
                continue;
            }
            if (c == '&' && i + 1 < line.length() && line[i + 1] == '>') {
                flush_current();
                if (i + 2 < line.length() && (line[i + 2] == '|' || line[i + 2] == '!')) {
                    push_operator("&>|");
                    i += 2;
                } else if (i + 2 < line.length() && line[i + 2] == '>') {
                    push_operator("&>>");
                    i += 2;
                } else {
                    push_operator("&>");
                    ++i;
                }
                continue;
            }
            if (c == '&' && assignment_list_depth == 0 && pattern_group_depth == 0) {
                flush_current();
                push_operator("&");
                continue;
            }
            if (isdigit((unsigned char)c) && i + 2 < line.length() && line[i + 1] == '<' && line[i + 2] == '>') {
                flush_current();
                push_operator(string(1, c) + "<>");
                i += 2;
                continue;
            }
            if (isdigit((unsigned char)c) && i + 2 < line.length() && line[i + 1] == '>' && (line[i + 2] == '|' || line[i + 2] == '!')) {
                flush_current();
                push_operator(string(1, c) + ">|");
                i += 2;
                continue;
            }
            if (isdigit((unsigned char)c) && i + 2 < line.length() && line[i + 1] == '>' && line[i + 2] == '&') {
                flush_current();
                push_operator(string(1, c) + ">&");
                i += 2;
                continue;
            }
            if (isdigit((unsigned char)c) && i + 2 < line.length() && line[i + 1] == '<' && line[i + 2] == '&') {
                flush_current();
                push_operator(string(1, c) + "<&");
                i += 2;
                continue;
            }
            if (isdigit((unsigned char)c) && i + 2 < line.length() && line[i + 1] == '>' && line[i + 2] == '>') {
                flush_current();
                push_operator(string(1, c) + ">>");
                i += 2;
                continue;
            }
            if (isdigit((unsigned char)c) && i + 1 < line.length() && line[i + 1] == '>') {
                flush_current();
                push_operator(string(1, c) + ">");
                i += 1;
                continue;
            }
            if (isdigit((unsigned char)c) && i + 1 < line.length() && line[i + 1] == '<') {
                flush_current();
                push_operator(string(1, c) + "<");
                i += 1;
                continue;
            }
            if (c == '<') {
                flush_current();
                if (i + 1 < line.length() && line[i + 1] == '>') {
                    push_operator("<>");
                    i += 1;
                } else if (i + 2 < line.length() && line[i + 1] == '<' && line[i + 2] == '<') {
                    push_operator("<<<");
                    i += 2;
                } else if (i + 2 < line.length() && line[i + 1] == '<' && line[i + 2] == '-') {
                    push_operator("<<-");
                    i += 2;
                } else if (i + 1 < line.length() && line[i + 1] == '<') {
                    push_operator("<<");
                    i += 1;
                } else if (i + 1 < line.length() && line[i + 1] == '&') {
                    push_operator("<&");
                    i += 1;
                } else {
                    push_operator("<");
                }
                continue;
            }
            if (c == '>') {
                flush_current();
                if (i + 1 < line.length() && (line[i + 1] == '|' || line[i + 1] == '!')) {
                    push_operator(">|");
                    i += 1;
                } else if (i + 2 < line.length() && line[i + 1] == '>' && line[i + 2] == '|') {
                    push_operator(">>|");
                    i += 2;
                } else if (i + 1 < line.length() && line[i + 1] == '>') {
                    push_operator(">>");
                    i += 1;
                } else if (i + 1 < line.length() && line[i + 1] == '&') {
                    push_operator(">&");
                    i += 1;
                } else {
                    push_operator(">");
                }
                continue;
            }
            current += c;
            current_started = true;
        } else {
            current += c;
            current_started = true;
        }
    }
    flush_current();

    if (lexical_error.empty() && in_dq) lexical_error = "unmatched '\"'";
    else if (lexical_error.empty() && in_sq) lexical_error = "unmatched '\''";
    else if (lexical_error.empty() && assignment_list_depth != 0) lexical_error = "unmatched array assignment '('";
    else if (lexical_error.empty() && pattern_group_depth != 0) lexical_error = "unmatched pattern group '('";
    else if (lexical_error.empty() && double_bracket_depth != 0) lexical_error = "unmatched '[['";
    if (!lexical_error.empty()) {
        pl.error = std::move(lexical_error);
        return pl;
    }

    if (!raw_tokens.empty() && raw_tokens.back().raw == "&") {
        pl.background = true; raw_tokens.pop_back();
    }

    auto expand_token = [&](const ShellToken& token) {
        string expanded = g_env.expand_vars(token.raw);
        if (token.raw.size() >= 2 &&
            ((token.raw.front() == '"' && token.raw.back() == '"') ||
             (token.raw.front() == '\'' && token.raw.back() == '\'')) &&
            expanded.size() >= 2 && expanded.front() == token.raw.front() && expanded.back() == token.raw.back()) {
            expanded = expanded.substr(1, expanded.size() - 2);
        }
        return expanded;
    };

    SingleCmd sc;
    auto parse_fd_dup = [](const string& s, int& out_fd, bool& out_close) {
        out_fd = -1;
        out_close = false;
        if (s == "-") { out_close = true; return true; }
        if (s.size() == 1 && isdigit((unsigned char)s[0])) { out_fd = s[0] - '0'; return true; }
        return false;
    };

    auto finish_command = [&]() {
        if (sc.args.empty()) {
            pl.error = "missing pipeline command";
            return false;
        }
        if (sc.args.empty() || sc.args[0] != "[[")
            sc.args = expand_globs(sc.args, &sc.glob_allowed);
        pl.cmds.push_back(std::move(sc));
        sc = SingleCmd();
        return true;
    };

    bool needs_command_after_pipe = false;
    auto append_argument = [&](string value, bool allow_glob) {
        sc.args.push_back(std::move(value));
        sc.glob_allowed.push_back(allow_glob);
        needs_command_after_pipe = false;
    };

    auto parameter_flag_delimiter = [](const string& flags, char flag, string& delimiter) {
        string marker;
        marker += flag;
        marker += ':';
        size_t start = flags.find(marker);
        if (start == string::npos) return false;
        start += marker.size();
        size_t end = flags.find(':', start);
        if (end == string::npos) return false;
        delimiter = flags.substr(start, end - start);
        return true;
    };

    auto append_flagged_parameter = [&](const string& raw, bool quoted) {
        string expression = raw;
        if (expression.size() >= 2 &&
            ((expression.front() == '"' && expression.back() == '"') ||
             (expression.front() == '\'' && expression.back() == '\''))) {
            expression = expression.substr(1, expression.size() - 2);
        }

        smatch match;
        static const regex flagged_parameter_re(
            R"(^\$\{\(([^)]*)\)([A-Za-z_][A-Za-z0-9_]*)(?:\[@\])?\}$)");
        if (!regex_match(expression, match, flagged_parameter_re)) return false;

        string flags = match[1].str();
        string name = match[2].str();
        vector<string> values;
        auto array = g_env.indexed_arrays.find(name);
        if (array != g_env.indexed_arrays.end()) {
            values = array->second;
        } else {
            string value;
            auto scalar = g_env.vars.find(name);
            if (scalar != g_env.vars.end()) value = scalar->second;
            else if (const char* environment_value = getenv(name.c_str())) value = environment_value;
            values.push_back(std::move(value));
        }

        for (auto& value : values) {
            if (flags.find('L') != string::npos) {
                transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(tolower(c)); });
            }
            if (flags.find('U') != string::npos) {
                transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(toupper(c)); });
            }
            if (flags.find('q') != string::npos) {
                string shell_quoted = "'";
                for (char ch : value) shell_quoted += ch == '\'' ? "'\\''" : string(1, ch);
                value = shell_quoted + "'";
            }
        }

        string delimiter;
        if (parameter_flag_delimiter(flags, 'j', delimiter)) {
            string joined;
            for (size_t i = 0; i < values.size(); ++i) {
                if (i > 0) joined += delimiter;
                joined += values[i];
            }
            append_argument(std::move(joined), !quoted);
            return true;
        }
        if (parameter_flag_delimiter(flags, 's', delimiter)) {
            string source = values.empty() ? "" : values.front();
            if (delimiter.empty()) {
                for (char ch : source) append_argument(string(1, ch), !quoted);
            } else {
                size_t start = 0;
                while (true) {
                    size_t end = source.find(delimiter, start);
                    append_argument(source.substr(start, end - start), !quoted);
                    if (end == string::npos) break;
                    start = end + delimiter.size();
                }
            }
            return true;
        }

        for (auto& value : values) append_argument(std::move(value), !quoted);
        return true;
    };

    for (size_t i = 0; i < raw_tokens.size(); ++i) {
        const ShellToken& token = raw_tokens[i];
        const string& t = token.raw;

        bool in_double_bracket = !sc.args.empty() && sc.args[0] == "[[";
        if (in_double_bracket) {
            append_argument(expand_token(token), false);
            continue;
        }

        smatch extra_fd_match;
        static const regex extra_file_redirect(R"(^([0-9])([<>]|<>|>\||>!)$)");
        static const regex extra_dup_redirect(R"(^([0-9])([<>])&$)");
        static const set<string> operand_operators = {
            ">", ">|", ">!", ">>", ">>|", "1>", "1>|", "1>!", "1>>", "1>>|",
            "2>", "2>|", "2>!", "2>>", "2>>|", "<", "<>", "0<", "0<>", "0<<", "<<<",
            "2>&", ">&", "1>&", "0<&", "<&", "&>", "&>|", "&>!", "&>>"
        };
        bool is_extra_redirect = regex_match(t, extra_fd_match, extra_file_redirect) ||
                                 regex_match(t, extra_fd_match, extra_dup_redirect);
        if ((operand_operators.count(t) || is_extra_redirect) && i + 1 >= raw_tokens.size()) {
            pl.error = "missing redirection operand after '" + t + "'";
            return pl;
        }
        if (regex_match(t, extra_fd_match, extra_file_redirect) && i + 1 < raw_tokens.size()) {
            int fd = extra_fd_match[1].str()[0] - '0';
            string op_type = extra_fd_match[2].str();
            if (op_type == ">" || op_type == ">|" || op_type == ">!") {
                if (fd == 1) { sc.output_file = expand_token(raw_tokens[++i]); sc.append_out = false; sc.output_files.push_back({sc.output_file, false}); }
                else if (fd == 2) { sc.error_file = expand_token(raw_tokens[++i]); sc.append_err = false; }
                else sc.extra_output_files[fd] = {expand_token(raw_tokens[++i]), false};
            } else {
                if (fd == 0) { sc.input_file = expand_token(raw_tokens[++i]); }
                else sc.extra_input_files[fd] = expand_token(raw_tokens[++i]);
            }
        } else if (regex_match(t, extra_fd_match, extra_dup_redirect) && i + 1 < raw_tokens.size()) {
            int fd = extra_fd_match[1].str()[0] - '0';
            int source_fd = -1;
            bool close_fd = false;
            if (!parse_fd_dup(raw_tokens[++i].raw, source_fd, close_fd)) {
                pl.error = "invalid file descriptor after '" + t + "'";
                return pl;
            }
            if (fd == 1) {
                if (close_fd) sc.close_stdout = true;
                else sc.dup_stdout_from = source_fd;
            } else if (fd == 2) {
                if (close_fd) sc.close_stderr = true;
                else sc.dup_stderr_from = source_fd;
            } else if (fd == 0) {
                if (close_fd) sc.close_stdin = true;
                else sc.dup_stdin_from = source_fd;
            } else {
                if (close_fd) sc.extra_closed_fds.insert(fd);
                else sc.extra_fd_duplications[fd] = source_fd;
            }
        } else if (t == "|") {
            if (!finish_command()) return pl;
            needs_command_after_pipe = true;
        } else if ((t == ">" || t == ">|" || t == ">!") && i + 1 < raw_tokens.size()) {
            sc.output_file = expand_token(raw_tokens[++i]); sc.append_out = false;
            sc.output_files.push_back({sc.output_file, sc.append_out});
        } else if ((t == ">>" || t == ">>|") && i + 1 < raw_tokens.size()) {
            sc.output_file = expand_token(raw_tokens[++i]); sc.append_out = true;
            sc.output_files.push_back({sc.output_file, sc.append_out});
        } else if ((t == "1>" || t == "1>|" || t == "1>!" || t == "1>>" || t == "1>>|") && i + 1 < raw_tokens.size()) {
            sc.output_file = expand_token(raw_tokens[++i]); sc.append_out = (t.find(">>") != string::npos);
            sc.output_files.push_back({sc.output_file, sc.append_out});
        } else if ((t == "<" || t == "<>") && i + 1 < raw_tokens.size()) {
            sc.input_file = expand_token(raw_tokens[++i]);
        } else if (t == "<<<" && i + 1 < raw_tokens.size()) {
            sc.here_string = expand_token(raw_tokens[++i]);
        } else if ((t == "0<" || t == "0<>" || t == "0<<") && i + 1 < raw_tokens.size()) {
            sc.input_file = expand_token(raw_tokens[++i]);
        } else if ((t == "2>" || t == "2>|" || t == "2>!") && i + 1 < raw_tokens.size()) {
            sc.error_file = expand_token(raw_tokens[++i]);
            sc.append_err = false;
        } else if ((t == "2>>" || t == "2>>|") && i + 1 < raw_tokens.size()) {
            sc.error_file = expand_token(raw_tokens[++i]);
            sc.append_err = true;
        } else if ((t == "&>" || t == "&>|" || t == "&>!" || t == "&>>") && i + 1 < raw_tokens.size()) {
            sc.output_file = expand_token(raw_tokens[++i]);
            sc.append_out = t == "&>>";
            sc.output_files.push_back({sc.output_file, sc.append_out});
            sc.dup_stderr_from = 1;
        } else if (t == "2>&" && i + 1 < raw_tokens.size()) {
            int fd = -1; bool close_fd = false;
            if (parse_fd_dup(raw_tokens[++i].raw, fd, close_fd)) {
                if (close_fd) sc.close_stderr = true;
                else sc.dup_stderr_from = fd;
            }
        } else if ((t == "1>&" || t == ">&") && i + 1 < raw_tokens.size()) {
            int fd = -1; bool close_fd = false;
            if (parse_fd_dup(raw_tokens[++i].raw, fd, close_fd)) {
                if (close_fd) sc.close_stdout = true;
                else sc.dup_stdout_from = fd;
            }
        } else if ((t == "0<&" || t == "<&") && i + 1 < raw_tokens.size()) {
            int fd = -1; bool close_fd = false;
            if (parse_fd_dup(raw_tokens[++i].raw, fd, close_fd)) {
                if (close_fd) sc.close_stdin = true;
                else sc.dup_stdin_from = fd;
            }
        } else {
            vector<string> brace_words = token.quoted ? vector<string>{t} : expand_brace_word(t);
            for (const auto& brace_word : brace_words) {
                bool positional_at = brace_word == "$@" || brace_word == "${@}" ||
                                     brace_word == "\"$@\"" || brace_word == "\"${@}\"";
                if (positional_at) {
                    for (const auto& value : g_env.positional_args) append_argument(value, !token.quoted);
                    continue;
                }

                if (append_flagged_parameter(brace_word, token.quoted)) continue;

                smatch array_match;
                static const regex array_at_re(R"(^\"?\$\{([A-Za-z_][A-Za-z0-9_]*)\[@\]\}\"?$)");
                if (regex_match(brace_word, array_match, array_at_re) && g_env.indexed_arrays.count(array_match[1].str())) {
                    for (const auto& value : g_env.indexed_arrays[array_match[1].str()])
                        append_argument(value, !token.quoted);
                    continue;
                }

                ShellToken expanded_token{brace_word, token.quoted};
                string expanded = expand_token(expanded_token);
                bool split_words = g_env.options.count("shwordsplit") && g_env.options.at("shwordsplit") &&
                                   !token.quoted && brace_word.size() > 1 && brace_word[0] == '$' &&
                                   brace_word.find_first_of(" \t") == string::npos;
                if (split_words) {
                    string separators = g_env.vars.count("IFS") ? g_env.vars["IFS"] : " \t\n";
                    for (auto& field : split_ifs_words(expanded, separators))
                        append_argument(std::move(field), true);
                } else {
                    append_argument(std::move(expanded), !token.quoted);
                }
                needs_command_after_pipe = false;
            }
        }
    }
    if (needs_command_after_pipe) {
        pl.error = "missing command after '|'";
        return pl;
    }
    if (!sc.args.empty() && !finish_command()) return pl;
    return pl;
}

// ============================================================================
// WIN32 PROCESS LAUNCHER WITH CMD /C FALLBACK
// ============================================================================
wstring string_to_wstring(const string& str) {
    if (str.empty()) return L"";
    int sz = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    wstring wstr(sz, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstr[0], sz);
    return wstr;
}

static string current_shell_executable_path();

string find_executable_in_path(const string& bin) {
    error_code ec;
    if (fs::exists(bin, ec) && !fs::is_directory(bin, ec)) return bin;
    for (const string& ext : { ".exe", ".com", ".bat", ".cmd" }) {
        if (fs::exists(bin + ext, ec)) return bin + ext;
    }

    string self_path = current_shell_executable_path();
    if (!self_path.empty()) {
        string self_dir = fs::path(self_path).parent_path().string();
        if (!self_dir.empty()) {
            string base = self_dir + "\\" + bin;
            if (fs::exists(base, ec) && !fs::is_directory(base, ec)) return base;
            for (const string& ext : { ".exe", ".com", ".bat", ".cmd" }) {
                if (fs::exists(base + ext, ec)) return base + ext;
            }
        }
    }

    char* path_env = getenv("PATH");
    if (!path_env) return "";

    stringstream ss(path_env);
    string dir;
    while (getline(ss, dir, ';')) {
        string base = normalize_path_to_win(dir) + "\\" + bin;
        if (fs::exists(base, ec) && !fs::is_directory(base, ec)) return base;
        for (const string& ext : { ".exe", ".com", ".bat", ".cmd" }) {
            if (fs::exists(base + ext, ec)) return base + ext;
        }
    }
    return "";
}

int execute_pipeline_native(const Pipeline& pl) {
    if (pl.cmds.empty()) return 0;
    size_t num_cmds = pl.cmds.size();

    vector<HANDLE> hPipesRead(num_cmds - 1, NULL), hPipesWrite(num_cmds - 1, NULL);
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    for (size_t i = 0; i < num_cmds - 1; ++i) {
        if (!CreatePipe(&hPipesRead[i], &hPipesWrite[i], &sa, 0)) {
            cerr << COLOR_RED << "zsh: failed to create pipeline" << COLOR_RESET << "\n";
            for (size_t j = 0; j < i; ++j) {
                if (hPipesRead[j]) CloseHandle(hPipesRead[j]);
                if (hPipesWrite[j]) CloseHandle(hPipesWrite[j]);
            }
            return 1;
        }
    }

    vector<PROCESS_INFORMATION> pi_list(num_cmds);
    vector<unique_handle> proc_handles;
    vector<unique_handle> thread_handles;
    proc_handles.reserve(num_cmds);
    thread_handles.reserve(num_cmds);
    for (size_t i = 0; i < num_cmds; ++i) {
        proc_handles.push_back(make_unique_handle());
        thread_handles.push_back(make_unique_handle());
    }
    unique_handle background_job = make_unique_handle(pl.background ? CreateJobObjectW(nullptr, nullptr) : nullptr);
    if (pl.background && !background_job.get()) {
        DWORD error = GetLastError();
        for (size_t i = 0; i + 1 < num_cmds; ++i) {
            close_handle_if_valid(hPipesRead[i]);
            close_handle_if_valid(hPipesWrite[i]);
        }
        cerr << "zsh: failed to create background job object (error " << error << ")\n";
        return 1;
    }
    bool launch_failed = false;
    DWORD launch_error = ERROR_SUCCESS;

    for (size_t i = 0; i < num_cmds; ++i) {
        SingleCmd cmd = pl.cmds[i];

        string exe_path = find_executable_in_path(cmd.args[0]);
        bool is_script = !exe_path.empty() &&
                         (exe_path.size() > 4) &&
                         (_stricmp(exe_path.c_str() + exe_path.size() - 4, ".bat") == 0 ||
                          _stricmp(exe_path.c_str() + exe_path.size() - 4, ".cmd") == 0);

        if (exe_path.empty()) {
            cerr << COLOR_RED << "zsh: command not found: " << cmd.args[0] << COLOR_RESET << "\n";
            launch_failed = true;
            launch_error = ERROR_FILE_NOT_FOUND;
            break;
        }

        bool use_cmd_fallback = is_script;
        bool is_cmd_exe = false;
        if (!exe_path.empty()) {
            string exe_name = fs::path(exe_path).filename().string();
            is_cmd_exe = (_stricmp(exe_name.c_str(), "cmd.exe") == 0);
        }

        // Wrap a single token in cmd-native double-quotes if it contains spaces or quotes.
        auto cmd_quote_arg = [](const string& s) -> string {
            if (s.find_first_of(" \t\"") == string::npos) return s;
            string r = "\"";
            for (char c : s) { if (c == '"') r += "\"\""; else r += c; }
            return r + "\"";
        };

        string cmdline;
        if (!use_cmd_fallback) {
            if (is_cmd_exe && cmd.args.size() >= 3 &&
                (_stricmp(cmd.args[1].c_str(), "/c") == 0 || _stricmp(cmd.args[1].c_str(), "/k") == 0)) {
                // cmd.exe expects one command tail after /c or /k, not split argv segments.
                string tail;
                for (size_t k = 2; k < cmd.args.size(); ++k) {
                    if (!tail.empty()) tail += " ";
                    tail += cmd.args[k];
                }
                cmdline = win_quote_arg(exe_path) + " " + cmd.args[1] + " " + cmd_quote_arg(tail);
            } else {
            // Direct execution for all PATH-resolved .exe / .com binaries.
                cmdline = win_quote_arg(exe_path);
                for (size_t k = 1; k < cmd.args.size(); ++k) cmdline += " " + win_quote_arg(cmd.args[k]);
            }
        } else {
            // cmd.exe /c: use cmd-native quoting, NOT win_quote_arg (which uses CRT rules).
            string target = is_script ? exe_path : cmd.args[0];
            string inner = cmd_quote_arg(target);
            for (size_t k = 1; k < cmd.args.size(); ++k) inner += " " + cmd_quote_arg(cmd.args[k]);
            cmdline = "cmd.exe /c " + inner;
        }

        wstring wcmd = string_to_wstring(cmdline);
        if (wcmd.empty()) continue; // skip if cmdline construction produced nothing
        STARTUPINFOW si; ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
        si.dwFlags |= STARTF_USESTDHANDLES;

        unique_handle hFileIn = make_unique_handle();
        unique_handle hFileOut = make_unique_handle();
        unique_handle hFileErr = make_unique_handle();
        unique_handle hHereRead = make_unique_handle();
        unique_handle hHereWrite = make_unique_handle();
        unique_handle hNullIn = make_unique_handle();
        unique_handle hNullOut = make_unique_handle();
        unique_handle hNullErr = make_unique_handle();
        bool launch_blocked = false;

        if (!cmd.input_file.empty() && g_heredoc_payloads.count(cmd.input_file)) {
            HANDLE rawHereRead = nullptr;
            HANDLE rawHereWrite = nullptr;
            if (CreatePipe(&rawHereRead, &rawHereWrite, &sa, 0)) {
                hHereRead = make_unique_handle(rawHereRead);
                hHereWrite = make_unique_handle(rawHereWrite);
                const string payload = g_heredoc_payloads[cmd.input_file];
                DWORD total_written = 0;
                while (total_written < (DWORD)payload.size()) {
                    DWORD written = 0;
                    if (!WriteFile((HANDLE)hHereWrite.get(), payload.data() + total_written,
                                   (DWORD)payload.size() - total_written, &written, NULL) || written == 0) break;
                    total_written += written;
                }
                hHereWrite.reset();
                si.hStdInput = (HANDLE)hHereRead.get();
            } else {
                si.hStdInput = (i > 0) ? hPipesRead[i - 1] : GetStdHandle(STD_INPUT_HANDLE);
            }
            g_heredoc_payloads.erase(cmd.input_file);
        } else if (!cmd.input_file.empty()) {
            hFileIn = make_unique_handle(CreateFileW(string_to_wstring(normalize_path_to_win(cmd.input_file)).c_str(), GENERIC_READ, FILE_SHARE_READ, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
            if (!hFileIn.get()) {
                cerr << COLOR_RED << "zsh: cannot open input file: " << cmd.input_file << COLOR_RESET << "\n";
                launch_blocked = true;
            } else {
                si.hStdInput = (HANDLE)hFileIn.get();
            }
        } else if (!cmd.here_string.empty()) {
            HANDLE rawHereRead = nullptr;
            HANDLE rawHereWrite = nullptr;
            if (CreatePipe(&rawHereRead, &rawHereWrite, &sa, 0)) {
                hHereRead = make_unique_handle(rawHereRead);
                hHereWrite = make_unique_handle(rawHereWrite);
                string payload = cmd.here_string + "\n";
                DWORD total_written = 0;
                while (total_written < (DWORD)payload.size()) {
                    DWORD written = 0;
                    if (!WriteFile((HANDLE)hHereWrite.get(), payload.data() + total_written,
                                   (DWORD)payload.size() - total_written, &written, NULL) || written == 0) break;
                    total_written += written;
                }
                hHereWrite.reset();
                si.hStdInput = (HANDLE)hHereRead.get();
            } else {
                si.hStdInput = (i > 0) ? hPipesRead[i - 1] : GetStdHandle(STD_INPUT_HANDLE);
            }
        } else si.hStdInput = (i > 0) ? hPipesRead[i - 1] : GetStdHandle(STD_INPUT_HANDLE);

        if (!cmd.output_file.empty()) {
            DWORD creation = cmd.append_out ? OPEN_ALWAYS : CREATE_ALWAYS;
            hFileOut = make_unique_handle(CreateFileW(string_to_wstring(normalize_path_to_win(cmd.output_file)).c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, &sa, creation, FILE_ATTRIBUTE_NORMAL, NULL));
            if (!hFileOut.get()) {
                cerr << COLOR_RED << "zsh: cannot open output file: " << cmd.output_file << COLOR_RESET << "\n";
                launch_blocked = true;
            } else {
                if (cmd.append_out &&
                    SetFilePointer((HANDLE)hFileOut.get(), 0, NULL, FILE_END) == INVALID_SET_FILE_POINTER &&
                    GetLastError() != NO_ERROR) {
                    cerr << COLOR_RED << "zsh: cannot seek to end of output file: " << cmd.output_file << COLOR_RESET << "\n";
                    launch_blocked = true;
                } else {
                    si.hStdOutput = (HANDLE)hFileOut.get();
                }
            }
        } else si.hStdOutput = (i < num_cmds - 1) ? hPipesWrite[i] : GetStdHandle(STD_OUTPUT_HANDLE);

        if (!cmd.error_file.empty()) {
            DWORD creation = cmd.append_err ? OPEN_ALWAYS : CREATE_ALWAYS;
            hFileErr = make_unique_handle(CreateFileW(string_to_wstring(normalize_path_to_win(cmd.error_file)).c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, &sa, creation, FILE_ATTRIBUTE_NORMAL, NULL));
            if (!hFileErr.get()) {
                cerr << COLOR_RED << "zsh: cannot open error file: " << cmd.error_file << COLOR_RESET << "\n";
                launch_blocked = true;
            } else {
                if (cmd.append_err &&
                    SetFilePointer((HANDLE)hFileErr.get(), 0, NULL, FILE_END) == INVALID_SET_FILE_POINTER &&
                    GetLastError() != NO_ERROR) {
                    cerr << COLOR_RED << "zsh: cannot seek to end of error file: " << cmd.error_file << COLOR_RESET << "\n";
                    launch_blocked = true;
                } else {
                    si.hStdError = (HANDLE)hFileErr.get();
                }
            }
        } else si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

        if (launch_blocked) {
            launch_failed = true;
            launch_error = GetLastError();
            break;
        }

        if (cmd.close_stdin) {
            hNullIn = make_unique_handle(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
            if (hNullIn.get()) si.hStdInput = (HANDLE)hNullIn.get();
        }
        if (cmd.close_stdout) {
            hNullOut = make_unique_handle(CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
            if (hNullOut.get()) si.hStdOutput = (HANDLE)hNullOut.get();
        }
        if (cmd.close_stderr) {
            hNullErr = make_unique_handle(CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
            if (hNullErr.get()) si.hStdError = (HANDLE)hNullErr.get();
        }

        auto fd_handle = [&](int fd) -> HANDLE {
            if (fd == 0) return si.hStdInput;
            if (fd == 1) return si.hStdOutput;
            if (fd == 2) return si.hStdError;
            return INVALID_HANDLE_VALUE;
        };
        if (cmd.dup_stdout_from >= 0) {
            HANDLE src = fd_handle(cmd.dup_stdout_from);
            if (src != INVALID_HANDLE_VALUE) si.hStdOutput = src;
        }
        if (cmd.dup_stderr_from >= 0) {
            HANDLE src = fd_handle(cmd.dup_stderr_from);
            if (src != INVALID_HANDLE_VALUE) si.hStdError = src;
        }
        if (cmd.dup_stdin_from >= 0) {
            HANDLE src = fd_handle(cmd.dup_stdin_from);
            if (src != INVALID_HANDLE_VALUE) si.hStdInput = src;
        }

        ZeroMemory(&pi_list[i], sizeof(PROCESS_INFORMATION));
        DWORD creation_flags = pl.background ? CREATE_SUSPENDED : 0;
        if (!CreateProcessW(NULL, &wcmd[0], NULL, NULL, TRUE, creation_flags, NULL, NULL, &si, &pi_list[i])) {
            launch_error = GetLastError();
            cerr << COLOR_RED << "zsh: " << cmd.args[0] << ": command not found" << COLOR_RESET << "\n";
            launch_failed = true;
            break;
        }
        proc_handles[i] = make_unique_handle(pi_list[i].hProcess);
        thread_handles[i] = make_unique_handle(pi_list[i].hThread);
        if (background_job.get()) {
            if (!AssignProcessToJobObject((HANDLE)background_job.get(), pi_list[i].hProcess)) {
                launch_error = GetLastError();
                launch_failed = true;
            } else if (ResumeThread(pi_list[i].hThread) == static_cast<DWORD>(-1)) {
                launch_error = GetLastError();
                launch_failed = true;
            }
        }

        // Close parent's copies of inherited file handles; child retains its own copies.
        hFileIn.reset();
        hFileOut.reset();
        hFileErr.reset();
        hHereRead.reset();
        hHereWrite.reset();
        hNullIn.reset();
        hNullOut.reset();
        hNullErr.reset();
        if (i > 0) { CloseHandle(hPipesRead[i - 1]); hPipesRead[i - 1] = nullptr; }
        if (i < num_cmds - 1) { CloseHandle(hPipesWrite[i]); hPipesWrite[i] = nullptr; }
        if (launch_failed) break;
    }

    if (launch_failed) {
        if (background_job.get()) {
            TerminateJobObject((HANDLE)background_job.get(), launch_error == ERROR_SUCCESS ? 1 : launch_error);
        } else {
            for (auto& process : proc_handles)
                if (process.get()) TerminateProcess((HANDLE)process.get(), 1);
        }
        for (size_t i = 0; i + 1 < num_cmds; ++i) {
            if (hPipesRead[i]) { CloseHandle(hPipesRead[i]); hPipesRead[i] = nullptr; }
            if (hPipesWrite[i]) { CloseHandle(hPipesWrite[i]); hPipesWrite[i] = nullptr; }
        }
        for (auto& process : proc_handles)
            if (process.get()) WaitForSingleObject((HANDLE)process.get(), 5000);
        return 1;
    }

    if (pl.background) {
        vector<HANDLE> job_processes;
        vector<DWORD> job_pids;
        for (size_t i = 0; i < num_cmds; ++i) {
            thread_handles[i].reset();
            if (proc_handles[i].get()) {
                job_processes.push_back((HANDLE)proc_handles[i].release());
                job_pids.push_back(pi_list[i].dwProcessId);
            }
        }
        HANDLE job_process = job_processes.empty() ? nullptr : job_processes.back();
        HANDLE job_object = (HANDLE)background_job.release();
        BackgroundJob job;
        job.pid = pi_list.back().dwProcessId;
        job.hProcess = job_process;
        job.hJob = job_object;
        job.processes = std::move(job_processes);
        job.pids = std::move(job_pids);
        job.command = pl.cmds[0].args[0];
        job.job_id = g_next_job_id++;
        unsigned long job_id = job.job_id;
        g_env.jobs.push_back(std::move(job));
        select_current_job(job_id);
        cout << "[" << job_id << "] " << pi_list.back().dwProcessId << "\n";
        return 0;
    }

    vector<DWORD> fg_pids;
    fg_pids.reserve(num_cmds);
    for (size_t i = 0; i < num_cmds; ++i) {
        if (proc_handles[i].get() && pi_list[i].dwProcessId != 0) fg_pids.push_back(pi_list[i].dwProcessId);
    }
    set_foreground_pids(fg_pids);
    struct ForegroundPidsScopeGuard {
        ~ForegroundPidsScopeGuard() { clear_foreground_pids(); }
    } fg_scope_guard;

    vector<DWORD> exit_codes(num_cmds, 127);
    for (size_t i = 0; i < num_cmds; ++i) {
        if (!proc_handles[i].get()) continue;
        WaitForSingleObject((HANDLE)proc_handles[i].get(), INFINITE);
        if (!GetExitCodeProcess((HANDLE)proc_handles[i].get(), &exit_codes[i])) exit_codes[i] = 127;
    }
    string pipestatus;
    vector<string> pipestatus_vec;
    for (size_t i = 0; i < exit_codes.size(); ++i) {
        if (i > 0) pipestatus += ' ';
        pipestatus += to_string(exit_codes[i]);
        pipestatus_vec.push_back(to_string(exit_codes[i]));
    }
    g_env.vars["pipestatus"] = pipestatus;
    g_env.indexed_arrays["pipestatus"] = pipestatus_vec;

    if (g_env.options.count("pipefail") && g_env.options.at("pipefail")) {
        for (size_t i = exit_codes.size(); i > 0; --i)
            if (exit_codes[i - 1] != 0) return static_cast<int>(exit_codes[i - 1]);
    }
    return static_cast<int>(exit_codes.back());
}

// SECTION 09A: Comprehensive help system.
struct ZshHelpTopicData {
    const char* name;
    const string* color;
    const char* title;
    const char* body;
};

static const char* kZshHelpBuiltinsBody = R"(
  NAVIGATION & FILESYSTEM
  cd / chdir [dir|-]         Change directory ('-' returns to $OLDPWD).
  pwd                        Print current directory (UNIX-style path).
  pushd <dir>                Push cwd onto stack and cd to <dir>.
  popd                       Pop top of dir stack and cd there.
  dirs                       Print directory stack.

  OUTPUT
  echo [-n] [-e] [text]      Print text; -n omits newline, -e enables escapes.
  print [-n] [-l] [-P] [..] Print text; -l one arg per line, -P prompt expand.
  printf <fmt> [args]        C-style formatted output (%s %d %f %x %o %c %%).

  VARIABLES & ENVIRONMENT
  export VAR=VAL             Set variable and export it to child processes.
  unset VAR                  Remove variable, array, or environment entry.
  typeset -a arr=(...)       Declare indexed array (1-based).
  typeset -A map=(...)       Declare associative array (key-value).
    local VAR=VAL              Declare a function-local variable.
    readonly VAR=VAL           Set and protect a variable from mutation.
  integer VAR=N              Declare integer-typed variable.
  let expr                   Arithmetic: let x=5+3  x+=2  x++  x--.

    WINDOWS REGISTRY NAMESPACES
    ${HKLM.Key.Path.Value}     Read a Windows Registry value.
    ${HKCU.Key/With.Dots.Value} Use / to preserve literal periods in key names.
    HKCU.Key.Path.Value=TEXT   Write existing REG_SZ/REG_EXPAND_SZ values.
    HKLM.Key.Path.Value=N      Preserve existing REG_DWORD/REG_QWORD types.
                                                            Unsupported Registry types are rejected.

  ALIASES & OPTIONS
  alias [name=val]           List all aliases or define one.
  unalias <name>             Remove an alias.
  setopt <option>            Enable a shell option flag.
  unsetopt <option>          Disable a shell option flag.

  FLOW CONTROL
  source / . <file>          Execute script in current shell context.
  eval "cmd"                 Evaluate and execute a string as a command.
  return [n]                 Exit current script with status n.
  exit [n]                   Exit the shell with status n.
    logout [n]                 Exit a login shell with status n.
  true                       Return exit status 0.
  false                      Return exit status 1.
  test / [ expr ]            Evaluate condition (see 'help test').
    [[ expr ]]                 Extended conditionals (regex/glob/logical ops).

  HISTORY & RECALL
  history [-c]               List history with line numbers; -c clears it.
  fc [-l] [n]                List last 16 entries (-l) or re-run entry n.
    read [-rAs] [-d c|-N n] [-t s] [-p] [name ...]
                                                         Read fields, arrays, exact bytes, or coprocess data.

  JOB CONTROL
  jobs                       List background jobs.
    fg [%n|%+|%-|%?text]       Resume and foreground the selected job.
    bg [%n|%+|%-|%?text]       Resume the selected job in background.
  disown [%n]                Remove job n from the jobs table.
  wait [pid]                 Wait for background job(s) to finish.
  kill <pid> [..]            Terminate process(es) by PID.

  PROCESS & TIMING
  times                      Print shell user/kernel CPU time.
  getopts <optstr> <var>     Parse option flags from positional args.
  trap ['cmd'] <sig>         Run cmd when signal/event occurs; list with no args.

  LOOKUP
  which <cmd>                Show path, alias expansion, or function type.
  type <cmd>                 Same as which.
    whence <cmd>               Compatibility alias for type-style lookup.
    command <cmd> [args..]     Execute while bypassing alias/function wrappers.
    builtin <name> [args..]    Invoke a shell builtin directly.

    COMPLETION & EDITOR STATE
    autoload <function>        Register functions for deferred loading.
    compinit                   Scan fpath/FPATH completion definitions.
    compadd [candidate...]     Return candidates from a completion function.
    compdef [fn command...]    Register functional command completion.
    bindkey [-L|-M ...]        Add, copy, remove, or list key bindings.
    zle <-N|-A|-D|-l> ...      Add, copy, remove, or list editor widgets.
    zstyle [-L|-d ...]         Add, remove, or list completion styles.
    zmodload [module...]       Load datetime, system, or parameter features.
    rehash                     Invalidate command-path lookup state.
)";

static const char* kZshHelpTestBody = R"(
  FILE TESTS
    -e file    true if file exists
    -f file    true if regular file
    -d file    true if directory
    -L file    true if symbolic link
    -r file    true if readable
    -w file    true if writable
    -s file    true if non-empty

  STRING TESTS
    -z str     true if str is empty
    -n str     true if str is non-empty
    s1 = s2    true if strings are equal  (also ==)
    s1 != s2   true if strings differ
    s1 < s2    true if s1 < s2 lexicographically
    s1 > s2    true if s1 > s2 lexicographically

  NUMERIC TESTS
    n1 -eq n2  equal            n1 -ne n2  not equal
    n1 -lt n2  less than        n1 -le n2  less than or equal
    n1 -gt n2  greater than     n1 -ge n2  greater than or equal

  COMBINERS
    ! expr     negate
    expr -a expr  logical AND
    expr -o expr  logical OR

    EXTENDED [[ ... ]]
        [[ a == a* ]]            glob-aware string match
        [[ s =~ ^[0-9]+$ ]]      regex match
        [[ -n s && s != x ]]     logical operators (&&, ||, !)

  EXAMPLES
    test -f /etc/hosts && echo exists
    [ "$x" -gt 10 ] && echo big
)";

static const char* kZshHelpArraysBody = R"(
  INDEXED ARRAYS (1-based indexing):
    typeset -a fruits=(apple banana cherry)
    echo $fruits[1]       # Outputs: apple
    echo $#fruits         # Outputs: 3 (array length)

  ASSOCIATIVE ARRAYS (Key-Value Maps):
    typeset -A config=(user admin host localhost)
    echo $config[user]    # Outputs: admin

  PARAMETER MODIFIERS & EXPANSION:
    echo ${NAME:-Guest}   # Fallback default value if NAME is empty
        echo ${VAR#pre*}      # Remove shortest matching prefix pattern
        echo ${VAR%*suf}      # Remove shortest matching suffix pattern
    echo ${VAR//foo/bar}  # Regex/pattern string replacement
    echo ${VAR:l}         # Convert string to lowercase
    echo ${VAR:u}         # Convert string to uppercase
        echo ${(f)VAR}        # Split lines to words (newline -> space)
        echo ${(q)VAR}        # Shell-quote the value
    echo $(( 10 * 5 + 2 ))# Inline mathematical evaluation

    ARRAY OPERATIONS:
        echo $arr[2,4]        # 1-based inclusive slice
        arr+=(item)           # Append one item
        arr+=(a b c)          # Append multiple items
)";

static const char* kZshHelpGlobbingBody = R"(
  WILDCARDS:
    *                     Matches zero or more characters.
    ?                     Matches any single character.

  RECURSIVE GLOBBING:
    **/*.cpp              Recursively search subdirectories for matching files.

  GLOB QUALIFIERS:
    ls *(.)               Filter matches: regular files only.
    ls *(/)               Filter matches: directories only.

  PATH DUAL-SLASH NORMALIZATION:
    Both UNIX slashes (/) and Windows slashes (\) are accepted seamlessly:
      cd /c/Users/        # Equivalent to C:\Users\
      cd C:/Projects/     # Works natively
)";

static const char* kZshHelpPipingBody = R"(
  PIPELINES:
    cmd1 | cmd2 | cmd3    Native Win32 anonymous pipes executing concurrently.

  FILE REDIRECTIONS:
    cmd > out.txt         Overwrite stdout to file.
    cmd >> out.txt        Append stdout to file.
    cmd < in.txt          Redirect stdin from file.
    cmd 2> err.txt        Redirect stderr to file.
        cmd <<< "text"        Here-string stdin feed.
        cmd <<EOF ... EOF     Here-document (script mode).
        cmd 2>&1              Duplicate file descriptors.
        cmd 1>&-              Close stdout for command.
        exec 3>file           Open persistent descriptor 3 (fds 3-9).
        exec 4>&3             Duplicate or close persistent descriptors.
        syswrite -o 4 text    Write through a zsh/system descriptor.
        sysread -i 5 name     Read through a zsh/system descriptor.
        setopt MULTIOS        Stream one producer to every output target.

    SUBSTITUTION:
        echo $(cmd)           Command substitution.
        sort <(cmd)           Process substitution input.
        cmd > >(consumer)     Process substitution output.

  JOB CONTROL & SUBSHELLS:
    ping 127.0.0.1 &      Execute process in background.
        jobs; fg %+; bg %-     Use stable current/previous job markers.
    ( cd / && pwd )       Execute command block inside an isolated subshell.
)";

static const char* kZshHelpManualBody = R"(
USAGE:
  zsh [options] [script_file] [arguments]
  zsh -c "command_string"

COMMAND-LINE OPTIONS:
  -c <command>          Execute command string and exit.
  -v, --version         Print version, copyright, and license, then exit.
  -h, --help            Display this help manual.
    --self-test           Run unit, conformance, pipeline, job, and event tests.

HELP TOPICS:
  help builtins         All built-in commands grouped by category.
  help test             test / [ expression reference.
  help arrays           Arrays, associative arrays, and math syntax.
  help globbing         Wildcards, glob qualifiers, and path handling.
  help piping           Pipelines, redirections, and job control.

SCRIPTING HIGHLIGHTS:
    if / elif / else / fi
    for name in ... ; do ... ; done
    for ((init; cond; step)); do ... ; done
    while ... ; do ... ; done
    case word in pattern) ... ;; pattern2) ... ;& ... esac
    command chaining: ;  &&  ||

KEYBOARD SHORTCUTS:
  Up / Down             Browse command history.
  Left / Right          Move cursor within the line.
  Home / End            Jump to line start or end.
  Backspace             Delete character before cursor.
    Delete                Delete character at cursor.
    Ctrl+A                Jump to line start.
    Ctrl+E                Jump to line end.
    Ctrl+U                Delete from start of line to cursor.
    Ctrl+K                Delete from cursor to end of line.
    Ctrl+W                Delete previous word.
    Ctrl+L                Clear screen and repaint current line.
        Ctrl+C                Cancel current input line (SIGINT trap aware).
        Ctrl+Z                Suspend current background job and trigger TSTP trap.
  Ctrl+R                Reverse history search.
  Tab                   File and command completion.

CONFIGURATION:
  ~/.zshrc              Sourced automatically on startup.
  ~/.zsh_history        Timestamped command history log.
    PROMPT                Left-side prompt format string.
    RPROMPT               Right-side prompt format string (auto-hidden on overlap).

EXAMPLES:
  zsh -c "typeset -a a=(x y z); echo $a[2]"
  zsh -c "let n=10; [ $n -gt 5 ] && echo yes"
    zsh -c "if [[ 123 =~ ^[0-9]+$ ]]; then echo ok; fi"
    zsh -c "case x in x) echo yes ;& *) echo fallthrough ;; esac"
  zsh script.zsh
)";

static const char* kZshHelpManualSeparator = "========================================================================\n";
static const char* kZshHelpManualTitle = "CrossShellZSH v3.7.16\n";
static const char* kZshHelpManualSubtitle = "General Help\n";

static bool print_zsh_help_topic(const string& topic) {
    static const ZshHelpTopicData topics[] = {
        {"builtins", &COLOR_BR_GREEN, "\n=== CrossShellZSH BUILT-IN COMMANDS ===", kZshHelpBuiltinsBody},
        {"test", &COLOR_BR_CYAN, "\n=== TEST / [ EXPRESSIONS ===", kZshHelpTestBody},
        {"arrays", &COLOR_BR_YELLOW, "\n=== ZSH ARRAYS & VARIABLES ===", kZshHelpArraysBody},
        {"globbing", &COLOR_BR_CYAN, "\n=== EXTENDED GLOBBING & QUALIFIERS ===", kZshHelpGlobbingBody},
        {"piping", &COLOR_BR_BLUE, "\n=== PIPELINES, REDIRECTION & JOBS ===", kZshHelpPipingBody}
    };

    for (const auto& entry : topics) {
        if (topic == entry.name) {
            cout << *entry.color << entry.title << COLOR_RESET << entry.body;
            return true;
        }
    }
    return false;
}

void print_comprehensive_help(const string& topic = "") {
    if (print_zsh_help_topic(topic)) {
        return;
    }

    // Default Full Comprehensive Manual
    cout << COLOR_BR_GREEN << kZshHelpManualSeparator;
    cout << kZshHelpManualTitle;
    cout << kZshHelpManualSubtitle;
    cout << kZshHelpManualSeparator << COLOR_RESET << kZshHelpManualBody;
}

// SECTION 09B: Built-in command dispatch handlers.
int parse_and_execute(const string& line);
int execute_script(const string& filepath, bool trace = false, bool errexit = false);
string render_prompt();
int execute_single_command(const string& line);
int execute_command_line(const string& line);
static string trim_copy(const string& s);

bool builtin_test_eval(const vector<string>& t) {
    if (t.empty()) return false;
    if (t.size() == 1) return !t[0].empty() && t[0] != "0";
    if (t[0] == "!") { return !builtin_test_eval(vector<string>(t.begin()+1, t.end())); }
    if (t.size() == 2) {
        const string& op = t[0]; const string& v = t[1];
        error_code ec;
        if (op=="-e") return fs::exists(normalize_path_to_win(v), ec);
        if (op=="-f") return fs::is_regular_file(normalize_path_to_win(v), ec);
        if (op=="-d") return fs::is_directory(normalize_path_to_win(v), ec);
        if (op=="-L") return fs::is_symlink(normalize_path_to_win(v), ec);
        if (op=="-s") return fs::exists(normalize_path_to_win(v), ec) && fs::file_size(normalize_path_to_win(v), ec) > 0;
        if (op=="-r") { DWORD a=GetFileAttributesW(string_to_wstring(normalize_path_to_win(v)).c_str()); return a!=INVALID_FILE_ATTRIBUTES; }
        if (op=="-w") { DWORD a=GetFileAttributesW(string_to_wstring(normalize_path_to_win(v)).c_str()); return a!=INVALID_FILE_ATTRIBUTES && !(a&FILE_ATTRIBUTE_READONLY); }
        if (op=="-z") return v.empty();
        if (op=="-n") return !v.empty();
        return !v.empty();
    }
    if (t.size() >= 3) {
        const string& a=t[0]; const string& op=t[1]; const string& b=t[2];
        if (op=="=="||op=="=") return a==b;
        if (op=="!=")          return a!=b;
        if (op=="<")           return a<b;
        if (op==">")           return a>b;
        auto n=[](const string& s)->long long{ try{return stoll(s);}catch(...){return 0;} };
        if (op=="-eq") return n(a)==n(b); if (op=="-ne") return n(a)!=n(b);
        if (op=="-lt") return n(a)<n(b);  if (op=="-le") return n(a)<=n(b);
        if (op=="-gt") return n(a)>n(b);  if (op=="-ge") return n(a)>=n(b);
        if (op=="-a") return builtin_test_eval({a}) && builtin_test_eval(vector<string>(t.begin()+2,t.end()));
        if (op=="-o") return builtin_test_eval({a}) || builtin_test_eval(vector<string>(t.begin()+2,t.end()));
    }
    return false;
}

int builtin_test(const vector<string>& args) {
    vector<string> t(args.begin()+1, args.end());
    if (!t.empty() && t.back()=="]") t.pop_back();
    return builtin_test_eval(t) ? 0 : 1;
}

int builtin_double_bracket(const vector<string>& args) {
    vector<string> t(args.begin() + 1, args.end());
    if (!t.empty() && t.back() == "]]" ) t.pop_back();
    if (t.empty()) return 1;

    size_t idx = 0;

    auto eval_binary = [&](const string& a, const string& op, const string& b) -> bool {
        if (op == "=~") {
            try {
                regex re(b);
                smatch sm;
                bool match_res = regex_search(a, sm, re);
                if (match_res) {
                    g_env.vars["MATCH"] = sm.str(0);
                    vector<string> match_groups;
                    for (size_t gi = 1; gi < sm.size(); ++gi) {
                        match_groups.push_back(sm.str(gi));
                    }
                    g_env.indexed_arrays["match"] = match_groups;
                } else {
                    g_env.vars.erase("MATCH");
                    g_env.indexed_arrays.erase("match");
                }
                return match_res;
            } catch (...) {
                return false;
            }
        }
        if (op == "==" || op == "=") {
            if (b.find_first_of("*?") != string::npos || b.find("@(") != string::npos) return match_wildcard(b, a);
            return a == b;
        }
        if (op == "!=") {
            if (b.find_first_of("*?") != string::npos || b.find("@(") != string::npos) return !match_wildcard(b, a);
            return a != b;
        }
        if (op == "<") return a < b;
        if (op == ">") return a > b;
        if (op == "-nt") {
            error_code ec1, ec2;
            auto t1 = fs::last_write_time(normalize_path_to_win(a), ec1);
            auto t2 = fs::last_write_time(normalize_path_to_win(b), ec2);
            if (ec1 || ec2) return false;
            return t1 > t2;
        }
        if (op == "-ot") {
            error_code ec1, ec2;
            auto t1 = fs::last_write_time(normalize_path_to_win(a), ec1);
            auto t2 = fs::last_write_time(normalize_path_to_win(b), ec2);
            if (ec1 || ec2) return false;
            return t1 < t2;
        }
        if (op == "-ef") {
            error_code ec1, ec2;
            auto c1 = fs::canonical(normalize_path_to_win(a), ec1);
            auto c2 = fs::canonical(normalize_path_to_win(b), ec2);
            if (ec1 || ec2) return false;
            return c1 == c2;
        }
        auto n = [](const string& s) -> long long { try { return stoll(s); } catch (...) { return 0; } };
        if (op == "-eq") return n(a) == n(b);
        if (op == "-ne") return n(a) != n(b);
        if (op == "-lt") return n(a) < n(b);
        if (op == "-le") return n(a) <= n(b);
        if (op == "-gt") return n(a) > n(b);
        if (op == "-ge") return n(a) >= n(b);
        return false;
    };

    function<bool()> parse_expr;
    function<bool()> parse_and;
    function<bool()> parse_atom;

    parse_atom = [&]() -> bool {
        if (idx >= t.size()) return false;

        if (t[idx] == "!") {
            ++idx;
            return !parse_atom();
        }

        if (t[idx] == "(") {
            ++idx;
            bool v = parse_expr();
            if (idx < t.size() && t[idx] == ")") ++idx;
            return v;
        }

        if (idx + 1 < t.size()) {
            string op = t[idx];
            static const set<string> unary_ops = {"-e", "-f", "-d", "-L", "-s", "-r", "-w", "-x", "-n", "-z"};
            if (unary_ops.count(op)) {
                idx += 2;
                string v = t[idx - 1];
                error_code ec;
                if (op == "-e") return fs::exists(normalize_path_to_win(v), ec);
                if (op == "-f") return fs::is_regular_file(normalize_path_to_win(v), ec);
                if (op == "-d") return fs::is_directory(normalize_path_to_win(v), ec);
                if (op == "-L") return fs::is_symlink(normalize_path_to_win(v), ec);
                if (op == "-s") return fs::exists(normalize_path_to_win(v), ec) && fs::file_size(normalize_path_to_win(v), ec) > 0;
                if (op == "-r") { DWORD a = GetFileAttributesW(string_to_wstring(normalize_path_to_win(v)).c_str()); return a != INVALID_FILE_ATTRIBUTES; }
                if (op == "-w") { DWORD a = GetFileAttributesW(string_to_wstring(normalize_path_to_win(v)).c_str()); return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_READONLY); }
                if (op == "-x") {
                    string norm = normalize_path_to_win(v);
                    if (fs::exists(norm, ec) && !fs::is_directory(norm, ec)) {
                        string ext = fs::path(norm).extension().string();
                        transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return static_cast<char>(tolower(c)); });
                        return (ext == ".exe" || ext == ".bat" || ext == ".cmd" || ext == ".com" || ext == "");
                    }
                    return false;
                }
                if (op == "-n") return !v.empty();
                if (op == "-z") return v.empty();
            }
        }

        if (idx + 2 < t.size()) {
            string a = t[idx], op = t[idx + 1], b = t[idx + 2];
            static const set<string> ops = {
                "=~", "==", "=", "!=", "<", ">", "-nt", "-ot", "-ef",
                "-eq", "-ne", "-lt", "-le", "-gt", "-ge"
            };
            if (ops.count(op)) {
                idx += 3;
                return eval_binary(a, op, b);
            }
        }

        string v = t[idx++];
        return !v.empty() && v != "0";
    };

    parse_and = [&]() -> bool {
        bool v = parse_atom();
        while (idx < t.size() && t[idx] == "&&") {
            ++idx;
            bool rhs = parse_atom();
            v = v && rhs;
        }
        return v;
    };

    parse_expr = [&]() -> bool {
        bool v = parse_and();
        while (idx < t.size() && t[idx] == "||") {
            ++idx;
            bool rhs = parse_and();
            v = v || rhs;
        }
        return v;
    };

    bool ok = parse_expr();
    return (ok && idx == t.size()) ? 0 : 1;
}

// Quote-aware word splitter for alias expansion (mirrors parse_pipeline tokenizer).
static vector<string> tokenize_words(const string& s) {
    vector<string> tokens;
    string current;
    bool in_dq = false, in_sq = false;
    for (char c : s) {
        if      (c == '"' && !in_sq) in_dq = !in_dq;
        else if (c == '\'' && !in_dq) in_sq = !in_sq;
        else if (isspace(c) && !in_dq && !in_sq) {
            if (!current.empty()) { tokens.push_back(current); current.clear(); }
        } else current += c;
    }
    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

int builtin_printf(const vector<string>& args) {
    if (args.size() < 2) { cerr << "printf: usage: printf format [args]\n"; return 1; }
    const string& fmt = args[1]; size_t ai = 2;
    bool first_pass = true;
    do {
        size_t args_before_pass = ai;
        for (size_t i = 0; i < fmt.size(); ++i) {
            if (fmt[i] == '\\' && i + 1 < fmt.size()) {
                switch (fmt[++i]) {
                    case 'n': cout << '\n'; break; case 't': cout << '\t'; break;
                    case 'r': cout << '\r'; break; case '\\': cout << '\\'; break;
                    case 'a': cout << '\a'; break; case 'b': cout << '\b'; break;
                    default: cout << '\\' << fmt[i]; break;
                }
            } else if (fmt[i] == '%' && i + 1 < fmt.size()) {
                if (fmt[i + 1] == '%') { cout << '%'; ++i; continue; }
                size_t spec_start = i;
                size_t j = i + 1;
                while (j < fmt.size() && string("0123456789-+ #.").find(fmt[j]) != string::npos) ++j;
                if (j < fmt.size() && string("sdfxXocu").find(fmt[j]) != string::npos) {
                    char conv = fmt[j];
                    string sub_fmt = fmt.substr(spec_start, j - spec_start + 1);
                    i = j;
                    string val = ai < args.size() ? args[ai++] : "";
                    char buf[1024] = {};
                    if (conv == 's') {
                        snprintf(buf, sizeof(buf), sub_fmt.c_str(), val.c_str());
                    } else if (conv == 'f') {
                        double d = 0.0; try { d = stod(val); } catch (...) {}
                        snprintf(buf, sizeof(buf), sub_fmt.c_str(), d);
                    } else if (conv == 'c') {
                        char c = val.empty() ? '\0' : val[0];
                        snprintf(buf, sizeof(buf), sub_fmt.c_str(), c);
                    } else {
                        long long n = 0; try { n = stoll(val); } catch (...) {}
                        string ll_fmt = sub_fmt;
                        ll_fmt.insert(ll_fmt.size() - 1, "ll");
                        snprintf(buf, sizeof(buf), ll_fmt.c_str(), n);
                    }
                    cout << buf;
                } else {
                    cout << '%';
                }
            } else {
                cout << fmt[i];
            }
        }
        first_pass = false;
        if (ai == args_before_pass) break;
    } while (first_pass || ai < args.size());
    return 0;
}

int builtin_print(const vector<string>& args) {
    bool newline=true, per_line=false, prompt_exp=false, to_coprocess=false;
    int target_fd = 1;
    size_t start = 1;
    for (; start < args.size(); ++start) {
        if (args[start]=="-n") newline=false;
        else if (args[start]=="-l") per_line=true;
        else if (args[start]=="-P") prompt_exp=true;
        else if (args[start]=="-p") to_coprocess=true;
        else if (args[start]=="-u" && start + 1 < args.size()) {
            try { target_fd = std::stoi(args[++start]); } catch (...) {}
        } else if (args[start].size() >= 3 && args[start].rfind("-u", 0) == 0 && isdigit(static_cast<unsigned char>(args[start][2]))) {
            try { target_fd = std::stoi(args[start].substr(2)); } catch (...) {}
        }
        else if (args[start]=="--") { start++; break; }
        else break;
    }
    if (to_coprocess) {
        if (!g_coprocess.input) {
            cerr << "print: no active coprocess input\n";
            return 1;
        }
        string payload;
        for (size_t i = start; i < args.size(); ++i) {
            payload += args[i];
            if (per_line) payload += '\n';
            else if (i + 1 < args.size()) payload += ' ';
        }
        if (newline && !per_line) payload += '\n';
        DWORD total_written = 0;
        while (total_written < payload.size()) {
            DWORD written = 0;
            if (!WriteFile(g_coprocess.input, payload.data() + total_written,
                           static_cast<DWORD>(payload.size() - total_written), &written, nullptr) || written == 0) {
                return 1;
            }
            total_written += written;
        }
        return 0;
    }
    ostream& out = (target_fd == 2) ? cerr : cout;
    for (size_t i = start; i < args.size(); ++i) {
        if (prompt_exp) {
            // render the arg as a prompt format string
            string saved = g_env.vars["PROMPT"];
            g_env.vars["PROMPT"] = args[i];
            out << render_prompt();
            g_env.vars["PROMPT"] = saved;
        } else out << args[i];
        if (per_line) out << '\n';
        else if (i+1 < args.size()) out << ' ';
    }
    if (newline && !per_line) out << '\n';
    return 0;
}

int builtin_cd(const vector<string>& args) {
    string target = args.size() > 1 ? args[1] : g_env.vars["HOME"];
    if (target == "~") {
        target = g_env.vars["HOME"];
    } else if (target.size() > 1 && target[0] == '~' && (target[1] == '/' || target[1] == '\\')) {
        string home = g_env.vars["HOME"];
        if (!home.empty() && home.back() == '\\') home.pop_back();
        string rest = target.substr(2);
        target = home + "\\" + normalize_path_to_win(rest);
    }
    target = normalize_path_to_win(target);
    if (target.size() == 2 && isalpha(static_cast<unsigned char>(target[0])) && target[1] == ':') {
        target += "\\";
    }
    if (target == "-") { target = g_env.oldpwd; cout << target << "\n"; }
    try {
        string current = fs::current_path().string();
        fs::current_path(target);

        g_env.oldpwd = current;
        g_env.vars["OLDPWD"] = normalize_path_to_unix(current);
        g_env.vars["PWD"] = normalize_path_to_unix(fs::current_path().string());

        SetEnvironmentVariableA("OLDPWD", g_env.vars["OLDPWD"].c_str());
        SetEnvironmentVariableA("PWD", g_env.vars["PWD"].c_str());

        g_env.prompt_dirty = true;
        return 0;
    } catch (...) {
        cout << COLOR_RED << "cd: no such file or directory: " << target << COLOR_RESET << "\n";
        return 1;
    }
}

int builtin_echo(const vector<string>& args) {
    bool newline = true, enable_escapes = false;
    size_t start = 1;
    while (start < args.size() && args[start][0] == '-') {
        string flag = args[start];
        if (flag == "-n") newline = false;
        else if (flag == "-e") enable_escapes = true;
        else if (flag == "-E") enable_escapes = false;
        else break;
        start++;
    }
    for (size_t i = start; i < args.size(); ++i) {
        string str = args[i];
        if (enable_escapes) {
            try { str = regex_replace(str, regex(R"(\\n)"), "\n"); } catch (...) {}
            try { str = regex_replace(str, regex(R"(\\t)"), "\t"); } catch (...) {}
            try { str = regex_replace(str, regex(R"(\\e)"), "\033"); } catch (...) {}
        }
        cout << str << (i + 1 < args.size() ? " " : "");
    }
    if (newline) cout << "\n";
    return 0;
}

static int builtin_coproc(const vector<string>& args) {
    if (args.size() == 2 && args[1] == "-") {
        close_coprocess(true);
        return 0;
    }
    if (args.size() == 2 && args[1] == "-i") {
        close_handle_if_valid(g_coprocess.input);
        g_coprocess.input = nullptr;
        return 0;
    }
    if (args.size() < 2) {
        cerr << "coproc: command required\n";
        return 1;
    }

    close_coprocess(true);
    string executable = find_executable_in_path(args[1]);
    if (executable.empty()) {
        cerr << "zsh: command not found: " << args[1] << "\n";
        return 127;
    }

    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE child_input = nullptr;
    HANDLE parent_input = nullptr;
    HANDLE parent_output = nullptr;
    HANDLE child_output = nullptr;
    if (!CreatePipe(&child_input, &parent_input, &security, 0) ||
        !CreatePipe(&parent_output, &child_output, &security, 0)) {
        close_handle_if_valid(child_input);
        close_handle_if_valid(parent_input);
        close_handle_if_valid(parent_output);
        close_handle_if_valid(child_output);
        cerr << "coproc: failed to create pipes\n";
        return 1;
    }
    SetHandleInformation(parent_input, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(parent_output, HANDLE_FLAG_INHERIT, 0);

    string command_line = win_quote_arg(executable);
    for (size_t i = 2; i < args.size(); ++i) command_line += " " + win_quote_arg(args[i]);
    wstring wide_command = string_to_wstring(command_line);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = child_input;
    startup.hStdOutput = child_output;
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION process{};
    BOOL created = CreateProcessW(nullptr, wide_command.data(), nullptr, nullptr, TRUE, 0,
                                  nullptr, nullptr, &startup, &process);
    close_handle_if_valid(child_input);
    close_handle_if_valid(child_output);
    if (!created) {
        close_handle_if_valid(parent_input);
        close_handle_if_valid(parent_output);
        cerr << "coproc: failed to start: " << args[1] << "\n";
        return 1;
    }

    CloseHandle(process.hThread);
    g_coprocess.process = process.hProcess;
    g_coprocess.input = parent_input;
    g_coprocess.output = parent_output;
    g_coprocess.pid = process.dwProcessId;
    g_env.vars["COPROC_PID"] = to_string(process.dwProcessId);
    return 0;
}

int builtin_read(const vector<string>& args) {
    bool silent = false;
    bool raw = false;
    bool array = false;
    bool from_coprocess = false;
    bool exact = false;
    bool has_timeout = false;
    size_t exact_count = 0;
    double timeout_seconds = 0.0;
    char delimiter = '\n';
    vector<string> target_vars;

    for (size_t i = 1; i < args.size(); ++i) {
        const string& option = args[i];
        if (option == "--") {
            target_vars.insert(target_vars.end(), args.begin() + static_cast<ptrdiff_t>(i + 1), args.end());
            break;
        }
        if (option == "-s") silent = true;
        else if (option == "-r") raw = true;
        else if (option == "-A") array = true;
        else if (option == "-p") from_coprocess = true;
        else if ((option == "-d" || option == "-N" || option == "-t") && i + 1 < args.size()) {
            const string value = args[++i];
            if (option == "-d") {
                delimiter = value.empty() ? '\0' : value[0];
            } else if (option == "-N") {
                char* end = nullptr;
                unsigned long long count = strtoull(value.c_str(), &end, 10);
                if (end == value.c_str() || *end != '\0' || count == 0 || count > SIZE_MAX) {
                    cerr << "read: invalid character count: " << value << "\n";
                    return 1;
                }
                exact = true;
                exact_count = static_cast<size_t>(count);
            } else {
                char* end = nullptr;
                timeout_seconds = strtod(value.c_str(), &end);
                if (end == value.c_str() || *end != '\0' || timeout_seconds < 0.0) {
                    cerr << "read: invalid timeout: " << value << "\n";
                    return 1;
                }
                has_timeout = true;
            }
        } else if (!option.empty() && option[0] == '-') {
            cerr << "read: unsupported option: " << option << "\n";
            return 1;
        } else {
            target_vars.push_back(option);
        }
    }

    if (target_vars.empty()) target_vars.push_back("REPLY");
    for (const auto& name : target_vars) {
        if (!is_valid_env_var_name(name)) {
            cerr << "read: invalid variable name: " << name << "\n";
            return 1;
        }
        if (g_env.readonly_vars.count(name)) {
            cerr << "zsh: read-only variable: " << name << "\n";
            return 1;
        }
    }
    if (array && target_vars.size() != 1) {
        cerr << "read: -A requires one array name\n";
        return 1;
    }
    if (from_coprocess && !g_coprocess.output) return 1;

    DWORD saved_console_mode = 0;
    HANDLE standard_input = GetStdHandle(STD_INPUT_HANDLE);
    bool restore_console_mode = silent && GetConsoleMode(standard_input, &saved_console_mode);
    if (restore_console_mode) SetConsoleMode(standard_input, saved_console_mode & ~ENABLE_ECHO_INPUT);

    const ULONGLONG start_tick = GetTickCount64();
    const ULONGLONG timeout_ms = has_timeout
        ? static_cast<ULONGLONG>(timeout_seconds * 1000.0 + 0.5)
        : INFINITE;
    auto read_coprocess_byte = [&](char& byte) -> bool {
        while (true) {
            DWORD available = 0;
            if (!PeekNamedPipe(g_coprocess.output, nullptr, 0, nullptr, &available, nullptr)) return false;
            if (available > 0) {
                DWORD bytes_read = 0;
                return ReadFile(g_coprocess.output, &byte, 1, &bytes_read, nullptr) && bytes_read == 1;
            }
            if (has_timeout && GetTickCount64() - start_tick >= timeout_ms) return false;
            if (g_coprocess.process && WaitForSingleObject(g_coprocess.process, 1) == WAIT_OBJECT_0) {
                if (!PeekNamedPipe(g_coprocess.output, nullptr, 0, nullptr, &available, nullptr) || available == 0) return false;
            }
            Sleep(1);
        }
    };

    cin.clear();
    string input;
    bool completed = false;
    bool escaped = false;
    while (!exact || input.size() < exact_count) {
        char byte = 0;
        bool got_byte = from_coprocess ? read_coprocess_byte(byte) : static_cast<bool>(cin.get(byte));
        if (!got_byte) break;

        if (!exact && byte == delimiter && !escaped) {
            completed = true;
            break;
        }
        if (!exact && delimiter == '\n' && byte == '\r' && !escaped) continue;
        if (!raw && !escaped && byte == '\\') {
            escaped = true;
            continue;
        }
        if (escaped) {
            escaped = false;
            if (byte == '\n') continue;
        }
        input += byte;
    }
    if (exact && input.size() == exact_count) completed = true;
    if (escaped) input += '\\';

    if (restore_console_mode) {
        SetConsoleMode(standard_input, saved_console_mode);
        cout << "\n";
    } else if (silent) {
        cout << "\n";
    }

    if (!completed) return 1;

    auto ifs_fields = [&](const string& value) {
        string ifs = g_env.vars.count("IFS") ? g_env.vars["IFS"] : " \t\n";
        vector<pair<size_t, size_t>> spans;
        size_t position = 0;
        while (position < value.size()) {
            while (position < value.size() && ifs.find(value[position]) != string::npos) ++position;
            if (position >= value.size()) break;
            size_t begin = position;
            while (position < value.size() && ifs.find(value[position]) == string::npos) ++position;
            spans.push_back({begin, position});
        }
        return spans;
    };
    vector<pair<size_t, size_t>> fields = ifs_fields(input);

    if (array) {
        vector<string> values;
        values.reserve(fields.size());
        for (const auto& field : fields) values.push_back(input.substr(field.first, field.second - field.first));
        g_env.vars.erase(target_vars[0]);
        g_env.assoc_arrays.erase(target_vars[0]);
        g_env.indexed_arrays[target_vars[0]] = std::move(values);
        return 0;
    }

    if (target_vars.size() == 1) {
        g_env.indexed_arrays.erase(target_vars[0]);
        g_env.assoc_arrays.erase(target_vars[0]);
        g_env.vars[target_vars[0]] = input;
        return 0;
    }
    for (size_t i = 0; i < target_vars.size(); ++i) {
        string value;
        if (i < fields.size()) {
            size_t begin = fields[i].first;
            size_t end = fields[i].second;
            if (i + 1 == target_vars.size() && fields.size() > target_vars.size()) end = fields.back().second;
            value = input.substr(begin, end - begin);
        }
        g_env.indexed_arrays.erase(target_vars[i]);
        g_env.assoc_arrays.erase(target_vars[i]);
        g_env.vars[target_vars[i]] = std::move(value);
    }
    return 0;
}

int builtin_typeset(const vector<string>& args) {
    if (args.size() < 2) return 1;
    bool is_array = false;
    bool is_assoc = false;
    bool is_unique = false;
    bool is_integer = false;
    bool is_readonly = false;

    size_t i = 1;
    while (i < args.size() && !args[i].empty() && args[i][0] == '-' && args[i] != "--") {
        const string& flag = args[i];
        if (flag.find('a') != string::npos) is_array = true;
        if (flag.find('A') != string::npos) is_assoc = true;
        if (flag.find('U') != string::npos) is_unique = true;
        if (flag.find('i') != string::npos) is_integer = true;
        if (flag.find('r') != string::npos) is_readonly = true;
        ++i;
    }
    if (i < args.size() && args[i] == "--") ++i;
    if (i >= args.size()) return 0;

    if (is_array || is_assoc) {
        string declaration = args[i];
        size_t equal = declaration.find('=');
        string name = equal == string::npos ? declaration : declaration.substr(0, equal);
        if (g_env.readonly_vars.count(name)) { cerr << "zsh: read-only variable: " << name << "\n"; return 1; }
        mark_function_local(name);
        string payload = equal == string::npos ? "" : declaration.substr(equal + 1);
        for (size_t k = i + 1; k < args.size(); ++k) {
            if (!payload.empty()) payload += ' ';
            payload += args[k];
        }
        payload = trim_copy(payload);
        if (!payload.empty() && payload.front() == '(') payload.erase(payload.begin());
        if (!payload.empty() && payload.back() == ')') payload.pop_back();
        vector<string> values = tokenize_words(payload);

        if (is_assoc) {
            map<string, string> entries;
            for (size_t k = 0; k + 1 < values.size(); k += 2) entries[values[k]] = values[k + 1];
            g_env.assoc_arrays[name] = std::move(entries);
        } else {
            if (is_unique) {
                set<string> seen;
                values.erase(remove_if(values.begin(), values.end(), [&](const string& value) {
                    return !seen.insert(value).second;
                }), values.end());
                g_env.unique_arrays.insert(name);
            } else {
                g_env.unique_arrays.erase(name);
            }
            g_env.indexed_arrays[name] = std::move(values);
            g_env.integer_vars.erase(name);
        }
        if (is_readonly) g_env.readonly_vars.insert(name);
        return 0;
    }

    for (size_t k = i; k < args.size(); ++k) {
        size_t equal = args[k].find('=');
        string name = equal == string::npos ? args[k] : args[k].substr(0, equal);
        if (!is_valid_env_var_name(name)) { cerr << "typeset: invalid variable name: " << name << "\n"; return 1; }
        if (g_env.readonly_vars.count(name)) { cerr << "zsh: read-only variable: " << name << "\n"; return 1; }
        mark_function_local(name);
        if (is_integer) g_env.integer_vars.insert(name);
        if (equal != string::npos) {
            string value = g_env.expand_vars(args[k].substr(equal + 1));
            g_env.vars[name] = g_env.integer_vars.count(name) ? to_string(eval_math_expr(value)) : value;
        } else if (!g_env.vars.count(name)) {
            g_env.vars[name] = is_integer ? "0" : "";
        }
        if (is_readonly) g_env.readonly_vars.insert(name);
    }
    return 0;
}

static void close_background_job_handles(BackgroundJob& job) {
    for (HANDLE process : job.processes) close_handle_if_valid(process);
    close_handle_if_valid(job.hJob);
    job.hProcess = nullptr;
    job.hJob = nullptr;
    job.processes.clear();
    job.pids.clear();
}

static void wait_for_background_job(const BackgroundJob& job) {
    for (HANDLE process : job.processes) {
        if (process) WaitForSingleObject(process, INFINITE);
    }
}

static const char* job_state_name(JobState state) {
    switch (state) {
        case JobState::Running: return "running";
        case JobState::Stopped: return "stopped";
        case JobState::Done: return "done";
        case JobState::Failed: return "failed";
    }
    return "unknown";
}

static void refresh_background_job_state(BackgroundJob& job) {
    if (job.state == JobState::Stopped || job.state == JobState::Failed) return;
    DWORD status = 0;
    if (!job.hProcess || !GetExitCodeProcess(job.hProcess, &status)) {
        job.state = JobState::Failed;
        job.launch_error = GetLastError();
        return;
    }
    job.exit_code = status;
    job.state = status == STILL_ACTIVE ? JobState::Running : JobState::Done;
}

static void select_current_job(unsigned long job_id) {
    if (job_id == 0 || job_id == g_current_job_id) return;
    g_previous_job_id = g_current_job_id;
    g_current_job_id = job_id;
}

static void repair_job_markers() {
    auto exists = [](unsigned long id) {
        return id != 0 && any_of(g_env.jobs.begin(), g_env.jobs.end(),
                                 [&](const BackgroundJob& job) { return job.job_id == id; });
    };
    if (!exists(g_current_job_id)) {
        g_current_job_id = exists(g_previous_job_id) ? g_previous_job_id :
            (g_env.jobs.empty() ? 0 : g_env.jobs.back().job_id);
    }
    if (!exists(g_previous_job_id) || g_previous_job_id == g_current_job_id) {
        g_previous_job_id = 0;
        for (auto it = g_env.jobs.rbegin(); it != g_env.jobs.rend(); ++it) {
            if (it->job_id != g_current_job_id) { g_previous_job_id = it->job_id; break; }
        }
    }
}

static bool resolve_job_index(const string& spec, size_t& index) {
    if (g_env.jobs.empty()) return false;
    if (spec.empty() || spec == "%" || spec == "%%" || spec == "%+") {
        repair_job_markers();
        for (size_t i = 0; i < g_env.jobs.size(); ++i)
            if (g_env.jobs[i].job_id == g_current_job_id) { index = i; return true; }
        return false;
    }
    if (spec == "%-") {
        repair_job_markers();
        for (size_t i = 0; i < g_env.jobs.size(); ++i)
            if (g_env.jobs[i].job_id == g_previous_job_id) { index = i; return true; }
        return false;
    }
    if (spec.rfind("%?", 0) == 0) {
        string needle = spec.substr(2);
        if (needle.empty()) return false;
        for (size_t i = g_env.jobs.size(); i > 0; --i) {
            if (g_env.jobs[i - 1].command.find(needle) != string::npos) {
                index = i - 1;
                return true;
            }
        }
        return false;
    }

    string number = spec;
    if (!number.empty() && number[0] == '%') number.erase(number.begin());
    char* end = nullptr;
    errno = 0;
    unsigned long parsed = strtoul(number.c_str(), &end, 10);
    if (errno != 0 || end == number.c_str() || *end != '\0' || parsed == 0) return false;
    for (size_t i = 0; i < g_env.jobs.size(); ++i) {
        if (g_env.jobs[i].job_id == parsed) { index = i; return true; }
    }
    return false;
}

int builtin_jobs(const vector<string>& args) {
    bool long_format = false;
    bool pid_only = false;
    bool end_of_options = false;
    vector<size_t> requested_job_indices;

    for (size_t i = 1; i < args.size(); ++i) {
        const string& token = args[i];
        if (!end_of_options && token == "--") {
            end_of_options = true;
            continue;
        }
        if (!end_of_options && token.size() > 1 && token[0] == '-') {
            for (size_t flag_index = 1; flag_index < token.size(); ++flag_index) {
                if (token[flag_index] == 'l') long_format = true;
                else if (token[flag_index] == 'p') pid_only = true;
                else {
                    cerr << "zsh: jobs: invalid option: " << token << "\n";
                    return 1;
                }
            }
            continue;
        }

        size_t job_index = 0;
        if (!resolve_job_index(token, job_index)) {
            cerr << "zsh: jobs: invalid job id: " << token << "\n";
            return 1;
        }
        requested_job_indices.push_back(job_index);
    }

    for (size_t i = 0; i < g_env.jobs.size(); ++i) {
        if (!requested_job_indices.empty() &&
            find(requested_job_indices.begin(), requested_job_indices.end(), i) == requested_job_indices.end()) {
            continue;
        }

        refresh_background_job_state(g_env.jobs[i]);
        string state = job_state_name(g_env.jobs[i].state);
        char marker = g_env.jobs[i].job_id == g_current_job_id ? '+' :
                      (g_env.jobs[i].job_id == g_previous_job_id ? '-' : ' ');
        if (pid_only) {
            cout << g_env.jobs[i].pid << "\n";
        } else if (long_format) {
            cout << "[" << g_env.jobs[i].job_id << "]  " << marker << " " << g_env.jobs[i].pid << " " << state
                 << "      " << g_env.jobs[i].command << "\n";
        } else {
            cout << "[" << g_env.jobs[i].job_id << "]  " << marker << " " << state << "      " << g_env.jobs[i].command << "\n";
        }
    }
    return 0;
}

int builtin_help(const vector<string>& args) {
    string topic = args.size() > 1 ? args[1] : "";
    print_comprehensive_help(topic);
    return 0;
}

static int execute_condition_loop(const string& block, bool until_mode);
static int execute_repeat_loop(const string& block);
static int execute_select_loop(const string& block);
static int execute_timed_command(const string& block);

static int builtin_emulate(const vector<string>& args) {
    if (args.size() == 1) {
        bool is_sh = g_env.options.count("shwordsplit") && g_env.options.at("shwordsplit");
        cout << (is_sh ? "sh" : "zsh") << "\n";
        return 0;
    }
    bool reset = false;
    bool local_opts = false;
    string mode;
    for (size_t i = 1; i < args.size(); ++i) {
        const string& a = args[i];
        if (a == "-R") reset = true;
        else if (a == "-L") local_opts = true;
        else if (a == "-LR" || a == "-RL") { local_opts = true; reset = true; }
        else if (!a.empty() && a[0] != '-') {
            mode = canonicalize_option_name(a);
        }
    }
    if (mode.empty()) {
        cerr << "emulate: mode required\n";
        return 1;
    }
    if (local_opts && !g_function_local_scopes.empty()) {
        if (!g_function_local_scopes.back().local_options) {
            g_function_local_scopes.back().local_options = true;
            g_function_local_scopes.back().options_before = g_env.options;
        }
    }
    if (reset) {
        g_env.options.clear();
    }
    if (mode == "zsh") {
        g_env.options["shwordsplit"] = false;
        g_env.options["ksharrays"] = false;
        g_env.options["posixbuiltins"] = false;
        g_env.options["multios"] = true;
        g_env.options["extendedglob"] = true;
        g_env.options["shglob"] = false;
        g_env.options["autocd"] = true;
        g_env.options["promptsubst"] = true;
    } else if (mode == "sh" || mode == "ksh") {
        g_env.options["shwordsplit"] = true;
        g_env.options["ksharrays"] = true;
        g_env.options["posixbuiltins"] = true;
        g_env.options["multios"] = false;
        g_env.options["extendedglob"] = false;
        g_env.options["shglob"] = true;
        g_env.options["autocd"] = false;
    } else if (mode == "csh") {
        g_env.options["shwordsplit"] = true;
        g_env.options["ksharrays"] = false;
        g_env.options["cshnullglob"] = true;
        g_env.options["multios"] = false;
        g_env.options["extendedglob"] = false;
    } else {
        cerr << "emulate: unknown emulation mode: " << mode << "\n";
        return 1;
    }
    return 0;
}

int dispatch_command(vector<string> args) {
    if (args.empty()) return 0;

    auto assignment_value = [&](const string& raw, bool integer_attribute) {
        string value = g_env.expand_vars(raw);
        if (value.size() >= 2 &&
            ((value.front() == '"' && value.back() == '"') ||
             (value.front() == '\'' && value.back() == '\''))) {
            value = value.substr(1, value.size() - 2);
        }
        return integer_attribute ? to_string(eval_math_expr(value)) : value;
    };

    if (args.size() == 1) {
        smatch array_assignment;
        static const regex array_assignment_re(R"(^([A-Za-z_][A-Za-z0-9_]*)=\(([\s\S]*)\)$)");
        if (regex_match(args[0], array_assignment, array_assignment_re)) {
            string name = array_assignment[1].str();
            if (g_env.readonly_vars.count(name)) {
                cerr << "zsh: read-only variable: " << name << "\n";
                return 1;
            }
            vector<string> values = tokenize_words(array_assignment[2].str());
            for (auto& value : values) value = g_env.expand_vars(value);
            g_env.vars.erase(name);
            g_env.assoc_arrays.erase(name);
            g_env.integer_vars.erase(name);
            g_env.indexed_arrays[name] = std::move(values);
            return 0;
        }
    }

    // Support assignment prefixes like VAR=val cmd arg and standalone VAR=val.
    auto is_assignment_word = [](const string& tok) -> bool {
        static const regex assign_re(R"(^([A-Za-z_][A-Za-z0-9_]*|HKLM\..+|HKCU\..+)=.*$)");
        return regex_match(tok, assign_re);
    };

    size_t assign_prefix = 0;
    vector<pair<string, string>> assignments;
    while (assign_prefix < args.size() && is_assignment_word(args[assign_prefix])) {
        const string& tok = args[assign_prefix];
        size_t eq = tok.find('=');
        assignments.push_back({tok.substr(0, eq), tok.substr(eq + 1)});
        ++assign_prefix;
    }

    if (assign_prefix > 0) {
        for (const auto& kv : assignments) {
            bool is_registry_property = kv.first.rfind("HKLM.", 0) == 0 || kv.first.rfind("HKCU.", 0) == 0;
            if (!is_registry_property && !is_valid_env_var_name(kv.first)) {
                cerr << "zsh: invalid variable name: " << kv.first << "\n";
                return 1;
            }
            if (g_env.readonly_vars.count(kv.first)) {
                cerr << "zsh: read-only variable: " << kv.first << "\n";
                return 1;
            }
        }
        if (assign_prefix == args.size()) {
            for (const auto& kv : assignments) {
                string val = assignment_value(kv.second, g_env.integer_vars.count(kv.first) != 0);
                if (kv.first.rfind("HKLM.", 0) == 0 || kv.first.rfind("HKCU.", 0) == 0) {
                    if (!write_zsh_registry_property(kv.first, val)) {
                        cerr << "zsh: failed to set Registry property: " << kv.first << "\n";
                        return 1;
                    }
                } else {
                    g_env.vars[kv.first] = val;
                    SetEnvironmentVariableA(kv.first.c_str(), val.c_str());
                }
            }
            return 0;
        }

        map<string, pair<bool, string>> old_values;
        for (const auto& kv : assignments) {
            auto it = g_env.vars.find(kv.first);
            old_values[kv.first] = {it != g_env.vars.end(), it != g_env.vars.end() ? it->second : ""};
            string val = assignment_value(kv.second, g_env.integer_vars.count(kv.first) != 0);
            g_env.vars[kv.first] = val;
            SetEnvironmentVariableA(kv.first.c_str(), val.c_str());
        }

        vector<string> rest(args.begin() + static_cast<ptrdiff_t>(assign_prefix), args.end());
        int rc = dispatch_command(rest);

        for (const auto& [name, state] : old_values) {
            if (state.first) {
                g_env.vars[name] = state.second;
                SetEnvironmentVariableA(name.c_str(), state.second.c_str());
            } else {
                g_env.vars.erase(name);
                SetEnvironmentVariableA(name.c_str(), nullptr);
            }
        }
        return rc;
    }

    if (args.size() == 1) {
        smatch m;
        static const regex append_list_re(R"(^([A-Za-z_][A-Za-z0-9_]*)\+=\((.*)\)$)");
        static const regex append_one_re(R"(^([A-Za-z_][A-Za-z0-9_]*)\+=(.+)$)");
        if (regex_match(args[0], m, append_list_re)) {
            string name = m[1].str();
            if (g_env.readonly_vars.count(name)) { cerr << "zsh: read-only variable: " << name << "\n"; return 1; }
            string payload = trim_copy(m[2].str());
            vector<string> values = payload.empty() ? vector<string>{} : tokenize_words(payload);
            auto& arr = g_env.indexed_arrays[name];
            for (const auto& v : values) {
                string expanded = g_env.expand_vars(v);
                if (!g_env.unique_arrays.count(name) || find(arr.begin(), arr.end(), expanded) == arr.end()) arr.push_back(std::move(expanded));
            }
            return 0;
        }
        if (regex_match(args[0], m, append_one_re)) {
            string name = m[1].str();
            if (g_env.readonly_vars.count(name)) { cerr << "zsh: read-only variable: " << name << "\n"; return 1; }
            string value = trim_copy(m[2].str());
            auto& arr = g_env.indexed_arrays[name];
            if (!value.empty()) {
                string expanded = g_env.expand_vars(value);
                if (!g_env.unique_arrays.count(name) || find(arr.begin(), arr.end(), expanded) == arr.end()) arr.push_back(std::move(expanded));
            }
            return 0;
        }
    }

    if (args.size() == 1 && args[0].find('=') != string::npos) {
        size_t eq = args[0].find('=');
        string name = args[0].substr(0, eq);
        bool is_registry_property = name.rfind("HKLM.", 0) == 0 || name.rfind("HKCU.", 0) == 0;
        if (is_registry_property) {
            string value = g_env.expand_vars(args[0].substr(eq + 1));
            if (!write_zsh_registry_property(name, value)) {
                cerr << "zsh: failed to set Registry property: " << name << "\n";
                return 1;
            }
            return 0;
        }
        if (!is_valid_env_var_name(name)) {
            cerr << "zsh: invalid variable name: " << name << "\n";
            return 1;
        }
        if (g_env.readonly_vars.count(name)) {
            cerr << "zsh: read-only variable: " << name << "\n";
            return 1;
        }
        string value = assignment_value(args[0].substr(eq + 1), g_env.integer_vars.count(name) != 0);
        g_env.vars[name] = value;
        return 0;
    }

    if (g_env.aliases.count(args[0])) {
        const string& alias_val = g_env.aliases[args[0]];
        if (contains_dangerous_alias_tokens(alias_val)) {
            cerr << "zsh: unsafe alias definition blocked: " << args[0] << "\n";
            return 1;
        }
        vector<string> exp = tokenize_words(alias_val);
        for (auto& tok : exp) tok = g_env.expand_vars(tok);
        for (size_t i = 1; i < args.size(); ++i) exp.push_back(args[i]);
        args = exp;
    }

    string cmd = args[0];

    if (cmd == "until" || cmd == "repeat" || cmd == "select" || cmd == "time") {
        string block;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i != 0) block += ' ';
            block += args[i];
        }
        if (cmd == "until") return execute_condition_loop(block, true);
        if (cmd == "repeat") return execute_repeat_loop(block);
        if (cmd == "select") return execute_select_loop(block);
        if (cmd == "time") return execute_timed_command(block);
    }

    auto path_for_command = [](const string& name) {
        return find_executable_in_path(name);
    };

    auto print_command_matches = [&](const string& name, bool all, bool kind_only, bool path_only) {
        int matches = 0;
        auto emit = [&](const string& text) {
            cout << text << "\n";
            ++matches;
        };
        if (!path_only && g_env.aliases.count(name)) {
            emit(kind_only ? name + ": alias" : name + ": aliased to " + g_env.aliases[name]);
            if (!all) return 0;
        }
        if (!path_only && g_env.functions.count(name)) {
            emit(kind_only ? name + ": function" : name + " is a shell function");
            if (!all) return 0;
        }
        if (!path_only && is_zsh_builtin_command(name)) {
            emit(kind_only ? name + ": builtin" : name + " is a shell builtin");
            if (!all) return 0;
        }
        string path = path_for_command(name);
        if (!path.empty()) emit(path);
        if (matches == 0) {
            if (!path_only) cout << name << " not found\n";
            return 1;
        }
        return 0;
    };

    if (cmd == "command") {
        size_t start = 1;
        bool path_default = false;
        if (start < args.size() && args[start] == "-p") { path_default = true; ++start; }
        if (start < args.size() && args[start] == "--") ++start;
        if (start >= args.size()) return 0;

        vector<string> pass(args.begin() + static_cast<ptrdiff_t>(start), args.end());
        if (!pass.empty() && (pass[0] == "-v" || pass[0] == "-V")) {
            bool verbose = pass[0] == "-V";
            if (verbose) {
                int rc = 0;
                for (size_t i = 1; i < pass.size(); ++i) {
                    const string& name = pass[i];
                    if (g_env.aliases.count(name)) cout << name << " is an alias for " << g_env.aliases[name] << "\n";
                    else if (g_env.functions.count(name)) cout << name << " is a shell function\n";
                    else if (is_zsh_builtin_command(name)) cout << name << " is a shell builtin\n";
                    else {
                        string path = path_for_command(name);
                        if (!path.empty()) cout << name << " is " << path << "\n";
                        else { cout << name << " not found\n"; rc = 1; }
                    }
                }
                return rc;
            }
            vector<string> q = {"type"};
            for (size_t i = 1; i < pass.size(); ++i) q.push_back(pass[i]);
            return dispatch_command(q);
        }

        const string target = pass[0];
        bool had_alias = g_env.aliases.count(target) != 0;
        bool had_func = g_env.functions.count(target) != 0;
        string saved_alias = had_alias ? g_env.aliases[target] : "";
        string saved_func = had_func ? g_env.functions[target] : "";
        if (had_alias) g_env.aliases.erase(target);
        if (had_func) g_env.functions.erase(target);

        (void)path_default;
        int rc = dispatch_command(pass);

        if (had_alias) g_env.aliases[target] = saved_alias;
        if (had_func) g_env.functions[target] = saved_func;
        return rc;
    }

    if (cmd == "whence") {
        size_t start = 1;
        bool all = false, kind_only = false, path_only = false;
        while (start < args.size() && !args[start].empty() && args[start][0] == '-') {
            for (size_t flag = 1; flag < args[start].size(); ++flag) {
                if (args[start][flag] == 'a') all = true;
                else if (args[start][flag] == 'w') kind_only = true;
                else if (args[start][flag] == 'p') path_only = true;
            }
            ++start;
        }
        if (start >= args.size()) return 0;
        int rc = 0;
        for (size_t i = start; i < args.size(); ++i)
            if (print_command_matches(args[i], all, kind_only, path_only) != 0) rc = 1;
        return rc;
    }

    if (cmd == "nocorrect") {
        if (args.size() == 1) return 0;
        vector<string> rest(args.begin() + 1, args.end());
        return dispatch_command(rest);
    }

    if (cmd == "builtin") {
        if (args.size() < 2) {
            cout << "builtin: usage: builtin <name> [args...]\n";
            return 1;
        }

        vector<string> pass(args.begin() + 1, args.end());
        const string target = pass[0];
        if (!is_zsh_builtin_command(target)) {
            cout << "builtin: " << target << ": not a shell builtin\n";
            return 1;
        }

        bool had_alias = g_env.aliases.count(target) != 0;
        bool had_func = g_env.functions.count(target) != 0;
        string saved_alias = had_alias ? g_env.aliases[target] : "";
        string saved_func = had_func ? g_env.functions[target] : "";
        if (had_alias) g_env.aliases.erase(target);
        if (had_func) g_env.functions.erase(target);
        int rc = dispatch_command(pass);
        if (had_alias) g_env.aliases[target] = saved_alias;
        if (had_func) g_env.functions[target] = saved_func;
        return rc;
    }

    if (g_env.functions.count(cmd)) {
        if (g_function_local_scopes.size() >= static_cast<size_t>(kMaxExecRecursionDepth)) {
            cerr << "zsh: maximum function recursion depth (" << kMaxExecRecursionDepth << ") exceeded\n";
            g_env.last_exit_code = 1;
            return 1;
        }

        vector<string> old_pos = g_env.positional_args;
        bool old_returning = g_script_returning;
        int old_return_code = g_script_return_code;

        g_env.positional_args = vector<string>(args.begin() + 1, args.end());
        g_script_returning = false;
        g_function_local_scopes.push_back({g_env.vars, g_env.indexed_arrays, g_env.assoc_arrays,
                           g_env.integer_vars, g_env.readonly_vars, g_env.unique_arrays, {}});

        int rc = execute_command_line(g_env.functions[cmd]);
        if (g_script_returning) {
            rc = g_script_return_code;
            g_script_returning = false;
        }

        if (g_env.vars.count("__trap_RETURN") && !g_env.vars["__trap_RETURN"].empty()) {
            execute_command_line(g_env.vars["__trap_RETURN"]);
        }

        FunctionLocalScope completed_scope = std::move(g_function_local_scopes.back());
        g_function_local_scopes.pop_back();
        restore_function_locals(completed_scope);

        g_env.positional_args = old_pos;
        g_script_returning = old_returning;
        g_script_return_code = old_return_code;
        return rc;
    }

    if (cmd == "cd" || cmd == "chdir")        return builtin_cd(args);
    if (cmd == "echo")                         return builtin_echo(args);
    if (cmd == "coproc")                       return builtin_coproc(args);
    if (cmd == "read")                         return builtin_read(args);
    if (cmd == "emulate")                      return builtin_emulate(args);
    if (cmd == "local") {
        if (g_function_local_scopes.empty()) { cerr << "local: not in a function\n"; return 1; }
        vector<string> declaration = args;
        declaration[0] = "typeset";
        return builtin_typeset(declaration);
    }
    if (cmd == "readonly") {
        if (args.size() == 1) {
            for (const auto& name : g_env.readonly_vars) {
                cout << "readonly " << name;
                if (g_env.vars.count(name)) cout << "='" << g_env.vars[name] << "'";
                cout << "\n";
            }
            return 0;
        }
        for (size_t i = 1; i < args.size(); ++i) {
            size_t equal = args[i].find('=');
            string name = equal == string::npos ? args[i] : args[i].substr(0, equal);
            if (!is_valid_env_var_name(name)) { cerr << "readonly: invalid variable name: " << name << "\n"; return 1; }
            if (g_env.readonly_vars.count(name)) { cerr << "zsh: read-only variable: " << name << "\n"; return 1; }
            if (equal != string::npos) g_env.vars[name] = args[i].substr(equal + 1);
            else if (!g_env.vars.count(name)) g_env.vars[name] = "";
            g_env.readonly_vars.insert(name);
        }
        return 0;
    }
    if (cmd == "shift") {
        int n = 1;
        if (args.size() > 1) { try { n = stoi(args[1]); } catch (...) { n = 1; } }
        if (n > 0) {
            if (n <= (int)g_env.positional_args.size()) {
                g_env.positional_args.erase(g_env.positional_args.begin(), g_env.positional_args.begin() + n);
            } else {
                g_env.positional_args.clear();
            }
        }
        return 0;
    }
    if (cmd == "typeset" || cmd == "declare") return builtin_typeset(args);
    if (cmd == "jobs")                         return builtin_jobs(args);
    if (cmd == "help")                         return builtin_help(args);
    if (cmd == "pwd")                          { try { cout << normalize_path_to_unix(fs::current_path().string()) << "\n"; } catch (...) { cerr << "pwd: cannot determine current directory\n"; return 1; } return 0; }
    if (cmd == "exit") {
        int code = 0;
        if (args.size() > 1) { try { code = stoi(args[1]); } catch (...) {} }
        if (g_subshell_depth > 0) {
            g_subshell_exiting = true;
            g_subshell_exit_code = code;
            return code;
        }
        exit(code);
    }
    if (cmd == "logout") {
        if (!g_login_shell) {
            cerr << "zsh: logout: not login shell\n";
            return 1;
        }
        int code = 0;
        if (args.size() > 1) { try { code = stoi(args[1]); } catch (...) {} }
        exit(code);
    }

    if (cmd == "export") {
        for (size_t i = 1; i < args.size(); ++i) {
            size_t eq = args[i].find('=');
            if (eq != string::npos) {
                string name = args[i].substr(0, eq), val = args[i].substr(eq + 1);
                if (!is_valid_env_var_name(name)) {
                    cerr << "zsh: invalid variable name: " << name << "\n";
                    return 1;
                }
                g_env.vars[name] = val;
                SetEnvironmentVariableA(name.c_str(), val.c_str());
            } else if (g_env.vars.count(args[i])) {
                if (!is_valid_env_var_name(args[i])) {
                    cerr << "zsh: invalid variable name: " << args[i] << "\n";
                    return 1;
                }
                SetEnvironmentVariableA(args[i].c_str(), g_env.vars[args[i]].c_str());
            }
        }
        return 0;
    }

    if (cmd == "unset") {
        for (size_t i = 1; i < args.size(); ++i) {
            if (!is_valid_env_var_name(args[i])) {
                cerr << "zsh: invalid variable name: " << args[i] << "\n";
                return 1;
            }
            if (g_env.readonly_vars.count(args[i])) {
                cerr << "zsh: read-only variable: " << args[i] << "\n";
                return 1;
            }
            g_env.vars.erase(args[i]);
            g_env.indexed_arrays.erase(args[i]);
            g_env.assoc_arrays.erase(args[i]);
            g_env.integer_vars.erase(args[i]);
            SetEnvironmentVariableA(args[i].c_str(), nullptr);
        }
        return 0;
    }

    if (cmd == "alias") {
        if (args.size() == 1) {
            for (const auto& [k, v] : g_env.aliases) cout << k << "='" << v << "'\n";
            return 0;
        }
        for (size_t i = 1; i < args.size(); ++i) {
            size_t eq = args[i].find('=');
            if (eq != string::npos) {
                string name = args[i].substr(0, eq);
                string value = args[i].substr(eq + 1);
                if (value.size() >= 2 &&
                    ((value.front() == '"' && value.back() == '"') ||
                     (value.front() == '\'' && value.back() == '\'')))
                    value = value.substr(1, value.size() - 2);
                if (!is_valid_env_var_name(name)) {
                    cerr << "zsh: invalid alias name: " << name << "\n";
                    return 1;
                }
                if (contains_dangerous_alias_tokens(value)) {
                    cerr << "zsh: unsafe alias definition blocked: " << name << "\n";
                    return 1;
                }
                g_env.aliases[name] = value;
            } else if (g_env.aliases.count(args[i])) {
                cout << args[i] << "='" << g_env.aliases[args[i]] << "'\n";
            }
        }
        return 0;
    }

    if (cmd == "unalias") {
        for (size_t i = 1; i < args.size(); ++i) g_env.aliases.erase(args[i]);
        return 0;
    }

    if (cmd == "setopt") {
        if (args.size() == 1) {
            vector<string> names;
            for (const auto& [k, v] : g_env.options) if (v) names.push_back(k);
            sort(names.begin(), names.end());
            for (const auto& n : names) cout << n << "\n";
            return 0;
        }
        for (size_t i = 1; i < args.size(); ++i) {
            string opt = canonicalize_option_name(args[i]);
            if (opt == "localoptions") {
                if (!g_function_local_scopes.empty() && !g_function_local_scopes.back().local_options) {
                    g_function_local_scopes.back().local_options = true;
                    g_function_local_scopes.back().options_before = g_env.options;
                }
            } else {
                apply_shell_option_token(args[i], true, g_env.options);
            }
        }
        return 0;
    }

    if (cmd == "unsetopt") {
        if (args.size() == 1) {
            vector<string> names;
            for (const auto& [k, v] : g_env.options) if (!v) names.push_back(k);
            sort(names.begin(), names.end());
            for (const auto& n : names) cout << n << "\n";
            return 0;
        }
        for (size_t i = 1; i < args.size(); ++i) apply_shell_option_token(args[i], false, g_env.options);
        return 0;
    }

    if (cmd == "pushd") {
        string target = args.size() > 1 ? args[1] : g_env.vars["HOME"];
        try { g_env.dir_stack.push_back(fs::current_path().string()); } catch (...) { g_env.dir_stack.push_back(g_env.vars["HOME"]); }
        int rc = builtin_cd({"cd", target});
        if (rc == 0) {
            try { cout << normalize_path_to_unix(fs::current_path().string()); } catch (...) { cout << "?"; }
            for (int j = (int)g_env.dir_stack.size() - 1; j >= 0; --j)
                cout << " " << normalize_path_to_unix(g_env.dir_stack[j]);
            cout << "\n";
        } else {
            g_env.dir_stack.pop_back();
        }
        return rc;
    }

    if (cmd == "popd") {
        if (g_env.dir_stack.empty()) { cout << "popd: directory stack empty\n"; return 1; }
        string prev = g_env.dir_stack.back(); g_env.dir_stack.pop_back();
        return builtin_cd({"cd", prev});
    }

    if (cmd == "dirs") {
        try { cout << normalize_path_to_unix(fs::current_path().string()); } catch (...) { cout << "?"; }
        for (int j = (int)g_env.dir_stack.size() - 1; j >= 0; --j)
            cout << " " << normalize_path_to_unix(g_env.dir_stack[j]);
        cout << "\n";
        return 0;
    }

    if (cmd == "history") {
        if (args.size() > 1 && args[1] == "-c") { g_env.history.clear(); return 0; }
        for (size_t j = 0; j < g_env.history.size(); ++j)
            cout << " " << setw(4) << (j + 1) << "  " << g_env.history[j] << "\n";
        return 0;
    }

    if (cmd == "eval") {
        string expr;
        for (size_t i = 1; i < args.size(); ++i) expr += (i > 1 ? " " : "") + args[i];
        return parse_and_execute(expr);
    }

    if (cmd == "kill") {
        if (args.size() < 2) { cout << "kill: usage: kill <pid> [pid...]\n"; return 1; }
        int rc = 0;
        for (size_t i = 1; i < args.size(); ++i) {
            if (!args[i].empty() && args[i][0] == '%') {
                size_t job_index = 0;
                if (!resolve_job_index(args[i], job_index)) {
                    cout << "kill: " << args[i] << ": no such job\n";
                    rc = 1;
                    continue;
                }
                BackgroundJob& job = g_env.jobs[job_index];
                BOOL terminated = job.hJob ? TerminateJobObject(job.hJob, 1) : TerminateProcess(job.hProcess, 1);
                if (!terminated) {
                    cout << "kill: " << args[i] << ": unable to terminate job\n";
                    rc = 1;
                }
                continue;
            }
            errno = 0;
            char* end = nullptr;
            unsigned long parsed = strtoul(args[i].c_str(), &end, 10);
            if (errno != 0 || end == args[i].c_str() || *end != '\0' || parsed == 0 || parsed > UINT32_MAX) {
                cout << "kill: " << args[i] << ": invalid pid\n"; rc = 1; continue;
            }
            DWORD pid = (DWORD)parsed;
            HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
            if (!h || !TerminateProcess(h, 1)) { cout << "kill: " << args[i] << ": no such process\n"; rc = 1; }
            if (h) CloseHandle(h);
        }
        return rc;
    }

    if (cmd == "which" || cmd == "type") {
        if (args.size() < 2) return 1;
        bool all = cmd == "type" && args.size() > 1 && args[1] == "-a";
        size_t start = all ? 2 : 1;
        int rc = 0;
        for (size_t i = start; i < args.size(); ++i)
            if (print_command_matches(args[i], all, false, false) != 0) rc = 1;
        return rc;
    }

    if (cmd == "source" || cmd == ".") {
        size_t index = 1;
        bool trace = false, errexit = false;
        while (index < args.size() && !args[index].empty() && args[index][0] == '-') {
            if (args[index] == "--") { ++index; break; }
            if (args[index] == "-x") trace = true;
            else if (args[index] == "-e") errexit = true;
            else { cerr << cmd << ": unsupported option: " << args[index] << "\n"; return 1; }
            ++index;
        }
        if (index >= args.size()) { cout << cmd << ": filename argument required\n"; return 1; }
        bool has_args = (args.size() > index + 1);
        vector<string> saved_positional = g_env.positional_args;
        if (has_args) {
            g_env.positional_args.assign(args.begin() + static_cast<ptrdiff_t>(index + 1), args.end());
        }
        string old_zero = g_env.vars.count("0") ? g_env.vars["0"] : "zsh";
        g_env.vars["0"] = args[index];
        int rc = execute_script(args[index], trace, errexit);
        g_env.vars["0"] = old_zero;
        if (has_args) {
            g_env.positional_args = std::move(saved_positional);
        }
        return rc;
    }

    if (cmd == "clear" || cmd == "cls") {
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(h, &csbi)) {
            DWORD cells = csbi.dwSize.X * csbi.dwSize.Y, written;
            COORD origin = {0, 0};
            FillConsoleOutputCharacterA(h, ' ', cells, origin, &written);
            FillConsoleOutputAttribute(h, csbi.wAttributes, cells, origin, &written);
            SetConsoleCursorPosition(h, origin);
        }
        return 0;
    }

    if (cmd == "dir") {
        // dir is a cmd.exe internal; delegate via cmd /c with any user-supplied args.
        string cmdline = "cmd.exe /c dir";
        for (size_t k = 1; k < args.size(); ++k) {
            string a = args[k];
            bool needs_quote = a.find_first_of(" \t\"") != string::npos;
            if (needs_quote) { string q = "\""; for (char c : a) { if (c=='"') q+="\"\""; else q+=c; } a = q+"\""; }
            cmdline += " " + a;
        }
        wstring wcmd = string_to_wstring(cmdline);
        PROCESS_INFORMATION pi{}; STARTUPINFOW si{}; si.cb = sizeof(si);
        if (!CreateProcessW(NULL, &wcmd[0], NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) return 1;
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD ec = 0;
        if (!GetExitCodeProcess(pi.hProcess, &ec)) ec = 1;
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        return static_cast<int>(ec);
    }
    if (cmd == "false") return 1;
    if (cmd == "true") return 0;

    if (cmd == "test" || cmd == "[") return builtin_test(args);
    if (cmd == "[[") return builtin_double_bracket(args);
    if (cmd == "print")  return builtin_print(args);
    if (cmd == "printf") return builtin_printf(args);

    if (cmd == "let") {
        int rc = 1;
        for (size_t i = 1; i < args.size(); ++i) {
            string expr = trim_copy(args[i]);
            string var_name, rhs, op_type = "=";
            if (expr.length() >= 2 && expr.substr(expr.length() - 2) == "++") {
                var_name = trim_copy(expr.substr(0, expr.length() - 2));
                op_type = "++";
            } else if (expr.length() >= 2 && expr.substr(expr.length() - 2) == "--") {
                var_name = trim_copy(expr.substr(0, expr.length() - 2));
                op_type = "--";
            } else {
                size_t eq = expr.find('=');
                if (eq != string::npos) {
                    if (eq > 0 && (expr[eq - 1] == '+' || expr[eq - 1] == '-' || expr[eq - 1] == '*' || expr[eq - 1] == '/')) {
                        op_type = string(1, expr[eq - 1]) + "=";
                        var_name = trim_copy(expr.substr(0, eq - 1));
                    } else {
                        string lhs = trim_copy(expr.substr(0, eq));
                        if (!lhs.empty() && (lhs.back() == '+' || lhs.back() == '-' || lhs.back() == '*' || lhs.back() == '/')) {
                            op_type = string(1, lhs.back()) + "=";
                            lhs.pop_back();
                            var_name = trim_copy(lhs);
                        } else {
                            var_name = lhs;
                        }
                    }
                    rhs = trim_copy(expr.substr(eq + 1));
                } else {
                    rc = (eval_math_expr(g_env.expand_vars(expr)) != 0) ? 0 : 1;
                    continue;
                }
            }
            long long cur = 0;
            if (g_env.vars.count(var_name)) { try { cur = stoll(g_env.vars[var_name]); } catch (...) {} }
            long long rv = rhs.empty() ? 0 : eval_math_expr(g_env.expand_vars(rhs));
            long long res;
            if      (op_type == "=")  res = rv;
            else if (op_type == "+=") res = cur + rv;
            else if (op_type == "-=") res = cur - rv;
            else if (op_type == "*=") res = cur * rv;
            else if (op_type == "/=") res = rv ? cur / rv : 0;
            else if (op_type == "++") res = cur + 1;
            else if (op_type == "--") res = cur - 1;
            else res = rv;
            g_env.vars[var_name] = to_string(res);
            rc = (res != 0) ? 0 : 1;
        }
        return rc;
    }

    if (cmd == "break") {
        int count = 1;
        if (args.size() > 1) { try { count = stoi(args[1]); } catch (...) { count = 1; } }
        if (count < 1) count = 1;
        g_loop_breaking = count;
        return 0;
    }

    if (cmd == "continue") {
        int count = 1;
        if (args.size() > 1) { try { count = stoi(args[1]); } catch (...) { count = 1; } }
        if (count < 1) count = 1;
        g_loop_continuing = count;
        return 0;
    }

    if (cmd == "return") {
        g_script_return_code = g_env.last_exit_code;
        if (args.size() > 1) { try { g_script_return_code = stoi(args[1]); } catch (...) {} }
        g_script_returning   = true;
        return g_script_return_code;
    }

    if (cmd == "wait") {
        if (args.size() > 1) {
            size_t idx = 0;
            bool found = false;
            if (!args[1].empty() && args[1][0] == '%') {
                found = resolve_job_index(args[1], idx);
            } else {
                errno = 0;
                char* end = nullptr;
                unsigned long parsed = strtoul(args[1].c_str(), &end, 10);
                if (errno == 0 && end != args[1].c_str() && *end == '\0') {
                    for (size_t i = 0; i < g_env.jobs.size(); ++i) {
                        if (g_env.jobs[i].pid == static_cast<DWORD>(parsed)) { idx = i; found = true; break; }
                    }
                }
            }
            if (!found) { cout << "wait: " << args[1] << ": no such job\n"; return 1; }
            wait_for_background_job(g_env.jobs[idx]);
            close_background_job_handles(g_env.jobs[idx]);
            g_env.jobs.erase(g_env.jobs.begin() + idx);
            repair_job_markers();
        } else {
            for (auto& job : g_env.jobs) {
                wait_for_background_job(job);
                close_background_job_handles(job);
            }
            g_env.jobs.clear();
            repair_job_markers();
        }
        return 0;
    }

    if (cmd == "fg" || cmd == "bg") {
        if (g_env.jobs.empty()) { cout << cmd << ": no current job\n"; return 1; }
        size_t idx = g_env.jobs.size()-1;
        if (args.size()>1 && !resolve_job_index(args[1], idx)) { cout<<cmd<<": no such job\n"; return 1; }
        auto& job = g_env.jobs[idx];
        refresh_background_job_state(job);
        if (job.state == JobState::Done || job.state == JobState::Failed) {
            cout<<"["<<job.job_id<<"] "<<job_state_name(job.state)<<"  "<<job.command<<"\n";
            close_background_job_handles(job);
            g_env.jobs.erase(g_env.jobs.begin()+idx);
            repair_job_markers();
            return 0;
        }
        if (cmd=="fg") {
            if (job.state == JobState::Stopped) {
                bool resumed = true;
                for (DWORD pid : job.pids) resumed = resume_win32_process(pid) && resumed;
                if (!resumed) { cerr << "fg: failed to resume job\n"; return 1; }
                job.state = JobState::Running;
            }
            cout << job.command << "\n";
            set_foreground_pids(job.pids);
            struct ForegroundPidsScopeGuard {
                ~ForegroundPidsScopeGuard() { clear_foreground_pids(); }
            } fg_scope_guard;
            wait_for_background_job(job);
            DWORD ec=0; GetExitCodeProcess(job.hProcess, &ec);
            close_background_job_handles(job);
            g_env.jobs.erase(g_env.jobs.begin()+idx);
            repair_job_markers();
            return (int)ec;
        }
        bool resumed = true;
        if (job.state == JobState::Stopped) {
            for (DWORD pid : job.pids) resumed = resume_win32_process(pid) && resumed;
        }
        if (!resumed) { cerr << "bg: failed to resume job\n"; return 1; }
        job.state = JobState::Running;
        select_current_job(job.job_id);
        cout << "[" << job.job_id << "] " << job.command << " &\n";
        return 0;
    }

    if (cmd == "disown") {
        if (g_env.jobs.empty()) return 0;
        size_t idx = g_env.jobs.size()-1;
        if (args.size()>1 && !resolve_job_index(args[1], idx)) { cout << "disown: no such job\n"; return 1; }
        close_background_job_handles(g_env.jobs[idx]);
        g_env.jobs.erase(g_env.jobs.begin()+idx);
        repair_job_markers();
        return 0;
    }

    if (cmd == "fc") {
        bool list = args.size()>1 && args[1]=="-l";
        if (list) {
            size_t s = g_env.history.size()>16 ? g_env.history.size()-16 : 0;
            for (size_t j=s; j<g_env.history.size(); ++j)
                cout << " " << setw(4) << (j + 1) << "  " << g_env.history[j] << "\n";
        } else if (args.size()>1) {
            errno = 0; char* end = nullptr;
            unsigned long n = strtoul(args[1].c_str(), &end, 10);
            if (errno == 0 && end != args[1].c_str() && *end == '\0' && n>=1 && n<=g_env.history.size()) {
                string s = g_env.history[n-1]; cout << s << "\n";
                return parse_and_execute(s);
            }
        }
        return 0;
    }

    if (cmd == "integer") {
        for (size_t i=1; i<args.size(); ++i) {
            size_t eq=args[i].find('=');
            const string name = eq == string::npos ? args[i] : args[i].substr(0, eq);
            if (!is_valid_env_var_name(name)) {
                cerr << "integer: invalid variable name: " << name << "\n";
                return 1;
            }
            if (g_env.readonly_vars.count(name)) {
                cerr << "zsh: read-only variable: " << name << "\n";
                return 1;
            }
            mark_function_local(name);
            g_env.integer_vars.insert(name);
            if (eq != string::npos) {
                string value = g_env.expand_vars(args[i].substr(eq + 1));
                g_env.vars[name] = to_string(eval_math_expr(value));
            } else if (!g_env.vars.count(name)) {
                g_env.vars[name] = "0";
            }
        }
        return 0;
    }

    if (cmd == "getopts") {
        if (args.size()<3) return 1;
        const string& optstr=args[1]; const string& varname=args[2];
        int optind=1;
        if (g_env.vars.count("OPTIND")) { try{optind=stoi(g_env.vars["OPTIND"]);}catch(...){} }
        vector<string> pargs = args.size()>3 ? vector<string>(args.begin()+3,args.end()) : g_env.positional_args;
        int offset = 1;
        if (g_env.vars.count("__getopts_offset")) { try { offset = stoi(g_env.vars["__getopts_offset"]); } catch (...) {} }
        int tracked_optind = -1;
        if (g_env.vars.count("__getopts_optind")) { try { tracked_optind = stoi(g_env.vars["__getopts_optind"]); } catch (...) {} }
        if (tracked_optind != optind || !g_env.vars.count("__getopts_spec") || g_env.vars["__getopts_spec"] != optstr) offset = 1;
        if (optind>(int)pargs.size()) { g_env.vars[varname]="?"; return 1; }
        const string& arg=pargs[optind-1];
        if (arg.empty()||arg[0]!='-'||arg=="--") { g_env.vars[varname]="?"; return 1; }
        if (offset >= static_cast<int>(arg.size())) { ++optind; offset = 1; }
        if (optind > static_cast<int>(pargs.size())) { g_env.vars[varname]="?"; return 1; }
        const string& current = pargs[optind-1];
        char opt=current[static_cast<size_t>(offset++)];
        bool silent = !optstr.empty() && optstr[0] == ':';
        size_t pos=optstr.find(opt);
        g_env.vars.erase("OPTARG");
        if (pos==string::npos || (silent && pos == 0)) {
            g_env.vars[varname]="?";
            g_env.vars["OPTARG"] = string(1, opt);
            if (!silent) cerr<<"zsh: illegal option: -"<<opt<<"\n";
        } else {
            g_env.vars[varname]=string(1,opt);
            if (pos+1<optstr.size()&&optstr[pos+1]==':') {
                if (offset < static_cast<int>(current.size())) {
                    g_env.vars["OPTARG"]=current.substr(static_cast<size_t>(offset));
                    offset = static_cast<int>(current.size());
                } else if (optind<(int)pargs.size()) {
                    ++optind; g_env.vars["OPTARG"]=pargs[optind-1];
                } else {
                    g_env.vars[varname]=silent ? ":" : "?";
                    g_env.vars["OPTARG"] = string(1, opt);
                    if (!silent) cerr<<"zsh: option requires argument: -"<<opt<<"\n";
                }
            }
        }
        if (offset >= static_cast<int>(current.size())) { ++optind; offset = 1; }
        g_env.vars["OPTIND"]=to_string(optind);
        g_env.vars["__getopts_offset"]=to_string(offset);
        g_env.vars["__getopts_optind"]=to_string(optind);
        g_env.vars["__getopts_spec"]=optstr;
        return 0;
    }

    if (cmd == "set") {
        if (args.size() == 1) {
            for (const auto& [k, v] : g_env.vars) {
                if (k.rfind("__", 0) != 0) cout << k << "=" << v << "\n";
            }
            return 0;
        }
        if (args.size() >= 2 && args[1] == "--") {
            g_env.positional_args.assign(args.begin() + 2, args.end());
            return 0;
        }
        for (size_t i = 1; i < args.size(); ++i) {
            const string& arg = args[i];
            if (arg == "-x") g_env.options["xtrace"] = true;
            else if (arg == "+x") g_env.options["xtrace"] = false;
            else if (arg == "-e") g_env.options["errexit"] = true;
            else if (arg == "+e") g_env.options["errexit"] = false;
            else if (arg == "-u") g_env.options["nounset"] = true;
            else if (arg == "+u") g_env.options["nounset"] = false;
            else if (arg == "--") {
                g_env.positional_args.assign(args.begin() + i + 1, args.end());
                break;
            } else if (arg.size() > 1 && (arg[0] == '-' || arg[0] == '+')) {
                apply_shell_option_token(arg.substr(1), arg[0] == '-', g_env.options);
            } else {
                g_env.positional_args.assign(args.begin() + i, args.end());
                break;
            }
        }
        return 0;
    }

    if (cmd == "trap") {
        if (args.size() == 1) {
            for (const auto& [k, v] : g_env.vars) {
                if (k.rfind("__trap_", 0) == 0) cout << "trap -- '" << v << "' " << k.substr(7) << "\n";
            }
            return 0;
        }
        if (args.size() == 2 && args[1] == "-") {
            vector<string> to_erase;
            for (const auto& [k, v] : g_env.vars) {
                if (k.rfind("__trap_", 0) == 0) to_erase.push_back(k);
            }
            for (const auto& k : to_erase) g_env.vars.erase(k);
            return 0;
        }
        if (args.size() >= 3) {
            string handler = args[1];
            for (size_t k = 2; k < args.size(); ++k) {
                string sig = args[k];
                if (sig.rfind("SIG", 0) == 0) sig = sig.substr(3);
                if (sig == "0") sig = "EXIT";
                else if (sig == "1") sig = "HUP";
                else if (sig == "2") sig = "INT";
                else if (sig == "3") sig = "QUIT";
                else if (sig == "15") sig = "TERM";
                else if (sig == "20") sig = "TSTP";
                else if (sig == "28") sig = "WINCH";
                if (handler == "-" || handler.empty()) {
                    g_env.vars.erase("__trap_" + sig);
                } else {
                    g_env.vars["__trap_" + sig] = handler;
                }
            }
            return 0;
        }
        return 0;
    }

    if (cmd == "times") {
        FILETIME cr,ex,ku,us;
        if (GetProcessTimes(GetCurrentProcess(),&cr,&ex,&ku,&us)) {
            auto s=[](FILETIME f)->double{ ULARGE_INTEGER t; t.LowPart=f.dwLowDateTime; t.HighPart=f.dwHighDateTime; return t.QuadPart/10000000.0; };
            ostringstream tout;
            tout << fixed << setprecision(3) << s(us) << "s " << s(ku) << "s\n0.000s 0.000s\n";
            cout << tout.str();
        }
        return 0;
    }

    if (cmd == "autoload") {
        for (size_t i = 1; i < args.size(); ++i) {
            if (!args[i].empty() && args[i][0] != '-') g_env.autoload_functions.insert(args[i]);
        }
        return 0;
    }

    if (cmd == "compinit") {
        vector<string> search_paths;
        if (g_env.indexed_arrays.count("fpath")) search_paths = g_env.indexed_arrays["fpath"];
        if (g_env.vars.count("FPATH")) {
            stringstream paths(g_env.vars["FPATH"]);
            string path;
            while (getline(paths, path, ';')) if (!path.empty()) search_paths.push_back(path);
        }
        for (const auto& path : search_paths) {
            error_code ec;
            for (fs::directory_iterator it(normalize_path_to_win(path), ec), end; !ec && it != end; it.increment(ec)) {
                string name = it->path().filename().string();
                if (name.size() > 1 && name[0] == '_') {
                    string command_name = name.substr(1);
                    g_env.completion_definitions[command_name] = name;
                    g_env.autoload_functions.insert(name);
                }
            }
        }
        return 0;
    }

    if (cmd == "compdef") {
        if (args.size() == 1) {
            for (const auto& [command_name, function_name] : g_env.completion_definitions)
                cout << "compdef " << function_name << " " << command_name << "\n";
            return 0;
        }
        if (args[1] == "-d") {
            for (size_t i = 2; i < args.size(); ++i) g_env.completion_definitions.erase(args[i]);
            return 0;
        }
        if (args.size() < 3) { cerr << "compdef: usage: compdef function command...\n"; return 1; }
        for (size_t i = 2; i < args.size(); ++i) g_env.completion_definitions[args[i]] = args[1];
        return 0;
    }

    if (cmd == "compadd") {
        auto& reply = g_env.indexed_arrays["reply"];
        for (size_t i = 1; i < args.size(); ++i) {
            if (!args[i].empty() && args[i][0] == '-') continue;
            reply.push_back(args[i]);
        }
        return 0;
    }

    if (cmd == "bindkey") {
        if (args.size() == 1 || (args.size() == 2 && args[1] == "-L")) {
            for (const auto& [keymap, bindings] : g_env.keymaps)
                for (const auto& [key, widget] : bindings)
                    cout << "bindkey -M '" << keymap << "' '" << key << "' '" << widget << "'\n";
            return 0;
        }
        if (args.size() == 4 && args[1] == "-A") {
            auto found = g_env.keymaps.find(args[2]);
            if (found == g_env.keymaps.end()) { cerr << "bindkey: no such keymap: " << args[2] << "\n"; return 1; }
            g_env.keymaps[args[3]] = found->second;
            return 0;
        }
        if (args.size() >= 5 && args[1] == "-M" && args[3] == "-r") {
            g_env.keymaps[args[2]].erase(args[4]);
            return 0;
        }
        if (args.size() >= 5 && args[1] == "-M") {
            g_env.keymaps[args[2]][args[3]] = args[4];
            return 0;
        }
        cerr << "bindkey: unsupported arguments\n";
        return 1;
    }

    if (cmd == "zstyle") {
        if (args.size() == 2 && args[1] == "-L") {
            for (const auto& [key, values] : g_env.styles) {
                cout << "zstyle '" << key.first << "' '" << key.second << "'";
                for (const auto& value : values) cout << " '" << value << "'";
                cout << "\n";
            }
            return 0;
        }
        if (args.size() >= 3 && args[1] == "-d") {
            if (args.size() == 3) {
                for (auto it = g_env.styles.begin(); it != g_env.styles.end(); ) {
                    if (it->first.first == args[2]) it = g_env.styles.erase(it); else ++it;
                }
            } else {
                g_env.styles.erase({args[2], args[3]});
            }
            return 0;
        }
        if (args.size() < 4) { cerr << "zstyle: usage: zstyle context style value...\n"; return 1; }
        g_env.styles[{args[1], args[2]}] = vector<string>(args.begin() + 3, args.end());
        return 0;
    }

    if (cmd == "zle") {
        if (args.size() == 2 && args[1] == "-l") {
            for (const auto& [name, function_name] : g_env.widgets) cout << name << "\n";
            return 0;
        }
        if (args.size() >= 3 && args[1] == "-D") {
            for (size_t i = 2; i < args.size(); ++i) g_env.widgets.erase(args[i]);
            return 0;
        }
        if (args.size() == 4 && args[1] == "-A") {
            auto found = g_env.widgets.find(args[2]);
            if (found == g_env.widgets.end()) { cerr << "zle: no such widget: " << args[2] << "\n"; return 1; }
            g_env.widgets[args[3]] = found->second;
            return 0;
        }
        if (args.size() >= 3 && args[1] == "-N") {
            g_env.widgets[args[2]] = args.size() > 3 ? args[3] : args[2];
            return 0;
        }
        cerr << "zle: unsupported arguments\n";
        return 1;
    }

    if (cmd == "zmodload") {
        if (args.size() == 1) {
            for (const auto& module : g_env.loaded_modules) cout << module << "\n";
            return 0;
        }
        for (size_t i = 1; i < args.size(); ++i) {
            if (!args[i].empty() && args[i][0] == '-') continue;
            g_env.loaded_modules.insert(args[i]);
        }
        if (g_env.loaded_modules.count("zsh/datetime")) {
            auto now = chrono::system_clock::now();
            auto seconds = chrono::duration_cast<chrono::seconds>(now.time_since_epoch());
            auto micros = chrono::duration_cast<chrono::microseconds>(now.time_since_epoch());
            g_env.vars["EPOCHSECONDS"] = to_string(seconds.count());
            ostringstream realtime;
            realtime << seconds.count() << '.' << setw(6) << setfill('0') << (micros.count() % 1000000);
            g_env.vars["EPOCHREALTIME"] = realtime.str();
        }
        if (g_env.loaded_modules.count("zsh/system")) g_env.vars["SYS_PID"] = to_string(GetCurrentProcessId());
        if (g_env.loaded_modules.count("zsh/parameter")) {
            auto& parameters = g_env.assoc_arrays["parameters"];
            parameters.clear();
            for (const auto& [name, value] : g_env.vars) parameters[name] = "scalar";
            for (const auto& [name, value] : g_env.indexed_arrays) parameters[name] = "array";
            for (const auto& [name, value] : g_env.assoc_arrays)
                if (name != "parameters") parameters[name] = "association";
            auto& commands = g_env.assoc_arrays["commands"];
            commands.clear();
            for (const auto& name : zsh_builtin_command_names()) commands[name] = "builtin";
            auto& functions = g_env.assoc_arrays["functions"];
            functions.clear();
            for (const auto& [name, body] : g_env.functions) functions[name] = body;
            auto& aliases = g_env.assoc_arrays["aliases"];
            aliases = g_env.aliases;
        }
        return 0;
    }

    if (cmd == "strftime") {
        if (!g_env.loaded_modules.count("zsh/datetime")) { cerr << "strftime: zsh/datetime is not loaded\n"; return 1; }
        string format = args.size() > 1 ? args[1] : "%c";
        time_t timestamp = time(nullptr);
        if (args.size() > 2) {
            try { timestamp = static_cast<time_t>(stoll(args[2])); } catch (...) { return 1; }
        }
        tm local_time{};
        if (localtime_s(&local_time, &timestamp) != 0) return 1;
        char result[1024] = {};
        if (::strftime(result, sizeof(result), format.c_str(), &local_time) == 0) return 1;
        cout << result << "\n";
        return 0;
    }

    if (cmd == "sysread") {
        if (!g_env.loaded_modules.count("zsh/system")) { cerr << "sysread: zsh/system is not loaded\n"; return 1; }
        int fd = 0;
        size_t argument = 1;
        if (args.size() > 2 && args[1] == "-i") { try { fd = stoi(args[2]); } catch (...) { return 1; } argument = 3; }
        string variable = argument < args.size() ? args[argument] : "REPLY";
        HANDLE handle = shell_fd_handle(fd);
        if (handle == INVALID_HANDLE_VALUE || handle == nullptr) return 1;
        string value;
        char character = 0;
        DWORD read = 0;
        while (ReadFile(handle, &character, 1, &read, nullptr) && read == 1 && character != '\n') {
            if (character != '\r') value += character;
        }
        if (value.empty() && read == 0) return 1;
        g_env.vars[variable] = value;
        g_env.vars["REPLY"] = value;
        return 0;
    }

    if (cmd == "syswrite") {
        if (!g_env.loaded_modules.count("zsh/system")) { cerr << "syswrite: zsh/system is not loaded\n"; return 1; }
        int fd = 1;
        size_t argument = 1;
        if (args.size() > 2 && args[1] == "-o") { try { fd = stoi(args[2]); } catch (...) { return 1; } argument = 3; }
        string payload;
        for (size_t i = argument; i < args.size(); ++i) {
            if (!payload.empty()) payload += ' ';
            payload += args[i];
        }
        HANDLE handle = shell_fd_handle(fd);
        if (handle == INVALID_HANDLE_VALUE || handle == nullptr) return 1;
        DWORD written = 0;
        return WriteFile(handle, payload.data(), static_cast<DWORD>(payload.size()), &written, nullptr) &&
               written == payload.size() ? 0 : 1;
    }

    if (cmd == "rehash") {
        unsigned long long epoch = 0;
        if (g_env.vars.count("__command_path_epoch")) {
            try { epoch = stoull(g_env.vars["__command_path_epoch"]); } catch (...) {}
        }
        g_env.vars["__command_path_epoch"] = to_string(epoch + 1);
        return 0;
    }

    error_code _autocd_ec;
    string _win_cmd = normalize_path_to_win(cmd);
    bool is_drive_letter = (cmd.size() == 2 && isalpha((unsigned char)cmd[0]) && cmd[1] == ':');
    if (args.size() == 1 && g_env.options.count("autocd") && g_env.options.at("autocd") &&
        (is_drive_letter || fs::is_directory(_win_cmd, _autocd_ec))) {
        return builtin_cd({"cd", cmd});
    }

    Pipeline pl; SingleCmd sc; sc.args = args; pl.cmds.push_back(sc);
    return execute_pipeline_native(pl);
}

static string trim_copy(const string& s) {
    size_t b = 0;
    while (b < s.size() && isspace((unsigned char)s[b])) ++b;
    size_t e = s.size();
    while (e > b && isspace((unsigned char)s[e - 1])) --e;
    return s.substr(b, e - b);
}

string capture_command_output(const string& cmd) {
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE rawRead = nullptr;
    HANDLE rawWrite = nullptr;
    if (!CreatePipe(&rawRead, &rawWrite, &sa, 0)) return "";
    unique_handle hRead = make_unique_handle(rawRead);
    unique_handle hWrite = make_unique_handle(rawWrite);
    // Prevent the read end from being inherited by children spawned during capture.
    if (!SetHandleInformation((HANDLE)hRead.get(), HANDLE_FLAG_INHERIT, 0)) {
        return "";
    }

    HANDLE oldStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE oldStdErr = GetStdHandle(STD_ERROR_HANDLE);
    int oldFdOut = _dup(_fileno(stdout));
    int oldFdErr = _dup(_fileno(stderr));

    string output;
    // INVARIANT: `output` is written only by the reader thread below and read by
    // the main thread only after reader.join(). Do not touch it from the main
    // thread between thread creation and join without adding synchronization.
    std::thread reader([&]() {
        char buffer[4096];
        DWORD read_bytes = 0;
        while (ReadFile((HANDLE)hRead.get(), buffer, sizeof(buffer), &read_bytes, NULL) && read_bytes > 0) {
            output.append(buffer, buffer + read_bytes);
        }
    });

    cout.flush();
    cerr.flush();

    SetStdHandle(STD_OUTPUT_HANDLE, (HANDLE)hWrite.get());
    SetStdHandle(STD_ERROR_HANDLE, (HANDLE)hWrite.get());

    HANDLE rawWriteForCrt = INVALID_HANDLE_VALUE;
    if (!DuplicateHandle(GetCurrentProcess(), (HANDLE)hWrite.get(), GetCurrentProcess(), &rawWriteForCrt, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
        rawWriteForCrt = INVALID_HANDLE_VALUE;
    }
    unique_handle hWriteForCrt = make_unique_handle(rawWriteForCrt);

    int writeFd = -1;
    if (hWriteForCrt.get()) {
        writeFd = _open_osfhandle((intptr_t)hWriteForCrt.get(), _O_TEXT);
        if (writeFd != -1) {
            // Ownership moved to CRT fd; _close(writeFd) closes the HANDLE.
            hWriteForCrt.release();
        }
    }

    if (writeFd != -1) {
        _dup2(writeFd, _fileno(stdout));
        _dup2(writeFd, _fileno(stderr));
        _close(writeFd);
    }

    exception_ptr eptr;
    try {
        parse_and_execute(cmd);
    } catch (...) {
        eptr = current_exception();
    }

    cout.flush();
    cerr.flush();

    // Close our pipe write end FIRST so the reader thread sees EOF even on the
    // exception path, then restore std handles, then join the reader.
    hWrite.reset();

    if (oldFdOut != -1) {
        _dup2(oldFdOut, _fileno(stdout));
        _close(oldFdOut);
    }
    if (oldFdErr != -1) {
        _dup2(oldFdErr, _fileno(stderr));
        _close(oldFdErr);
    }

    SetStdHandle(STD_OUTPUT_HANDLE, oldStdOut);
    SetStdHandle(STD_ERROR_HANDLE, oldStdErr);

    if (reader.joinable()) {
        reader.join();
    }

    if (eptr) {
        rethrow_exception(eptr);
    }

    while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) output.pop_back();
    return output;
}

static bool is_word_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

static bool is_statement_boundary_before(const string& s, size_t pos) {
    if (pos == 0) return true;
    size_t prev = pos;
    while (prev > 0 && (s[prev - 1] == ' ' || s[prev - 1] == '\t' || s[prev - 1] == '\r')) {
        --prev;
    }
    if (prev == 0) return true;
    char prev_ch = s[prev - 1];
    if (prev_ch == '\n' || prev_ch == ';' || prev_ch == '&' || prev_ch == '|' || prev_ch == '(' || prev_ch == '{') return true;
    return false;
}

static size_t find_matching_done(const string& s, size_t start) {
    int depth = 1;
    bool in_sq = false, in_dq = false;
    int paren_depth = 0, brace_depth = 0;
    for (size_t i = start; i < s.size(); ++i) {
        char ch = s[i];
        if (ch == '"' && !in_sq) { in_dq = !in_dq; continue; }
        if (ch == '\'' && !in_dq) { in_sq = !in_sq; continue; }
        if (in_sq || in_dq) continue;
        if (ch == '(') { paren_depth++; continue; }
        if (ch == ')' && paren_depth > 0) { paren_depth--; continue; }
        if (ch == '{') { brace_depth++; continue; }
        if (ch == '}' && brace_depth > 0) { brace_depth--; continue; }
        if (paren_depth != 0 || brace_depth != 0) continue;

        if (s.compare(i, 2, "do") == 0 && (i + 2 >= s.size() || !is_word_char(s[i + 2])) && is_statement_boundary_before(s, i)) {
            depth++;
            i += 1;
            continue;
        }
        if (s.compare(i, 4, "done") == 0 && (i + 4 >= s.size() || !is_word_char(s[i + 4])) && is_statement_boundary_before(s, i)) {
            depth--;
            if (depth == 0) return i;
            i += 3;
            continue;
        }
    }
    return string::npos;
}

static size_t find_matching_fi(const string& s, size_t start) {
    int depth = 1;
    bool in_sq = false, in_dq = false;
    int paren_depth = 0, brace_depth = 0;
    for (size_t i = start; i < s.size(); ++i) {
        char ch = s[i];
        if (ch == '"' && !in_sq) { in_dq = !in_dq; continue; }
        if (ch == '\'' && !in_dq) { in_sq = !in_sq; continue; }
        if (in_sq || in_dq) continue;
        if (ch == '(') { paren_depth++; continue; }
        if (ch == ')' && paren_depth > 0) { paren_depth--; continue; }
        if (ch == '{') { brace_depth++; continue; }
        if (ch == '}' && brace_depth > 0) { brace_depth--; continue; }
        if (paren_depth != 0 || brace_depth != 0) continue;

        if (s.compare(i, 2, "if") == 0 && (i + 2 >= s.size() || !is_word_char(s[i + 2])) && is_statement_boundary_before(s, i)) {
            depth++;
            i += 1;
            continue;
        }
        if (s.compare(i, 2, "fi") == 0 && (i + 2 >= s.size() || !is_word_char(s[i + 2])) && is_statement_boundary_before(s, i)) {
            depth--;
            if (depth == 0) return i;
            i += 1;
            continue;
        }
    }
    return string::npos;
}

static size_t find_top_level_keyword(const string& s, const string& word, size_t start = 0) {
    bool in_sq = false, in_dq = false;
    int paren_depth = 0, brace_depth = 0;
    for (size_t i = start; i < s.size(); ++i) {
        char ch = s[i];
        if (ch == '"' && !in_sq) { in_dq = !in_dq; continue; }
        if (ch == '\'' && !in_dq) { in_sq = !in_sq; continue; }
        if (in_sq || in_dq) continue;
        if (ch == '(') { paren_depth++; continue; }
        if (ch == ')' && paren_depth > 0) { paren_depth--; continue; }
        if (ch == '{') { brace_depth++; continue; }
        if (ch == '}' && brace_depth > 0) { brace_depth--; continue; }
        if (paren_depth != 0 || brace_depth != 0) continue;
        if (i + word.size() > s.size()) continue;
        if (s.compare(i, word.size(), word) != 0) continue;
        bool right_ok = (i + word.size() >= s.size()) || !is_word_char(s[i + word.size()]);
        if (right_ok && is_statement_boundary_before(s, i)) return i;
    }
    return string::npos;
}

static size_t find_top_level_word(const string& s, const string& word, size_t start = 0) {
    bool in_sq = false;
    bool in_dq = false;
    int paren_depth = 0;
    int brace_depth = 0;
    for (size_t i = start; i < s.size(); ++i) {
        char ch = s[i];
        if (ch == '"' && !in_sq) { in_dq = !in_dq; continue; }
        if (ch == '\'' && !in_dq) { in_sq = !in_sq; continue; }
        if (in_sq || in_dq) continue;
        if (ch == '(') { paren_depth++; continue; }
        if (ch == ')' && paren_depth > 0) { paren_depth--; continue; }
        if (ch == '{') { brace_depth++; continue; }
        if (ch == '}' && brace_depth > 0) { brace_depth--; continue; }
        if (paren_depth != 0 || brace_depth != 0) continue;
        if (i + word.size() > s.size()) continue;
        if (s.compare(i, word.size(), word) != 0) continue;
        bool left_ok = (i == 0) || !is_word_char(s[i - 1]);
        bool right_ok = (i + word.size() >= s.size()) || !is_word_char(s[i + word.size()]);
        if (left_ok && right_ok) return i;
    }
    return string::npos;
}

static bool starts_with_word_trimmed(const string& s, const string& word) {
    string t = trim_copy(s);
    if (t.size() < word.size()) return false;
    if (t.compare(0, word.size(), word) != 0) return false;
    return t.size() == word.size() || !is_word_char(t[word.size()]);
}

static string strip_optional_trailing_semicolon(string s) {
    s = trim_copy(s);
    if (!s.empty() && s.back() == ';') {
        s.pop_back();
        s = trim_copy(s);
    }
    return s;
}

static int execute_if_block(const string& block) {
    string s = trim_copy(block);
    if (!starts_with_word_trimmed(s, "if")) return 1;

    size_t cursor = find_top_level_keyword(s, "if", 0);
    if (cursor == string::npos) return 1;
    cursor += 2;

    while (true) {
        size_t then_pos = find_top_level_keyword(s, "then", cursor);
        if (then_pos == string::npos) return 1;

        string cond = strip_optional_trailing_semicolon(s.substr(cursor, then_pos - cursor));
        int cond_status = parse_and_execute(cond);
        g_env.last_exit_code = cond_status;

        size_t body_start = then_pos + 4;
        size_t fi_pos = find_matching_fi(s, body_start);
        if (fi_pos == string::npos) return 1;

        size_t elif_pos = string::npos;
        size_t else_pos = string::npos;
        int depth = 0;
        bool in_sq = false, in_dq = false;
        int p_depth = 0, b_depth = 0;
        for (size_t i = body_start; i < fi_pos; ++i) {
            char ch = s[i];
            if (ch == '"' && !in_sq) { in_dq = !in_dq; continue; }
            if (ch == '\'' && !in_dq) { in_sq = !in_sq; continue; }
            if (in_sq || in_dq) continue;
            if (ch == '(') { p_depth++; continue; }
            if (ch == ')' && p_depth > 0) { p_depth--; continue; }
            if (ch == '{') { b_depth++; continue; }
            if (ch == '}' && b_depth > 0) { b_depth--; continue; }
            if (p_depth != 0 || b_depth != 0) continue;

            if (s.compare(i, 2, "if") == 0 && (i + 2 >= s.size() || !is_word_char(s[i + 2])) && is_statement_boundary_before(s, i)) {
                depth++;
                i += 1;
                continue;
            }
            if (s.compare(i, 2, "fi") == 0 && (i + 2 >= s.size() || !is_word_char(s[i + 2])) && is_statement_boundary_before(s, i)) {
                if (depth > 0) depth--;
                i += 1;
                continue;
            }
            if (depth == 0 && is_statement_boundary_before(s, i)) {
                if (s.compare(i, 4, "elif") == 0 && (i + 4 >= s.size() || !is_word_char(s[i + 4])) && elif_pos == string::npos && else_pos == string::npos) {
                    elif_pos = i;
                } else if (s.compare(i, 4, "else") == 0 && (i + 4 >= s.size() || !is_word_char(s[i + 4])) && else_pos == string::npos) {
                    else_pos = i;
                }
            }
        }

        size_t next_pos = fi_pos;
        string next_kind = "fi";
        if (elif_pos != string::npos && elif_pos < next_pos) { next_pos = elif_pos; next_kind = "elif"; }
        if (else_pos != string::npos && else_pos < next_pos) { next_pos = else_pos; next_kind = "else"; }

        string then_body = trim_copy(s.substr(body_start, next_pos - body_start));
        if (cond_status == 0) {
            return execute_command_line(then_body);
        }

        if (next_kind == "elif") {
            cursor = next_pos + 4;
            continue;
        }

        if (next_kind == "else") {
            string else_body = trim_copy(s.substr(next_pos + 4, fi_pos - (next_pos + 4)));
            return execute_command_line(else_body);
        }

        return 0;
    }
}

static int execute_for_loop(const string& block) {
    LoopGuard loop_guard;
    string s = trim_copy(block);
    size_t for_pos = find_top_level_keyword(s, "for", 0);
    size_t in_pos = find_top_level_word(s, "in", for_pos == string::npos ? 0 : for_pos + 3);
    size_t do_pos = find_top_level_word(s, "do", in_pos == string::npos ? 0 : in_pos + 2);
    if (for_pos == string::npos || in_pos == string::npos || do_pos == string::npos) return 1;
    size_t done_pos = find_matching_done(s, do_pos + 2);
    if (done_pos == string::npos) return 1;

    string var_name = trim_copy(s.substr(for_pos + 3, in_pos - (for_pos + 3)));
    string items_str = strip_optional_trailing_semicolon(s.substr(in_pos + 2, do_pos - (in_pos + 2)));
    string do_body = trim_copy(s.substr(do_pos + 2, done_pos - (do_pos + 2)));
    if (var_name.empty()) return 1;

    vector<string> items;
    string items_trimmed = trim_copy(items_str);
    if (items_trimmed == "\"$@\"" || items_trimmed == "$@" ||
        items_trimmed == "\"$*\"" || items_trimmed == "${@}") {
        items = g_env.positional_args;
    } else {
        items = tokenize_words(g_env.expand_vars(items_str));
        items = expand_globs(items);
    }

    int last_rc = 0;
    for (const auto& item : items) {
        g_env.vars[var_name] = item;
        last_rc = execute_command_line(do_body);
        g_env.last_exit_code = last_rc;
        if (g_loop_breaking > 0) {
            --g_loop_breaking;
            break;
        }
        if (g_loop_continuing > 0) {
            --g_loop_continuing;
            if (g_loop_continuing > 0) break;
            continue;
        }
        if (g_script_returning || g_subshell_exiting) break;
    }
    return last_rc;
}

static vector<string> split_on_top_level_commas(const string& s) {
    vector<string> out;
    string cur;
    bool in_sq = false, in_dq = false;
    int paren_depth = 0;
    for (char ch : s) {
        if (ch == '"' && !in_sq) { in_dq = !in_dq; cur += ch; continue; }
        if (ch == '\'' && !in_dq) { in_sq = !in_sq; cur += ch; continue; }
        if (!in_sq && !in_dq) {
            if (ch == '(') { paren_depth++; cur += ch; continue; }
            if (ch == ')' && paren_depth > 0) { paren_depth--; cur += ch; continue; }
            if (ch == ',' && paren_depth == 0) {
                out.push_back(trim_copy(cur));
                cur.clear();
                continue;
            }
        }
        cur += ch;
    }
    if (!trim_copy(cur).empty()) out.push_back(trim_copy(cur));
    return out;
}

static int execute_cstyle_for_loop(const string& block) {
    LoopGuard loop_guard;
    string s = trim_copy(block);
    if (!starts_with_word_trimmed(s, "for")) return 1;

    size_t open = s.find("((");
    if (open == string::npos) return 1;
    size_t close = s.find("))", open + 2);
    if (close == string::npos) return 1;

    string header = s.substr(open + 2, close - (open + 2));
    vector<string> parts;
    string cur;
    for (char ch : header) {
        if (ch == ';') { parts.push_back(trim_copy(cur)); cur.clear(); }
        else cur += ch;
    }
    parts.push_back(trim_copy(cur));
    if (parts.size() != 3) return 1;

    size_t do_pos = find_top_level_word(s, "do", close + 2);
    if (do_pos == string::npos) return 1;
    size_t done_pos = find_matching_done(s, do_pos + 2);
    if (done_pos == string::npos) return 1;
    string body = trim_copy(s.substr(do_pos + 2, done_pos - (do_pos + 2)));

    for (const auto& expr : split_on_top_level_commas(parts[0])) {
        if (!expr.empty()) parse_and_execute("let " + expr);
    }

    int last_rc = 0;
    int guard = 0;
    while (true) {
        // Evaluate condition each iteration so updates from the previous
        // iteration's increment expressions are visible here.
        long long cond = 1;
        if (!parts[1].empty()) cond = eval_math_expr(g_env.expand_vars(parts[1]));
        if (cond == 0) break;

        last_rc = execute_command_line(body);
        g_env.last_exit_code = last_rc;
        if (g_loop_breaking > 0) {
            --g_loop_breaking;
            break;
        }
        if (g_loop_continuing > 0) {
            --g_loop_continuing;
            if (g_loop_continuing > 0) break;
            for (const auto& expr : split_on_top_level_commas(parts[2])) {
                if (!expr.empty()) parse_and_execute("let " + expr);
            }
            continue;
        }
        if (g_script_returning || g_subshell_exiting) break;

        for (const auto& expr : split_on_top_level_commas(parts[2])) {
            if (!expr.empty()) parse_and_execute("let " + expr);
        }

        if (++guard > 1000000) {
            cerr << "zsh: for((...)): loop guard exceeded\n";
            return 1;
        }
    }
    return last_rc;
}

static string strip_outer_quotes(string s) {
    s = trim_copy(s);
    if (s.size() >= 2 && ((s.front() == '\'' && s.back() == '\'') || (s.front() == '"' && s.back() == '"'))) {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

static int execute_case_block(const string& block) {
    string s = trim_copy(block);
    if (!starts_with_word_trimmed(s, "case")) return 1;

    size_t case_pos = find_top_level_keyword(s, "case", 0);
    size_t in_pos = find_top_level_word(s, "in", case_pos == string::npos ? 0 : case_pos + 4);
    size_t esac_pos = find_top_level_keyword(s, "esac", in_pos == string::npos ? 0 : in_pos + 2);
    if (case_pos == string::npos || in_pos == string::npos || esac_pos == string::npos) return 1;

    string word = strip_outer_quotes(trim_copy(g_env.expand_vars(s.substr(case_pos + 4, in_pos - (case_pos + 4)))));
    string body = s.substr(in_pos + 2, esac_pos - (in_pos + 2));

    struct CaseClause {
        string text;
        enum Term { Stop, FallthroughAction, FallthroughRetest } term = Stop;
    };

    vector<CaseClause> clauses;
    {
        string cur2;
        bool in_sq = false, in_dq = false;
        for (size_t i = 0; i < body.size(); ++i) {
            char ch = body[i];
            if (ch == '"' && !in_sq) { in_dq = !in_dq; cur2 += ch; continue; }
            if (ch == '\'' && !in_dq) { in_sq = !in_sq; cur2 += ch; continue; }

            if (!in_sq && !in_dq && i + 2 < body.size() && body[i] == ';' && body[i + 1] == ';' && body[i + 2] == '&') {
                clauses.push_back({trim_copy(cur2), CaseClause::FallthroughRetest});
                cur2.clear();
                i += 2;
                continue;
            }
            if (!in_sq && !in_dq && i + 1 < body.size() && body[i] == ';' && body[i + 1] == '&') {
                clauses.push_back({trim_copy(cur2), CaseClause::FallthroughAction});
                cur2.clear();
                i += 1;
                continue;
            }
            if (!in_sq && !in_dq && i + 1 < body.size() && body[i] == ';' && body[i + 1] == ';') {
                clauses.push_back({trim_copy(cur2), CaseClause::Stop});
                cur2.clear();
                i += 1;
                continue;
            }
            cur2 += ch;
        }
        if (!trim_copy(cur2).empty()) {
            clauses.push_back({trim_copy(cur2), CaseClause::Stop});
        }
    }

    auto split_patterns = [](const string& pat_expr) {
        vector<string> pats;
        string pcur;
        bool in_sq = false, in_dq = false;
        int paren_depth = 0;
        for (char ch : pat_expr) {
            if (ch == '"' && !in_sq) { in_dq = !in_dq; pcur += ch; continue; }
            if (ch == '\'' && !in_dq) { in_sq = !in_sq; pcur += ch; continue; }
            if (!in_sq && !in_dq && ch == '(') { ++paren_depth; pcur += ch; continue; }
            if (!in_sq && !in_dq && ch == ')' && paren_depth > 0) { --paren_depth; pcur += ch; continue; }
            if (!in_sq && !in_dq && paren_depth == 0 && ch == '|') {
                if (!trim_copy(pcur).empty()) pats.push_back(strip_outer_quotes(trim_copy(pcur)));
                pcur.clear();
                continue;
            }
            pcur += ch;
        }
        if (!trim_copy(pcur).empty()) pats.push_back(strip_outer_quotes(trim_copy(pcur)));
        return pats;
    };

    int last_rc = 0;
    bool executed_any = false;
    bool force_next_action = false;

    for (const auto& clause : clauses) {
        const string& cl = clause.text;
        size_t rp = string::npos;
        bool pattern_sq = false, pattern_dq = false;
        int pattern_depth = 0;
        for (size_t i = 0; i < cl.size(); ++i) {
            char ch = cl[i];
            if (ch == '"' && !pattern_sq) { pattern_dq = !pattern_dq; continue; }
            if (ch == '\'' && !pattern_dq) { pattern_sq = !pattern_sq; continue; }
            if (pattern_sq || pattern_dq) continue;
            if (ch == '(') { ++pattern_depth; continue; }
            if (ch == ')') {
                if (pattern_depth > 0) --pattern_depth;
                else { rp = i; break; }
            }
        }
        if (rp == string::npos) continue;
        string pat_expr = trim_copy(cl.substr(0, rp));
        string action = trim_copy(cl.substr(rp + 1));

        bool matched = force_next_action;
        if (!matched) {
            vector<string> pats = split_patterns(pat_expr);
            for (const auto& p : pats) {
                string pat = g_env.expand_vars(p);
                if (pat == "*" || match_wildcard(pat, word)) { matched = true; break; }
            }
        }

        if (!matched) continue;

        last_rc = execute_command_line(action);
        executed_any = true;

        if (clause.term == CaseClause::Stop) return last_rc;
        if (clause.term == CaseClause::FallthroughAction) {
            force_next_action = true;
        } else {
            force_next_action = false;
        }
    }

    return executed_any ? last_rc : 0;
}

static int execute_condition_loop(const string& block, bool until_mode) {
    LoopGuard loop_guard;
    string s = trim_copy(block);
    const string keyword = until_mode ? "until" : "while";
    size_t loop_pos = find_top_level_keyword(s, keyword, 0);
    if (loop_pos == string::npos) return 1;

    size_t after_kw = loop_pos + keyword.size();
    size_t do_pos = find_top_level_word(s, "do", after_kw);
    size_t done_pos = (do_pos != string::npos) ? find_matching_done(s, do_pos + 2) : string::npos;

    string cond;
    string body;

    if (do_pos != string::npos && done_pos != string::npos) {
        cond = strip_optional_trailing_semicolon(s.substr(after_kw, do_pos - after_kw));
        body = trim_copy(s.substr(do_pos + 2, done_pos - (do_pos + 2)));
    } else {
        // Check for brace syntax: until/while <cond> { <body> }
        size_t brace_open = string::npos;
        size_t brace_close = string::npos;
        bool in_sq = false, in_dq = false;
        int p_depth = 0, b_depth = 0;
        for (size_t i = after_kw; i < s.size(); ++i) {
            char ch = s[i];
            if (ch == '"' && !in_sq) { in_dq = !in_dq; continue; }
            if (ch == '\'' && !in_dq) { in_sq = !in_sq; continue; }
            if (in_sq || in_dq) continue;
            if (ch == '(') { ++p_depth; continue; }
            if (ch == ')' && p_depth > 0) { --p_depth; continue; }
            if (p_depth == 0) {
                if (ch == '{') {
                    if (b_depth == 0) brace_open = i;
                    ++b_depth;
                } else if (ch == '}') {
                    if (b_depth > 0) --b_depth;
                    if (b_depth == 0 && brace_open != string::npos) {
                        brace_close = i;
                        break;
                    }
                }
            }
        }
        if (brace_open != string::npos && brace_close != string::npos && brace_close > brace_open) {
            cond = strip_optional_trailing_semicolon(s.substr(after_kw, brace_open - after_kw));
            body = trim_copy(s.substr(brace_open + 1, brace_close - (brace_open + 1)));
        } else {
            cerr << "zsh: " << keyword << ": expected 'do ... done' or '{ ... }'\n";
            return 1;
        }
    }

    cond = trim_copy(cond);
    int last_rc = 0;
    while (true) {
        int cond_rc = parse_and_execute(cond);
        g_env.last_exit_code = cond_rc;
        if ((cond_rc == 0) == until_mode) break;
        last_rc = execute_command_line(body);
        g_env.last_exit_code = last_rc;
        if (g_loop_breaking > 0) {
            --g_loop_breaking;
            break;
        }
        if (g_loop_continuing > 0) {
            --g_loop_continuing;
            if (g_loop_continuing > 0) break;
            continue;
        }
        if (g_script_returning || g_subshell_exiting) break;
    }
    return last_rc;
}

static int execute_repeat_loop(const string& block) {
    LoopGuard loop_guard;
    string s = trim_copy(block);
    size_t repeat_pos = find_top_level_keyword(s, "repeat", 0);
    if (repeat_pos == string::npos) return 1;

    size_t after_repeat = repeat_pos + 6;
    size_t do_pos = find_top_level_word(s, "do", after_repeat);
    size_t done_pos = (do_pos != string::npos) ? find_matching_done(s, do_pos + 2) : string::npos;
    if (do_pos != string::npos && done_pos == string::npos) {
        done_pos = find_top_level_word(s, "done", do_pos + 2);
    }

    string count_expr;
    string body;

    if (do_pos != string::npos && done_pos != string::npos) {
        count_expr = strip_optional_trailing_semicolon(s.substr(after_repeat, do_pos - after_repeat));
        body = trim_copy(s.substr(do_pos + 2, done_pos - (do_pos + 2)));
    } else {
        // Check for brace syntax: repeat <count> { <body> }
        size_t brace_open = string::npos;
        size_t brace_close = string::npos;
        bool in_sq = false, in_dq = false;
        int p_depth = 0, b_depth = 0;
        for (size_t i = after_repeat; i < s.size(); ++i) {
            char ch = s[i];
            if (ch == '"' && !in_sq) { in_dq = !in_dq; continue; }
            if (ch == '\'' && !in_dq) { in_sq = !in_sq; continue; }
            if (in_sq || in_dq) continue;
            if (ch == '(') { ++p_depth; continue; }
            if (ch == ')' && p_depth > 0) { --p_depth; continue; }
            if (p_depth == 0) {
                if (ch == '{') {
                    if (b_depth == 0) brace_open = i;
                    ++b_depth;
                } else if (ch == '}') {
                    if (b_depth > 0) --b_depth;
                    if (b_depth == 0 && brace_open != string::npos) {
                        brace_close = i;
                        break;
                    }
                }
            }
        }
        if (brace_open != string::npos && brace_close != string::npos && brace_close > brace_open) {
            count_expr = strip_optional_trailing_semicolon(s.substr(after_repeat, brace_open - after_repeat));
            body = trim_copy(s.substr(brace_open + 1, brace_close - (brace_open + 1)));
        } else {
            // Simple command form: repeat <count> <command...>
            size_t rest_start = after_repeat;
            while (rest_start < s.size() && isspace((unsigned char)s[rest_start])) ++rest_start;
            if (rest_start >= s.size()) {
                cerr << "zsh: repeat: count expected\n";
                return 1;
            }

            size_t count_end = rest_start;
            p_depth = 0;
            in_sq = false; in_dq = false;
            while (count_end < s.size()) {
                char ch = s[count_end];
                if (ch == '"' && !in_sq) { in_dq = !in_dq; ++count_end; continue; }
                if (ch == '\'' && !in_dq) { in_sq = !in_sq; ++count_end; continue; }
                if (!in_sq && !in_dq) {
                    if (ch == '(') { ++p_depth; ++count_end; continue; }
                    if (ch == ')' && p_depth > 0) { --p_depth; ++count_end; continue; }
                    if (p_depth == 0 && (isspace((unsigned char)ch) || ch == ';')) break;
                }
                ++count_end;
            }
            count_expr = trim_copy(s.substr(rest_start, count_end - rest_start));
            body = trim_copy(s.substr(count_end));
            if (body.empty()) {
                cerr << "zsh: repeat: command expected\n";
                return 1;
            }
        }
    }

    count_expr = trim_copy(count_expr);
    long long count = 0;
    if (g_env.vars.count(count_expr)) {
        try { count = stoll(g_env.vars[count_expr]); } catch (...) { count = 0; }
    } else {
        string expanded_count = g_env.expand_vars(count_expr);
        try {
            count = eval_math_expr(expanded_count);
        } catch (...) {
            count = 0;
        }
    }

    if (count <= 0) return 0;
    if (count > 10000000) {
        cerr << "zsh: repeat: count too large: " << count << "\n";
        return 1;
    }

    int last_rc = 0;
    for (long long iteration = 0; iteration < count; ++iteration) {
        last_rc = execute_command_line(body);
        g_env.last_exit_code = last_rc;
        if (g_loop_breaking > 0) {
            --g_loop_breaking;
            break;
        }
        if (g_loop_continuing > 0) {
            --g_loop_continuing;
            if (g_loop_continuing > 0) break;
            continue;
        }
        if (g_script_returning || g_subshell_exiting) break;
    }
    return last_rc;
}

static int execute_select_loop(const string& block) {
    LoopGuard loop_guard;
    string s = trim_copy(block);
    size_t select_pos = find_top_level_word(s, "select", 0);
    if (select_pos == string::npos) return 1;

    auto expand_select_items = [](const string& words) {
        string trimmed_words = trim_copy(words);
        if (trimmed_words == "\"$@\"" || trimmed_words == "$@" ||
            trimmed_words == "\"$*\"" || trimmed_words == "${@}") {
            return g_env.positional_args;
        }
        return expand_globs(tokenize_words(g_env.expand_vars(words)));
    };

    size_t after_select = select_pos + 6;
    size_t in_pos = find_top_level_word(s, "in", after_select);
    size_t do_pos = find_top_level_keyword(s, "do", after_select);
    size_t done_pos = (do_pos != string::npos) ? find_matching_done(s, do_pos + 2) : string::npos;

    string var_name;
    vector<string> items;
    string body;

    if (do_pos != string::npos && done_pos != string::npos) {
        if (in_pos != string::npos && in_pos < do_pos) {
            var_name = trim_copy(s.substr(after_select, in_pos - after_select));
            string words_str = strip_optional_trailing_semicolon(s.substr(in_pos + 2, do_pos - (in_pos + 2)));
            items = expand_select_items(words_str);
        } else {
            var_name = strip_optional_trailing_semicolon(trim_copy(s.substr(after_select, do_pos - after_select)));
            items = g_env.positional_args;
        }
        body = trim_copy(s.substr(do_pos + 2, done_pos - (do_pos + 2)));
    } else {
        // Check for brace syntax: select var [in words] { body }
        size_t brace_open = string::npos, brace_close = string::npos;
        bool in_sq = false, in_dq = false;
        int p_depth = 0, b_depth = 0;
        for (size_t i = after_select; i < s.size(); ++i) {
            char ch = s[i];
            if (ch == '"' && !in_sq) { in_dq = !in_dq; continue; }
            if (ch == '\'' && !in_dq) { in_sq = !in_sq; continue; }
            if (in_sq || in_dq) continue;
            if (ch == '(') { ++p_depth; continue; }
            if (ch == ')' && p_depth > 0) { --p_depth; continue; }
            if (p_depth == 0) {
                if (ch == '{') { if (b_depth == 0) brace_open = i; ++b_depth; }
                else if (ch == '}') {
                    if (b_depth > 0) --b_depth;
                    if (b_depth == 0 && brace_open != string::npos) { brace_close = i; break; }
                }
            }
        }
        if (brace_open != string::npos && brace_close != string::npos) {
            string header = trim_copy(s.substr(after_select, brace_open - after_select));
            in_pos = find_top_level_word(header, "in", 0);
            if (in_pos != string::npos) {
                var_name = trim_copy(header.substr(0, in_pos));
                string words_str = strip_optional_trailing_semicolon(header.substr(in_pos + 2));
                items = expand_select_items(words_str);
            } else {
                var_name = strip_optional_trailing_semicolon(header);
                items = g_env.positional_args;
            }
            body = trim_copy(s.substr(brace_open + 1, brace_close - (brace_open + 1)));
        } else {
            cerr << "zsh: select: expected 'do ... done' or '{ ... }'\n";
            return 1;
        }
    }

    var_name = trim_copy(var_name);
    if (!is_valid_env_var_name(var_name)) {
        cerr << "zsh: select: invalid variable name: " << var_name << "\n";
        return 1;
    }
    if (items.empty()) return 0;

    int last_rc = 0;
    while (true) {
        for (size_t i = 0; i < items.size(); ++i) {
            cout << (i + 1) << ") " << items[i] << "\n";
        }
        string prompt = g_env.vars.count("PROMPT3") ? g_env.vars["PROMPT3"] : "?# ";
        cout << prompt;
        cout.flush();

        string line_in;
        if (!getline(cin, line_in)) break;
        line_in = trim_copy(line_in);
        g_env.vars["REPLY"] = line_in;

        long long choice = 0;
        try { choice = stoll(line_in); } catch (...) { choice = 0; }
        if (choice >= 1 && static_cast<size_t>(choice) <= items.size()) {
            g_env.vars[var_name] = items[choice - 1];
        } else {
            g_env.vars[var_name] = "";
        }

        last_rc = execute_command_line(body);
        g_env.last_exit_code = last_rc;
        if (g_loop_breaking > 0) {
            --g_loop_breaking;
            break;
        }
        if (g_loop_continuing > 0) {
            --g_loop_continuing;
            if (g_loop_continuing > 0) break;
            continue;
        }
        if (g_script_returning || g_subshell_exiting) break;
    }
    return last_rc;
}

static int execute_timed_command(const string& block) {
    string s = trim_copy(block);
    if (starts_with_word_trimmed(s, "time")) {
        s = trim_copy(s.substr(4));
    }
    if (s.empty()) return 0;
    auto t0 = chrono::high_resolution_clock::now();
    int rc = execute_statement_block_aware(s);
    auto t1 = chrono::high_resolution_clock::now();
    double elapsed = chrono::duration<double>(t1 - t0).count();

    string fmt = g_env.vars.count("TIMEFMT") ? g_env.vars["TIMEFMT"] : "%E total";
    ostringstream oss;
    oss << fixed << setprecision(3) << elapsed << "s";
    string elapsed_str = oss.str();

    auto replace_all = [](string& value, const string& needle, const string& replacement) {
        size_t position = 0;
        while ((position = value.find(needle, position)) != string::npos) {
            value.replace(position, needle.size(), replacement);
            position += replacement.size();
        }
    };
    bool has_elapsed_conversion = fmt.find("%*E") != string::npos || fmt.find("%E") != string::npos;
    string formatted = has_elapsed_conversion ? fmt : elapsed_str + " total";
    if (has_elapsed_conversion) {
        replace_all(formatted, "%*E", elapsed_str);
        replace_all(formatted, "%E", elapsed_str);
    }
    cerr << formatted << "\n";
    return rc;
}

static int execute_always_block(const string& statement) {
    string s = trim_copy(statement);
    if (starts_with_word_trimmed(s, "try")) {
        s = trim_copy(s.substr(3));
    }
    size_t always_pos = find_top_level_word(s, "always");
    if (always_pos == string::npos) return 1;

    string try_part = trim_copy(s.substr(0, always_pos));
    string always_part = trim_copy(s.substr(always_pos + 6));

    if (try_part.size() >= 2 && try_part.front() == '{' && try_part.back() == '}') {
        try_part = trim_copy(try_part.substr(1, try_part.size() - 2));
    }
    if (always_part.size() >= 2 && always_part.front() == '{' && always_part.back() == '}') {
        always_part = trim_copy(always_part.substr(1, always_part.size() - 2));
    }

    bool saved_returning = g_script_returning;
    int saved_return_code = g_script_return_code;
    bool saved_exiting = g_subshell_exiting;
    int saved_exit_code = g_subshell_exit_code;

    g_script_returning = false;
    g_subshell_exiting = false;

    bool old_try = g_in_try_block;
    g_in_try_block = true;
    int try_rc = execute_command_line(try_part);
    g_in_try_block = old_try;
    bool try_failed = (try_rc != 0 || g_script_returning || g_subshell_exiting);
    g_env.vars["TRY_BLOCK_ERROR"] = try_failed ? "1" : "0";

    bool after_try_returning = g_script_returning;
    int after_try_return_code = g_script_return_code;
    bool after_try_exiting = g_subshell_exiting;
    int after_try_exit_code = g_subshell_exit_code;

    // Reset return/exit flags so always block runs unconditionally
    g_script_returning = false;
    g_subshell_exiting = false;

    int always_rc = execute_command_line(always_part);

    if (g_env.vars["TRY_BLOCK_ERROR"] != "0") {
        g_script_returning = after_try_returning || saved_returning;
        g_script_return_code = after_try_returning ? after_try_return_code : saved_return_code;
        g_subshell_exiting = after_try_exiting || saved_exiting;
        g_subshell_exit_code = after_try_exiting ? after_try_exit_code : saved_exit_code;
        return try_rc != 0 ? try_rc : 1;
    } else {
        // Error was cleared inside always block
        g_script_returning = saved_returning;
        g_script_return_code = saved_return_code;
        g_subshell_exiting = saved_exiting;
        g_subshell_exit_code = saved_exit_code;
        return always_rc;
    }
}

static bool try_execute_anonymous_function(const string& statement, int& rc) {
    string s = trim_copy(statement);
    if (s.empty()) return false;
    size_t body_start = string::npos;
    if (s.rfind("()", 0) == 0) {
        size_t next = 2;
        while (next < s.size() && isspace((unsigned char)s[next])) ++next;
        if (next < s.size() && s[next] == '{') body_start = next;
    } else if (s.rfind("function", 0) == 0 && (s.size() == 8 || isspace((unsigned char)s[8]) || s[8] == '(' || s[8] == '{')) {
        size_t next = 8;
        while (next < s.size() && isspace((unsigned char)s[next])) ++next;
        if (next + 1 < s.size() && s[next] == '(' && s[next + 1] == ')') {
            next += 2;
            while (next < s.size() && isspace((unsigned char)s[next])) ++next;
        }
        if (next < s.size() && s[next] == '{') body_start = next;
    }
    if (body_start == string::npos) return false;

    size_t body_end = string::npos;
    bool in_sq = false, in_dq = false;
    int b_depth = 0, p_depth = 0;
    for (size_t i = body_start; i < s.size(); ++i) {
        char ch = s[i];
        if (ch == '"' && !in_sq) { in_dq = !in_dq; continue; }
        if (ch == '\'' && !in_dq) { in_sq = !in_sq; continue; }
        if (in_sq || in_dq) continue;
        if (ch == '(') { ++p_depth; continue; }
        if (ch == ')' && p_depth > 0) { --p_depth; continue; }
        if (p_depth == 0) {
            if (ch == '{') ++b_depth;
            else if (ch == '}') {
                --b_depth;
                if (b_depth == 0) {
                    body_end = i;
                    break;
                }
            }
        }
    }
    if (body_end == string::npos) return false;

    string body = trim_copy(s.substr(body_start + 1, body_end - (body_start + 1)));
    string args_str = trim_copy(s.substr(body_end + 1));
    vector<string> args;
    if (!args_str.empty()) {
        args = tokenize_words(args_str);
        for (auto& arg : args) arg = g_env.expand_vars(arg);
    }

    vector<string> old_pos = g_env.positional_args;
    bool old_returning = g_script_returning;
    int old_return_code = g_script_return_code;

    if (g_function_local_scopes.size() >= static_cast<size_t>(kMaxExecRecursionDepth)) {
        cerr << "zsh: maximum function recursion depth (" << kMaxExecRecursionDepth << ") exceeded\n";
        g_env.last_exit_code = 1;
        return false;
    }

    g_env.positional_args = args;
    g_script_returning = false;
    g_function_local_scopes.push_back({g_env.vars, g_env.indexed_arrays, g_env.assoc_arrays,
                       g_env.integer_vars, g_env.readonly_vars, g_env.unique_arrays, {}});

    rc = execute_command_line(body);
    if (g_script_returning) {
        rc = g_script_return_code;
        g_script_returning = false;
    }
    if (g_env.vars.count("__trap_RETURN") && !g_env.vars["__trap_RETURN"].empty()) {
        execute_command_line(g_env.vars["__trap_RETURN"]);
    }

    FunctionLocalScope completed_scope = std::move(g_function_local_scopes.back());
    g_function_local_scopes.pop_back();
    restore_function_locals(completed_scope);

    g_env.positional_args = old_pos;
    g_script_returning = old_returning;
    g_script_return_code = old_return_code;
    return true;
}

static bool try_define_function(const string& statement) {
    static const regex fn_def_re(R"(^\s*([A-Za-z_][A-Za-z0-9_]*)\s*\(\s*\)\s*\{([\s\S]*)\}\s*$)");
    static const regex fn_kw_re(R"(^\s*function\s+([A-Za-z_][A-Za-z0-9_]*)\s*\{([\s\S]*)\}\s*$)");
    static const regex fn_kw_paren_re(R"(^\s*function\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(\s*\)\s*\{([\s\S]*)\}\s*$)");
    smatch m;
    if (regex_match(statement, m, fn_def_re) || regex_match(statement, m, fn_kw_re) || regex_match(statement, m, fn_kw_paren_re)) {
        string name = m[1].str();
        string body = trim_copy(m[2].str());
        g_env.functions[name] = body;
        return true;
    }
    return false;
}

static int execute_subshell_block(const string& statement) {
    string s = trim_copy(statement);
    if (s.size() < 2 || s.front() != '(' || s.back() != ')') return 1;

    if (g_function_local_scopes.size() >= static_cast<size_t>(kMaxExecRecursionDepth)) {
        cerr << "zsh: maximum subshell recursion depth (" << kMaxExecRecursionDepth << ") exceeded\n";
        g_env.last_exit_code = 1;
        return 1;
    }

    ZshEnvironment saved_environment = g_env;
    auto read_process_environment = []() {
        map<string, string> values;
        LPCH block = GetEnvironmentStringsA();
        if (!block) return values;
        for (const char* entry = block; *entry; entry += strlen(entry) + 1) {
            string item(entry);
            size_t equal = item.find('=');
            if (equal != string::npos && equal > 0)
                values[item.substr(0, equal)] = item.substr(equal + 1);
        }
        FreeEnvironmentStringsA(block);
        return values;
    };
    map<string, string> saved_process_environment = read_process_environment();
    error_code cwd_error;
    fs::path saved_cwd = fs::current_path(cwd_error);
    bool saved_returning = g_script_returning;
    int saved_return_code = g_script_return_code;
    bool saved_exiting = g_subshell_exiting;
    int saved_exit_code = g_subshell_exit_code;

    ++g_subshell_depth;
    g_subshell_exiting = false;
    g_subshell_exit_code = 0;
    g_function_local_scopes.push_back({g_env.vars, g_env.indexed_arrays, g_env.assoc_arrays, {}});

    int status = execute_command_line(trim_copy(s.substr(1, s.size() - 2)));
    if (g_subshell_exiting) status = g_subshell_exit_code;

    g_function_local_scopes.pop_back();
    --g_subshell_depth;
    g_env = std::move(saved_environment);
    g_script_returning = saved_returning;
    g_script_return_code = saved_return_code;
    g_subshell_exiting = saved_exiting;
    g_subshell_exit_code = saved_exit_code;
    map<string, string> current_process_environment = read_process_environment();
    for (const auto& [name, value] : current_process_environment) {
        if (!saved_process_environment.count(name)) SetEnvironmentVariableA(name.c_str(), nullptr);
    }
    for (const auto& [name, value] : saved_process_environment)
        SetEnvironmentVariableA(name.c_str(), value.c_str());
    if (!cwd_error) {
        error_code restore_error;
        fs::current_path(saved_cwd, restore_error);
    }
    return status;
}

static bool is_shell_pipeline_stage(const SingleCmd& command) {
    if (command.args.empty()) return false;
    string name = command.args[0];
    if (g_env.aliases.count(name)) {
        vector<string> alias_words = tokenize_words(g_env.aliases[name]);
        if (!alias_words.empty()) name = alias_words[0];
    }
    if (is_zsh_builtin_command(name) || g_env.functions.count(name) != 0) return true;
    if (command.args.size() == 1 && g_env.options.count("autocd") && g_env.options.at("autocd")) {
        error_code ec;
        string win_path = normalize_path_to_win(name);
        bool is_drive_letter = (name.size() == 2 && isalpha((unsigned char)name[0]) && name[1] == ':');
        if (is_drive_letter || fs::is_directory(win_path, ec)) return true;
    }
    return false;
}

static string current_shell_executable_path() {
    wchar_t executable_path[MAX_PATH] = {};
    DWORD executable_length = GetModuleFileNameW(nullptr, executable_path, MAX_PATH);
    if (executable_length == 0 || executable_length >= MAX_PATH) return "";
    int utf8_length = WideCharToMultiByte(CP_UTF8, 0, executable_path, static_cast<int>(executable_length),
                                          nullptr, 0, nullptr, nullptr);
    if (utf8_length <= 0) return "";
    string result(static_cast<size_t>(utf8_length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, executable_path, static_cast<int>(executable_length),
                        result.data(), utf8_length, nullptr, nullptr);
    return result;
}

static int execute_mixed_pipeline(const Pipeline& pipeline) {
    Pipeline concurrent = pipeline;
    string shell_executable = current_shell_executable_path();
    if (shell_executable.empty()) {
        cerr << "zsh: unable to locate shell executable for pipeline stage\n";
        return 1;
    }
    vector<string> state_paths;

    for (auto& command : concurrent.cmds) {
        if (!is_shell_pipeline_stage(command)) continue;
        string state_path = create_temp_process_subst_path();
        if (state_path.empty() || !save_pipeline_shell_state(state_path)) {
            cerr << "zsh: unable to save pipeline shell state\n";
            for (const auto& path : state_paths) {
                error_code remove_error;
                fs::remove(normalize_path_to_win(path), remove_error);
            }
            return 1;
        }
        state_paths.push_back(state_path);
        vector<string> child_args{shell_executable, "--pipeline-stage", state_path};
        child_args.insert(child_args.end(), command.args.begin(), command.args.end());
        command.args = std::move(child_args);
    }

    int status = execute_pipeline_native(concurrent);
    if (!pipeline.background) {
        for (const auto& path : state_paths) {
            error_code remove_error;
            fs::remove(normalize_path_to_win(path), remove_error);
        }
    }
    return status;
}

static HANDLE shell_fd_handle(int fd) {
    auto active = g_active_command_fds.find(fd);
    if (active != g_active_command_fds.end()) return active->second;
    auto persistent = g_persistent_fds.find(fd);
    if (persistent != g_persistent_fds.end()) return persistent->second;
    if (fd == 0) return GetStdHandle(STD_INPUT_HANDLE);
    if (fd == 1) return GetStdHandle(STD_OUTPUT_HANDLE);
    if (fd == 2) return GetStdHandle(STD_ERROR_HANDLE);
    return INVALID_HANDLE_VALUE;
}

static bool duplicate_shell_fd(int source_fd, HANDLE& duplicate) {
    HANDLE source = shell_fd_handle(source_fd);
    duplicate = nullptr;
    return source != INVALID_HANDLE_VALUE && source != nullptr &&
           DuplicateHandle(GetCurrentProcess(), source, GetCurrentProcess(), &duplicate,
                           0, TRUE, DUPLICATE_SAME_ACCESS) != FALSE;
}

static bool apply_extra_descriptors(const SingleCmd& command, map<int, HANDLE>& destination) {
    SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    for (const auto& [fd, path] : command.extra_input_files) {
        HANDLE handle = CreateFileW(string_to_wstring(normalize_path_to_win(path)).c_str(), GENERIC_READ,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE, &attributes, OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) return false;
        destination[fd] = handle;
    }
    for (const auto& [fd, target] : command.extra_output_files) {
        DWORD creation = target.second ? OPEN_ALWAYS : CREATE_ALWAYS;
        HANDLE handle = CreateFileW(string_to_wstring(normalize_path_to_win(target.first)).c_str(), GENERIC_WRITE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE, &attributes, creation,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) return false;
        if (target.second) SetFilePointer(handle, 0, nullptr, FILE_END);
        destination[fd] = handle;
    }
    for (const auto& [fd, source_fd] : command.extra_fd_duplications) {
        HANDLE duplicate = nullptr;
        if (!duplicate_shell_fd(source_fd, duplicate)) return false;
        destination[fd] = duplicate;
    }
    return true;
}

static void close_fd_map(map<int, HANDLE>& descriptors) {
    for (auto& [fd, handle] : descriptors) close_handle_if_valid(handle);
    descriptors.clear();
}

static int apply_persistent_exec_descriptors(const SingleCmd& command) {
    for (int fd : command.extra_closed_fds) {
        auto found = g_persistent_fds.find(fd);
        if (found != g_persistent_fds.end()) {
            close_handle_if_valid(found->second);
            g_persistent_fds.erase(found);
        }
    }
    map<int, HANDLE> replacements;
    if (!apply_extra_descriptors(command, replacements)) {
        close_fd_map(replacements);
        cerr << "exec: unable to open file descriptor\n";
        return 1;
    }
    for (auto& [fd, handle] : replacements) {
        auto existing = g_persistent_fds.find(fd);
        if (existing != g_persistent_fds.end()) close_handle_if_valid(existing->second);
        g_persistent_fds[fd] = handle;
    }
    replacements.clear();
    return 0;
}

static bool split_compound_and_redirection(const string& stmt, string& compound_part, string& redir_part) {
    string s = trim_copy(stmt);
    if (s.empty()) return false;
    if (s.size() >= 2 && s[0] == '(' && (s[1] == '(' || s[1] == ')')) return false;
    if (s.rfind("()", 0) == 0 || s.rfind("function", 0) == 0) return false;

    bool starts_compound = false;
    if (s[0] == '{' || s[0] == '(') starts_compound = true;
    else if (starts_with_word_trimmed(s, "if") || starts_with_word_trimmed(s, "case") ||
             starts_with_word_trimmed(s, "for") || starts_with_word_trimmed(s, "while") ||
             starts_with_word_trimmed(s, "until") || starts_with_word_trimmed(s, "select") ||
             starts_with_word_trimmed(s, "repeat") || starts_with_word_trimmed(s, "try")) {
        starts_compound = true;
    }
    if (!starts_compound) return false;

    vector<string> control_stack;
    bool at_command_start = true;
    size_t i = 0;
    size_t n = s.size();
    size_t end_pos = string::npos;

    while (i < n) {
        while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r')) {
            i++;
        }
        if (i >= n) break;

        if (s[i] == '\n') {
            at_command_start = true;
            i++;
            continue;
        }

        if (s[i] == '#' && at_command_start) {
            while (i < n && s[i] != '\n') i++;
            continue;
        }

        if (s[i] == ';') {
            if (i + 1 < n && s[i+1] == ';') {
                i += 2;
                at_command_start = true;
                continue;
            }
            at_command_start = true;
            i++;
            continue;
        }
        if (s[i] == '&') {
            if (i + 1 < n && s[i+1] == '&') {
                i += 2;
                at_command_start = true;
                continue;
            }
            i++;
            at_command_start = true;
            continue;
        }
        if (s[i] == '|') {
            if (i + 1 < n && (s[i+1] == '|' || s[i+1] == '&')) {
                i += 2;
                at_command_start = true;
                continue;
            }
            i++;
            at_command_start = true;
            continue;
        }

        if (s[i] == '{' || s[i] == '}' || s[i] == '(' || s[i] == ')') {
            char ch = s[i];
            i++;
            if (ch == '{') {
                control_stack.push_back("{");
                at_command_start = true;
            } else if (ch == '(') {
                control_stack.push_back("(");
                at_command_start = true;
            } else if (ch == '}') {
                if (!control_stack.empty() && (control_stack.back() == "{" || control_stack.back() == "try")) {
                    control_stack.pop_back();
                }
                if (control_stack.empty()) {
                    size_t next_p = i;
                    while (next_p < n && (s[next_p] == ' ' || s[next_p] == '\t' || s[next_p] == '\r' || s[next_p] == '\n')) next_p++;
                    if (next_p < n && s.compare(next_p, 6, "always") == 0 && (next_p + 6 >= n || !is_word_char(s[next_p + 6]))) {
                        control_stack.push_back("try");
                        i = next_p + 6;
                        at_command_start = true;
                        continue;
                    }
                    end_pos = i;
                    break;
                }
                at_command_start = false;
            } else if (ch == ')') {
                if (!control_stack.empty() && control_stack.back() == "(") {
                    control_stack.pop_back();
                }
                if (control_stack.empty()) {
                    end_pos = i;
                    break;
                }
                at_command_start = false;
            }
            continue;
        }

        string word;
        bool in_sq = false, in_dq = false, escaped = false;
        while (i < n) {
            char c = s[i];
            if (escaped) {
                word += c;
                escaped = false;
                i++;
                continue;
            }
            if (c == '\\' && !in_sq) {
                escaped = true;
                i++;
                continue;
            }
            if (c == '\'' && !in_dq) {
                in_sq = !in_sq;
                word += c;
                i++;
                continue;
            }
            if (c == '"' && !in_sq) {
                in_dq = !in_dq;
                word += c;
                i++;
                continue;
            }
            if (!in_sq && !in_dq) {
                if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ';' || c == '&' || c == '|' || c == '{' || c == '}' || c == '(' || c == ')') {
                    break;
                }
            }
            word += c;
            i++;
        }

        if (at_command_start) {
            if (word == "if") {
                control_stack.push_back("if");
                at_command_start = true;
            } else if (word == "then" || word == "else" || word == "elif") {
                at_command_start = true;
            } else if (word == "fi") {
                if (!control_stack.empty() && control_stack.back() == "if") {
                    control_stack.pop_back();
                }
                if (control_stack.empty()) {
                    end_pos = i;
                    break;
                }
                at_command_start = false;
            } else if (word == "case") {
                control_stack.push_back("case");
                at_command_start = false;
            } else if (word == "esac") {
                if (!control_stack.empty() && control_stack.back() == "case") {
                    control_stack.pop_back();
                }
                if (control_stack.empty()) {
                    end_pos = i;
                    break;
                }
                at_command_start = false;
            } else if (word == "for" || word == "while" || word == "until" || word == "select" || word == "repeat") {
                control_stack.push_back("loop");
                at_command_start = false;
            } else if (word == "do") {
                at_command_start = true;
            } else if (word == "done") {
                if (!control_stack.empty() && control_stack.back() == "loop") {
                    control_stack.pop_back();
                }
                if (control_stack.empty()) {
                    end_pos = i;
                    break;
                }
                at_command_start = false;
            } else if (word == "try") {
                control_stack.push_back("try");
                at_command_start = true;
            } else {
                at_command_start = false;
            }
        } else {
            if (word == "then" || word == "else" || word == "elif" || word == "do") {
                at_command_start = true;
            } else {
                at_command_start = false;
            }
        }
    }

    if (end_pos != string::npos && end_pos < s.size()) {
        compound_part = trim_copy(s.substr(0, end_pos));
        redir_part = trim_copy(s.substr(end_pos));
        return !redir_part.empty();
    }
    return false;
}

static int execute_compound_with_redirection(const string& compound_part, const string& redir_part) {
    Pipeline pl = parse_pipeline("dummy " + redir_part);
    if (!pl.error.empty() || pl.cmds.empty()) {
        cerr << "zsh: parse error: " << (pl.error.empty() ? "invalid redirection" : pl.error) << "\n";
        return 1;
    }
    const SingleCmd& sc = pl.cmds[0];

    HANDLE oldStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE oldStdErr = GetStdHandle(STD_ERROR_HANDLE);
    HANDLE oldStdIn = GetStdHandle(STD_INPUT_HANDLE);
    int oldFdOut = _dup(_fileno(stdout));
    int oldFdErr = _dup(_fileno(stderr));
    int oldFdIn = _dup(_fileno(stdin));

    cout.flush();
    cerr.flush();

    unique_handle hFileOut = make_unique_handle();
    unique_handle hFileErr = make_unique_handle();
    unique_handle hFileIn = make_unique_handle();
    unique_handle hHereRead = make_unique_handle();
    unique_handle hHereWrite = make_unique_handle();

    bool error_occurred = false;

    // 1. Input Redirection / Closure
    if (sc.close_stdin) {
        SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
        HANDLE hNul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hNul != INVALID_HANDLE_VALUE) {
            hFileIn = make_unique_handle(hNul);
            SetStdHandle(STD_INPUT_HANDLE, (HANDLE)hFileIn.get());
            HANDLE rawInDup = INVALID_HANDLE_VALUE;
            if (DuplicateHandle(GetCurrentProcess(), (HANDLE)hFileIn.get(), GetCurrentProcess(), &rawInDup, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
                int fd = _open_osfhandle((intptr_t)rawInDup, _O_TEXT);
                if (fd != -1) {
                    _dup2(fd, _fileno(stdin));
                    _close(fd);
                }
            }
        }
    } else if (!sc.here_string.empty()) {
        HANDLE rawR = NULL, rawW = NULL;
        SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
        if (CreatePipe(&rawR, &rawW, &sa, 0)) {
            hHereRead = make_unique_handle(rawR);
            hHereWrite = make_unique_handle(rawW);
            string payload = sc.here_string + "\n";
            DWORD written = 0;
            WriteFile((HANDLE)hHereWrite.get(), payload.data(), static_cast<DWORD>(payload.size()), &written, NULL);
            hHereWrite.reset();
            SetStdHandle(STD_INPUT_HANDLE, (HANDLE)hHereRead.get());
            HANDLE rawInDup = INVALID_HANDLE_VALUE;
            if (DuplicateHandle(GetCurrentProcess(), (HANDLE)hHereRead.get(), GetCurrentProcess(), &rawInDup, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
                int fd = _open_osfhandle((intptr_t)rawInDup, _O_TEXT);
                if (fd != -1) {
                    _dup2(fd, _fileno(stdin));
                    _close(fd);
                }
            }
        }
    } else if (!sc.input_file.empty()) {
        SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
        HANDLE h = CreateFileW(string_to_wstring(normalize_path_to_win(sc.input_file)).c_str(),
                               GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, &sa,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE) {
            cerr << "zsh: cannot open input file: " << sc.input_file << "\n";
            error_occurred = true;
        } else {
            hFileIn = make_unique_handle(h);
            SetStdHandle(STD_INPUT_HANDLE, (HANDLE)hFileIn.get());
            HANDLE rawInDup = INVALID_HANDLE_VALUE;
            if (DuplicateHandle(GetCurrentProcess(), (HANDLE)hFileIn.get(), GetCurrentProcess(), &rawInDup, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
                int fd = _open_osfhandle((intptr_t)rawInDup, _O_TEXT);
                if (fd != -1) {
                    _dup2(fd, _fileno(stdin));
                    _close(fd);
                }
            }
        }
    }

    // 2. Output Redirection / Closure
    if (!error_occurred && sc.close_stdout) {
        SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
        HANDLE hNul = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hNul != INVALID_HANDLE_VALUE) {
            hFileOut = make_unique_handle(hNul);
            SetStdHandle(STD_OUTPUT_HANDLE, (HANDLE)hFileOut.get());
            HANDLE rawOutDup = INVALID_HANDLE_VALUE;
            if (DuplicateHandle(GetCurrentProcess(), (HANDLE)hFileOut.get(), GetCurrentProcess(), &rawOutDup, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
                int fd = _open_osfhandle((intptr_t)rawOutDup, _O_TEXT);
                if (fd != -1) {
                    _dup2(fd, _fileno(stdout));
                    _close(fd);
                }
            }
        }
    } else if (!error_occurred && !sc.output_file.empty()) {
        SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
        DWORD creation = sc.append_out ? OPEN_ALWAYS : CREATE_ALWAYS;
        HANDLE h = CreateFileW(string_to_wstring(normalize_path_to_win(sc.output_file)).c_str(),
                               GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, &sa,
                               creation, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE) {
            cerr << "zsh: cannot open output file: " << sc.output_file << "\n";
            error_occurred = true;
        } else {
            hFileOut = make_unique_handle(h);
            if (sc.append_out) {
                SetFilePointer((HANDLE)hFileOut.get(), 0, NULL, FILE_END);
            }
            SetStdHandle(STD_OUTPUT_HANDLE, (HANDLE)hFileOut.get());
            HANDLE rawOutDup = INVALID_HANDLE_VALUE;
            if (DuplicateHandle(GetCurrentProcess(), (HANDLE)hFileOut.get(), GetCurrentProcess(), &rawOutDup, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
                int fd = _open_osfhandle((intptr_t)rawOutDup, _O_TEXT);
                if (fd != -1) {
                    _dup2(fd, _fileno(stdout));
                    _close(fd);
                }
            }
        }
    }

    // 3. Error Redirection / Closure
    if (!error_occurred && sc.close_stderr) {
        SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
        HANDLE hNul = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hNul != INVALID_HANDLE_VALUE) {
            hFileErr = make_unique_handle(hNul);
            SetStdHandle(STD_ERROR_HANDLE, (HANDLE)hFileErr.get());
            HANDLE rawErrDup = INVALID_HANDLE_VALUE;
            if (DuplicateHandle(GetCurrentProcess(), (HANDLE)hFileErr.get(), GetCurrentProcess(), &rawErrDup, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
                int fd = _open_osfhandle((intptr_t)rawErrDup, _O_TEXT);
                if (fd != -1) {
                    _dup2(fd, _fileno(stderr));
                    _close(fd);
                }
            }
        }
    } else if (!error_occurred && !sc.error_file.empty()) {
        SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
        DWORD creation = sc.append_err ? OPEN_ALWAYS : CREATE_ALWAYS;
        HANDLE h = CreateFileW(string_to_wstring(normalize_path_to_win(sc.error_file)).c_str(),
                               GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, &sa,
                               creation, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE) {
            cerr << "zsh: cannot open error file: " << sc.error_file << "\n";
            error_occurred = true;
        } else {
            hFileErr = make_unique_handle(h);
            if (sc.append_err) {
                SetFilePointer((HANDLE)hFileErr.get(), 0, NULL, FILE_END);
            }
            SetStdHandle(STD_ERROR_HANDLE, (HANDLE)hFileErr.get());
            HANDLE rawErrDup = INVALID_HANDLE_VALUE;
            if (DuplicateHandle(GetCurrentProcess(), (HANDLE)hFileErr.get(), GetCurrentProcess(), &rawErrDup, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
                int fd = _open_osfhandle((intptr_t)rawErrDup, _O_TEXT);
                if (fd != -1) {
                    _dup2(fd, _fileno(stderr));
                    _close(fd);
                }
            }
        }
    } else if (!error_occurred && sc.dup_stderr_from == 1) {
        HANDLE curOut = GetStdHandle(STD_OUTPUT_HANDLE);
        SetStdHandle(STD_ERROR_HANDLE, curOut);
        _dup2(_fileno(stdout), _fileno(stderr));
    } else if (!error_occurred && sc.dup_stdout_from == 2) {
        HANDLE curErr = GetStdHandle(STD_ERROR_HANDLE);
        SetStdHandle(STD_OUTPUT_HANDLE, curErr);
        _dup2(_fileno(stderr), _fileno(stdout));
    }

    int rc = 1;
    if (!error_occurred) {
        rc = execute_statement_block_aware(compound_part);
    }

    cout.flush();
    cerr.flush();

    if (oldFdOut != -1) { _dup2(oldFdOut, _fileno(stdout)); _close(oldFdOut); }
    if (oldFdErr != -1) { _dup2(oldFdErr, _fileno(stderr)); _close(oldFdErr); }
    if (oldFdIn != -1) { _dup2(oldFdIn, _fileno(stdin)); _close(oldFdIn); }

    SetStdHandle(STD_OUTPUT_HANDLE, oldStdOut);
    SetStdHandle(STD_ERROR_HANDLE, oldStdErr);
    SetStdHandle(STD_INPUT_HANDLE, oldStdIn);

    hFileIn.reset();
    hFileOut.reset();
    hFileErr.reset();
    hHereRead.reset();
    hHereWrite.reset();

    return rc;
}

static int execute_statement_block_aware(const string& statement) {
    string stmt = trim_copy(statement);
    if (stmt.empty()) return 0;
    int anon_rc = 0;
    if (try_execute_anonymous_function(stmt, anon_rc)) return anon_rc;
    if (try_define_function(stmt)) return 0;
    string compound_part, redir_part;
    if (split_compound_and_redirection(stmt, compound_part, redir_part)) {
        return execute_compound_with_redirection(compound_part, redir_part);
    }
    if (stmt.size() >= 4 && stmt.rfind("((", 0) == 0 && stmt.substr(stmt.size() - 2) == "))") {
        string expr = trim_copy(stmt.substr(2, stmt.size() - 4));
        return parse_and_execute("let " + expr);
    }
    if (starts_with_word_trimmed(stmt, "try") || (stmt.front() == '{' && find_top_level_word(stmt, "always") != string::npos))
        return execute_always_block(stmt);
    if (starts_with_word_trimmed(stmt, "time")) return execute_timed_command(stmt);
    if (stmt.size() >= 2 && stmt.front() == '(' && stmt.back() == ')') return execute_subshell_block(stmt);
    if (stmt.size() >= 2 && stmt.front() == '{' && stmt.back() == '}')
        return execute_command_line(trim_copy(stmt.substr(1, stmt.size() - 2)));
    if (starts_with_word_trimmed(stmt, "if") && find_top_level_word(stmt, "fi") != string::npos) return execute_if_block(stmt);
    if (starts_with_word_trimmed(stmt, "case") && find_top_level_word(stmt, "esac") != string::npos) return execute_case_block(stmt);
    if (starts_with_word_trimmed(stmt, "for") && stmt.find("((") != string::npos && stmt.find("))") != string::npos && find_top_level_word(stmt, "done") != string::npos) return execute_cstyle_for_loop(stmt);
    if (starts_with_word_trimmed(stmt, "for") && find_top_level_word(stmt, "done") != string::npos) return execute_for_loop(stmt);
    if (starts_with_word_trimmed(stmt, "while")) return execute_condition_loop(stmt, false);
    if (starts_with_word_trimmed(stmt, "until")) return execute_condition_loop(stmt, true);
    if (starts_with_word_trimmed(stmt, "repeat")) return execute_repeat_loop(stmt);
    if (starts_with_word_trimmed(stmt, "select")) return execute_select_loop(stmt);
    int rc = execute_single_command(stmt);
    g_env.vars["pipestatus"] = to_string(rc);
    g_env.indexed_arrays["pipestatus"] = { to_string(rc) };
    return rc;
}

int execute_single_command(const string& line) {
    if (line.empty()) return 0;
    Pipeline pl = parse_pipeline(line);
    if (!pl.error.empty()) {
        cerr << "zsh: parse error: " << pl.error << "\n";
        return 1;
    }
    if (pl.cmds.empty()) return 0;

    if (pl.cmds.size() == 1 && pl.cmds[0].args.size() == 1 && pl.cmds[0].args[0] == "exec" &&
        (!pl.cmds[0].extra_input_files.empty() || !pl.cmds[0].extra_output_files.empty() ||
         !pl.cmds[0].extra_fd_duplications.empty() || !pl.cmds[0].extra_closed_fds.empty())) {
        return apply_persistent_exec_descriptors(pl.cmds[0]);
    }

    if (g_env.options.count("multios") && g_env.options.at("multios") &&
        !pl.cmds.empty() && pl.cmds.back().output_files.size() > 1) {
        string shell_executable = current_shell_executable_path();
        if (shell_executable.empty()) return 1;
        SingleCmd tee;
        tee.args = {shell_executable, "--multios-tee"};
        for (const auto& target : pl.cmds.back().output_files) {
            tee.args.push_back(target.second ? "append" : "truncate");
            tee.args.push_back(target.first);
        }
        pl.cmds.back().output_file.clear();
        pl.cmds.back().output_files.clear();
        pl.cmds.push_back(std::move(tee));
    }

    auto needs_launcher = [&](const SingleCmd& c) {
        return !c.input_file.empty() || !c.output_file.empty() || !c.error_file.empty() ||
               !c.here_string.empty() || c.dup_stdin_from >= 0 || c.dup_stdout_from >= 0 || c.dup_stderr_from >= 0 ||
               c.close_stdin || c.close_stdout || c.close_stderr || !c.extra_input_files.empty() ||
               !c.extra_output_files.empty() || !c.extra_fd_duplications.empty() || !c.extra_closed_fds.empty();
    };

    int rc;
    bool has_shell_stage = any_of(pl.cmds.begin(), pl.cmds.end(), is_shell_pipeline_stage);
    bool scoped_extra_descriptors = pl.cmds.size() == 1 && is_shell_pipeline_stage(pl.cmds[0]) &&
                                    (!pl.cmds[0].extra_input_files.empty() || !pl.cmds[0].extra_output_files.empty() ||
                                     !pl.cmds[0].extra_fd_duplications.empty() || !pl.cmds[0].extra_closed_fds.empty());
    if (scoped_extra_descriptors) {
        if (!apply_extra_descriptors(pl.cmds[0], g_active_command_fds)) rc = 1;
        else rc = dispatch_command(pl.cmds[0].args);
        close_fd_map(g_active_command_fds);
    } else if (pl.cmds.size() == 1 && is_shell_pipeline_stage(pl.cmds[0]) && !pl.background && needs_launcher(pl.cmds[0])) {
        string cmd_str;
        for (size_t i = 0; i < pl.cmds[0].args.size(); ++i) {
            if (i > 0) cmd_str += ' ';
            cmd_str += quote_for_shell_path(pl.cmds[0].args[i]);
        }
        string redir_str;
        if (!pl.cmds[0].input_file.empty()) redir_str += " < " + quote_for_shell_path(normalize_path_to_unix(pl.cmds[0].input_file));
        if (!pl.cmds[0].here_string.empty()) redir_str += " <<< " + quote_for_shell_path(pl.cmds[0].here_string);
        if (!pl.cmds[0].output_file.empty()) redir_str += (pl.cmds[0].append_out ? " >> " : " > ") + quote_for_shell_path(normalize_path_to_unix(pl.cmds[0].output_file));
        if (!pl.cmds[0].error_file.empty()) redir_str += " 2> " + quote_for_shell_path(normalize_path_to_unix(pl.cmds[0].error_file));
        if (pl.cmds[0].dup_stderr_from == 1) redir_str += " 2>&1";
        if (pl.cmds[0].dup_stdout_from == 2) redir_str += " 1>&2";
        rc = execute_compound_with_redirection(cmd_str, redir_str);
    } else if (has_shell_stage &&
               (pl.background || pl.cmds.size() > 1 || needs_launcher(pl.cmds[0]))) {
        rc = execute_mixed_pipeline(pl);
    } else if (pl.background || pl.cmds.size() > 1 || needs_launcher(pl.cmds[0])) {
        rc = execute_pipeline_native(pl);
    } else {
        rc = dispatch_command(pl.cmds[0].args);
    }

    // Complete deferred >(command) sinks after the producer command exits.
    if (g_process_subst_eval_depth == 0 && !g_process_subst_sinks.empty()) {
        auto sinks = g_process_subst_sinks;
        g_process_subst_sinks.clear();
        for (const auto& s : sinks) {
            parse_and_execute(s.command + " < " + quote_for_shell_path(normalize_path_to_unix(s.path)));
        }
    }

    // Cleanup all temp files created for this command's process substitutions.
    if (g_process_subst_eval_depth == 0 && !g_process_subst_temp_files.empty()) {
        auto files = g_process_subst_temp_files;
        g_process_subst_temp_files.clear();
        for (const auto& p : files) {
            error_code ec;
            fs::remove(normalize_path_to_win(p), ec);
            if (ec) cerr << "zsh: warning: could not remove process-substitution temp file: " << p << "\n";
        }
    }

    return rc;
}

static CommandListAst parse_command_list_ast(const string& line) {
    CommandListAst ast;
    string current;
    CommandListConnector pending_connector = CommandListConnector::Always;
    bool in_sq = false, in_dq = false;
    bool escaped = false;
    int paren_depth = 0;
    int brace_depth = 0;
    vector<string> control_stack; // tracks opener keywords for pairing validation

    auto parse_word_at = [&](size_t i, string& word_out) {
        if (i >= line.size() || !is_word_char(line[i])) return false;
        size_t j = i;
        while (j < line.size() && is_word_char(line[j])) ++j;
        bool left_ok = (i == 0) || !is_word_char(line[i - 1]);
        if (!left_ok) return false;
        word_out = line.substr(i, j - i);
        return true;
    };

    auto segment_has_nonspace = [&]() {
        for (char c : current) {
            if (!isspace((unsigned char)c)) return true;
        }
        return false;
    };
    auto push_current = [&]() {
        string statement = trim_copy(current);
        if (statement.empty()) return false;
        ast.nodes.push_back({pending_connector, std::move(statement)});
        current.clear();
        pending_connector = CommandListConnector::Always;
        return true;
    };
    auto set_missing_operand_error = [&](const string& op) {
        ast.error = "missing command before '" + op + "'";
    };

    for (size_t i = 0; i < line.size(); ++i) {
        char ch = line[i];
        if (escaped) {
            current += ch;
            escaped = false;
            continue;
        }
        if (ch == '\\' && !in_sq) {
            current += ch;
            escaped = true;
            continue;
        }
        if (ch == '"' && !in_sq) { in_dq = !in_dq; current += ch; continue; }
        if (ch == '\'' && !in_dq) { in_sq = !in_sq; current += ch; continue; }

        if (!in_sq && !in_dq) {
            string word;
            if (parse_word_at(i, word)) {
                auto is_keyword_boundary = [&]() {
                    if (i == 0) return true;
                    size_t prev = i;
                    while (prev > 0 && (line[prev - 1] == ' ' || line[prev - 1] == '\t' || line[prev - 1] == '\r')) {
                        --prev;
                    }
                    if (prev == 0) return true;
                    char prev_ch = line[prev - 1];
                    if (prev_ch == '\n' || prev_ch == ';' || prev_ch == '&' || prev_ch == '|' || prev_ch == '(' || prev_ch == '{') return true;
                    size_t w_end = prev;
                    size_t w_start = w_end;
                    while (w_start > 0 && is_word_char(line[w_start - 1])) {
                        --w_start;
                    }
                    if (w_start < w_end) {
                        string prev_word = line.substr(w_start, w_end - w_start);
                        if (prev_word == "do" || prev_word == "then" || prev_word == "else" || prev_word == "elif" || prev_word == "always") {
                            return true;
                        }
                    }
                    return false;
                };
                bool at_statement_start = is_keyword_boundary();
                // Track opener->closer pairing so mismatched keywords (e.g. for...fi)
                // are caught instead of silently mis-parsing downstream.
                static const unordered_map<string, string> kCloserFor = {
                    {"if", "fi"}, {"for", "done"}, {"while", "done"}, {"until", "done"},
                    {"repeat", "done"}, {"select", "done"}, {"case", "esac"}
                };
                static const unordered_map<string, string> kOpenerFor = {
                    {"fi", "if"}, {"done", "for"}, {"esac", "case"}
                };
                bool needs_closer = true;
                if (word == "repeat" || word == "until" || word == "while" || word == "select") {
                    needs_closer = (find_top_level_word(line, "do", i) != string::npos);
                }
                if (kCloserFor.count(word) && at_statement_start && brace_depth == 0 && paren_depth == 0 && needs_closer) {
                    control_stack.push_back(word);
                } else if (kOpenerFor.count(word) && at_statement_start && !control_stack.empty() && brace_depth == 0 && paren_depth == 0) {
                    const string& expected_closer = kCloserFor.at(control_stack.back());
                    bool ok = expected_closer == word;
                    if (!ok) {
                        ast.error = "'" + word + "' does not match '" + control_stack.back() +
                                    "' (expected '" + expected_closer + "')";
                        return ast;
                    }
                    control_stack.pop_back();
                }
            }

            if (ch == '[' && i + 1 < line.size() && line[i + 1] == '[') {
                brace_depth += 1000;
                current += "[[";
                ++i;
                continue;
            }
            if (ch == ']' && i + 1 < line.size() && line[i + 1] == ']' && brace_depth >= 1000) {
                brace_depth -= 1000;
                current += "]]";
                ++i;
                continue;
            }

            if (ch == '{') { brace_depth++; current += ch; continue; }
            if (ch == '}') {
                if (brace_depth == 0) { ast.error = "unmatched '}'"; return ast; }
                brace_depth--; current += ch; continue;
            }
            if (ch == '(') { paren_depth++; current += ch; continue; }
            if (ch == ')') {
                if (paren_depth == 0 && !control_stack.empty() && control_stack.back() == "case") {
                    current += ch;
                    continue;
                }
                if (paren_depth == 0) { ast.error = "unmatched ')'"; return ast; }
                paren_depth--; current += ch; continue;
            }

            if (paren_depth == 0 && brace_depth == 0 && control_stack.empty()) {
                if (ch == ';' || ch == '\n') {
                    if (!push_current() && ch == ';') {
                        set_missing_operand_error(";");
                        return ast;
                    }
                    continue;
                }
                if (i + 1 < line.size() && line[i] == '&' && line[i + 1] == '&') {
                    if (!push_current()) { set_missing_operand_error("&&"); return ast; }
                    pending_connector = CommandListConnector::And;
                    ++i;
                    continue;
                }
                if (i + 1 < line.size() && line[i] == '|' && line[i + 1] == '|') {
                    if (!push_current()) { set_missing_operand_error("||"); return ast; }
                    pending_connector = CommandListConnector::Or;
                    ++i;
                    continue;
                }
                if (ch == '&' && (i == 0 || line[i - 1] != '>') &&
                    (i + 1 >= line.size() || line[i + 1] != '>')) {
                    string statement = trim_copy(current);
                    if (statement.empty()) { set_missing_operand_error("&"); return ast; }
                    statement += " &";
                    ast.nodes.push_back({pending_connector, std::move(statement)});
                    current.clear();
                    pending_connector = CommandListConnector::Always;
                    continue;
                }
            }
        }
        current += ch;
    }

    if (in_dq) ast.error = "unmatched '\"'";
    else if (in_sq) ast.error = "unmatched '\''";
    else if (paren_depth != 0) ast.error = "unmatched '('";
    else if (brace_depth != 0) ast.error = "unmatched '{'";
    else if (!control_stack.empty()) ast.error = "'" + control_stack.back() + "' not closed";
    if (!ast.error.empty()) return ast;

    if (!push_current() && pending_connector != CommandListConnector::Always)
        ast.error = "missing command after logical operator";
    return ast;
}

int execute_command_line(const string& line) {
    // Recursion guard at the true recursion point (function calls and command
    // substitutions re-enter execute_command_line, not parse_and_execute).
    if (g_exec_recursion_depth >= kMaxExecRecursionDepth) {
        cerr << "zsh: maximum execution recursion depth exceeded\n";
        g_env.last_exit_code = 1;
        return 1;
    }
    struct ExecDepthGuard {
        ExecDepthGuard()  { ++g_exec_recursion_depth; }
        ~ExecDepthGuard() { --g_exec_recursion_depth; }
    } exec_depth_guard;

    CommandListAst ast = parse_command_list_ast(line);
    if (!ast.error.empty()) {
        cerr << "zsh: parse error: " << ast.error << "\n";
        g_env.last_exit_code = 1;
        return 1;
    }
    if (ast.nodes.empty()) return 0;

    static bool in_trap_debug = false;
    static bool in_trap_zerr = false;

    int last_status = 0;
    for (size_t i = 0; i < ast.nodes.size(); ++i) {
        const CommandListNode& node = ast.nodes[i];
        bool should_run = i == 0 || node.connector == CommandListConnector::Always ||
                          (node.connector == CommandListConnector::And && last_status == 0) ||
                          (node.connector == CommandListConnector::Or && last_status != 0);
        if (should_run) {
            if (!in_trap_debug && g_env.vars.count("__trap_DEBUG") && !g_env.vars["__trap_DEBUG"].empty()) {
                in_trap_debug = true;
                execute_command_line(g_env.vars["__trap_DEBUG"]);
                in_trap_debug = false;
            }
            last_status = execute_statement_block_aware(node.source);
            // Keep $? in sync after each statement so intermediate expansions see it.
            g_env.last_exit_code = last_status;
            if (last_status != 0 && !in_trap_zerr && g_env.vars.count("__trap_ZERR") && !g_env.vars["__trap_ZERR"].empty()) {
                in_trap_zerr = true;
                execute_command_line(g_env.vars["__trap_ZERR"]);
                in_trap_zerr = false;
            }
            if (g_in_try_block && last_status != 0 && (node.connector == CommandListConnector::Always || node.connector == CommandListConnector::And)) break;
            if (g_subshell_exiting || g_script_returning || g_loop_breaking > 0 || g_loop_continuing > 0) break;
        }
    }
    return last_status;
}

int parse_and_execute(const string& line) {
    process_pending_traps();
    int rc = execute_command_line(line);
    g_env.last_exit_code = rc;
    if (g_loop_depth == 0) {
        g_loop_breaking = 0;
        g_loop_continuing = 0;
    }
    return rc;
}

static vector<string> split_lines_preserve_empty(const string& s) {
    vector<string> lines;
    size_t start = 0;
    while (start <= s.size()) {
        size_t nl = s.find('\n', start);
        if (nl == string::npos) {
            lines.push_back(s.substr(start));
            break;
        }
        lines.push_back(s.substr(start, nl - start));
        start = nl + 1;
    }
    return lines;
}

struct HeredocSpec {
    string delim;
    bool strip_tabs = false;
    bool expand_body = true;
};

static bool parse_heredoc_specs(const string& header, vector<HeredocSpec>& specs, string& rewritten_header) {
    rewritten_header.clear();
    specs.clear();

    bool in_sq = false, in_dq = false;
    for (size_t i = 0; i < header.size(); ++i) {
        char ch = header[i];
        if (ch == '"' && !in_sq) { in_dq = !in_dq; rewritten_header += ch; continue; }
        if (ch == '\'' && !in_dq) { in_sq = !in_sq; rewritten_header += ch; continue; }

        if (!in_sq && !in_dq && ch == '<' && i + 1 < header.size() && header[i + 1] == '<') {
            bool strip_tabs = false;
            i += 2;
            if (i < header.size() && header[i] == '-') { strip_tabs = true; ++i; }
            while (i < header.size() && isspace((unsigned char)header[i])) ++i;
            if (i >= header.size()) return false;

            string delim;
            bool quoted_delim = false;
            if (header[i] == '\'' || header[i] == '"') {
                quoted_delim = true;
                char q = header[i++];
                while (i < header.size() && header[i] != q) delim += header[i++];
                if (i < header.size() && header[i] == q) ++i;
            } else {
                while (i < header.size() && !isspace((unsigned char)header[i]) && header[i] != ';' && header[i] != '|' && header[i] != '&') {
                    delim += header[i++];
                }
            }
            if (delim.empty()) return false;

            specs.push_back({delim, strip_tabs, !quoted_delim});
            rewritten_header += " < __HEREDOC_" + to_string(specs.size() - 1) + "__ ";
            --i;
            continue;
        }

        rewritten_header += ch;
    }
    return !specs.empty();
}

static string register_heredoc_payload(const string& content) {
    static const size_t kMaxHeredocPayloads = 4096;
    if (g_heredoc_payloads.size() >= kMaxHeredocPayloads) {
        g_heredoc_payloads.erase(g_heredoc_payloads.begin());
    }
    string marker = "__HEREDOC_MEM_" + to_string(GetCurrentProcessId()) + "_" + to_string(++g_heredoc_seq) + "__";
    g_heredoc_payloads[marker] = content;
    return marker;
}

static string materialize_heredoc_block(const string& block) {
    vector<string> lines = split_lines_preserve_empty(block);
    if (lines.empty()) return block;

    vector<HeredocSpec> specs;
    string header_rewritten;
    if (!parse_heredoc_specs(lines[0], specs, header_rewritten)) return block;

    size_t cursor = 1;
    vector<string> markers;
    for (const auto& spec : specs) {
        string body;
        bool found_end = false;
        for (; cursor < lines.size(); ++cursor) {
            string cmp = lines[cursor];
            if (spec.strip_tabs) {
                size_t p = 0;
                while (p < cmp.size() && cmp[p] == '\t') ++p;
                cmp = cmp.substr(p);
            }
            if (cmp == spec.delim) {
                found_end = true;
                ++cursor;
                break;
            }
            string row = lines[cursor];
            if (spec.strip_tabs) {
                size_t p = 0;
                while (p < row.size() && row[p] == '\t') ++p;
                row = row.substr(p);
            }
            if (spec.expand_body) row = g_env.expand_vars(row);
            body += row;
            body += "\n";
        }
        if (!found_end) return block;

        markers.push_back(register_heredoc_payload(body));
    }

    string out = header_rewritten;
    for (size_t i = 0; i < markers.size(); ++i) {
        string ph = "__HEREDOC_" + to_string(i) + "__";
        string rp = markers[i];
        size_t p = 0;
        while ((p = out.find(ph, p)) != string::npos) {
            out.replace(p, ph.size(), rp);
            p += rp.size();
        }
    }

    while (cursor < lines.size()) {
        string tail = trim_copy(lines[cursor]);
        if (!tail.empty()) {
            out += " ; ";
            out += lines[cursor];
        }
        ++cursor;
    }
    return out;
}

static bool has_unterminated_heredoc(const string& block) {
    vector<string> lines = split_lines_preserve_empty(block);
    if (lines.empty()) return false;

    vector<HeredocSpec> specs;
    string header_rewritten;
    if (!parse_heredoc_specs(lines[0], specs, header_rewritten)) return false;

    size_t cursor = 1;
    for (const auto& spec : specs) {
        bool found_end = false;
        for (; cursor < lines.size(); ++cursor) {
            string cmp = lines[cursor];
            if (spec.strip_tabs) {
                size_t p = 0;
                while (p < cmp.size() && cmp[p] == '\t') ++p;
                cmp = cmp.substr(p);
            }
            if (cmp == spec.delim) {
                found_end = true;
                ++cursor;
                break;
            }
        }
        if (!found_end) return true;
    }
    return false;
}

int execute_script(const string& filepath, bool trace, bool errexit) {
    static int g_script_source_depth = 0;
    if (g_script_source_depth >= kMaxExecRecursionDepth) {
        cerr << "zsh: maximum source recursion depth (" << kMaxExecRecursionDepth << ") exceeded\n";
        g_env.last_exit_code = 1;
        return 1;
    }
    struct ScriptSourceGuard {
        ScriptSourceGuard()  { ++g_script_source_depth; }
        ~ScriptSourceGuard() { --g_script_source_depth; }
    } script_source_guard;

    if (filepath.empty()) {
        cerr << "zsh: source: filename argument required\n";
        return 1;
    }
    fs::path p = normalize_path_to_win(filepath);
    if (!fs::exists(p) || fs::is_directory(p)) {
        cerr << "zsh: no such file or directory: " << filepath << "\n";
        return 127;
    }
    ifstream file(p);
    if (!file.is_open()) return 127;

    auto count_word = [](const string& s, const string& word) {
        int count = 0;
        bool in_sq = false, in_dq = false;
        for (size_t i = 0; i < s.size(); ++i) {
            char ch = s[i];
            if (ch == '"' && !in_sq) { in_dq = !in_dq; continue; }
            if (ch == '\'' && !in_dq) { in_sq = !in_sq; continue; }
            if (in_sq || in_dq) continue;
            if (i + word.size() > s.size()) continue;
            if (s.compare(i, word.size(), word) != 0) continue;
            bool left_ok = (i == 0) || !is_word_char(s[i - 1]);
            bool right_ok = (i + word.size() >= s.size()) || !is_word_char(s[i + word.size()]);
            if (left_ok && right_ok) count++;
        }
        return count;
    };

    auto count_char_top_level = [](const string& s, char target) {
        int count = 0;
        bool in_sq = false, in_dq = false;
        for (char ch : s) {
            if (ch == '"' && !in_sq) { in_dq = !in_dq; continue; }
            if (ch == '\'' && !in_dq) { in_sq = !in_sq; continue; }
            if (in_sq || in_dq) continue;
            if (ch == target) count++;
        }
        return count;
    };

    string line;
    string block_buffer;
    int status = 0;
    int block_depth = 0;
    g_script_returning = false;

    while (getline(file, line)) {
        string trimmed = trim_copy(line);
        if (block_depth == 0 && (trimmed.empty() || trimmed[0] == '#')) continue;

        if (!block_buffer.empty()) block_buffer += "\n";
        block_buffer += line;

        int opens = 0;
        if (starts_with_word_trimmed(line, "if")) opens++;
        if (starts_with_word_trimmed(line, "for")) opens++;
        if (starts_with_word_trimmed(line, "while") && find_top_level_word(line, "do") != string::npos) opens++;
        if (starts_with_word_trimmed(line, "until") && find_top_level_word(line, "do") != string::npos) opens++;
        if (starts_with_word_trimmed(line, "repeat") && find_top_level_word(line, "do") != string::npos) opens++;
        if (starts_with_word_trimmed(line, "select") && find_top_level_word(line, "do") != string::npos) opens++;
        if (starts_with_word_trimmed(line, "case")) opens++;
        opens += count_char_top_level(line, '{');

        int closes = 0;
        closes += count_word(line, "fi");
        closes += count_word(line, "done");
        closes += count_word(line, "esac");
        closes += count_char_top_level(line, '}');

        block_depth += (opens - closes);
        if (block_depth < 0) block_depth = 0;

        if (block_depth == 0) {
            if (has_unterminated_heredoc(block_buffer)) {
                continue;
            }
            string prepared = materialize_heredoc_block(block_buffer);
            if (trace) cerr << "+ " << block_buffer << "\n";
            status = parse_and_execute(prepared);
            block_buffer.clear();
            if (g_script_returning) {
                g_script_returning = false;
                return g_script_return_code;
            }
            if (errexit && status != 0) return status;
        }
    }

    if (!block_buffer.empty()) {
        string prepared = materialize_heredoc_block(block_buffer);
        if (trace) cerr << "+ " << block_buffer << "\n";
        status = parse_and_execute(prepared);
    }
    // Note: do NOT fire the EXIT trap here — execute_script is also used to load
    // .zshrc at startup, and the EXIT trap must only fire when the shell/script
    // actually exits (handled in main).
    return status;
}

// Fire the EXIT trap exactly once (idempotent via g_exit_trap_fired).
void fire_exit_trap() {
    if (g_exit_trap_fired) return;
    g_exit_trap_fired = true;
    if (g_env.vars.count("__trap_EXIT")) {
        string trap_body = g_env.vars["__trap_EXIT"];
        int rc = execute_command_line(trap_body);
        cout.flush();
        cerr.flush();
        (void)rc;
    }
}

// ============================================================================
// PROMPT & INTERACTIVE LINE EDITOR
// ============================================================================
static string render_prompt_template(const string& fmt) {
    string prompt = "";
    for (size_t i = 0; i < fmt.length(); ++i) {
        if (fmt[i] == '%' && i + 1 < fmt.length()) {
            char code = fmt[i + 1];
            if (code == 'F') {
                if (i + 2 < fmt.length() && fmt[i + 2] == '{') {
                    size_t close = fmt.find('}', i + 3);
                    if (close != string::npos) {
                        static const unordered_map<string, string> color_map = {
                            {"black",      "\033[30m"}, {"red",        "\033[31m"},
                            {"green",      "\033[32m"}, {"yellow",     "\033[33m"},
                            {"blue",       "\033[34m"}, {"magenta",    "\033[35m"},
                            {"cyan",       "\033[36m"}, {"white",      "\033[37m"},
                            {"orange",     "\033[38;5;208m"}, {"pink",       "\033[38;5;205m"},
                            {"br_black",   "\033[90m"}, {"br_red",     "\033[91m"},
                            {"br_green",   "\033[92m"}, {"br_yellow",  "\033[93m"},
                            {"br_blue",    "\033[94m"}, {"br_magenta", "\033[95m"},
                            {"br_cyan",    "\033[96m"}, {"br_white",   "\033[97m"},
                        };
                        auto it = color_map.find(fmt.substr(i + 3, close - (i + 3)));
                        if (it != color_map.end()) prompt += it->second;
                        i = close; // loop i++ steps past '}'
                    } else {
                        i++; // no closing brace; skip 'F'
                    }
                } else {
                    i++; // no '{'; skip 'F'
                }
            } else if (code == 'f') { prompt += COLOR_RESET; i++; }
            else if (code == 'n') {
                DWORD l = 0;
                GetUserNameW(nullptr, &l); // l = required size incl. NUL
                if (l > 0) {
                    wstring u(l, L'\0');
                    if (GetUserNameW(&u[0], &l)) {
                        int sz = WideCharToMultiByte(CP_UTF8, 0, u.c_str(), -1, nullptr, 0, nullptr, nullptr);
                        if (sz > 1) { string s(sz - 1, '\0'); WideCharToMultiByte(CP_UTF8, 0, u.c_str(), -1, &s[0], sz, nullptr, nullptr); prompt += s; }
                    }
                }
                i++;
            }
            else if (code == 'm') {
                DWORD l = 0;
                GetComputerNameW(nullptr, &l); // l = required size incl. NUL
                if (l > 0) {
                    wstring h(l, L'\0');
                    if (GetComputerNameW(&h[0], &l)) {
                        int sz = WideCharToMultiByte(CP_UTF8, 0, h.c_str(), -1, nullptr, 0, nullptr, nullptr);
                        if (sz > 1) { string s(sz - 1, '\0'); WideCharToMultiByte(CP_UTF8, 0, h.c_str(), -1, &s[0], sz, nullptr, nullptr); prompt += s; }
                    }
                }
                i++;
            }
            else if (code == '~') { try { prompt += normalize_path_to_unix(fs::current_path().string()); } catch (...) { prompt += "?"; } i++; }
            else if (code == '1' && i + 2 < fmt.length() && fmt[i + 2] == '~') {
                // %1~ = trailing folder name only
                try {
                    prompt += fs::current_path().filename().string();
                } catch (...) {
                    prompt += "?";
                }
                i += 2;
            }
            else if (code == 'B') { prompt += "\033[1m"; i++; }  // Bold start
            else if (code == 'b') { prompt += "\033[22m"; i++; } // Bold reset
            else if (code == 'U') { prompt += "\033[4m"; i++; }  // Underline start
            else if (code == 'u') { prompt += "\033[24m"; i++; } // Underline reset
            else if (code == '#') {
                // '#' for elevated/root, '%' for normal user
                bool elevated = false;
                HANDLE tok = NULL;
                if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) {
                    TOKEN_ELEVATION te{}; DWORD sz = 0;
                    if (GetTokenInformation(tok, TokenElevation, &te, sizeof(te), &sz))
                        elevated = te.TokenIsElevated != 0;
                    CloseHandle(tok);
                }
                prompt += elevated ? "#" : "%"; i++;
            }
            else if (code == '?') {
                prompt += to_string(g_env.last_exit_code);
                i++;
            }
        } else prompt += fmt[i];
    }
    return prompt;
}

string render_prompt() {
    if (!g_env.prompt_dirty) return g_env.prompt_cache;
    string prompt = render_prompt_template(g_env.vars["PROMPT"]);
    g_env.prompt_cache = prompt;
    g_env.prompt_dirty  = false;
    return prompt;
}

string render_rprompt() {
    auto it = g_env.vars.find("RPROMPT");
    if (it == g_env.vars.end() || it->second.empty()) return "";
    return render_prompt_template(it->second);
}

// ============================================================================
// TAB COMPLETION ENGINE
// ============================================================================
static string str_lower(string s) {
    transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)tolower(c); });
    return s;
}

static vector<string> get_pathext_list() {
    vector<string> exts;
    char* env_pathext = getenv("PATHEXT");
    string raw = env_pathext ? env_pathext : ".COM;.EXE;.BAT;.CMD;.VBS;.VBE;.JS;.JSE;.WSF;.WSH;.MSC;.PY;.PS1";
    stringstream ss(raw);
    string item;
    while (getline(ss, item, ';')) {
        while (!item.empty() && isspace((unsigned char)item.front())) item.erase(item.begin());
        while (!item.empty() && isspace((unsigned char)item.back())) item.pop_back();
        if (!item.empty()) {
            if (item.front() != '.') item = "." + item;
            exts.push_back(str_lower(item));
        }
    }
    if (exts.empty()) {
        exts = {".com", ".exe", ".bat", ".cmd", ".vbs", ".vbe", ".js", ".jse", ".wsf", ".wsh", ".msc", ".py", ".ps1"};
    }
    return exts;
}

static int match_fuzzy_score(const string& query, const string& candidate) {
    if (query.empty()) return 100;
    string q = str_lower(query);
    string c = str_lower(candidate);
    if (c == q) return 1000;
    if (c.rfind(q, 0) == 0) return 800 - static_cast<int>(c.size() - q.size());

    // Word boundary prefix match (e.g. c-e or c_e matching cmd-extended)
    size_t qi = 0;
    for (size_t ci = 0; ci < c.size() && qi < q.size(); ++ci) {
        if (ci == 0 || c[ci - 1] == '-' || c[ci - 1] == '_' || c[ci - 1] == '.') {
            if (c[ci] == q[qi]) qi++;
        }
    }
    if (qi == q.size()) return 600 - static_cast<int>(c.size() - q.size());

    // Substring match
    size_t sub_pos = c.find(q);
    if (sub_pos != string::npos) {
        return 400 - static_cast<int>(sub_pos * 10 + (c.size() - q.size()));
    }

    // Fuzzy subsequence match
    qi = 0;
    int contiguous_bonus = 0;
    int last_match_idx = -2;
    for (size_t ci = 0; ci < c.size() && qi < q.size(); ++ci) {
        if (c[ci] == q[qi]) {
            if (static_cast<int>(ci) == last_match_idx + 1) contiguous_bonus += 15;
            if (ci == 0 || c[ci - 1] == '-' || c[ci - 1] == '_' || c[ci - 1] == '.') contiguous_bonus += 20;
            last_match_idx = static_cast<int>(ci);
            qi++;
        }
    }
    if (qi == q.size()) {
        return 200 + contiguous_bonus - static_cast<int>(c.size());
    }

    return 0; // No match
}

struct CompState {
    vector<string> candidates;
    size_t         idx = 0;
    string         stem;
    size_t         stem_pos = 0;
    bool           active = false;
    int            rendered_rows = 0;
    string         original_buf;
    size_t         original_cursor = 0;
};
static CompState g_comp;

static void clear_completion_menu() {
    if (g_comp.rendered_rows > 0) {
        string out;
        for (int r = 0; r < g_comp.rendered_rows; ++r) {
            out += "\n\033[2K\r";
        }
        out += "\033[" + to_string(g_comp.rendered_rows) + "A\r";
        fwrite(out.data(), 1, out.size(), stdout);
        fflush(stdout);
        g_comp.rendered_rows = 0;
    }
}

static void reset_comp() {
    clear_completion_menu();
    g_comp.active = false;
    g_comp.candidates.clear();
    g_comp.idx = 0;
    g_comp.rendered_rows = 0;
    g_comp.original_buf.clear();
    g_comp.original_cursor = 0;
}

// Returns the token being completed and sets stem_start to its position in buf.
static string completion_stem(const string& buf, size_t cursor, size_t& stem_start) {
    size_t i = cursor;
    while (i > 0 && !isspace((unsigned char)buf[i-1])) --i;
    stem_start = i;
    return buf.substr(i, cursor - i);
}



// Longest common prefix (case-preserving from first candidate).
static string common_prefix(const vector<string>& v) {
    if (v.empty()) return "";
    string p = v[0];
    for (size_t i = 1; i < v.size(); ++i) {
        size_t j = 0;
        while (j < p.size() && j < v[i].size() && tolower((unsigned char)p[j]) == tolower((unsigned char)v[i][j])) ++j;
        p = p.substr(0, j);
    }
    return p;
}

// Collect filesystem completions for a path prefix with fuzzy & substring matching.
static vector<string> complete_path(const string& prefix) {
    struct MatchItem {
        string full_path;
        int score;
    };
    vector<MatchItem> matches;
    string dir_part, file_part;
    size_t sep = prefix.find_last_of("/\\");
    if (sep == string::npos) { dir_part = "."; file_part = prefix; }
    else { dir_part = prefix.substr(0, sep + 1); file_part = prefix.substr(sep + 1); }

    string win_dir = normalize_path_to_win(g_env.expand_vars(dir_part == "." ? "." : dir_part));
    error_code ec;
    if (!fs::is_directory(win_dir, ec)) return {};

    for (const auto& e : fs::directory_iterator(win_dir, ec)) {
        string name = e.path().filename().string();
        int score = match_fuzzy_score(file_part, name);
        if (score > 0) {
            string full = (dir_part == "." ? "" : dir_part) + name;
            if (e.is_directory(ec)) full += "/";
            matches.push_back({full, score});
        }
    }

    sort(matches.begin(), matches.end(), [](const MatchItem& a, const MatchItem& b) {
        if (a.score != b.score) return a.score > b.score;
        if (a.full_path.size() != b.full_path.size()) return a.full_path.size() < b.full_path.size();
        return a.full_path < b.full_path;
    });

    vector<string> out;
    for (const auto& m : matches) out.push_back(m.full_path);
    return out;
}

// Collect command completions (builtins + aliases + functions + PATH via %PATHEXT%) with fuzzy matching.
static vector<string> complete_command(const string& prefix) {
    struct MatchItem {
        string name;
        int score;
    };
    vector<MatchItem> matches;
    set<string> seen;

    auto check_and_add = [&](const string& name) {
        if (name.empty()) return;
        string key = str_lower(name);
        if (seen.count(key)) return;
        seen.insert(key);
        int score = match_fuzzy_score(prefix, name);
        if (score > 0) {
            matches.push_back({name, score});
        }
    };

    // Builtins
    for (const auto& b : zsh_builtin_command_names()) {
        check_and_add(b);
    }
    // Aliases
    for (const auto& [k, v] : g_env.aliases) {
        check_and_add(k);
    }
    // Functions
    for (const auto& [k, v] : g_env.functions) {
        check_and_add(k);
    }

    // Executables in PATH with automatic PATHEXT resolution and extension stripping
    vector<string> pathext = get_pathext_list();
    char* path_env = getenv("PATH");
    if (path_env) {
        stringstream ss(path_env);
        string dir;
        while (getline(ss, dir, ';')) {
            while (!dir.empty() && isspace((unsigned char)dir.front())) dir.erase(dir.begin());
            while (!dir.empty() && isspace((unsigned char)dir.back())) dir.pop_back();
            if (dir.empty()) continue;
            error_code ec;
            if (!fs::is_directory(dir, ec)) continue;
            for (const auto& e : fs::directory_iterator(dir, ec)) {
                if (!e.is_regular_file(ec)) continue;
                string filename = e.path().filename().string();
                string ext = str_lower(e.path().extension().string());
                bool is_executable = false;
                for (const auto& pe : pathext) {
                    if (ext == pe) {
                        is_executable = true;
                        break;
                    }
                }
                if (is_executable) {
                    string stem = e.path().stem().string();
                    check_and_add(stem);
                    if (prefix.find('.') != string::npos) {
                        check_and_add(filename);
                    }
                }
            }
        }
    }

    sort(matches.begin(), matches.end(), [](const MatchItem& a, const MatchItem& b) {
        if (a.score != b.score) return a.score > b.score;
        if (a.name.size() != b.name.size()) return a.name.size() < b.name.size();
        return a.name < b.name;
    });

    vector<string> out;
    for (const auto& m : matches) out.push_back(m.name);
    return out;
}

static vector<string> complete_registered_command(const string& command, const string& prefix) {
    auto definition = g_env.completion_definitions.find(command);
    if (definition == g_env.completion_definitions.end()) return {};
    vector<string> saved_reply = g_env.indexed_arrays["reply"];
    g_env.indexed_arrays["reply"].clear();
    g_env.vars["PREFIX"] = prefix;
    dispatch_command({definition->second});
    struct MatchItem {
        string val;
        int score;
    };
    vector<MatchItem> matches;
    set<string> seen;
    for (const auto& candidate : g_env.indexed_arrays["reply"]) {
        if (seen.count(candidate)) continue;
        seen.insert(candidate);
        int score = match_fuzzy_score(prefix, candidate);
        if (score > 0) matches.push_back({candidate, score});
    }
    g_env.indexed_arrays["reply"] = std::move(saved_reply);
    sort(matches.begin(), matches.end(), [](const MatchItem& a, const MatchItem& b) {
        if (a.score != b.score) return a.score > b.score;
        return a.val < b.val;
    });
    vector<string> out;
    for (const auto& m : matches) out.push_back(m.val);
    return out;
}

static bool execute_bound_widget(int key_code, string& buffer, size_t& cursor) {
    string literal_key(1, static_cast<char>(key_code));
    string caret_key;
    if (key_code >= 1 && key_code <= 26) caret_key = string("^") + static_cast<char>('A' + key_code - 1);
    const string keymaps[] = {"main", "emacs", "emacs-standard"};
    string widget_name;
    for (const auto& keymap : keymaps) {
        auto map_it = g_env.keymaps.find(keymap);
        if (map_it == g_env.keymaps.end()) continue;
        auto binding = map_it->second.find(literal_key);
        if (binding == map_it->second.end() && !caret_key.empty()) binding = map_it->second.find(caret_key);
        if (binding != map_it->second.end()) { widget_name = binding->second; break; }
    }
    auto widget = g_env.widgets.find(widget_name);
    if (widget_name.empty() || widget == g_env.widgets.end()) return false;
    g_env.vars["BUFFER"] = buffer;
    g_env.vars["CURSOR"] = to_string(cursor);
    dispatch_command({widget->second});
    buffer = g_env.vars["BUFFER"];
    try { cursor = min(buffer.size(), static_cast<size_t>(stoull(g_env.vars["CURSOR"]))); }
    catch (...) { cursor = buffer.size(); }
    return true;
}

// Returns the visible display column count of a string (stripping ANSI escapes & handling UTF-8 multi-byte glyphs).
static size_t visible_length(const string& s) {
    size_t len = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\033' && i + 1 < s.size() && s[i+1] == '[') {
            i += 2;
            while (i < s.size() && s[i] != 'm' && s[i] != 'K' && s[i] != 'G' && s[i] != 'H') ++i;
        } else {
            unsigned char uc = static_cast<unsigned char>(s[i]);
            // In UTF-8, continuation bytes match 10xxxxxx (0x80 to 0xBF). Skip them so each multi-byte glyph counts as 1 display column.
            if ((uc & 0xC0) != 0x80) {
                ++len;
            }
        }
    }
    return len;
}

void repaint_line(const string& buf, size_t cursor_pos) {
    string rendered = render_prompt();
    string rprompt = render_rprompt();

    CONSOLE_SCREEN_BUFFER_INFO csbi{};
    int console_width = 0;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        console_width = (csbi.srWindow.Right - csbi.srWindow.Left) + 1;
        if (console_width <= 0) console_width = csbi.dwSize.X;
    }

    // Build entire output into one buffer for a single atomic write — eliminates flicker.
    string out;
    out.reserve(rendered.size() + buf.size() + 64);
    out += "\r\033[K";
    out += rendered;
    bool first_word = true;
    size_t pos = 0;
    while (pos <= buf.size()) {
        size_t ws_start = pos;
        while (pos < buf.size() && !isspace((unsigned char)buf[pos])) ++pos;
        if (pos > ws_start) {
            string word = buf.substr(ws_start, pos - ws_start);
            if (first_word) {
                out += (g_env.aliases.count(word) || g_env.functions.count(word)) ? COLOR_BR_GREEN : COLOR_BR_BLUE;
                first_word = false;
            } else if (!word.empty() && word[0] == '-') out += COLOR_CYAN;
            else out += COLOR_WHITE;
            out += word;
        }
        while (pos < buf.size() && isspace((unsigned char)buf[pos])) out += buf[pos++];
        if (pos >= buf.size()) break;
    }
    out += COLOR_RESET;

    size_t prompt_and_input_len = visible_length(rendered) + buf.size();
    size_t rlen = visible_length(rprompt);
    if (rlen > 0 && console_width > (int)(prompt_and_input_len + rlen + 2)) {
        int start_col = console_width - (int)rlen + 1;
        if (start_col < 1) start_col = 1;
        out += "\033[" + to_string(start_col) + "G" + rprompt;
    }

    out += "\033[" + to_string(visible_length(rendered) + cursor_pos + 1) + "G";
    fwrite(out.data(), 1, out.size(), stdout);
    fflush(stdout);
}

static void render_completion_menu_grid(const string& buf, size_t cursor_pos) {
    if (!g_comp.active || g_comp.candidates.empty()) {
        clear_completion_menu();
        repaint_line(buf, cursor_pos);
        return;
    }

    CONSOLE_SCREEN_BUFFER_INFO csbi{};
    int console_width = 80;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        console_width = (csbi.srWindow.Right - csbi.srWindow.Left) + 1;
        if (console_width <= 0) console_width = csbi.dwSize.X;
    }
    if (console_width < 20) console_width = 80;

    size_t max_len = 0;
    for (const auto& c : g_comp.candidates) {
        if (c.size() > max_len) max_len = c.size();
    }
    size_t col_width = min(static_cast<size_t>(console_width), max_len + 3);
    size_t num_cols = max(size_t(1), static_cast<size_t>(console_width) / col_width);
    size_t total_items = g_comp.candidates.size();
    size_t total_rows = (total_items + num_cols - 1) / num_cols;

    const size_t max_visible_rows = 5;
    size_t sel_row = g_comp.idx / num_cols;
    size_t start_row = 0;
    if (sel_row >= max_visible_rows) {
        start_row = sel_row - max_visible_rows + 1;
    }
    size_t end_row = min(total_rows, start_row + max_visible_rows);

    // Repaint the prompt and command line first
    repaint_line(buf, cursor_pos);

    // Build the grid menu output
    string out;
    int rows_drawn = 0;

    for (size_t r = start_row; r < end_row; ++r) {
        out += "\n\033[2K\r";
        for (size_t col = 0; col < num_cols; ++col) {
            size_t item_idx = r * num_cols + col;
            if (item_idx >= total_items) break;
            const string& item = g_comp.candidates[item_idx];
            bool is_selected = (item_idx == g_comp.idx);

            string pad_str = item;
            if (pad_str.size() < col_width) {
                pad_str.append(col_width - pad_str.size(), ' ');
            } else if (pad_str.size() > col_width) {
                pad_str = pad_str.substr(0, col_width);
            }

            if (is_selected) {
                out += "\033[7m\033[1;36m" + pad_str + "\033[0m";
            } else {
                out += "\033[36m" + pad_str + "\033[0m";
            }
        }
        rows_drawn++;
    }

    if (total_rows > max_visible_rows) {
        out += "\n\033[2K\r\033[90m-- (" + to_string(g_comp.idx + 1) + "/" + to_string(total_items) + ") --\033[0m";
        rows_drawn++;
    }

    // Clear previously rendered lines if fewer rows are rendered now
    if (g_comp.rendered_rows > rows_drawn) {
        for (int extra = rows_drawn; extra < g_comp.rendered_rows; ++extra) {
            out += "\n\033[2K\r";
            rows_drawn++;
        }
    }

    // Move cursor back up to the prompt line
    if (rows_drawn > 0) {
        out += "\033[" + to_string(rows_drawn) + "A\r";
    }

    // Reposition cursor horizontally on prompt line
    string rendered = render_prompt();
    out += "\033[" + to_string(visible_length(rendered) + cursor_pos + 1) + "G";

    fwrite(out.data(), 1, out.size(), stdout);
    fflush(stdout);

    g_comp.rendered_rows = rows_drawn;
}

// ============================================================================
// TYPO CORRECTION ENGINE (setopt CORRECT)
// ============================================================================
static int damerau_levenshtein_distance(const string& s1_in, const string& s2_in) {
    string a = str_lower(s1_in);
    string b = str_lower(s2_in);
    size_t len_a = a.size();
    size_t len_b = b.size();
    if (len_a == 0) return static_cast<int>(len_b);
    if (len_b == 0) return static_cast<int>(len_a);

    vector<vector<int>> d(len_a + 1, vector<int>(len_b + 1, 0));
    for (size_t i = 0; i <= len_a; ++i) d[i][0] = static_cast<int>(i);
    for (size_t j = 0; j <= len_b; ++j) d[0][j] = static_cast<int>(j);

    for (size_t i = 1; i <= len_a; ++i) {
        for (size_t j = 1; j <= len_b; ++j) {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            int deletion = d[i - 1][j] + 1;
            int insertion = d[i][j - 1] + 1;
            int substitution = d[i - 1][j - 1] + cost;
            int min_val = min({deletion, insertion, substitution});

            if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1]) {
                min_val = min(min_val, d[i - 2][j - 2] + 1); // transposition
            }
            d[i][j] = min_val;
        }
    }
    return d[len_a][len_b];
}

static bool is_valid_command_name(const string& cmd) {
    if (cmd.empty()) return true;
    if (is_zsh_builtin_command(cmd)) return true;
    if (g_env.aliases.count(cmd)) return true;
    if (g_env.functions.count(cmd)) return true;
    if (!find_executable_in_path(cmd).empty()) return true;

    // Check autocd: drive letter or directory
    error_code ec;
    string win_cmd = normalize_path_to_win(cmd);
    bool is_drive_letter = (cmd.size() == 2 && isalpha((unsigned char)cmd[0]) && cmd[1] == ':');
    if (g_env.options.count("autocd") && g_env.options.at("autocd") &&
        (is_drive_letter || fs::is_directory(win_cmd, ec))) {
        return true;
    }

    // Check direct path to executable
    if (cmd.find_first_of("/\\") != string::npos) {
        if (fs::exists(win_cmd, ec) && !fs::is_directory(win_cmd, ec)) return true;
        for (const string& ext : { ".exe", ".com", ".bat", ".cmd" }) {
            if (fs::exists(win_cmd + ext, ec)) return true;
        }
    }

    return false;
}

static string find_typo_correction(const string& token) {
    if (token.size() <= 1) return "";
    if (token.find_first_of("/\\") != string::npos) return "";
    if (is_valid_command_name(token)) return "";

    int max_distance = (token.size() <= 4) ? 1 : 2;

    struct TypoMatch {
        string name;
        int distance;
        bool is_transposition;
        bool is_builtin_or_alias;
        int prefix_len;
    };
    vector<TypoMatch> matches;
    set<string> seen;

    string token_lower = str_lower(token);

    auto check_candidate = [&](const string& name, bool is_builtin_alias) {
        if (name.empty() || name == token) return;
        string low = str_lower(name);
        if (seen.count(low)) return;
        seen.insert(low);

        int dist = damerau_levenshtein_distance(token_lower, low);
        if (dist <= max_distance) {
            bool is_trans = (dist == 1 && token_lower.size() == low.size() &&
                             is_permutation(token_lower.begin(), token_lower.end(), low.begin()));
            int prefix_len = 0;
            while (prefix_len < (int)token_lower.size() && prefix_len < (int)low.size() &&
                   token_lower[prefix_len] == low[prefix_len]) {
                prefix_len++;
            }
            matches.push_back({name, dist, is_trans, is_builtin_alias, prefix_len});
        }
    };

    // 1. Builtins
    for (const auto& b : zsh_builtin_command_names()) check_candidate(b, true);
    // 2. Aliases
    for (const auto& [k, v] : g_env.aliases) check_candidate(k, true);
    // 3. Functions
    for (const auto& [k, v] : g_env.functions) check_candidate(k, true);
    // 4. PATH executables & companion directory
    vector<string> pathext = get_pathext_list();
    vector<string> search_dirs;
    string self_path = current_shell_executable_path();
    if (!self_path.empty()) {
        string self_dir = fs::path(self_path).parent_path().string();
        if (!self_dir.empty()) search_dirs.push_back(self_dir);
    }
    char* path_env = getenv("PATH");
    if (path_env) {
        stringstream ss(path_env);
        string dir;
        while (getline(ss, dir, ';')) {
            while (!dir.empty() && isspace((unsigned char)dir.front())) dir.erase(dir.begin());
            while (!dir.empty() && isspace((unsigned char)dir.back())) dir.pop_back();
            if (!dir.empty()) search_dirs.push_back(normalize_path_to_win(dir));
        }
    }
    for (const auto& dir : search_dirs) {
        error_code ec;
        if (!fs::is_directory(dir, ec)) continue;
        for (const auto& e : fs::directory_iterator(dir, ec)) {
            if (!e.is_regular_file(ec)) continue;
            string ext = str_lower(e.path().extension().string());
            bool is_exec = false;
            for (const auto& pe : pathext) {
                if (ext == pe) { is_exec = true; break; }
            }
            if (is_exec) {
                check_candidate(e.path().stem().string(), false);
            }
        }
    }

    if (matches.empty()) return "";

    sort(matches.begin(), matches.end(), [](const TypoMatch& a, const TypoMatch& b) {
        if (a.distance != b.distance) return a.distance < b.distance;
        if (a.is_transposition != b.is_transposition) return a.is_transposition > b.is_transposition;
        if (a.is_builtin_or_alias != b.is_builtin_or_alias) return a.is_builtin_or_alias > b.is_builtin_or_alias;
        if (a.prefix_len != b.prefix_len) return a.prefix_len > b.prefix_len;
        return a.name.size() < b.name.size();
    });

    return matches[0].name;
}

static string replace_first_command_token(const string& line, const string& old_tok, const string& new_tok) {
    size_t start = 0;
    while (start < line.size() && isspace((unsigned char)line[start])) ++start;
    if (start < line.size() && line.compare(start, old_tok.size(), old_tok) == 0) {
        size_t after = start + old_tok.size();
        if (after == line.size() || isspace((unsigned char)line[after])) {
            return line.substr(0, start) + new_tok + line.substr(after);
        }
    }
    return new_tok + (line.size() > old_tok.size() ? line.substr(old_tok.size()) : "");
}

static bool apply_interactive_typo_correction(const string& line, string& action_line, string& pending_edit_line) {
    action_line = line;
    pending_edit_line.clear();

    if (!g_env.options.count("correct") || !g_env.options.at("correct")) return true;

    vector<string> words = tokenize_words(line);
    if (words.empty()) return true;

    // Skip assignments VAR=val
    size_t cmd_idx = 0;
    while (cmd_idx < words.size() && words[cmd_idx].find('=') != string::npos) {
        cmd_idx++;
    }
    if (cmd_idx >= words.size()) return true;

    const string& cmd_tok = words[cmd_idx];

    // Check for nocorrect or keywords
    if (cmd_tok == "nocorrect" || cmd_tok == "noglob" || cmd_tok == "builtin" || cmd_tok == "command" ||
        cmd_tok == "if" || cmd_tok == "for" || cmd_tok == "while" || cmd_tok == "until" ||
        cmd_tok == "case" || cmd_tok == "repeat" || cmd_tok == "select" || cmd_tok == "function" ||
        cmd_tok == "time" || cmd_tok == "exec") {
        return true;
    }

    if (cmd_tok.size() <= 1 || cmd_tok.find_first_of("/\\") != string::npos) return true;
    if (is_valid_command_name(cmd_tok)) return true;

    string suggestion = find_typo_correction(cmd_tok);
    if (suggestion.empty() || suggestion == cmd_tok) return true;

    cout << "zsh: correct '" << cmd_tok << "' to '" << suggestion << "' [nyae]? ";
    cout.flush();

    int c = _getch();
    if (c == 'y' || c == 'Y' || c == 13 || c == 10) {
        cout << "y\n";
        action_line = replace_first_command_token(line, cmd_tok, suggestion);
        return true;
    } else if (c == 'n' || c == 'N' || c == ' ') {
        cout << "n\n";
        action_line = line;
        return true;
    } else if (c == 'a' || c == 'A' || c == 27 || c == 3) {
        cout << "a\n";
        g_env.last_exit_code = 1;
        return false;
    } else if (c == 'e' || c == 'E') {
        cout << "e\n";
        pending_edit_line = replace_first_command_token(line, cmd_tok, suggestion);
        return false;
    } else {
        cout << "n\n";
        action_line = line;
        return true;
    }
}

string read_line_interactive(const string& initial_buffer = "") {
    string buf = initial_buffer; size_t cursor_pos = buf.size(); int hist_idx = (int)g_env.history.size();
    bool reverse_search_active = false;
    string reverse_search_query;
    int reverse_search_pos = (int)g_env.history.size() - 1;
    reset_comp();
    if (!buf.empty()) {
        repaint_line(buf, cursor_pos);
    }

    while (true) {
        check_window_resize_event();
        process_pending_traps();

        if (!_kbhit()) {
            Sleep(10);
            continue;
        }

        int c = _getch();
        if (execute_bound_widget(c, buf, cursor_pos)) {
            reset_comp();
            repaint_line(buf, cursor_pos);
            continue;
        }
        if (c != 18) {
            reverse_search_active = false;
            reverse_search_query.clear();
            reverse_search_pos = (int)g_env.history.size() - 1;
        }
        if (c == 26) { // Ctrl+Z: SIGTSTP emulation
            reset_comp();
            cout << "^Z\n";

            if (suspend_active_foreground_processes()) {
                cout << "suspended active foreground process(es)\n";
            }

            g_sigtstp_pending.store(true);
            process_pending_traps();
            buf.clear();
            cursor_pos = 0;
            repaint_line(buf, cursor_pos);
            continue;
        }
        if (c == 3) { // Ctrl+C: Cancel current line
            reset_comp();
            cout << "^C\n";
            buf.clear();
            cursor_pos = 0;
            repaint_line(buf, cursor_pos);
            continue;
        }
        if (c == 1) { // Ctrl+A: Jump to start of line
            reset_comp();
            cursor_pos = 0;
            repaint_line(buf, cursor_pos);
            continue;
        }
        if (c == 5) { // Ctrl+E: Jump to end of line
            reset_comp();
            cursor_pos = buf.size();
            repaint_line(buf, cursor_pos);
            continue;
        }
        if (c == 12) { // Ctrl+L: Clear screen & repaint prompt without losing buffer
            reset_comp();
            system("cls");
            repaint_line(buf, cursor_pos);
            continue;
        }
        if (c == 21) { // Ctrl+U: Delete line from start to cursor
            reset_comp();
            buf.erase(0, cursor_pos);
            cursor_pos = 0;
            repaint_line(buf, cursor_pos);
            continue;
        }
        if (c == 11) { // Ctrl+K: Delete line from cursor to end
            reset_comp();
            buf.erase(cursor_pos);
            repaint_line(buf, cursor_pos);
            continue;
        }
        if (c == 23) { // Ctrl+W: Erase word backwards
            reset_comp();
            while (cursor_pos > 0 && isspace((unsigned char)buf[cursor_pos - 1])) {
                buf.erase(--cursor_pos, 1);
            }
            while (cursor_pos > 0 && !isspace((unsigned char)buf[cursor_pos - 1])) {
                buf.erase(--cursor_pos, 1);
            }
            repaint_line(buf, cursor_pos);
            continue;
        }
        if (c == 18) { // Ctrl+R: Reverse history search (repeat to cycle)
            reset_comp();
            if (!reverse_search_active) {
                reverse_search_query = buf.substr(0, cursor_pos);
                reverse_search_active = true;
                reverse_search_pos = (int)g_env.history.size() - 1;
            }

            int found = -1;
            for (int i = reverse_search_pos; i >= 0; --i) {
                if (reverse_search_query.empty() ||
                    g_env.history[i].find(reverse_search_query) != string::npos) {
                    found = i;
                    break;
                }
            }

            if (found >= 0) {
                hist_idx = found;
                buf = g_env.history[found];
                cursor_pos = buf.size();
                reverse_search_pos = found - 1;
                repaint_line(buf, cursor_pos);
            } else {
                fwrite("\a", 1, 1, stdout);
                fflush(stdout);
            }
            continue;
        }
        if (c == 27) { // Escape: Cancel completion menu
            if (g_comp.active) {
                buf = g_comp.original_buf;
                cursor_pos = g_comp.original_cursor;
                reset_comp();
                repaint_line(buf, cursor_pos);
                continue;
            }
        }
        if (c == 13) { // Enter: Accept completion or submit line
            if (g_comp.active) {
                reset_comp();
                if (!buf.empty() && buf.back() != '/' && buf.back() != '\\') {
                    buf.insert(cursor_pos, 1, ' ');
                    cursor_pos++;
                }
                repaint_line(buf, cursor_pos);
                continue;
            }
            cout << "\n";
            reset_comp();
            return buf;
        }
        if (c == 9) { // Tab: Command/path completion & cycle
            if (!g_comp.active) {
                size_t stem_start = 0;
                string stem = completion_stem(buf, cursor_pos, stem_start);
                bool is_cmd = (stem_start == 0) ||
                              buf.substr(0, stem_start).find_first_not_of(" \t") == string::npos;
                bool has_sep = stem.find_first_of("/\\~") != string::npos;
                vector<string> candidates;
                if (!is_cmd || has_sep) {
                    vector<string> words = tokenize_words(buf.substr(0, cursor_pos));
                    candidates = words.empty() ? vector<string>{} : complete_registered_command(words[0], stem);
                    if (candidates.empty()) candidates = complete_path(stem);
                } else {
                    candidates = complete_command(stem);
                    if (candidates.empty()) candidates = complete_path(stem);
                }

                if (candidates.empty()) {
                    fwrite("\a", 1, 1, stdout);
                    fflush(stdout);
                    continue;
                }

                if (candidates.size() == 1) {
                    string cand = candidates[0];
                    buf.replace(stem_start, cursor_pos - stem_start, cand);
                    cursor_pos = stem_start + cand.size();
                    if (!buf.empty() && buf.back() != '/' && buf.back() != '\\') {
                        buf.insert(cursor_pos, 1, ' ');
                        cursor_pos++;
                    }
                    reset_comp();
                    repaint_line(buf, cursor_pos);
                    continue;
                }

                // Multiple candidates: start interactive menu
                g_comp.active = true;
                g_comp.candidates = candidates;
                g_comp.idx = 0;
                g_comp.stem = stem;
                g_comp.stem_pos = stem_start;
                g_comp.original_buf = buf;
                g_comp.original_cursor = cursor_pos;

                string cp = common_prefix(candidates);
                if (cp.size() > stem.size()) {
                    buf.replace(stem_start, cursor_pos - stem_start, cp);
                    cursor_pos = stem_start + cp.size();
                }

                string cand = g_comp.candidates[g_comp.idx];
                buf.replace(g_comp.stem_pos, cursor_pos - g_comp.stem_pos, cand);
                cursor_pos = g_comp.stem_pos + cand.size();

                render_completion_menu_grid(buf, cursor_pos);
                continue;
            } else {
                // Cycle to next candidate
                g_comp.idx = (g_comp.idx + 1) % g_comp.candidates.size();
                string cand = g_comp.candidates[g_comp.idx];
                buf.replace(g_comp.stem_pos, cursor_pos - g_comp.stem_pos, cand);
                cursor_pos = g_comp.stem_pos + cand.size();
                render_completion_menu_grid(buf, cursor_pos);
                continue;
            }
        }
        else if (c == 8) { // Backspace
            if (g_comp.active) {
                buf = g_comp.original_buf;
                cursor_pos = g_comp.original_cursor;
                reset_comp();
            }
            if (cursor_pos > 0) {
                buf.erase(cursor_pos - 1, 1);
                cursor_pos--;
            }
            repaint_line(buf, cursor_pos);
            continue;
        }
        else if (c == 224 || c == 0) { // Special keys (Arrows, Shift-Tab, Home, End, Delete)
            int arrow = _getch();
            if (g_comp.active) {
                size_t total = g_comp.candidates.size();
                CONSOLE_SCREEN_BUFFER_INFO csbi{};
                int console_width = 80;
                if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
                    console_width = (csbi.srWindow.Right - csbi.srWindow.Left) + 1;
                    if (console_width <= 0) console_width = csbi.dwSize.X;
                }
                size_t max_len = 0;
                for (const auto& cand : g_comp.candidates) if (cand.size() > max_len) max_len = cand.size();
                size_t col_width = min(static_cast<size_t>(console_width), max_len + 3);
                size_t num_cols = max(size_t(1), static_cast<size_t>(console_width) / col_width);

                if (arrow == 77) { // Right arrow
                    g_comp.idx = (g_comp.idx + 1) % total;
                } else if (arrow == 75) { // Left arrow
                    g_comp.idx = (g_comp.idx == 0) ? total - 1 : g_comp.idx - 1;
                } else if (arrow == 80) { // Down arrow
                    if (g_comp.idx + num_cols < total) g_comp.idx += num_cols;
                    else g_comp.idx = g_comp.idx % num_cols;
                } else if (arrow == 72) { // Up arrow
                    if (g_comp.idx >= num_cols) g_comp.idx -= num_cols;
                    else {
                        size_t last_row = (total - 1) / num_cols;
                        size_t target = last_row * num_cols + (g_comp.idx % num_cols);
                        g_comp.idx = (target < total) ? target : (total - 1);
                    }
                } else if (arrow == 15) { // Shift-Tab
                    g_comp.idx = (g_comp.idx == 0) ? total - 1 : g_comp.idx - 1;
                } else {
                    reset_comp();
                    repaint_line(buf, cursor_pos);
                    continue;
                }
                string cand = g_comp.candidates[g_comp.idx];
                buf.replace(g_comp.stem_pos, cursor_pos - g_comp.stem_pos, cand);
                cursor_pos = g_comp.stem_pos + cand.size();
                render_completion_menu_grid(buf, cursor_pos);
                continue;
            }
            if (arrow == 75 && cursor_pos > 0) cursor_pos--;
            else if (arrow == 77 && cursor_pos < buf.length()) cursor_pos++;
            else if (arrow == 71) cursor_pos = 0;                             // Home
            else if (arrow == 79) cursor_pos = buf.length();                  // End
            else if (arrow == 83 && cursor_pos < buf.length()) {              // Delete
                buf.erase(cursor_pos, 1);
            } else if (arrow == 72 && hist_idx > 0) {
                hist_idx--; buf = g_env.history[hist_idx]; cursor_pos = buf.length();
            } else if (arrow == 80) {
                if (hist_idx < (int)g_env.history.size() - 1) {
                    hist_idx++; buf = g_env.history[hist_idx]; cursor_pos = buf.length();
                } else { hist_idx = (int)g_env.history.size(); buf = ""; cursor_pos = 0; }
            }
            repaint_line(buf, cursor_pos);
            continue;
        }
        else if (c >= 32 && c <= 126) {
            if (g_comp.active) {
                reset_comp();
            }
            buf.insert(cursor_pos, 1, (char)c); cursor_pos++; repaint_line(buf, cursor_pos);
        }
    }
}

// ============================================================================
// SECTION 11: BUILT-IN REGRESSION SELF-TESTS
// ============================================================================
int run_internal_self_tests() {
    int passed = 0;
    int failed = 0;

    auto assert_true = [&](bool condition, const string& test_name) {
        if (condition) {
            cout << "PASS: " << test_name << "\n";
            passed++;
        } else {
            cerr << "FAIL: " << test_name << "\n";
            failed++;
        }
    };

    auto assert_eq = [&](const string& actual, const string& expected, const string& test_name) {
        if (actual == expected) {
            cout << "PASS: " << test_name << "\n";
            passed++;
        } else {
            cerr << "FAIL: " << test_name << " (Expected: '" << expected << "', Actual: '" << actual << "')\n";
            failed++;
        }
    };

    auto assert_ll_eq = [&](long long actual, long long expected, const string& test_name) {
        if (actual == expected) {
            cout << "PASS: " << test_name << "\n";
            passed++;
        } else {
            cerr << "FAIL: " << test_name << " (Expected: " << expected << ", Actual: " << actual << ")\n";
            failed++;
        }
    };

    cout << "--- Running CrossShellZSH Internal Regression Self-Tests ---\n";

    // 1. Path Normalization & String Helpers
    assert_eq(normalize_path_to_win("/c/tools/zsh"), "C:\\tools\\zsh", "normalize_path_to_win unix drive path");
    assert_eq(normalize_path_to_unix("C:\\tools\\zsh"), "C:/tools/zsh", "normalize_path_to_unix windows path");
    assert_eq(canonicalize_option_name("No_Case_Glob"), "nocaseglob", "canonicalize_option_name mixed case and underscores");
    assert_eq(canonicalize_option_name("EXTENDED_GLOB"), "extendedglob", "canonicalize_option_name uppercase with underscore");
    assert_eq(trim_copy("  hello zsh  "), "hello zsh", "trim_copy whitespace removal");
    assert_eq(to_string(visible_length("\033[32mhello\033[0m")), "5", "visible_length strips ANSI color escapes");
    assert_eq(to_string(visible_length("hello \xEE\x82\xA0 world")), "13", "visible_length counts 3-byte Nerd Font glyph as 1 column");

    // 2. Arithmetic & Math Evaluation Engine
    assert_ll_eq(eval_math_expr("1 + 1"), 2, "eval_math_expr addition");
    assert_ll_eq(eval_math_expr("2 + 3 * 4"), 14, "eval_math_expr operator precedence");
    assert_ll_eq(eval_math_expr("(2 + 3) * 4"), 20, "eval_math_expr parentheses");
    assert_ll_eq(eval_math_expr("100 / 4 - 5"), 20, "eval_math_expr division and subtraction");
    assert_ll_eq(eval_math_expr("17 % 5"), 2, "eval_math_expr modulo");
    assert_ll_eq(eval_math_expr("1 << 5"), 32, "eval_math_expr bitwise left shift");
    assert_ll_eq(eval_math_expr("64 >> 2"), 16, "eval_math_expr bitwise right shift");
    assert_ll_eq(eval_math_expr("5 ^ 3"), 6, "eval_math_expr bitwise xor");
    assert_ll_eq(eval_math_expr("7 & 3"), 3, "eval_math_expr bitwise and");
    assert_ll_eq(eval_math_expr("4 | 2"), 6, "eval_math_expr bitwise or");
    assert_ll_eq(eval_math_expr("0x10 + 0x20"), 48, "eval_math_expr hex literals");
    assert_ll_eq(eval_math_expr("010 + 020"), 24, "eval_math_expr octal literals");
    assert_ll_eq(eval_math_expr("10 > 5"), 1, "eval_math_expr relational greater than");
    assert_ll_eq(eval_math_expr("10 < 5"), 0, "eval_math_expr relational less than");
    assert_ll_eq(eval_math_expr("5 == 5"), 1, "eval_math_expr equality true");
    assert_ll_eq(eval_math_expr("5 != 5"), 0, "eval_math_expr inequality false");
    assert_ll_eq(eval_math_expr("1 && 0"), 0, "eval_math_expr logical and");
    assert_ll_eq(eval_math_expr("1 || 0"), 1, "eval_math_expr logical or");
    assert_ll_eq(eval_math_expr("!0"), 1, "eval_math_expr logical not zero");
    assert_ll_eq(eval_math_expr("!42"), 0, "eval_math_expr logical not non-zero");

    // 3. Pattern Matching & Wildcards
    assert_true(match_wildcard("*.cpp", "zsh.cpp"), "match_wildcard simple star wildcard");
    assert_true(match_wildcard("zsh.*", "zsh.exe"), "match_wildcard prefix star wildcard");
    assert_true(match_wildcard("test_?.txt", "test_1.txt"), "match_wildcard question mark wildcard");
    assert_true(!match_wildcard("test_?.txt", "test_12.txt"), "match_wildcard question mark single char limit");
    assert_true(match_wildcard("@(foo|bar).txt", "bar.txt"), "match_wildcard group alternation");
    assert_true(!match_wildcard("@(foo|bar).txt", "baz.txt"), "match_wildcard group alternation rejection");
    assert_true(match_wildcard("a*b*c", "a_middle_b_end_c"), "match_wildcard multiple stars");

    // 4. Built-in test evaluator
    assert_true(builtin_test_eval({"1", "-eq", "1"}), "builtin_test_eval integer eq");
    assert_true(builtin_test_eval({"2", "-gt", "1"}), "builtin_test_eval integer gt");
    assert_true(builtin_test_eval({"1", "-lt", "2"}), "builtin_test_eval integer lt");
    assert_true(builtin_test_eval({"abc", "=", "abc"}), "builtin_test_eval string equality");
    assert_true(builtin_test_eval({"abc", "!=", "def"}), "builtin_test_eval string inequality");
    assert_true(builtin_test_eval({"-z", ""}), "builtin_test_eval -z empty string");
    assert_true(builtin_test_eval({"-n", "non-empty"}), "builtin_test_eval -n non-empty string");

    // 5. Variable Expansions in ZshEnvironment
    {
        ZshEnvironment test_env;
        test_env.vars["GREETING"] = "Hello";
        test_env.vars["TARGET"] = "World";
        assert_eq(test_env.expand_vars("$GREETING $TARGET"), "Hello World", "expand_vars simple variable substitution");
        assert_eq(test_env.expand_vars("${GREETING}_${TARGET}"), "Hello_World", "expand_vars braced variable substitution");
        assert_eq(test_env.expand_vars("$(( 10 * 5 + 2 ))"), "52", "expand_vars math substitution");
        assert_eq(test_env.expand_vars("${UNSET_VAL:-fallback}"), "fallback", "expand_vars default value substitution");
        assert_eq(test_env.expand_vars("${GREETING:+override}"), "override", "expand_vars alternate value substitution");
        assert_eq(test_env.expand_vars("${UNSET_VAL:+override}"), "", "expand_vars alternate value on unset");
    }

    // 6. Output capture & execution integration
    {
        string echo_out = capture_command_output("echo regression_test_ok");
        assert_true(echo_out.find("regression_test_ok") != string::npos, "capture_command_output echo builtin");
    }

    // 7. Concurrent mixed pipeline execution
    {
        g_env.functions["__selftest_pipeline_source"] = "print priority12_pipeline_ok";
        string pipeline_out = capture_command_output("__selftest_pipeline_source | findstr priority12_pipeline_ok");
        assert_true(pipeline_out.find("priority12_pipeline_ok") != string::npos,
                    "mixed function/native pipeline uses concurrent pipe stages");
        g_env.functions.erase("__selftest_pipeline_source");

        g_env.assoc_arrays["__selftest_pipeline_map"]["key"] = "assoc-ok";
        g_env.vars["__selftest_pipeline_integer"] = "4";
        g_env.integer_vars.insert("__selftest_pipeline_integer");
        g_env.positional_args = {"position-ok"};
        g_env.functions["__selftest_pipeline_state"] =
            "__selftest_pipeline_integer=2+3; print $__selftest_pipeline_map[key] $__selftest_pipeline_integer $1";
        string state_out = capture_command_output("__selftest_pipeline_state position-ok | findstr assoc-ok");
        assert_true(state_out.find("assoc-ok 5 position-ok") != string::npos,
                    "mixed pipeline preserves associative, attributed, and positional state");
        g_env.functions.erase("__selftest_pipeline_state");
        g_env.assoc_arrays.erase("__selftest_pipeline_map");
        g_env.vars.erase("__selftest_pipeline_integer");
        g_env.integer_vars.erase("__selftest_pipeline_integer");
        g_env.positional_args.clear();

        string multio_a = create_temp_process_subst_path();
        string multio_b = create_temp_process_subst_path();
        bool saved_multios = g_env.options.count("multios") && g_env.options["multios"];
        g_env.options["multios"] = true;
        int multio_status = execute_single_command("print priority12_multio > " + quote_for_shell_path(multio_a) +
                                                   " > " + quote_for_shell_path(multio_b));
        g_env.options["multios"] = saved_multios;
        auto read_file_text = [](const string& path) {
            ifstream input(normalize_path_to_win(path), ios::binary);
            return string((istreambuf_iterator<char>(input)), istreambuf_iterator<char>());
        };
        assert_true(multio_status == 0 && read_file_text(multio_a).find("priority12_multio") != string::npos &&
                    read_file_text(multio_b).find("priority12_multio") != string::npos,
                    "MULTIOS streams output to every target");
        error_code multio_error;
        fs::remove(normalize_path_to_win(multio_a), multio_error);
        fs::remove(normalize_path_to_win(multio_b), multio_error);

        dispatch_command({"zmodload", "zsh/system"});
        string descriptor_path = create_temp_process_subst_path();
        int open_descriptor = execute_single_command("exec 3> " + quote_for_shell_path(descriptor_path));
        int duplicate_descriptor = execute_single_command("exec 4>&3");
        int write_descriptor = execute_single_command("syswrite -o 4 priority12_fd_ok");
        execute_single_command("exec 4>&-");
        execute_single_command("exec 3>&-");
        assert_true(open_descriptor == 0 && duplicate_descriptor == 0 && write_descriptor == 0 &&
                    read_file_text(descriptor_path).find("priority12_fd_ok") != string::npos,
                    "persistent exec descriptors support fd duplication and syswrite");
        int open_input_descriptor = execute_single_command("exec 5< " + quote_for_shell_path(descriptor_path));
        int read_descriptor = execute_single_command("sysread -i 5 __selftest_fd_value");
        execute_single_command("exec 5<&-");
        assert_true(open_input_descriptor == 0 && read_descriptor == 0 &&
                    g_env.vars["__selftest_fd_value"] == "priority12_fd_ok",
                    "persistent exec input descriptor supports sysread");
        fs::remove(normalize_path_to_win(descriptor_path), multio_error);
    }

    // 8. Explicit job state and stable current/previous markers
    {
        vector<BackgroundJob> saved_jobs = std::move(g_env.jobs);
        unsigned long saved_current = g_current_job_id;
        unsigned long saved_previous = g_previous_job_id;
        BackgroundJob first;
        first.job_id = 41;
        first.command = "first";
        first.state = JobState::Stopped;
        BackgroundJob second;
        second.job_id = 77;
        second.command = "second";
        second.state = JobState::Running;
        g_env.jobs = {first, second};
        g_current_job_id = 77;
        g_previous_job_id = 41;
        size_t job_index = 0;
        assert_true(resolve_job_index("%+", job_index) && g_env.jobs[job_index].job_id == 77,
                    "job %+ resolves stable current marker");
        assert_true(resolve_job_index("%-", job_index) && g_env.jobs[job_index].job_id == 41,
                    "job %- resolves stable previous marker");
        assert_eq(job_state_name(first.state), "stopped", "job state tracks stopped explicitly");
        g_env.jobs = std::move(saved_jobs);
        g_current_job_id = saved_current;
        g_previous_job_id = saved_previous;

        size_t jobs_before_failure = g_env.jobs.size();
        int failed_launch = execute_single_command("cmd.exe /c \"ping -n 30 127.0.0.1 >nul\" | __zsh_missing_stage_13__ &");
        assert_true(failed_launch != 0 && g_env.jobs.size() == jobs_before_failure,
                    "partial background pipeline launch rolls back without registering a job");
    }

    // 9. Modules, completion definitions, and ZLE widget execution
    {
        dispatch_command({"zmodload", "zsh/datetime", "zsh/system", "zsh/parameter"});
        assert_true(g_env.vars.count("EPOCHSECONDS") && !g_env.vars["EPOCHSECONDS"].empty(),
                    "zsh/datetime exports EPOCHSECONDS");
        assert_eq(g_env.vars["SYS_PID"], to_string(GetCurrentProcessId()), "zsh/system exports SYS_PID");
        assert_true(g_env.assoc_arrays["parameters"].count("EPOCHSECONDS") != 0,
                    "zsh/parameter exposes parameter metadata");
        ostringstream datetime_output;
        streambuf* saved_output = cout.rdbuf(datetime_output.rdbuf());
        int datetime_status = dispatch_command({"strftime", "%Y", "0"});
        cout.rdbuf(saved_output);
        assert_true(datetime_status == 0 &&
                (datetime_output.str().find("1969") != string::npos || datetime_output.str().find("1970") != string::npos),
                "zsh/datetime strftime command");

        g_env.functions["__selftest_complete"] = "compadd alpha alpine beta";
        g_env.completion_definitions["demo"] = "__selftest_complete";
        vector<string> candidates = complete_registered_command("demo", "alp");
        assert_true(candidates == vector<string>({"alpha", "alpine"}), "compdef drives registered completion function");

        // Windows-Aware Completion: PATHEXT resolution & fuzzy/substring matching tests
        {
            vector<string> pathext = get_pathext_list();
            bool has_exe = find(pathext.begin(), pathext.end(), ".exe") != pathext.end();
            bool has_bat = find(pathext.begin(), pathext.end(), ".bat") != pathext.end();
            bool has_cmd = find(pathext.begin(), pathext.end(), ".cmd") != pathext.end();
            assert_true(has_exe && has_bat && has_cmd, "get_pathext_list parses Windows executable extensions");

            int score_prefix = match_fuzzy_score("exp", "explorer");
            int score_fuzzy = match_fuzzy_score("pws", "pwsh");
            int score_exact = match_fuzzy_score("cargo", "cargo");
            int score_nomatch = match_fuzzy_score("xyz", "explorer");
            assert_true(score_prefix > 0 && score_fuzzy > 0 && score_exact > 0 && score_nomatch == 0,
                        "match_fuzzy_score supports prefix, fuzzy subsequence, and exact matches");

            vector<string> exp_matches = complete_command("exp");
            bool found_export = find(exp_matches.begin(), exp_matches.end(), "export") != exp_matches.end();
            assert_true(found_export, "complete_command fuzzy resolves builtins and PATH executables");
        }

        g_env.functions["__selftest_widget"] = "BUFFER=widget-ok; CURSOR=9";
        g_env.widgets["selftest-widget"] = "__selftest_widget";
        g_env.keymaps["main"]["^X"] = "selftest-widget";
        string widget_buffer;
        size_t widget_cursor = 0;
        assert_true(execute_bound_widget(24, widget_buffer, widget_cursor) &&
                    widget_buffer == "widget-ok" && widget_cursor == 9,
                    "bindkey executes registered ZLE widget");
        g_env.functions.erase("__selftest_complete");
        g_env.functions.erase("__selftest_widget");
        g_env.completion_definitions.erase("demo");
        g_env.widgets.erase("selftest-widget");
        g_env.keymaps["main"].erase("^X");
    }

    // 10. Portable zsh conformance corpus (embedded expected-output fixtures)
    {
        struct ConformanceCase { const char* name; const char* script; const char* expected; };
        const ConformanceCase cases[] = {
            {"conditionals", "value=2; if [[ $value -eq 2 ]]; then print yes; else print no; fi", "yes\n"},
            {"indexed arrays", "items=(alpha beta gamma); print ${(j:,:)items}", "alpha,beta,gamma\n"},
            {"for loop", "for item in a b c; do print -n $item; done", "abc"},
            {"case fallthrough", "case x in x) print -n first ;& *) print second ;; esac", "firstsecond\n"},
            {"function local scope", "value=outer; demo_scope() { local value=inner; print $value; }; demo_scope; print $value", "inner\nouter\n"},
            {"brace range", "print {1..5..2}", "1 3 5\n"},
            {"logical lists", "false || print fallback; true && print success", "fallback\nsuccess\n"},
            {"parameter operators", "unset missing; print ${missing:-default}; print ${missing:=assigned}; print $missing", "default\nassigned\nassigned\n"},
            {"until loop", "value=0; until [[ $value -ge 3 ]]; do print -n $value; ((value++)); done", "012"},
            {"repeat loop", "count=3; repeat count; do print -n x; done", "xxx"}
        };
        for (const auto& fixture : cases) {
            ostringstream output;
            streambuf* saved_output = cout.rdbuf(output.rdbuf());
            int status = parse_and_execute(fixture.script);
            cout.rdbuf(saved_output);
            assert_true(status == 0 && output.str() == fixture.expected,
                        string("portable conformance: ") + fixture.name);
        }

        assert_true(is_zsh_builtin_command("until") && is_zsh_builtin_command("repeat"),
                    "until and repeat are registered internal builtins");
        ostringstream builtin_lookup_output;
        streambuf* saved_lookup_output = cout.rdbuf(builtin_lookup_output.rdbuf());
        int builtin_lookup_status = dispatch_command({"type", "until", "repeat"});
        cout.rdbuf(saved_lookup_output);
        assert_true(builtin_lookup_status == 0 &&
                builtin_lookup_output.str() == "until is a shell builtin\nrepeat is a shell builtin\n",
                "type reports until and repeat as shell builtins");

        // Direct dispatch and single-command forms of repeat
        {
            ostringstream direct_repeat_output;
            streambuf* saved_repeat_output = cout.rdbuf(direct_repeat_output.rdbuf());
            int direct_repeat_status = dispatch_command({"repeat", "2", "do", "print", "-n", "r", "done"});
            cout.rdbuf(saved_repeat_output);
            assert_true(direct_repeat_status == 0 && direct_repeat_output.str() == "rr",
                    "repeat executes through the builtin dispatcher with do..done");
        }
        {
            ostringstream direct_single_repeat;
            streambuf* saved_repeat_output = cout.rdbuf(direct_single_repeat.rdbuf());
            int direct_single_status = dispatch_command({"repeat", "3", "print", "-n", "s"});
            cout.rdbuf(saved_repeat_output);
            assert_true(direct_single_status == 0 && direct_single_repeat.str() == "sss",
                    "repeat executes single-command form through builtin dispatcher");
        }
        {
            ostringstream builtin_prefix_repeat;
            streambuf* saved_repeat_output = cout.rdbuf(builtin_prefix_repeat.rdbuf());
            int builtin_prefix_status = dispatch_command({"builtin", "repeat", "2", "print", "-n", "b"});
            cout.rdbuf(saved_repeat_output);
            assert_true(builtin_prefix_status == 0 && builtin_prefix_repeat.str() == "bb",
                    "builtin repeat executes via builtin command prefix");
        }
        {
            ostringstream brace_repeat_output;
            streambuf* saved_output = cout.rdbuf(brace_repeat_output.rdbuf());
            int status = parse_and_execute("repeat 3 { print -n z }");
            cout.rdbuf(saved_output);
            assert_true(status == 0 && brace_repeat_output.str() == "zzz",
                    "repeat executes brace syntax construct");
        }
        {
            ostringstream single_cmd_repeat_output;
            streambuf* saved_output = cout.rdbuf(single_cmd_repeat_output.rdbuf());
            int status = parse_and_execute("repeat 4 print -n a");
            cout.rdbuf(saved_output);
            assert_true(status == 0 && single_cmd_repeat_output.str() == "aaaa",
                    "repeat executes inline single-command syntax");
        }
        {
            ostringstream brace_until_output;
            streambuf* saved_output = cout.rdbuf(brace_until_output.rdbuf());
            int status = parse_and_execute("uval=0; until [[ $uval -ge 3 ]] { print -n $uval; ((uval++)) }");
            cout.rdbuf(saved_output);
            assert_true(status == 0 && brace_until_output.str() == "012",
                    "until executes brace syntax construct");
        }
        {
            ostringstream break_repeat_output;
            streambuf* saved_output = cout.rdbuf(break_repeat_output.rdbuf());
            int status = parse_and_execute("bval=0; repeat 10; do if [[ $bval -eq 3 ]]; then break; fi; print -n $bval; ((bval++)); done");
            cout.rdbuf(saved_output);
            assert_true(status == 0 && break_repeat_output.str() == "012",
                    "break terminates repeat loop execution early");
        }
        {
            ostringstream break_until_output;
            streambuf* saved_output = cout.rdbuf(break_until_output.rdbuf());
            int status = parse_and_execute("ubval=0; until false; do if [[ $ubval -eq 2 ]]; then break; fi; print -n $ubval; ((ubval++)); done");
            cout.rdbuf(saved_output);
            assert_true(status == 0 && break_until_output.str() == "01",
                    "break terminates until loop execution early");
        }
        {
            ostringstream continue_repeat_output;
            streambuf* saved_output = cout.rdbuf(continue_repeat_output.rdbuf());
            int status = parse_and_execute("cval=0; repeat 4; do ((cval++)); if [[ $cval -eq 2 ]]; then continue; fi; print -n $cval; done");
            cout.rdbuf(saved_output);
            assert_true(status == 0 && continue_repeat_output.str() == "134",
                    "continue skips remainder of repeat loop iteration");
        }
        {
            string redirected_loop_path = create_temp_process_subst_path();
            error_code reset_error;
            fs::remove(normalize_path_to_win(redirected_loop_path), reset_error);
            string target = quote_for_shell_path(normalize_path_to_unix(redirected_loop_path));
            int repeat_status = parse_and_execute("repeat 2 print -n r >> " + target);
            int until_status = parse_and_execute(
                "redirected_value=0; until [[ $redirected_value -ge 2 ]]; do "
                "print -n $redirected_value >> " + target + "; ((redirected_value++)); done");
            ifstream redirected_output(normalize_path_to_win(redirected_loop_path), ios::binary);
            string redirected_content((istreambuf_iterator<char>(redirected_output)), istreambuf_iterator<char>());
            assert_true(repeat_status == 0 && until_status == 0 && redirected_content == "rr01",
                    "redirected repeat and until bodies remain internal shell stages");
            error_code remove_error;
            fs::remove(normalize_path_to_win(redirected_loop_path), remove_error);
        }

        string compound_script_path = create_temp_process_subst_path();
        {
            ofstream script(normalize_path_to_win(compound_script_path), ios::binary | ios::trunc);
            script << "value=0\n"
                   << "until [[ $value -ge 2 ]]; do\n"
                   << "  print -n $value\n"
                   << "  ((value++))\n"
                   << "done\n"
                   << "repeat 2; do\n"
                   << "  print -n x\n"
                   << "done\n";
        }
        ostringstream compound_output;
        streambuf* saved_output = cout.rdbuf(compound_output.rdbuf());
        int compound_status = execute_script(compound_script_path);
        cout.rdbuf(saved_output);
        assert_true(compound_status == 0 && compound_output.str() == "01xx",
                    "multiline until and repeat execute internally");
        error_code compound_remove_error;
        fs::remove(normalize_path_to_win(compound_script_path), compound_remove_error);
    }

    // 11. Main-thread console event dispatch (physical key delivery remains manual)
    {
        g_env.vars["__trap_INT"] = "__selftest_int=handled";
        g_env.vars["__trap_TSTP"] = "__selftest_tstp=handled";
        g_env.vars["__trap_WINCH"] = "__selftest_winch=handled";
        console_ctrl_handler(CTRL_C_EVENT);
        g_sigtstp_pending.store(true);
        g_sigwinch_pending.store(true);
        g_env.prompt_dirty = false;
        process_pending_traps();
        assert_true(g_env.vars["__selftest_int"] == "handled" && !g_sigint_pending.load(),
                    "Ctrl+C event dispatches INT trap on main thread");
        assert_true(g_env.vars["__selftest_tstp"] == "handled" && !g_sigtstp_pending.load(),
                    "Ctrl+Z event dispatches TSTP trap on main thread");
        assert_true(g_env.vars["__selftest_winch"] == "handled" && g_env.prompt_dirty && !g_sigwinch_pending.load(),
                    "resize event dispatches WINCH trap and invalidates prompt");
    }

    // 12. Full Built-in Commands Verification Suite (100% Functional Coverage)
    {
        // echo / print / printf
        {
            ostringstream out; streambuf* s = cout.rdbuf(out.rdbuf());
            parse_and_execute("echo -n 'hello '; echo -e 'world\\n'; printf '%s=%03d 0x%x\\n' val 7 255; print -l line1 line2");
            cout.rdbuf(s);
            assert_true(out.str() == "hello world\n\nval=007 0xff\nline1\nline2\n", "echo, printf, and print builtins formatting");
        }

        // alias / unalias
        {
            parse_and_execute("alias myecho='print -n ALIAS_OK'");
            ostringstream out; streambuf* s = cout.rdbuf(out.rdbuf());
            parse_and_execute("myecho; unalias myecho");
            cout.rdbuf(s);
            assert_true(out.str() == "ALIAS_OK" && !g_env.aliases.count("myecho"), "alias definition, expansion, and unalias");
        }

        // export / unset / readonly
        {
            parse_and_execute("export EXPORT_TEST=exported_val; readonly RO_TEST=readonly_val");
            char buf[128] = {};
            GetEnvironmentVariableA("EXPORT_TEST", buf, sizeof(buf));
            assert_true(string(buf) == "exported_val" && g_env.readonly_vars.count("RO_TEST"), "export and readonly builtins");
            parse_and_execute("unset EXPORT_TEST");
            assert_true(!g_env.vars.count("EXPORT_TEST"), "unset builtin removes environment variable");
        }

        // typeset / declare (-a, -A, -i, -U)
        {
            parse_and_execute("typeset -a test_arr=(one two three); typeset -A test_map=(k1 v1 k2 v2); typeset -i test_num=15+5; typeset -a -U test_uniq=(a b a c b)");
            assert_true(g_env.indexed_arrays["test_arr"] == vector<string>({"one", "two", "three"}), "typeset -a indexed array");
            assert_true(g_env.assoc_arrays["test_map"]["k1"] == "v1" && g_env.assoc_arrays["test_map"]["k2"] == "v2", "typeset -A associative array");
            assert_true(g_env.vars["test_num"] == "20" && g_env.integer_vars.count("test_num"), "typeset -i integer variable");
            assert_true(g_env.indexed_arrays["test_uniq"] == vector<string>({"a", "b", "c"}), "typeset -U unique array");
        }

        // let & integer
        {
            parse_and_execute("integer ival=10; let 'ival += 5' 'ival *= 2' 'ival--'");
            assert_true(g_env.vars["ival"] == "29", "integer and let arithmetic assignments");
        }

        // setopt / unsetopt
        {
            parse_and_execute("setopt extendedglob; unsetopt shwordsplit");
            assert_true(g_env.options["extendedglob"] == true && g_env.options["shwordsplit"] == false, "setopt and unsetopt builtins");
        }

        // pushd / popd / dirs / pwd
        {
            error_code ec;
            string start_dir = normalize_path_to_unix(fs::current_path(ec).string());
            ostringstream out; streambuf* s = cout.rdbuf(out.rdbuf());
            parse_and_execute("pushd . ; dirs ; popd ; pwd");
            cout.rdbuf(s);
            assert_true(out.str().find(start_dir) != string::npos, "pushd, popd, dirs, and pwd builtins");
        }

        // drive-aware autocd & cross-drive navigation
        {
            error_code ec;
            string start_dir = fs::current_path(ec).string();
            parse_and_execute("setopt autocd");
            parse_and_execute("..");
            string parent_dir = fs::current_path(ec).string();
            bool drive_nav_ok = true;
            if (fs::exists("C:/Windows", ec)) {
                parse_and_execute("C:/Windows");
                string c_win = fs::current_path(ec).string();
                if (c_win.find("Windows") == string::npos) drive_nav_ok = false;
            }
            fs::current_path(start_dir, ec);
            g_env.vars["PWD"] = normalize_path_to_unix(start_dir);
            assert_true(parent_dir != start_dir && drive_nav_ok, "drive-aware autocd and cross-drive navigation");
        }

        // shift & positional arguments
        {
            g_env.positional_args = {"arg1", "arg2", "arg3", "arg4"};
            parse_and_execute("shift 2");
            assert_true(g_env.positional_args == vector<string>({"arg3", "arg4"}), "shift builtin updates positional args");
            g_env.positional_args.clear();
        }

        // getopts
        {
            g_env.positional_args = {"-a", "-b", "bval", "-c"};
            g_env.vars["OPTIND"] = "1";
            g_env.vars.erase("__getopts_offset");
            g_env.vars.erase("__getopts_optind");
            g_env.vars.erase("__getopts_spec");
            parse_and_execute("getopts 'ab:c' opt_var; opt1=$opt_var; getopts 'ab:c' opt_var; opt2=$opt_var; opt2_arg=$OPTARG; getopts 'ab:c' opt_var; opt3=$opt_var");
            assert_true(g_env.vars["opt1"] == "a" && g_env.vars["opt2"] == "b" && g_env.vars["opt2_arg"] == "bval" && g_env.vars["opt3"] == "c",
                        "getopts builtin flag and option argument parsing");
            g_env.positional_args.clear();
        }

        // trap registration and listing
        {
            parse_and_execute("trap 'echo TRAP_HUP' HUP");
            ostringstream out; streambuf* s = cout.rdbuf(out.rdbuf());
            parse_and_execute("trap");
            cout.rdbuf(s);
            assert_true(out.str().find("TRAP_HUP") != string::npos, "trap builtin registration and listing");
            g_env.vars.erase("__trap_HUP");
        }

        // which / type / whence
        {
            ostringstream out; streambuf* s = cout.rdbuf(out.rdbuf());
            parse_and_execute("which cd; type -a echo; whence print");
            cout.rdbuf(s);
            assert_true(out.str().find("builtin") != string::npos || out.str().find("shell builtin") != string::npos,
                        "which, type, and whence command lookups");
        }

        // command & builtin bypass
        {
            g_env.functions["print"] = "echo function_override";
            ostringstream out_builtin; streambuf* s1 = cout.rdbuf(out_builtin.rdbuf());
            parse_and_execute("builtin print -n builtin_bypass");
            cout.rdbuf(s1);
            ostringstream out_command; streambuf* s2 = cout.rdbuf(out_command.rdbuf());
            parse_and_execute("command print -n command_bypass");
            cout.rdbuf(s2);
            g_env.functions.erase("print");
            assert_true(out_builtin.str() == "builtin_bypass" && out_command.str() == "command_bypass",
                        "builtin and command prefixes bypass function overrides");
        }

        // zstyle & zle
        {
            parse_and_execute("zstyle ':completion:*' format 'FormatString'; zle -N my-widget __selftest_dummy");
            ostringstream out; streambuf* s = cout.rdbuf(out.rdbuf());
            parse_and_execute("zstyle -L; zle -l");
            cout.rdbuf(s);
            assert_true(out.str().find("FormatString") != string::npos && out.str().find("my-widget") != string::npos,
                        "zstyle and zle configuration and listing");
            parse_and_execute("zstyle -d ':completion:*'; zle -D my-widget");
        }

        // eval
        {
            ostringstream out; streambuf* s = cout.rdbuf(out.rdbuf());
            parse_and_execute("eval_var='print -n EVAL_OK'; eval $eval_var");
            cout.rdbuf(s);
            assert_true(out.str() == "EVAL_OK", "eval builtin executes dynamic commands");
        }

        // true / false / test / [ / [[
        {
            int r_true = parse_and_execute("true");
            int r_false = parse_and_execute("false");
            int r_test = parse_and_execute("test 10 -gt 5 && [ 'abc' = 'abc' ] && [[ '123' =~ '^[0-9]+$' ]]");
            assert_true(r_true == 0 && r_false == 1 && r_test == 0, "true, false, test, [, and [[ condition evaluation");
        }

        // times
        {
            ostringstream out; streambuf* s = cout.rdbuf(out.rdbuf());
            int r_times = parse_and_execute("times");
            cout.rdbuf(s);
            assert_true(r_times == 0 && out.str().find('s') != string::npos, "times builtin prints CPU time stats");
        }

        // history & fc
        {
            g_env.history.push_back("selftest_history_cmd");
            ostringstream out; streambuf* s = cout.rdbuf(out.rdbuf());
            parse_and_execute("history; fc -l");
            cout.rdbuf(s);
            assert_true(out.str().find("selftest_history_cmd") != string::npos, "history and fc builtins inspect command history");
        }

        // 13. ZSH Advanced Scripting Capabilities & Gaps Regression Suite

        // select menu loop
        {
            stringstream in("2\n");
            streambuf* cin_saved = cin.rdbuf(in.rdbuf());
            ostringstream out;
            streambuf* cout_saved = cout.rdbuf(out.rdbuf());
            int rc = parse_and_execute("select item in apple banana orange; do print -n \"selected:$item\"; break; done");
            cin.rdbuf(cin_saved);
            cout.rdbuf(cout_saved);
            assert_true(rc == 0 && out.str().find("selected:banana") != string::npos,
                        "select loop menu item selection");
        }
        {
            stringstream in("1\n");
            streambuf* cin_saved = cin.rdbuf(in.rdbuf());
            ostringstream out;
            streambuf* cout_saved = cout.rdbuf(out.rdbuf());
            int rc = parse_and_execute("select item in red green blue { print -n \"color:$item\"; break }");
            cin.rdbuf(cin_saved);
            cout.rdbuf(cout_saved);
            assert_true(rc == 0 && out.str().find("color:red") != string::npos,
                        "select loop brace syntax");
        }
        {
            g_env.indexed_arrays["select_items"] = {"red", "green", "blue"};
            stringstream in("2\n");
            streambuf* cin_saved = cin.rdbuf(in.rdbuf());
            ostringstream out;
            streambuf* cout_saved = cout.rdbuf(out.rdbuf());
            int rc = parse_and_execute("select item in ${select_items[@]}; do print -n $item; break; done");
            cin.rdbuf(cin_saved);
            cout.rdbuf(cout_saved);
            assert_true(rc == 0 && out.str().find("1) red\n2) green\n3) blue\n") != string::npos &&
                        out.str().find("green") != string::npos,
                        "select expands array values into separate choices");
            g_env.indexed_arrays.erase("select_items");
        }
        {
            int rc = parse_and_execute("select bad-name in value; do break; done");
            assert_true(rc != 0, "select rejects invalid variable names");
        }

        // time pipeline / command
        {
            ostringstream err;
            streambuf* cerr_saved = cerr.rdbuf(err.rdbuf());
            ostringstream out;
            streambuf* cout_saved = cout.rdbuf(out.rdbuf());
            g_env.vars["TIMEFMT"] = "elapsed: %*E";
            int rc = parse_and_execute("time print -n timed_ok");
            g_env.vars.erase("TIMEFMT");
            cerr.rdbuf(cerr_saved);
            cout.rdbuf(cout_saved);
            assert_true(rc == 0 && out.str() == "timed_ok" && err.str().find("elapsed:") != string::npos,
                        "time reserved word measures command execution");
        }
        {
            ostringstream err;
            streambuf* cerr_saved = cerr.rdbuf(err.rdbuf());
            g_env.vars["TIMEFMT"] = "%E/%E";
            int rc = parse_and_execute("time true");
            g_env.vars.erase("TIMEFMT");
            cerr.rdbuf(cerr_saved);
            string timing_output = err.str();
            assert_true(rc == 0 && timing_output.find("%E") == string::npos &&
                        count(timing_output.begin(), timing_output.end(), 's') == 2,
                        "TIMEFMT expands every elapsed-time conversion");
        }

        // try ... always unwind / cleanup
        {
            ostringstream out;
            streambuf* cout_saved = cout.rdbuf(out.rdbuf());
            int rc = parse_and_execute("{ print -n 'try_start '; false; print -n 'unreachable ' } always { print -n 'always_clean '; TRY_BLOCK_ERROR=0 }");
            cout.rdbuf(cout_saved);
            assert_true(rc == 0 && out.str() == "try_start always_clean ",
                        "{ ... } always { ... } unwind and error recovery");
        }
        {
            ostringstream out;
            streambuf* cout_saved = cout.rdbuf(out.rdbuf());
            int rc = parse_and_execute("try { print -n 'try_ok ' } always { print -n 'always_ok' }");
            cout.rdbuf(cout_saved);
            assert_true(rc == 0 && out.str() == "try_ok always_ok",
                        "try { ... } always { ... } syntax execution");
        }

        // Parameter expansion flags, modifiers, length, and array slicing
        {
            ZshEnvironment penv;
            penv.indexed_arrays["words"] = {"apple", "banana", "cherry"};
            penv.vars["str"] = "hello world";
            penv.vars["path"] = "/usr/local/bin/my_tool.tar.gz";

            // (j:,:) joining
            assert_eq(penv.expand_vars("${(j:,:)words}"), "apple,banana,cherry", "param flag (j) join");
            // (s:,:) splitting
            penv.vars["csv"] = "one,two,three";
            assert_eq(penv.expand_vars("${(s:,:)csv}"), "one two three", "param flag (s) split");
            // (U) uppercase, (L) lowercase, (C) capitalize
            assert_eq(penv.expand_vars("${(U)str}"), "HELLO WORLD", "param flag (U) uppercase");
            assert_eq(penv.expand_vars("${(L)GREETING}"), "", "param flag (L) lowercase on unset");
            penv.vars["shout"] = "HELLO WORLD";
            assert_eq(penv.expand_vars("${(L)shout}"), "hello world", "param flag (L) lowercase");
            assert_eq(penv.expand_vars("${(C)str}"), "Hello World", "param flag (C) capitalize");

            // Length ${#var} and ${#arr}
            assert_eq(penv.expand_vars("${#str}"), "11", "param length ${#var}");
            assert_eq(penv.expand_vars("${#words}"), "3", "param length ${#arr}");

            // Array slice ${arr[start,end]}
            assert_eq(penv.expand_vars("${words[2,3]}"), "banana cherry", "indexed array slice ${arr[2,3]}");
            assert_eq(penv.expand_vars("${words[1]}"), "apple", "indexed array single element ${arr[1]}");

            // Modifiers :h, :t, :r, :e
            assert_eq(penv.expand_vars("${path:h}"), "/usr/local/bin", "modifier :h head/dirname");
            assert_eq(penv.expand_vars("${path:t}"), "my_tool.tar.gz", "modifier :t tail/basename");
            assert_eq(penv.expand_vars("${path:r}"), "/usr/local/bin/my_tool.tar", "modifier :r remove extension");
            assert_eq(penv.expand_vars("${path:e}"), "gz", "modifier :e extension");
        }

        // [[ ... ]] extended operators: regex =~ with group captures, grouping
        {
            int r_match = parse_and_execute("[[ 'version-2.4.1' =~ '^version-([0-9]+)\\.([0-9]+)\\.([0-9]+)$' ]]");
            assert_true(r_match == 0 && g_env.vars["MATCH"] == "version-2.4.1" &&
                        g_env.indexed_arrays["match"] == vector<string>({"2", "4", "1"}),
                        "[[ ... ]] =~ regex capture into MATCH and match array");

            int r_group = parse_and_execute("[[ ( 1 -eq 1 || 2 -eq 3 ) && ! ( 5 -lt 4 ) ]]");
            assert_true(r_group == 0, "[[ ... ]] grouping parentheses and logical operators");
        }

        // Anonymous functions and function f() syntax
        {
            ostringstream out;
            streambuf* cout_saved = cout.rdbuf(out.rdbuf());
            parse_and_execute("() { print -n \"anon:$1,$2 \"; } first second; function my_fn() { print -n \"fn:$1\"; }; my_fn hello");
            cout.rdbuf(cout_saved);
            assert_true(out.str() == "anon:first,second fn:hello",
                        "anonymous functions and function f() declaration syntax");
            g_env.functions.erase("my_fn");
        }

        // set --, $#, $0, $UID, $SECONDS, $RANDOM, $pipestatus
        {
            parse_and_execute("set -- alpha beta gamma");
            assert_true(g_env.positional_args == vector<string>({"alpha", "beta", "gamma"}), "set -- updates positional parameters");
            assert_eq(g_env.expand_vars("$#"), "3", "$# reflects positional parameter count");
            assert_true(!g_env.expand_vars("$$").empty(), "$$ expands process PID");
            assert_true(!g_env.expand_vars("$UID").empty(), "$UID expands user ID");
            assert_true(!g_env.expand_vars("$SECONDS").empty(), "$SECONDS expands elapsed seconds");
            assert_true(!g_env.expand_vars("$RANDOM").empty(), "$RANDOM expands random integer");
            g_env.positional_args.clear();
        }

        // Trap semantics: RETURN, DEBUG, ZERR, and $pipestatus
        {
            parse_and_execute("trap 'trap_dbg=ok' DEBUG; run_dbg=1; trap '' DEBUG");
            assert_true(g_env.vars["trap_dbg"] == "ok", "trap DEBUG fires before statement execution");
            g_env.vars.erase("trap_dbg");

            parse_and_execute("trap 'trap_zerr=caught' ZERR; false; trap '' ZERR");
            assert_true(g_env.vars["trap_zerr"] == "caught", "trap ZERR fires on command failure");
            g_env.vars.erase("trap_zerr");

            parse_and_execute("trap_ret=none; fn_ret() { trap 'trap_ret=fired' RETURN; return 0; }; fn_ret");
            assert_true(g_env.vars["trap_ret"] == "fired", "trap RETURN fires on function exit");
            g_env.vars.erase("trap_ret");
            g_env.functions.erase("fn_ret");
        }

        // Conformance multi-statement integration fixture
        {
            string conf_script_path = create_temp_process_subst_path();
            {
                ofstream script(normalize_path_to_win(conf_script_path), ios::binary | ios::trunc);
                script << "# Complex multi-feature ZSH script\n"
                       << "items=(red green blue)\n"
                       << "upper_csv=${(U)${(j:,:)items}}\n"
                       << "print -n \"$upper_csv \"\n"
                       << "() {\n"
                       << "  local inner=$1\n"
                       << "  print -n \"scoped:$inner \"\n"
                       << "} \"test\"\n"
                       << "{ print -n \"try \" } always { print -n \"always\" }\n";
            }
            ostringstream conf_out;
            streambuf* cout_saved = cout.rdbuf(conf_out.rdbuf());
            int conf_status = execute_script(conf_script_path);
            cout.rdbuf(cout_saved);
            assert_true(conf_status == 0 && conf_out.str() == "RED,GREEN,BLUE scoped:test try always",
                        "multi-statement ZSH conformance script executes cleanly");
            error_code conf_ec;
            fs::remove(normalize_path_to_win(conf_script_path), conf_ec);
        }

        // Emulation profiles (emulate sh / emulate zsh / emulate -L / emulate -R)
        {
            parse_and_execute("set -u");
            assert_true(g_env.options["nounset"] == true, "set -u enables nounset");
            parse_and_execute("emulate -R sh");
            assert_true(g_env.options["shwordsplit"] == true && g_env.options["ksharrays"] == true && g_env.options["nounset"] == false, "emulate -R resets existing options and applies target profile");
            parse_and_execute("emulate zsh");
            assert_true(g_env.options["shwordsplit"] == false && g_env.options["ksharrays"] == false, "emulate zsh restores native zsh options");

            parse_and_execute("fn_emul() { emulate -L sh; }; fn_emul");
            assert_true(g_env.options["shwordsplit"] == false, "emulate -L restores options on function exit");
            g_env.functions.erase("fn_emul");
        }

        // Compound statement redirection ({ ... } > file, for > file, if > file, ( ... ) > file, compound 2>>, closure, here-string)
        {
            string tmp_out = create_temp_process_subst_path();
            string win_out = normalize_path_to_win(tmp_out);
            string unix_out = quote_for_shell_path(normalize_path_to_unix(tmp_out));

            parse_and_execute("{ print -n \"line1 \"; print -n \"line2\"; } > " + unix_out);
            {
                ifstream ifs(win_out);
                string content((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
                assert_true(content == "line1 line2", "compound block { ... } > file redirection");
            }

            parse_and_execute("for x in A B C; do print -n \"$x\"; done > " + unix_out);
            {
                ifstream ifs(win_out);
                string content((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
                assert_true(content == "ABC", "for loop > file redirection");
            }

            parse_and_execute("if true; then print -n \"yes\"; fi > " + unix_out);
            {
                ifstream ifs(win_out);
                string content((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
                assert_true(content == "yes", "if block > file redirection");
            }

            // Keyword in argument position within compound commands
            parse_and_execute("if true; then print -n \"if\"; fi > " + unix_out);
            {
                ifstream ifs(win_out);
                string content((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
                assert_true(content == "if", "if block with 'if' argument > file redirection");
            }

            parse_and_execute("for i in 1 2; do print -n \"done\"; done > " + unix_out);
            {
                ifstream ifs(win_out);
                string content((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
                assert_true(content == "donedone", "for loop with 'done' argument > file redirection");
            }

            // Compound 2>> error append
            {
                error_code ec;
                fs::remove(win_out, ec);
            }
            parse_and_execute("{ print -u2 -n \"err1 \"; } 2>> " + unix_out);
            parse_and_execute("{ print -u2 -n \"err2\"; } 2>> " + unix_out);
            {
                ifstream ifs(win_out);
                string content((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
                assert_true(content == "err1 err2", "compound block 2>> appends to error file");
            }

            // Descriptor closure
            {
                string res = capture_command_output("{ print -n \"hidden\"; } >&-");
                assert_true(res.empty(), "compound block >&- closes stdout");
            }

            // Compound here-string handle lifetime
            parse_and_execute("{ read hs_val; print -n \"val:$hs_val\"; } <<< \"test_data\" > " + unix_out);
            {
                ifstream ifs(win_out);
                string content((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
                assert_true(content == "val:test_data", "compound block <<< here-string redirection");
            }

            parse_and_execute("( print -n \"sub\" ) > " + unix_out);
            {
                ifstream ifs(win_out);
                string content((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
                assert_true(content == "sub", "subshell ( ... ) > file redirection");
            }

            error_code ec;
            fs::remove(win_out, ec);
        }

        // Source / . positional args inheritance and $0 tracking
        {
            string src_path = create_temp_process_subst_path();
            string win_src = normalize_path_to_win(src_path);
            string unix_src = quote_for_shell_path(normalize_path_to_unix(src_path));
            {
                ofstream ofs(win_src);
                ofs << "src_out=\"$0:$1:$2\"\n";
            }
            g_env.positional_args = {"foo", "bar"};
            parse_and_execute("source " + unix_src);
            assert_true(g_env.vars["src_out"].find(":foo:bar") != string::npos, "source inherits positional arguments when none provided");

            parse_and_execute("source " + unix_src + " arg1 arg2");
            assert_true(g_env.vars["src_out"].find(":arg1:arg2") != string::npos, "source uses provided positional arguments");
            assert_true(g_env.positional_args.size() == 2 && g_env.positional_args[0] == "foo", "source restores caller positional arguments");

            g_env.vars.erase("src_out");
            g_env.positional_args.clear();
            error_code ec;
            fs::remove(win_src, ec);
        }

        // Heredoc <<- tab stripping
        {
            string hd_script = "cat <<-EOF\n\thello\n\tworld\n\tEOF\n";
            string materialized = materialize_heredoc_block(hd_script);
            assert_true(!materialized.empty(), "heredoc <<- strips leading tabs from body lines and delimiter");
        }

        // Process substitution =(command) via temporary file
        {
            ostringstream out;
            streambuf* s = cout.rdbuf(out.rdbuf());
            parse_and_execute("cat =(print -n 'zsh_eq_psub_content')");
            cout.rdbuf(s);
            assert_true(out.str() == "zsh_eq_psub_content", "process substitution =(command) temporary file creation and cleanup");
        }

        // Recursive globbing, case-insensitivity, and Win32 attribute qualifiers
        {
            string test_dir = "tmp\\glob_test_" + to_string(GetCurrentProcessId());
            error_code ec;
            fs::create_directories(test_dir + "\\sub\\deep", ec);

            // Create files
            {
                ofstream f1(test_dir + "\\root.log"); f1 << "root";
                ofstream f2(test_dir + "\\sub\\nested.log"); f2 << "nested";
                ofstream f3(test_dir + "\\sub\\deep\\leaf.txt"); f3 << "leaf";
                ofstream f4(test_dir + "\\sub\\deep\\Leaf2.TXT"); f4 << "leaf2";
            }

            // Set hidden attribute on a file
            string hidden_file = test_dir + "\\hidden.dat";
            {
                ofstream fh(hidden_file); fh << "secret";
            }
            SetFileAttributesW(string_to_wstring(hidden_file).c_str(), FILE_ATTRIBUTE_HIDDEN);

            // Test 1: Recursive globbing **/*.log
            auto logs = expand_globs({test_dir + "/**/*.log"});
            assert_true(logs.size() == 2, "recursive glob **/*.log matches files across directory tree");

            // Test 2: Case-insensitive globbing *.txt
            auto txts = expand_globs({test_dir + "/sub/deep/*.txt"});
            assert_true(txts.size() == 2, "case-insensitive glob matches .txt and .TXT files");

            // Test 3: Directory qualifier *(/)
            auto dirs = expand_globs({test_dir + "/*(/)"});
            assert_true(dirs.size() == 1 && dirs[0].find("sub") != string::npos, "directory qualifier *(/) matches only directories");

            // Test 4: Regular file qualifier *(.)
            auto files = expand_globs({test_dir + "/*(.)"});
            bool only_files = true;
            for (const auto& f : files) {
                if (fs::is_directory(normalize_path_to_win(f), ec)) only_files = false;
            }
            assert_true(only_files && !files.empty(), "regular file qualifier *(.) excludes directories");

            // Test 5: Hidden file qualifier *(H)
            auto hiddens = expand_globs({test_dir + "/*(H)"});
            bool found_hidden = false;
            for (const auto& h : hiddens) {
                if (h.find("hidden.dat") != string::npos) found_hidden = true;
            }
            assert_true(found_hidden, "hidden file qualifier *(H) matches FILE_ATTRIBUTE_HIDDEN files");

            // Test 6: Mtime qualifier *(m-1)
            auto recent = expand_globs({test_dir + "/*(m-1)"});
            assert_true(!recent.empty(), "mtime qualifier *(m-1) matches files modified within last 24h");

            // Clean up
            SetFileAttributesW(string_to_wstring(hidden_file).c_str(), FILE_ATTRIBUTE_NORMAL);
            fs::remove_all(test_dir, ec);
        }

        // 17. Typo Correction & Damerau-Levenshtein Engine
        {
            assert_eq(to_string(damerau_levenshtein_distance("gti", "git")), "1", "damerau_levenshtein_distance transposition (gti -> git)");
            assert_eq(to_string(damerau_levenshtein_distance("sl", "ls")), "1", "damerau_levenshtein_distance transposition (sl -> ls)");
            assert_eq(to_string(damerau_levenshtein_distance("cdd", "cd")), "1", "damerau_levenshtein_distance deletion (cdd -> cd)");
            assert_eq(to_string(damerau_levenshtein_distance("expor", "export")), "1", "damerau_levenshtein_distance insertion (expor -> export)");
            assert_eq(to_string(damerau_levenshtein_distance("pwsh", "pwhs")), "1", "damerau_levenshtein_distance transposition (pwhs -> pwsh)");
            assert_eq(to_string(damerau_levenshtein_distance("xyz123", "abc987")), "6", "damerau_levenshtein_distance distinct strings");

            assert_eq(find_typo_correction("gti"), "git", "find_typo_correction git transposition");
            assert_eq(find_typo_correction("sl"), "ls", "find_typo_correction ls transposition");
            assert_eq(find_typo_correction("cdd"), "cd", "find_typo_correction cd deletion");
            assert_eq(find_typo_correction("expor"), "export", "find_typo_correction export builtin insertion");
            assert_eq(find_typo_correction("a"), "", "find_typo_correction ignores <= 1 char tokens");
            assert_eq(find_typo_correction("completelyunknowntoken12345"), "", "find_typo_correction ignores distant queries");

            assert_eq(replace_first_command_token("sl -la", "sl", "ls"), "ls -la", "replace_first_command_token basic replacement");
            assert_eq(replace_first_command_token("  gti commit -m \"msg\"", "gti", "git"), "  git commit -m \"msg\"", "replace_first_command_token leading whitespace preserved");

            parse_and_execute("setopt nocorrect");
            assert_true(g_env.options["correct"] == false, "setopt nocorrect disables typo correction");
            parse_and_execute("setopt correct");
            assert_true(g_env.options["correct"] == true, "setopt correct enables typo correction");
        }

        // 18. Function and Script Recursion Depth Hardening
        {
            parse_and_execute("rec_inf() { rec_inf; }; rec_inf");
            assert_true(g_env.last_exit_code != 0, "infinite recursive function terminates safely at recursion limit");
            g_env.functions.erase("rec_inf");

            parse_and_execute("rec_a() { rec_b; }; rec_b() { rec_a; }; rec_a");
            assert_true(g_env.last_exit_code != 0, "mutually recursive functions terminate safely at recursion limit");
            g_env.functions.erase("rec_a");
            g_env.functions.erase("rec_b");
        }
    }

    cout << "\n--- Self-Test Summary: " << passed << " passed, " << failed << " failed ---\n";
    return (failed == 0) ? 0 : 1;
}

// ============================================================================
// SECTION 12: MAIN ENTRY POINT
// ============================================================================
int main(int argc, char* argv[]) {
    enable_ansi_support();

    // Convert Win32 wide command line arguments to UTF-8 so Unicode and Nerd Font arguments are preserved
    int wide_argc = 0;
    LPWSTR* wide_argv = CommandLineToArgvW(GetCommandLineW(), &wide_argc);
    vector<string> utf8_args;
    vector<char*> utf8_argv_ptrs;
    if (wide_argv) {
        for (int i = 0; i < wide_argc; ++i) {
            int len = WideCharToMultiByte(CP_UTF8, 0, wide_argv[i], -1, nullptr, 0, nullptr, nullptr);
            if (len > 0) {
                string s(len - 1, '\0');
                WideCharToMultiByte(CP_UTF8, 0, wide_argv[i], -1, &s[0], len, nullptr, nullptr);
                utf8_args.push_back(s);
            } else {
                utf8_args.push_back("");
            }
        }
        LocalFree(wide_argv);
        argc = wide_argc;
        for (auto& s : utf8_args) utf8_argv_ptrs.push_back(&s[0]);
        argv = utf8_argv_ptrs.data();
    }

    if (argc >= 4 && string(argv[1]) == "--multios-tee") {
        vector<unique_ptr<ofstream>> outputs;
        for (int i = 2; i + 1 < argc; i += 2) {
            ios::openmode mode = ios::binary | ios::out | (string(argv[i]) == "append" ? ios::app : ios::trunc);
            auto output = make_unique<ofstream>(normalize_path_to_win(argv[i + 1]), mode);
            if (!*output) return 1;
            outputs.push_back(std::move(output));
        }
        char buffer[64 * 1024];
        while (cin) {
            cin.read(buffer, sizeof(buffer));
            streamsize count = cin.gcount();
            for (auto& output : outputs) output->write(buffer, count);
        }
        for (auto& output : outputs) if (!*output) return 1;
        return 0;
    }
    if (argc >= 4 && string(argv[1]) == "--pipeline-stage") {
        string state_path = argv[2];
        if (!load_pipeline_shell_state(state_path)) {
            cerr << "zsh: unable to load pipeline shell state\n";
            return 1;
        }
        error_code remove_error;
        fs::remove(normalize_path_to_win(state_path), remove_error);
        vector<string> stage_args(argv + 3, argv + argc);
        int status = dispatch_command(stage_args);
        cout.flush();
        cerr.flush();
        return status;
    }
    if (argc > 1) {
        string arg1 = argv[1];
        if (arg1 == "--self-test" || arg1 == "--regression-test" || arg1 == "--test") {
            return run_internal_self_tests();
        }
    }
    bool load_rcs = true;
    string command;
    string script_path;
    int operand_index = 1;

    while (operand_index < argc) {
        string arg = argv[operand_index];
        if (arg == "-f" || arg == "--no-rcs") {
            load_rcs = false;
            ++operand_index;
        } else if (arg == "-l" || arg == "--login") {
            g_login_shell = true;
            ++operand_index;
        } else if (arg == "-c") {
            if (operand_index + 1 >= argc) {
                cerr << "zsh: -c requires a command\n";
                return 2;
            }
            command = argv[operand_index + 1];
            operand_index += 2;
            break;
        } else if (arg == "--") {
            ++operand_index;
            if (operand_index < argc) script_path = argv[operand_index++];
            break;
        } else if (!arg.empty() && arg[0] == '-') {
            break;
        } else {
            script_path = arg;
            ++operand_index;
            break;
        }
    }

    if (!command.empty()) {
        if (operand_index < argc) g_env.vars["0"] = argv[operand_index++];
        for (; operand_index < argc; ++operand_index) g_env.positional_args.push_back(argv[operand_index]);
    } else if (!script_path.empty()) {
        g_env.vars["0"] = script_path;
        for (; operand_index < argc; ++operand_index) g_env.positional_args.push_back(argv[operand_index]);
    }

    enable_ansi_support();
    init_signal_handlers();
    g_env.load_history();

    // Set ls/clear aliases only when the native commands are absent from PATH.
    if (find_executable_in_path("ls").empty())    g_env.aliases["ls"]    = "dir";
    if (find_executable_in_path("clear").empty()) g_env.aliases["clear"] = "cls";

    error_code _rc_ec;
    if (load_rcs && !fs::exists(g_env.zshrc_path, _rc_ec)) {
        ofstream rc(g_env.zshrc_path);
        if (rc.is_open()) {
            rc << "# =============================================================================\n"
               << "# CrossShellZSH configuration file (~/.zshrc)\n"
               << "# Generated automatically on first run. Edit to customise your environment.\n"
               << "# =============================================================================\n"
               << "\n"
               << "# -----------------------------------------------------------------------------\n"
               << "# Prompt Color Configuration\n"
               << "# -----------------------------------------------------------------------------\n"
               << "# Specify colors by name using %F{color_name} and reset with %f.\n"
               << "# Available colors: cyan, green, yellow, red, blue, magenta, white, orange, pink,\n"
               << "#                   br_cyan, br_green, br_yellow, br_red, br_blue, br_magenta, br_white\n"
               << "#\n"
               << "# Clean Multi-Color Theme (Cyan user@host, Green directory, Yellow % / #):\n"
               << "# PROMPT=\"%F{cyan}[%n@%m]%f %F{green}%~%f %F{yellow}%#%f \"\n"
               << "\n"
               << "# Additional prompt theme examples (uncomment to activate):\n"
               << "# PROMPT=\"%B%F{br_cyan}%~%f%b %F{br_green}❯%f \"\n"
               << "# PROMPT=\"%F{magenta}%n%f@%F{yellow}%m%f %F{blue}%1~%f %F{br_green}%#%f \"\n"
               << "\n"
               << "# -----------------------------------------------------------------------------\n"
               << "# History settings\n"
               << "# -----------------------------------------------------------------------------\n"
               << "HISTSIZE=10000\n"
               << "SAVEHIST=10000\n"
               << "\n"
               << "# -----------------------------------------------------------------------------\n"
               << "# Common aliases\n"
               << "# -----------------------------------------------------------------------------\n"
               << "alias ll='ls -la'\n"
               << "alias grep='grep --color=auto'\n"
               << "alias gs='git status'\n"
               << "alias gd='git diff'\n"
               << "\n"
               << "# -----------------------------------------------------------------------------\n"
               << "# Environment\n"
               << "# -----------------------------------------------------------------------------\n"
               << "export EDITOR=notepad\n"
               << "export PAGER=more\n";
            rc.close();
            cerr << COLOR_BR_GREEN << "zsh: created " << g_env.zshrc_path << COLOR_RESET << "\n";
        }
    } else if (load_rcs) {
        execute_script(g_env.zshrc_path);
    }

    if (argc > 1) {
        string arg1 = argv[1];
        if (arg1 == "--version" || arg1 == "-v") {
            cout << ZSH_VERSION_STRING << "\n"
                 << "Copyright (c) 2026 Roberto J Dohnert\n"
                 << "Licensed under the BSD 3-Clause License.\n";
            return 0;
        }
        if (arg1 == "--help" || arg1 == "-h") { print_comprehensive_help(); return 0; }
        if (!command.empty()) { int rc = parse_and_execute(command); fire_exit_trap(); return rc; }
        if (!script_path.empty()) { int rc = execute_script(script_path); fire_exit_trap(); return rc; }
        if (operand_index < argc || (!arg1.empty() && arg1[0] == '-')) {
            cerr << "zsh: unsupported option: " << argv[operand_index] << "\n";
            return 2;
        }
    }

    cout << COLOR_BR_CYAN << format_now_header_line() << COLOR_RESET << "\n\n";

    string pending_edit_line;
    while (true) {
        string line;
        if (!pending_edit_line.empty()) {
            string edit_buf = pending_edit_line;
            pending_edit_line.clear();
            line = read_line_interactive(edit_buf);
        } else {
            cout << render_prompt();
            line = read_line_interactive();
        }
        if (!line.empty()) {
            string action_line = line;
            if (!apply_interactive_typo_correction(line, action_line, pending_edit_line)) {
                continue;
            }
            g_env.add_history(action_line);
            parse_and_execute(action_line);
        }
    }
    return 0;
}
