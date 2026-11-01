#pragma once

// Runtime utilities for the busybox multi-call binary: UTF conversion,
// path/arg helpers, shell context, tokenization, and glob expansion.

#include "busybox.h"

// ============================================================================
// UNICODE (UTF-8 <-> UTF-16) TRANSLATION LAYER
// ============================================================================

std::wstring utf8_to_utf16(const std::string& str);
std::string utf16_to_utf8(const std::wstring& wstr);
std::string path_to_utf8(const fs::path& path);

// ============================================================================
// WIN32 UTILITIES & SELF-EXE RESOLUTION
// ============================================================================

std::string get_self_exe_path();
std::string to_lower(std::string s);
std::string normalize_path(std::string path);
std::string escape_win32_arg(const std::string& arg);
std::string escape_cmd_metachars(const std::string& arg);
std::string resolve_executable_path(const std::string& cmd);

// Shell-style wildcard match (* ? [..]) of `value` against `pattern`.
// Used by the `case` statement and [[ ... ]] pattern tests.
bool wildcard_match(const std::string& value, const std::string& pattern);

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
    // True when running an interactive REPL (vs. a script / -c command). Used to
    // decide whether destructive operations may prompt for confirmation.
    bool interactive = false;

    bool is_script_mode() const { return !interactive; }

    // When true (set via `set -o pipefail`), a pipeline returns the rightmost
    // non-zero stage status instead of always the last stage's status.
    bool pipefail = false;

    // User-defined shell functions: name -> body source text. The body is
    // re-parsed and executed on each call (see CommandNode). Stored as source
    // rather than AST to avoid a shell<->runtime circular dependency.
    std::map<std::string, std::string> functions;

    // Stack of variable names marked `local` within the current function call,
    // used to save/restore their prior values on function entry/exit.
    std::vector<std::map<std::string, std::string>> local_scopes;

    // Trap handlers: signal/condition name (e.g. "EXIT", "INT") -> command source.
    std::map<std::string, std::string> traps;

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
        variables["SH_VERSION"] = "BusyBox v" BUSYBOX_VERSION "-PROD";
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

    // Reap finished background jobs: close their handles and remove them from
    // the job list so repeated background launches do not leak handles or
    // accumulate stale entries. Returns the number of jobs reaped.
    size_t reap_jobs() {
        std::lock_guard<std::mutex> lock(ctx_mutex);
        size_t reaped = 0;
        for (auto it = jobs.begin(); it != jobs.end();) {
            if (it->is_running && it->hProcess != NULL) {
                DWORD code = 0;
                if (GetExitCodeProcess(it->hProcess, &code) && code != STILL_ACTIVE) {
                    it->is_running = false;
                    CloseHandle(it->hProcess);
                    it->hProcess = NULL;
                }
            }
            if (!it->is_running && it->hProcess == NULL) {
                it = jobs.erase(it);
                ++reaped;
            } else {
                ++it;
            }
        }
        return reaped;
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

bool is_valid_assignment_name(const std::string& name);
void restore_nested_environment(const ShellContext& parent, ShellContext& nested);
void restore_process_environment(const ShellContext& parent, ShellContext& nested);

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

extern HANDLE g_hJobObject;
extern std::mutex g_proc_mutex;
extern std::set<HANDLE> g_active_processes;

void register_active_process(HANDLE h);
void unregister_active_process(HANDLE h);
void init_job_object();
void init_vt100_and_binary_mode();
BOOL WINAPI console_signal_handler(DWORD dwCtrlType);

// ============================================================================
// LEXER, TOKENIZER & CHARACTER-LEVEL GLOB EXPANSION ENGINE
// ============================================================================

enum class LexTokenType { Word, Pipe, And, Or, Semi, Ampersand, RedirectIn, RedirectOut, RedirectAppend, RedirectErr, RedirectErrToOut, RedirectHeredoc, RedirectHerestring, LParen, RParen, LBrace, RBrace, Newline };

struct Token {
    LexTokenType type;
    std::string value;
};

std::vector<Token> tokenize(const std::string& line);

struct ArgChar {
    char c;
    bool is_quoted;
};

using ArgString = std::vector<ArgChar>;

std::string strip_pattern(const std::string& val, std::string pattern, bool is_prefix, bool longest);
ArgString expand_arg_variables(const std::string& input, ShellContext& ctx);
std::string argstring_to_string(const ArgString& arg);
std::vector<std::string> expand_globs(const std::vector<ArgString>& args);
