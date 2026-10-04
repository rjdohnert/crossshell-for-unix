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
 * CrossShellKSH - Standardized Section Index
 * ==========================================
 * 01. Platform Includes, Types, and Core Data Structures
 * 02. Runtime Globals and Environment State
 * 03. Shared Helpers and Utility Routines
 * 04. Trap, Signal, and Interrupt Handling
 * 05. Function Scope, Snapshots, and Restore
 * 06. File Descriptor, Redirection, and Co-process Handles
 * 07. History, Variable Hooks, and Session State
 * 08. Parser, Lexical Validation, Tokenizer, and Expansion Engine
 * 09. Pipelines, Co-process Launch, Process Wiring, and Job Control
 * 10. Built-in Commands, Co-process I/O, and Dispatch
 * 11. Script Execution and Evaluation Flow
 * 12. Startup, CLI Parsing, and Main Entry Point
 * 13. Built-in Self-Tests and Verification
 *
 * Section Header Convention:
 *   // SECTION NN: <area summary>
 *   // SECTION NNX: <sub-area summary>
 */

#include <iostream>
#include <string>
#include <string_view>
#include <array>
#include <vector>
#include <deque>
#include <map>
#include <unordered_map>
#include <set>
#include <optional>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <cerrno>
#include <climits>
#include <cmath>
#include <functional>
#include <mutex>
#include <thread>
#include <cwctype>
#include <regex>
#include <chrono>
#include <ctime>
#include <windows.h>
#include <sddl.h>
#include <winternl.h>
#include <io.h>
#include <fcntl.h>
#include <conio.h>

#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "User32.lib")

using namespace std;

struct RegistryValueMetadata {
    DWORD type = REG_NONE;
    std::vector<BYTE> data;
};

// ksh93 Environment State & Variables Registry
struct KshEnvironment {
    std::map<std::wstring, std::wstring> variables;
    std::map<std::wstring, RegistryValueMetadata> registry_metadata;
    std::map<std::wstring, std::map<std::wstring, std::wstring>> arrays;
    std::map<std::wstring, bool> associative_flags;
    std::map<std::wstring, wchar_t> justify_modes;
    std::map<std::wstring, int> justify_widths;
    std::map<std::wstring, bool> exported;
    std::map<std::wstring, bool> readonly_flags;
    std::map<std::wstring, bool> integer_flags;
    std::map<std::wstring, bool> uppercase_flags;
    std::map<std::wstring, bool> lowercase_flags;
    std::map<std::wstring, bool> nameref_flags;
};

struct StartupOptions {
    bool disable_profile = false;
    bool profile_override_set = false;
    std::wstring profile_override_path;
    bool command_string_set = false;
    std::wstring command_string;
    bool script_test_set = false;
    std::wstring script_test_path;
};

struct ScriptContext {
    std::wstring script_name;
    std::vector<std::wstring> args;
};

struct ShellFunctionDefinition {
    std::vector<std::wstring> body_lines;
};

struct HistoryEntry {
    std::wstring command;
    std::wstring timestamp;
};

struct BackgroundJob {
    int id;
    DWORD pid;
    HANDLE process_handle;
    std::vector<DWORD> pids;
    std::vector<HANDLE> process_handles;
    std::wstring command;
    bool completed;
    DWORD exit_code;
    bool completion_reported;
    std::vector<std::wstring> temp_file_paths;
};

struct CoProcessState {
    HANDLE input_write = INVALID_HANDLE_VALUE;
    HANDLE output_read = INVALID_HANDLE_VALUE;
    HANDLE process = INVALID_HANDLE_VALUE;
    DWORD pid = 0;
    std::wstring temp_file_path;
    bool active = false;
};

struct RedirectionSpec {
    struct CustomFdAction {
        enum class Kind {
            OpenRead,
            OpenWriteTruncate,
            OpenWriteAppend,
            Duplicate,
            Close
        };

        int fd = -1;
        Kind kind = Kind::OpenRead;
        std::wstring path;
        int duplicate_source_fd = -1;
    };

    bool has_stdin = false;
    std::wstring stdin_path;
    bool has_stdout = false;
    bool append_stdout = false;
    std::wstring stdout_path;
    bool stdout_to_stderr = false;
    bool has_stderr = false;
    bool append_stderr = false;
    std::wstring stderr_path;
    bool stderr_to_stdout = false;
    std::vector<CustomFdAction> custom_fd_actions;
};

struct CustomTypeMember {
    std::wstring name;
    bool is_integer = false;
    bool is_array = false;
    bool is_associative = false;
    std::wstring type_name;
    std::wstring default_value;
};

struct CustomTypeDefinition {
    std::wstring name;
    std::vector<CustomTypeMember> members;
    bool is_dynamic_namespace = false;
    std::function<bool(const std::wstring&, std::wstring&, RegistryValueMetadata&)> property_getter;
    std::function<bool(const std::wstring&, const std::wstring&)> property_setter;
};

struct VariableScopeSnapshot {
    bool had_scalar = false;
    std::wstring scalar_value;
    bool had_array = false;
    std::optional<std::map<std::wstring, std::wstring>> array_value;
    bool had_associative = false;
    bool associative_value = false;
    bool had_justify_mode = false;
    wchar_t justify_mode_value = 0;
    bool had_justify_width = false;
    int justify_width_value = 0;
    bool had_environment = false;
    std::wstring environment_value;
    bool had_exported = false;
    bool exported_value = false;
    bool had_readonly = false;
    bool readonly_value = false;
    bool had_integer = false;
    bool integer_value = false;
    bool had_uppercase = false;
    bool uppercase_value = false;
    bool had_lowercase = false;
    bool lowercase_value = false;
    bool had_nameref = false;
    bool nameref_value = false;
};

struct FunctionScopeContext {
    std::unordered_map<std::wstring, VariableScopeSnapshot> locals;
    bool return_requested = false;
    int return_status = 0;
};

enum class CommandResolutionKind;

// Runtime state section: shell globals, history, traps, and job tracking.
std::map<std::wstring, CustomTypeDefinition> g_main_custom_types;
KshEnvironment g_main_env;
std::vector<ScriptContext> g_main_script_context_stack;
std::map<std::wstring, ShellFunctionDefinition> g_main_shell_functions;
std::vector<FunctionScopeContext> g_main_function_scope_stack;
std::set<HANDLE> g_pipeline_thread_handles;
std::mutex g_pipeline_threads_mutex;
thread_local std::vector<HistoryEntry> g_command_history;
thread_local std::vector<BackgroundJob> g_background_jobs;
thread_local CoProcessState g_coprocess;
thread_local std::vector<std::wstring> g_directory_stack;
std::map<std::wstring, std::wstring> g_main_aliases;
thread_local std::map<std::wstring, std::wstring> g_trap_handlers;
struct CommandHashEntry {
    std::wstring path;
    int hits = 0;
};
thread_local std::map<std::wstring, CommandHashEntry> g_command_hash_table;

void enable_ansi_support() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE && hOut != nullptr) {
        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode)) {
            SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
    HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
    if (hErr != INVALID_HANDLE_VALUE && hErr != nullptr) {
        DWORD mode = 0;
        if (GetConsoleMode(hErr, &mode)) {
            SetConsoleMode(hErr, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
}

constexpr int kMinCustomFd = 3;
constexpr int kMaxCustomFd = 9;
constexpr size_t kCustomFdTableSize = 10;

std::array<HANDLE, kCustomFdTableSize> make_invalid_custom_fd_table() {
    std::array<HANDLE, kCustomFdTableSize> table = {};
    table.fill(INVALID_HANDLE_VALUE);
    return table;
}

thread_local std::array<HANDLE, kCustomFdTableSize> g_custom_fd_table = make_invalid_custom_fd_table();
thread_local int g_next_job_id = 1;
thread_local std::wstring g_history_file_path;
thread_local std::wstring g_last_pipeline_error_detail;
thread_local bool g_is_interactive_session = false;
thread_local bool g_powershell_bypass_enabled = false;
thread_local bool g_expansion_error = false;
thread_local int g_expansion_nesting_depth = 0;
thread_local int g_eval_nesting_depth = 0;
volatile LONG g_in_interactive_loop = 0;
volatile LONG g_foreground_process_active = 0;
volatile LONG g_pending_int_trap = 0;
volatile LONG g_pending_break_trap = 0;
volatile LONG g_pending_hup_trap = 0;
volatile LONG g_pending_chld_trap = 0;
volatile LONG g_pending_alrm_trap = 0;
volatile LONG g_pending_usr1_trap = 0;
volatile LONG g_pending_usr2_trap = 0;
volatile LONG g_pending_segv_trap = 0;
volatile LONG g_pending_fpe_trap = 0;
volatile LONG g_pending_term_trap = 0;
volatile LONG g_int_trap_active = 0;
volatile LONG g_break_trap_active = 0;
volatile LONG g_hup_trap_active = 0;
volatile LONG g_alrm_trap_active = 0;
volatile LONG g_usr1_trap_active = 0;
volatile LONG g_usr2_trap_active = 0;
volatile LONG g_segv_trap_active = 0;
volatile LONG g_fpe_trap_active = 0;
volatile LONG g_term_trap_active = 0;

thread_local KshEnvironment* g_current_env = &g_main_env;
thread_local std::map<std::wstring, CustomTypeDefinition>* g_current_custom_types = &g_main_custom_types;
thread_local std::vector<ScriptContext>* g_current_script_context_stack = &g_main_script_context_stack;
thread_local std::map<std::wstring, ShellFunctionDefinition>* g_current_shell_functions = &g_main_shell_functions;
thread_local std::vector<FunctionScopeContext>* g_current_function_scope_stack = &g_main_function_scope_stack;
thread_local std::map<std::wstring, std::wstring>* g_current_aliases = &g_main_aliases;

#define ksh_env (*g_current_env)
#define g_custom_types (*g_current_custom_types)
#define g_script_context_stack (*g_current_script_context_stack)
#define g_shell_functions (*g_current_shell_functions)
#define g_function_scope_stack (*g_current_function_scope_stack)
#define g_aliases (*g_current_aliases)

thread_local bool g_inprocess_pipeline_stage = false;
thread_local int g_subshell_nesting_depth = 0;

struct ScopedSubshellDepthGuard {
    ScopedSubshellDepthGuard() {
        g_subshell_nesting_depth++;
    }
    ~ScopedSubshellDepthGuard() {
        if (g_subshell_nesting_depth > 0) {
            g_subshell_nesting_depth--;
        }
    }
};

inline BOOL set_shell_environment_variable(LPCWSTR name, LPCWSTR value) {
    if (g_inprocess_pipeline_stage || g_subshell_nesting_depth > 0) {
        return TRUE;
    }
    return ::SetEnvironmentVariableW(name, value);
}
inline DWORD get_shell_environment_variable(LPCWSTR name, LPWSTR buffer, DWORD size) {
    if (!g_inprocess_pipeline_stage && g_subshell_nesting_depth == 0) {
        return ::GetEnvironmentVariableW(name, buffer, size);
    }
    std::map<std::wstring, std::wstring>::const_iterator it = ksh_env.variables.find(name != nullptr ? name : L"");
    if (it == ksh_env.variables.end()) {
        return 0;
    }
    const std::wstring& value = it->second;
    if (buffer == nullptr || size == 0) {
        return static_cast<DWORD>(value.size() + 1);
    }
    if (size <= value.size()) {
        if (size > 0) buffer[0] = L'\0';
        return static_cast<DWORD>(value.size() + 1);
    }
    std::copy(value.begin(), value.end(), buffer);
    buffer[value.size()] = L'\0';
    return static_cast<DWORD>(value.size());
}
#define SetEnvironmentVariableW set_shell_environment_variable
#define GetEnvironmentVariableW get_shell_environment_variable

thread_local bool g_running_trap_handler = false;
thread_local bool g_xtrace_enabled = false;
thread_local bool g_errexit_enabled = false;
thread_local bool g_nounset_enabled = false;
thread_local bool g_vi_mode_enabled = false;
thread_local bool g_pipefail_enabled = false;
thread_local int g_errexit_disabled_depth = 0;
thread_local bool g_exit_trap_executed = false;
const wchar_t k_array_at_quoted_separator = 0x1F;
constexpr size_t kDefaultHistoryLimit = 1000;
constexpr size_t kMaxCommandSubstitutionOutputBytes = 8 * 1024 * 1024;
constexpr size_t kMaxHereDocContentBytes = 8 * 1024 * 1024;
constexpr int kMaxExpansionNestingDepth = 64;
constexpr int kMaxEvalNestingDepth = 64;
constexpr int kMaxSubstitutionParseDepth = 128;
constexpr size_t kMaxRegexPatternLength = 512;
constexpr size_t kMaxRegexInputLength = 8192;
constexpr unsigned long long kMaxPatternMatchSteps = 200000;
constexpr int kMaxFunctionCallDepth = 200;
constexpr size_t kMaxScriptFileSizeBytes = 64 * 1024 * 1024;  // 64 MiB
constexpr size_t kMaxFunctionHeaderLineLength = 1024;
constexpr int kMaxArithmeticParseDepth = 200;
constexpr DWORD kDefaultProcessSubstitutionConnectTimeoutMs = 120000;
constexpr DWORD kMinProcessSubstitutionConnectTimeoutMs = 1000;
constexpr DWORD kMaxProcessSubstitutionConnectTimeoutMs = 600000;
constexpr size_t kMaxArrayIndex = 1000000;       // prevent OOM via arr[huge]=val
constexpr size_t kMaxShellVariables = 10000;     // per-session variable table cap
constexpr size_t kMaxShellFunctions = 1000;      // per-session function table cap
constexpr size_t kMaxShellAliases = 1000;        // per-session alias table cap
constexpr size_t kMaxTrapHandlers = 64;          // per-session trap-handler table cap
constexpr size_t kMaxHistoryLineBytes = 16384;   // 16 KiB per history line
constexpr size_t kMaxHistoryLoadEntries = 10000;  // cap history file load size
const wchar_t* kGetoptsCursorVar = L"__ksh_GETOPTS_POS";
const wchar_t* kGetoptsOptindMirrorVar = L"__ksh_GETOPTS_OPTIND";

struct StartupOptions;
void register_child_process_times(HANDLE hProcess);

std::wstring evaluate_arithmetic(const std::wstring& expr);
std::wstring evaluate_command_substitutions(const std::wstring& input);
std::wstring expand_substitutions_left_to_right(const std::wstring& input);
bool execute_script_file(const std::wstring& script_path, const std::vector<std::wstring>& script_args, bool& should_exit_shell);
bool execute_command_line(const std::wstring& input_line, bool& should_exit_shell, bool bypass_aliases = false, bool bypass_function_lookup = false);
std::wstring trim_copy(const std::wstring& value);
std::wstring trim_copy(std::wstring_view value);
std::vector<std::wstring> ksh_tokenize_preserve_quotes(const std::wstring& input);
std::wstring get_interactive_prompt();
std::wstring format_prompt_template(const std::wstring& prompt_template);
std::wstring resolve_history_file_path();
bool file_exists_regular(const std::wstring& path);
bool ensure_default_home_kshrc_exists(const std::wstring& profile_path);
bool resolve_cmd_exe_path(std::wstring& cmd_path);
bool resolve_powershell_exe_path(std::wstring& powershell_path);
bool resolve_bash_exe_path(std::wstring& bash_path);
bool load_startup_profile(const StartupOptions& options, bool& should_exit_shell);
bool parse_startup_arguments(int argc, wchar_t* argv[], StartupOptions& options, int& first_positional_index, bool& show_help, bool& show_version);
std::vector<std::wstring> get_array_values_vector(const std::wstring& name);
std::wstring resolve_variable_name(const std::wstring& name, int depth = 0);
bool assign_parameter_value(const std::wstring& lhs, const std::wstring& rhs, bool array_hint, bool local_scope, std::wstring& error_message, bool is_nameref = false);
bool parse_array_reference_expression(const std::wstring& expression, std::wstring& base_name, std::wstring& index_text, bool& has_index);
bool parse_non_negative_index(const std::wstring& text, size_t& out_index);
bool match_glob_pattern_recursive(
    const std::wstring& pattern,
    size_t pattern_index,
    const std::wstring& candidate,
    size_t candidate_index,
    bool case_sensitive,
    unsigned long long& step_budget);
// SECTION 03: Shared helpers, command expansion, and shell state.
bool get_flag_value(const std::map<std::wstring, bool>& flags, const std::wstring& name);
bool parse_redirections(std::vector<std::wstring>& tokens, RedirectionSpec& redir, std::wstring& error_message);
std::vector<std::wstring> split_pipeline_segments(const std::wstring& input);
bool is_custom_fd_in_range(int fd);
bool is_valid_handle_value(HANDLE handle);
HANDLE get_persistent_custom_fd(int fd);
bool has_unquoted_shell_metacharacters(const std::wstring& input);
bool is_valid_shell_identifier(const std::wstring& name);
std::wstring to_lower_copy(const std::wstring& value);
std::wstring to_lower_copy(std::wstring_view value);
bool starts_with_case_insensitive(std::wstring_view value, std::wstring_view prefix);
std::wstring get_status_value(const std::wstring& default_value);
bool execute_defined_function(const std::wstring& function_name, const std::vector<std::wstring>& function_args, bool& should_exit_shell);
std::wstring get_command_name(const std::wstring& full_command);
bool execute_builtin_coproc(const std::vector<std::wstring>& tokens);
bool close_coprocess(bool terminate_process);
bool validate_shell_lexical_state(const std::wstring& input, std::wstring& error_message);
bool starts_with_for_control_keyword(const std::wstring& trimmed);

// SECTION 04: Trap normalization, handlers, and pending dispatch.
std::wstring normalize_trap_event_name(const std::wstring& raw);
bool has_trap_handler(const std::wstring& event_name);
bool execute_registered_trap(const std::wstring& event_name, bool& should_exit_shell);
void process_pending_traps(bool& should_exit_shell);
void run_exit_trap_once(bool& should_exit_shell);
BOOL WINAPI ksh_console_ctrl_handler(DWORD ctrl_type);
CommandResolutionKind resolve_command_kind(const std::wstring& name, std::wstring& detail);

// SECTION 05A: Function helper implementations shared by runtime and builtins.
bool get_flag_value(const std::map<std::wstring, bool>& flags, const std::wstring& name) {
    std::map<std::wstring, bool>::const_iterator it = flags.find(name);
    return it != flags.end() && it->second;
}

void set_flag_value(std::map<std::wstring, bool>& flags, const std::wstring& name, bool value) {
    flags[name] = value;
}

bool is_executing_function_scope() {
    return !g_function_scope_stack.empty();
}

bool is_function_return_requested() {
    return !g_function_scope_stack.empty() && g_function_scope_stack.back().return_requested;
}

void request_function_return(int status_code) {
    if (g_function_scope_stack.empty()) {
        return;
    }

    g_function_scope_stack.back().return_requested = true;
    g_function_scope_stack.back().return_status = status_code;
}

// SECTION 05B: Function scope snapshots, restore, and environment sync.
void snapshot_local_variable_if_needed(const std::wstring& name) {
    if (!is_executing_function_scope()) {
        return;
    }

    FunctionScopeContext& context = g_function_scope_stack.back();
    if (context.locals.find(name) != context.locals.end()) {
        return;
    }

    VariableScopeSnapshot snapshot;

    std::map<std::wstring, std::wstring>::const_iterator scalar_it = ksh_env.variables.find(name);
    if (scalar_it != ksh_env.variables.end()) {
        snapshot.had_scalar = true;
        snapshot.scalar_value = scalar_it->second;
    }

    std::map<std::wstring, std::map<std::wstring, std::wstring>>::const_iterator array_it = ksh_env.arrays.find(name);
    if (array_it != ksh_env.arrays.end()) {
        snapshot.had_array = true;
        snapshot.array_value = array_it->second;
    }

    std::map<std::wstring, bool>::const_iterator assoc_it = ksh_env.associative_flags.find(name);
    if (assoc_it != ksh_env.associative_flags.end()) {
        snapshot.had_associative = true;
        snapshot.associative_value = assoc_it->second;
    }

    std::map<std::wstring, wchar_t>::const_iterator justify_mode_it = ksh_env.justify_modes.find(name);
    if (justify_mode_it != ksh_env.justify_modes.end()) {
        snapshot.had_justify_mode = true;
        snapshot.justify_mode_value = justify_mode_it->second;
    }

    std::map<std::wstring, int>::const_iterator justify_width_it = ksh_env.justify_widths.find(name);
    if (justify_width_it != ksh_env.justify_widths.end()) {
        snapshot.had_justify_width = true;
        snapshot.justify_width_value = justify_width_it->second;
    }

    DWORD env_len = GetEnvironmentVariableW(name.c_str(), nullptr, 0);
    if (env_len > 0) {
        snapshot.had_environment = true;
        std::wstring environment_value;
        environment_value.resize(env_len - 1);
        if (env_len > 1) {
            GetEnvironmentVariableW(name.c_str(), &environment_value[0], env_len);
        }
        snapshot.environment_value = std::move(environment_value);
    }

    snapshot.had_exported = get_flag_value(ksh_env.exported, name);
    snapshot.exported_value = snapshot.had_exported;
    snapshot.had_readonly = get_flag_value(ksh_env.readonly_flags, name);
    snapshot.readonly_value = snapshot.had_readonly;
    snapshot.had_integer = get_flag_value(ksh_env.integer_flags, name);
    snapshot.integer_value = snapshot.had_integer;
    snapshot.had_uppercase = get_flag_value(ksh_env.uppercase_flags, name);
    snapshot.uppercase_value = snapshot.had_uppercase;
    snapshot.had_lowercase = get_flag_value(ksh_env.lowercase_flags, name);
    snapshot.lowercase_value = snapshot.had_lowercase;
    snapshot.had_nameref = get_flag_value(ksh_env.nameref_flags, name);
    snapshot.nameref_value = snapshot.had_nameref;

    context.locals.insert(std::make_pair(name, std::move(snapshot)));
}

void restore_function_scope_variables(FunctionScopeContext& context) {
    for (std::unordered_map<std::wstring, VariableScopeSnapshot>::const_iterator it = context.locals.begin(); it != context.locals.end(); ++it) {
        const std::wstring& name = it->first;
        const VariableScopeSnapshot& snapshot = it->second;

        if (snapshot.had_scalar) {
            ksh_env.variables[name] = snapshot.scalar_value;
        } else {
            ksh_env.variables.erase(name);
        }

        if (snapshot.had_array) {
            ksh_env.arrays[name] = *snapshot.array_value;
        } else {
            ksh_env.arrays.erase(name);
        }

        if (snapshot.had_associative) {
            ksh_env.associative_flags.insert_or_assign(name, snapshot.associative_value);
        } else {
            ksh_env.associative_flags.erase(name);
        }

        if (snapshot.had_justify_mode) {
            ksh_env.justify_modes.insert_or_assign(name, snapshot.justify_mode_value);
        } else {
            ksh_env.justify_modes.erase(name);
        }

        if (snapshot.had_justify_width) {
            ksh_env.justify_widths.insert_or_assign(name, snapshot.justify_width_value);
        } else {
            ksh_env.justify_widths.erase(name);
        }

        if (snapshot.had_exported) {
            ksh_env.exported.insert_or_assign(name, snapshot.exported_value);
        } else {
            ksh_env.exported.erase(name);
        }

        if (snapshot.had_readonly) {
            ksh_env.readonly_flags.insert_or_assign(name, snapshot.readonly_value);
        } else {
            ksh_env.readonly_flags.erase(name);
        }

        if (snapshot.had_integer) {
            ksh_env.integer_flags.insert_or_assign(name, snapshot.integer_value);
        } else {
            ksh_env.integer_flags.erase(name);
        }

        if (snapshot.had_uppercase) {
            ksh_env.uppercase_flags.insert_or_assign(name, snapshot.uppercase_value);
        } else {
            ksh_env.uppercase_flags.erase(name);
        }

        if (snapshot.had_lowercase) {
            ksh_env.lowercase_flags.insert_or_assign(name, snapshot.lowercase_value);
        } else {
            ksh_env.lowercase_flags.erase(name);
        }

        if (snapshot.had_nameref) {
            ksh_env.nameref_flags.insert_or_assign(name, snapshot.nameref_value);
        } else {
            ksh_env.nameref_flags.erase(name);
        }

        if (snapshot.had_environment) {
            SetEnvironmentVariableW(name.c_str(), snapshot.environment_value.c_str());
        } else {
            SetEnvironmentVariableW(name.c_str(), nullptr);
        }
    }
}

void sync_exported_environment_variable(const std::wstring& name) {
    std::map<std::wstring, std::wstring>::const_iterator value_it = ksh_env.variables.find(name);
    if (value_it != ksh_env.variables.end() && get_flag_value(ksh_env.exported, name)) {
           SetEnvironmentVariableW(name.c_str(), value_it->second.c_str());
    } else {
        SetEnvironmentVariableW(name.c_str(), nullptr);
    }
}

// SECTION 06: Persistent custom file descriptor helpers.
bool is_custom_fd_in_range(int fd) {
    return fd >= kMinCustomFd && fd <= kMaxCustomFd;
}

bool is_valid_handle_value(HANDLE handle) {
    return handle != nullptr && handle != INVALID_HANDLE_VALUE;
}

HANDLE get_persistent_custom_fd(int fd) {
    if (!is_custom_fd_in_range(fd)) {
        return INVALID_HANDLE_VALUE;
    }
    return g_custom_fd_table[static_cast<size_t>(fd)];
}

class RedStderrBuffer : public std::wstreambuf {
public:
    explicit RedStderrBuffer(std::wstreambuf* target)
        : target_(target), stderr_handle_(GetStdHandle(STD_ERROR_HANDLE)), is_console_(false) {
        if (stderr_handle_ != nullptr && stderr_handle_ != INVALID_HANDLE_VALUE) {
            DWORD mode = 0;
            if (GetConsoleMode(stderr_handle_, &mode)) {
                is_console_ = true;
            }
        }
    }

protected:
    int_type overflow(int_type ch) override {
        if (traits_type::eq_int_type(ch, traits_type::eof())) {
            return traits_type::not_eof(ch);
        }

        const wchar_t wide_ch = traits_type::to_char_type(ch);
        if (!write_with_color(&wide_ch, 1)) {
            return traits_type::eof();
        }
        return ch;
    }

    std::streamsize xsputn(const wchar_t* s, std::streamsize n) override {
        if (n <= 0) {
            return 0;
        }

        if (!write_with_color(s, n)) {
            return 0;
        }
        return n;
    }

    int sync() override {
        return target_->pubsync();
    }

private:
    bool write_with_color(const wchar_t* text, std::streamsize count) {
        WORD previous_attributes = 0;
        const bool changed_color = try_set_red(previous_attributes);
        const std::streamsize written = target_->sputn(text, count);
        if (changed_color) {
            SetConsoleTextAttribute(stderr_handle_, previous_attributes);
        }
        return written == count;
    }

    bool try_set_red(WORD& previous_attributes) {
        if (!is_console_) {
            return false;
        }

        if (stderr_handle_ == nullptr || stderr_handle_ == INVALID_HANDLE_VALUE) {
            return false;
        }

        CONSOLE_SCREEN_BUFFER_INFO info;
        if (!GetConsoleScreenBufferInfo(stderr_handle_, &info)) {
            return false;
        }

        previous_attributes = info.wAttributes;
        if (!SetConsoleTextAttribute(stderr_handle_, FOREGROUND_RED | FOREGROUND_INTENSITY)) {
            return false;
        }

        return true;
    }

    std::wstreambuf* target_;
    HANDLE stderr_handle_;
    bool is_console_;
};

std::wstreambuf* g_original_wcerr_buffer = nullptr;
RedStderrBuffer* g_red_stderr_buffer = nullptr;

void enable_red_error_output() {
    if (g_red_stderr_buffer != nullptr) {
        return;
    }

    g_original_wcerr_buffer = std::wcerr.rdbuf();
    static RedStderrBuffer red_buffer(g_original_wcerr_buffer);
    g_red_stderr_buffer = &red_buffer;
    std::wcerr.rdbuf(g_red_stderr_buffer);
}

bool IsRunningAsAdmin() {
    SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
    PSID admin_group = nullptr;

    if (!AllocateAndInitializeSid(
            &nt_authority,
            2,
            SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS,
            0, 0, 0, 0, 0, 0,
            &admin_group)) {
        return false;
    }

    BOOL is_admin = FALSE;
    CheckTokenMembership(NULL, admin_group, &is_admin);
    FreeSid(admin_group);

    return is_admin == TRUE;
}

bool string_equals_case_insensitive(const std::wstring& left, const std::wstring& right) {
    if (left.size() != right.size()) {
        return false;
    }

    for (size_t i = 0; i < left.size(); ++i) {
        if (std::towlower(left[i]) != std::towlower(right[i])) {
            return false;
        }
    }

    return true;
}

std::wstring get_environment_value(const wchar_t* name) {
    if (name == nullptr || *name == L'\0') {
        return L"";
    }

    DWORD needed = GetEnvironmentVariableW(name, nullptr, 0);
    if (needed == 0) {
        return L"";
    }

    std::vector<wchar_t> buffer(needed);
    DWORD written = GetEnvironmentVariableW(name, buffer.data(), needed);
    if (written == 0 || written >= needed) {
        return L"";
    }

    return std::wstring(buffer.data(), written);
}

std::wstring get_current_working_directory_for_prompt() {
    DWORD needed = GetCurrentDirectoryW(0, nullptr);
    if (needed == 0) {
        return L".";
    }

    std::vector<wchar_t> buffer(needed);
    DWORD written = GetCurrentDirectoryW(needed, buffer.data());
    if (written == 0 || written >= needed) {
        return L".";
    }

    return std::wstring(buffer.data(), written);
}

std::wstring get_username_for_prompt() {
    wchar_t username[256] = { 0 };
    DWORD username_len = ARRAYSIZE(username);
    if (GetUserNameW(username, &username_len) && username_len > 0) {
        size_t actual_len = std::wcslen(username);
        return std::wstring(username, actual_len);
    }
    return L"";
}

std::wstring get_domain_for_prompt() {
    std::wstring domain = get_environment_value(L"USERDNSDOMAIN");
    if (!domain.empty()) {
        return domain;
    }
    domain = get_environment_value(L"USERDOMAIN");
    if (!domain.empty()) {
        return domain;
    }
    return L"";
}

std::wstring get_hostname_for_prompt() {
    wchar_t hostname[256] = { 0 };
    DWORD host_len = ARRAYSIZE(hostname);
    if (GetComputerNameW(hostname, &host_len) && host_len > 0) {
        return std::wstring(hostname, host_len);
    }
    return L"";
}

std::wstring format_prompt_template(const std::wstring& prompt_template) {
    const std::wstring username = get_username_for_prompt();
    const std::wstring domain = get_domain_for_prompt();
    const std::wstring cwd = get_current_working_directory_for_prompt();
    const std::wstring hostname = get_hostname_for_prompt();
    const wchar_t prompt_char = IsRunningAsAdmin() ? L'#' : L'$';

    std::wstring formatted;
    formatted.reserve(prompt_template.size() + 32);

    for (size_t i = 0; i < prompt_template.size(); ++i) {
        wchar_t ch = prompt_template[i];
        if (ch == L'%' && i + 1 < prompt_template.size()) {
            wchar_t token = prompt_template[i + 1];
            bool consumed = true;
            if (token == L'u') {
                formatted += username;
            } else if (token == L'd') {
                formatted += domain;
            } else if (token == L'w') {
                formatted += cwd;
            } else if (token == L'm') {
                formatted += hostname;
            } else if (token == L'#') {
                formatted.push_back(prompt_char);
            } else if (token == L'%') {
                formatted.push_back(L'%');
            } else {
                consumed = false;
            }

            if (consumed) {
                ++i;
                continue;
            }
        }

        formatted.push_back(ch);
    }

    return formatted;
}

bool get_current_directory_path(std::wstring& current_directory) {
    DWORD needed = GetCurrentDirectoryW(0, nullptr);
    if (needed == 0) {
        return false;
    }

    std::vector<wchar_t> buffer(needed);
    DWORD written = GetCurrentDirectoryW(needed, buffer.data());
    if (written == 0 || written >= needed) {
        return false;
    }

    current_directory.assign(buffer.data(), written);
    return true;
}

std::wstring get_interactive_prompt() {
    std::wstring prompt_template;

    auto scalar_it = ksh_env.variables.find(L"PS1");
    if (scalar_it != ksh_env.variables.end()) {
        prompt_template = scalar_it->second;
    } else {
        prompt_template = get_environment_value(L"PS1");
    }

    if (!prompt_template.empty()) {
        return format_prompt_template(prompt_template);
    }

    return IsRunningAsAdmin() ? L"# " : L"$ ";
}

bool clear_console_screen() {
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    if (console == nullptr || console == INVALID_HANDLE_VALUE) {
        return false;
    }

    CONSOLE_SCREEN_BUFFER_INFO screen_info;
    if (!GetConsoleScreenBufferInfo(console, &screen_info)) {
        return false;
    }

    const DWORD cell_count =
        static_cast<DWORD>(screen_info.dwSize.X) * static_cast<DWORD>(screen_info.dwSize.Y);
    COORD top_left = { 0, 0 };
    DWORD written = 0;

    if (!FillConsoleOutputCharacterW(console, L' ', cell_count, top_left, &written)) {
        return false;
    }

    if (!FillConsoleOutputAttribute(console, screen_info.wAttributes, cell_count, top_left, &written)) {
        return false;
    }

    if (!SetConsoleCursorPosition(console, top_left)) {
        return false;
    }

    return true;
}

std::wstring get_command_name(const std::wstring& full_command) {
    std::wstring trimmed = trim_copy(full_command);
    if (trimmed.empty()) return L"";
    if (trimmed[0] == L'"') {
        size_t end_quote = trimmed.find(L'"', 1);
        if (end_quote != std::wstring::npos) {
            return trimmed.substr(1, end_quote - 1);
        }
    }
    size_t space = trimmed.find_first_of(L" \t");
    if (space != std::wstring::npos) {
        return trimmed.substr(0, space);
    }
    return trimmed;
}

bool is_windows_internal_command(const std::wstring& cmd) {
    static const std::vector<std::wstring> internals = {
        L"dir", L"copy", L"move", L"del", L"erase", L"type", L"ren", L"rename",
        L"md", L"mkdir", L"rd", L"rmdir", L"cls", L"color", L"date", L"time",
        L"ver", L"vol", L"path", L"prompt", L"assoc", L"ftype", L"mklink"
    };
    std::wstring lower_cmd = to_lower_copy(cmd);
    return std::find(internals.begin(), internals.end(), lower_cmd) != internals.end();
}

bool is_truthy_setting_value(const std::wstring& value) {
    const std::wstring trimmed = trim_copy(value);
    std::wstring lowered;
    lowered.reserve(trimmed.size());
    for (wchar_t ch : trimmed) {
        lowered.push_back(static_cast<wchar_t>(std::towlower(ch)));
    }
    return lowered == L"1" || lowered == L"true" || lowered == L"yes" || lowered == L"on";
}

bool get_setting_bool(const std::wstring& name, bool default_value) {
    std::map<std::wstring, std::wstring>::const_iterator it = ksh_env.variables.find(name);
    if (it != ksh_env.variables.end()) {
        return is_truthy_setting_value(it->second);
    }

    wchar_t buffer[64];
    DWORD ret = GetEnvironmentVariableW(name.c_str(), buffer, _countof(buffer));
    if (ret > 0 && ret < _countof(buffer)) {
        return is_truthy_setting_value(buffer);
    }

    return default_value;
}

bool is_hardened_mode_enabled() {
    return get_setting_bool(L"ksh_HARDENED_MODE", false);
}

bool is_regex_enabled() {
    if (is_hardened_mode_enabled()) {
        return false;
    }
    return get_setting_bool(L"ksh_ENABLE_REGEX", true);
}

bool is_performance_telemetry_enabled() {
    return get_setting_bool(L"ksh_PERF_TRACE", false);
}

struct SubshellPathPerfCounters {
    unsigned long long command_subst_fast_count = 0;
    unsigned long long command_subst_fast_ms = 0;
    unsigned long long command_subst_snapshot_count = 0;
    unsigned long long command_subst_snapshot_ms = 0;
    unsigned long long command_subst_direct_count = 0;
    unsigned long long command_subst_direct_ms = 0;
    unsigned long long parenthesized_fast_count = 0;
    unsigned long long parenthesized_fast_ms = 0;
    unsigned long long parenthesized_snapshot_count = 0;
    unsigned long long parenthesized_snapshot_ms = 0;
    unsigned long long parenthesized_direct_count = 0;
    unsigned long long parenthesized_direct_ms = 0;
};

thread_local SubshellPathPerfCounters g_subshell_path_perf_counters;

// Performance section: subshell path accounting and trace emission.
void record_command_substitution_path_perf(bool used_fast_capture, bool used_snapshot_restore, ULONGLONG elapsed_ms) {
    if (used_fast_capture) {
        g_subshell_path_perf_counters.command_subst_fast_count++;
        g_subshell_path_perf_counters.command_subst_fast_ms += static_cast<unsigned long long>(elapsed_ms);
        return;
    }

    if (used_snapshot_restore) {
        g_subshell_path_perf_counters.command_subst_snapshot_count++;
        g_subshell_path_perf_counters.command_subst_snapshot_ms += static_cast<unsigned long long>(elapsed_ms);
        return;
    }

    g_subshell_path_perf_counters.command_subst_direct_count++;
    g_subshell_path_perf_counters.command_subst_direct_ms += static_cast<unsigned long long>(elapsed_ms);
}

void record_parenthesized_subshell_path_perf(bool used_fast_path, bool used_snapshot_restore, ULONGLONG elapsed_ms) {
    if (used_fast_path) {
        g_subshell_path_perf_counters.parenthesized_fast_count++;
        g_subshell_path_perf_counters.parenthesized_fast_ms += static_cast<unsigned long long>(elapsed_ms);
        return;
    }

    if (used_snapshot_restore) {
        g_subshell_path_perf_counters.parenthesized_snapshot_count++;
        g_subshell_path_perf_counters.parenthesized_snapshot_ms += static_cast<unsigned long long>(elapsed_ms);
        return;
    }

    g_subshell_path_perf_counters.parenthesized_direct_count++;
    g_subshell_path_perf_counters.parenthesized_direct_ms += static_cast<unsigned long long>(elapsed_ms);
}

struct ShellStateSnapshot {
    KshEnvironment environment;
    std::map<std::wstring, ShellFunctionDefinition> shell_functions;
    std::map<std::wstring, std::wstring> aliases;
    std::map<std::wstring, CustomTypeDefinition> custom_types;
    std::array<HANDLE, kCustomFdTableSize> custom_fd_table = make_invalid_custom_fd_table();
    std::map<std::wstring, std::wstring> trap_handlers;
    std::wstring saved_dir;
};

inline ShellStateSnapshot capture_shell_state_snapshot() {
    ShellStateSnapshot snap;
    snap.environment = ksh_env;
    snap.shell_functions = g_shell_functions;
    snap.aliases = g_aliases;
    snap.custom_types = g_custom_types;
    snap.custom_fd_table = g_custom_fd_table;
    snap.trap_handlers = g_trap_handlers;
    wchar_t dir_buf[MAX_PATH];
    if (GetCurrentDirectoryW(MAX_PATH, dir_buf) > 0) {
        snap.saved_dir = dir_buf;
    }
    return snap;
}

inline void restore_shell_state_snapshot_if_present(
    std::optional<ShellStateSnapshot>& snapshot_opt,
    const std::wstring& status_default_value) {
    if (!snapshot_opt.has_value()) {
        return;
    }

    ShellStateSnapshot& snapshot = *snapshot_opt;
    std::wstring status = get_status_value(status_default_value);
    std::swap(ksh_env, snapshot.environment);
    ksh_env.variables[L"?"] = status;
    std::swap(g_shell_functions, snapshot.shell_functions);
    std::swap(g_aliases, snapshot.aliases);
    std::swap(g_custom_types, snapshot.custom_types);
    g_trap_handlers = snapshot.trap_handlers;

    for (size_t fd = 0; fd < kCustomFdTableSize; ++fd) {
        if (g_custom_fd_table[fd] != snapshot.custom_fd_table[fd]) {
            if (is_valid_handle_value(g_custom_fd_table[fd])) {
                CloseHandle(g_custom_fd_table[fd]);
            }
            g_custom_fd_table[fd] = snapshot.custom_fd_table[fd];
        }
    }
    if (!snapshot.saved_dir.empty()) {
        wchar_t cur[MAX_PATH];
        if (GetCurrentDirectoryW(MAX_PATH, cur) > 0) {
            if (_wcsicmp(cur, snapshot.saved_dir.c_str()) != 0) {
                SetCurrentDirectoryW(snapshot.saved_dir.c_str());
            }
        }
    }
    snapshot_opt.reset();
}

class ScopedSnapshotRestoreGuard {
public:
    ScopedSnapshotRestoreGuard(
        std::optional<ShellStateSnapshot>& snapshot,
        const wchar_t* default_status)
        : snapshot_ref_(snapshot),
          status_default_(default_status != nullptr ? default_status : L"0") {}

    ~ScopedSnapshotRestoreGuard() {
        if (!dismissed_) {
            restore_shell_state_snapshot_if_present(snapshot_ref_, status_default_);
        }
    }

    void dismiss() {
        dismissed_ = true;
    }

private:
    std::optional<ShellStateSnapshot>& snapshot_ref_;
    std::wstring status_default_;
    bool dismissed_ = false;
};

void log_performance_trace(const std::wstring& phase, ULONGLONG elapsed_ms, const std::wstring& detail = L"") {
    std::wcerr << L"[perf] " << phase << L": " << elapsed_ms << L" ms";
    if (!detail.empty()) {
        std::wcerr << L" (" << detail << L")";
    }
    std::wcerr << L"\n";
}

bool pattern_match_case_sensitive() {
    // Default to ksh-style case-sensitive matching; allow explicit opt-out.
    return !get_setting_bool(L"ksh_PATTERN_CASE_INSENSITIVE", false);
}

class ScopedExpansionDepth {
public:
    ScopedExpansionDepth() : active_(false) {
        if (g_expansion_nesting_depth >= kMaxExpansionNestingDepth) {
            return;
        }
        g_expansion_nesting_depth++;
        active_ = true;
    }

    ~ScopedExpansionDepth() {
        if (active_ && g_expansion_nesting_depth > 0) {
            g_expansion_nesting_depth--;
        }
    }

    bool active() const {
        return active_;
    }

private:
    bool active_;
};

class ScopedEvalDepth {
public:
    ScopedEvalDepth() : active_(false) {
        if (g_eval_nesting_depth >= kMaxEvalNestingDepth) {
            return;
        }
        g_eval_nesting_depth++;
        active_ = true;
    }

    ~ScopedEvalDepth() {
        if (active_ && g_eval_nesting_depth > 0) {
            g_eval_nesting_depth--;
        }
    }

    bool active() const {
        return active_;
    }

private:
    bool active_;
};

class ScopedTrapExecutionFlag {
public:
    explicit ScopedTrapExecutionFlag(bool& flag) : flag_(flag) {
        flag_ = true;
    }

    ~ScopedTrapExecutionFlag() {
        flag_ = false;
    }

private:
    bool& flag_;
};

// History section: control variables and timestamp policy.
inline std::wstring get_system_env_var(const std::wstring& name) {
    wchar_t buffer[256];
    DWORD ret = GetEnvironmentVariableW(name.c_str(), buffer, _countof(buffer));
    if (ret > 0 && ret < _countof(buffer)) {
        return std::wstring(buffer, ret);
    }
    if (ret >= _countof(buffer)) {
        std::vector<wchar_t> dyn(ret + 1);
        DWORD dyn_ret = GetEnvironmentVariableW(name.c_str(), dyn.data(), static_cast<DWORD>(dyn.size()));
        if (dyn_ret > 0 && dyn_ret < dyn.size()) {
            return std::wstring(dyn.data(), dyn_ret);
        }
    }
    return L"";
}

std::wstring get_history_control_value(const std::wstring& name) {
    std::map<std::wstring, std::wstring>::const_iterator it = ksh_env.variables.find(name);
    if (it != ksh_env.variables.end()) {
        return it->second;
    }

    return get_system_env_var(name);
}

bool history_timestamps_enabled() {
    return !trim_copy(get_history_control_value(L"HISTTIMEFORMAT")).empty();
}

// Variable hook section: get/set/init dispatch state.
thread_local std::set<std::wstring> g_active_get_hooks;
thread_local std::set<std::wstring> g_active_set_hooks;
thread_local std::set<std::wstring> g_active_init_hooks;
thread_local HANDLE g_subshell_stdout = INVALID_HANDLE_VALUE;
thread_local HANDLE g_pipeline_stdin = INVALID_HANDLE_VALUE;
thread_local HANDLE g_pipeline_stdout = INVALID_HANDLE_VALUE;
thread_local HANDLE g_pipeline_stderr = INVALID_HANDLE_VALUE;
thread_local std::wstring* g_builtin_capture_output = nullptr;

bool read_registry_property(const std::wstring& property_path, std::wstring& value, RegistryValueMetadata& metadata) {
    const size_t separator = property_path.find(L'.');
    if (separator == std::wstring::npos || separator == 0 || separator + 1 >= property_path.size()) {
        return false;
    }

    std::wstring root_name = property_path.substr(0, separator);
    std::wstring remainder = property_path.substr(separator + 1);
    HKEY root = nullptr;
    if (root_name == L"HKLM" || root_name == L"HKEY_LOCAL_MACHINE") {
        root = HKEY_LOCAL_MACHINE;
    } else if (root_name == L"HKCU" || root_name == L"HKEY_CURRENT_USER") {
        root = HKEY_CURRENT_USER;
    } else {
        return false;
    }

    const size_t value_separator = remainder.rfind(L'.');
    if (value_separator == std::wstring::npos || value_separator == 0 || value_separator + 1 >= remainder.size()) {
        return false;
    }

    std::wstring key_path = remainder.substr(0, value_separator);
    std::wstring value_name = remainder.substr(value_separator + 1);
    if (key_path.find(L'/') != std::wstring::npos) {
        std::replace(key_path.begin(), key_path.end(), L'/', L'\\');
    } else {
        std::replace(key_path.begin(), key_path.end(), L'.', L'\\');
    }
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, key_path.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }

    DWORD type = REG_NONE;
    DWORD size = 0;
    LONG status = RegQueryValueExW(key, value_name.c_str(), nullptr, &type, nullptr, &size);
    if (status != ERROR_SUCCESS) {
        RegCloseKey(key);
        return false;
    }

    std::vector<BYTE> data(size);
    if (size > 0) {
        status = RegQueryValueExW(key, value_name.c_str(), nullptr, &type, data.data(), &size);
    }
    RegCloseKey(key);
    if (status != ERROR_SUCCESS) {
        return false;
    }
    data.resize(size);

    metadata.type = type;
    metadata.data = data;
    value.clear();
    if (type == REG_DWORD && data.size() >= sizeof(DWORD)) {
        value = std::to_wstring(*reinterpret_cast<const DWORD*>(data.data()));
    } else if (type == REG_QWORD && data.size() >= sizeof(ULONGLONG)) {
        value = std::to_wstring(*reinterpret_cast<const ULONGLONG*>(data.data()));
    } else if (type == REG_BINARY) {
        static const wchar_t hex[] = L"0123456789ABCDEF";
        for (BYTE byte : data) {
            value.push_back(hex[(byte >> 4) & 0x0F]);
            value.push_back(hex[byte & 0x0F]);
        }
    } else if (type == REG_MULTI_SZ) {
        const size_t wchar_count = data.size() / sizeof(wchar_t);
        const wchar_t* text = reinterpret_cast<const wchar_t*>(data.data());
        size_t offset = 0;
        while (offset < wchar_count && text[offset] != L'\0') {
            const size_t value_start = offset;
            while (offset < wchar_count && text[offset] != L'\0') {
                ++offset;
            }
            if (offset >= wchar_count) {
                return false;
            }
            if (!value.empty()) value.push_back(L'\n');
            value.append(text + value_start, offset - value_start);
            ++offset;
        }
    } else if (type == REG_SZ || type == REG_EXPAND_SZ) {
        const wchar_t* text = reinterpret_cast<const wchar_t*>(data.data());
        size_t length = data.size() / sizeof(wchar_t);
        while (length > 0 && text[length - 1] == L'\0') --length;
        value.assign(text, length);
    } else if (type != REG_SZ && type != REG_EXPAND_SZ) {
        return false;
    }
    return true;
}

bool write_registry_property(const std::wstring& property_path, const std::wstring& value) {
    const size_t separator = property_path.find(L'.');
    if (separator == std::wstring::npos || separator == 0 || separator + 1 >= property_path.size()) {
        return false;
    }

    const std::wstring root_name = property_path.substr(0, separator);
    const std::wstring remainder = property_path.substr(separator + 1);
    HKEY root = nullptr;
    if (root_name == L"HKLM" || root_name == L"HKEY_LOCAL_MACHINE") {
        root = HKEY_LOCAL_MACHINE;
    } else if (root_name == L"HKCU" || root_name == L"HKEY_CURRENT_USER") {
        root = HKEY_CURRENT_USER;
    } else {
        return false;
    }

    const size_t value_separator = remainder.rfind(L'.');
    if (value_separator == std::wstring::npos || value_separator == 0 || value_separator + 1 >= remainder.size()) {
        return false;
    }

    std::wstring key_path = remainder.substr(0, value_separator);
    const std::wstring value_name = remainder.substr(value_separator + 1);
    if (key_path.find(L'/') != std::wstring::npos) {
        std::replace(key_path.begin(), key_path.end(), L'/', L'\\');
    } else {
        std::replace(key_path.begin(), key_path.end(), L'.', L'\\');
    }

    HKEY key = nullptr;
    if (RegOpenKeyExW(root, key_path.c_str(), 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &key) != ERROR_SUCCESS) {
        return false;
    }

    DWORD type = REG_SZ;
    DWORD existing_size = 0;
    if (RegQueryValueExW(key, value_name.c_str(), nullptr, &type, nullptr, &existing_size) != ERROR_SUCCESS) {
        type = REG_SZ;
    }

    std::vector<BYTE> data;
    if (type == REG_DWORD) {
        wchar_t* end = nullptr;
        unsigned long parsed = std::wcstoul(value.c_str(), &end, 0);
        if (end == value.c_str() || (end != nullptr && *end != L'\0') || parsed > MAXDWORD) {
            RegCloseKey(key);
            return false;
        }
        data.resize(sizeof(DWORD));
        const DWORD number = static_cast<DWORD>(parsed);
        std::memcpy(data.data(), &number, sizeof(number));
    } else if (type == REG_QWORD) {
        wchar_t* end = nullptr;
        unsigned long long parsed = std::wcstoull(value.c_str(), &end, 0);
        if (end == value.c_str() || (end != nullptr && *end != L'\0')) {
            RegCloseKey(key);
            return false;
        }
        data.resize(sizeof(ULONGLONG));
        const ULONGLONG number = static_cast<ULONGLONG>(parsed);
        std::memcpy(data.data(), &number, sizeof(number));
    } else if (type == REG_SZ || type == REG_EXPAND_SZ) {
        data.resize((value.size() + 1) * sizeof(wchar_t));
        std::memcpy(data.data(), value.c_str(), data.size());
        type = (type == REG_EXPAND_SZ) ? REG_EXPAND_SZ : REG_SZ;
    } else {
        RegCloseKey(key);
        return false;
    }

    const LONG status = RegSetValueExW(key, value_name.c_str(), 0, type, data.data(), static_cast<DWORD>(data.size()));
    RegCloseKey(key);
    return status == ERROR_SUCCESS;
}

void register_registry_namespaces() {
    for (const wchar_t* name : {L"HKLM", L"HKCU"}) {
        CustomTypeDefinition& definition = g_custom_types[name];
        definition.name = name;
        definition.is_dynamic_namespace = true;
        definition.property_getter = [](const std::wstring& property_path, std::wstring& value, RegistryValueMetadata& metadata) {
            return read_registry_property(property_path, value, metadata);
        };
        definition.property_setter = [](const std::wstring& property_path, const std::wstring& value) {
            return write_registry_property(property_path, value);
        };
    }
}

void ensure_registry_namespaces_registered() {
    static std::once_flag registry_namespace_once;
    std::call_once(registry_namespace_once, register_registry_namespaces);
}

std::wstring get_variable_value_with_hooks(const std::wstring& name, bool& is_set) {
    is_set = false;
    ensure_registry_namespaces_registered();
    std::wstring resolved_name = resolve_variable_name(name);

    const size_t namespace_separator = resolved_name.find(L'.');
    if (namespace_separator != std::wstring::npos) {
        const std::wstring namespace_name = resolved_name.substr(0, namespace_separator);
        std::map<std::wstring, CustomTypeDefinition>::const_iterator namespace_it = g_custom_types.find(namespace_name);
        if (namespace_it != g_custom_types.end() && namespace_it->second.is_dynamic_namespace && namespace_it->second.property_getter) {
            std::wstring registry_value;
            RegistryValueMetadata metadata;
            if (namespace_it->second.property_getter(resolved_name, registry_value, metadata)) {
                ksh_env.registry_metadata[resolved_name] = std::move(metadata);
                is_set = true;
                return registry_value;
            }
        }
    }
    
    if (g_shell_functions.find(resolved_name + L".get") != g_shell_functions.end() &&
        g_active_get_hooks.find(resolved_name) == g_active_get_hooks.end()) {
        
        g_active_get_hooks.insert(resolved_name);
        
        std::wstring old_sh_val;
        bool had_sh_val = false;
        auto sh_it = ksh_env.variables.find(L".sh.value");
        if (sh_it != ksh_env.variables.end()) {
            old_sh_val = sh_it->second;
            had_sh_val = true;
        }
        
        auto val_it = ksh_env.variables.find(resolved_name);
        std::wstring current_val = (val_it != ksh_env.variables.end()) ? val_it->second : L"";
        ksh_env.variables[L".sh.value"] = current_val;
        
        bool dummy = false;
        execute_defined_function(resolved_name + L".get", {}, dummy);
        
        std::wstring returned_val = ksh_env.variables[L".sh.value"];
        
        if (had_sh_val) {
            ksh_env.variables[L".sh.value"] = old_sh_val;
        } else {
            ksh_env.variables.erase(L".sh.value");
        }
        
        g_active_get_hooks.erase(resolved_name);
        is_set = true;
        return returned_val;
    }
    
    auto it = ksh_env.variables.find(resolved_name);
    if (it != ksh_env.variables.end()) {
        is_set = true;
        return it->second;
    }
    
    return L"";
}

std::wstring get_environment_value(const std::wstring& name) {
    bool is_set = false;
    std::wstring val = get_variable_value_with_hooks(name, is_set);
    if (is_set) {
        return val;
    }

    return get_system_env_var(name);
}

// History section: persistence policy, timestamp formatting, and replay limits.
bool history_dedupe_enabled() {
    std::wstring value = trim_copy(get_history_control_value(L"HISTCONTROL"));
    if (value.empty()) {
        std::wstring legacy = trim_copy(get_history_control_value(L"HISTDEDUPE"));
        if (legacy.empty()) {
            legacy = trim_copy(get_history_control_value(L"HISTORY_DEDUPE"));
        }
        if (legacy.empty()) {
            return false;
        }

        for (wchar_t ch : legacy) {
            if (ch == L'1' || ch == L'y' || ch == L'Y' || ch == L't' || ch == L'T') {
                return true;
            }
        }
        return false;
    }

    return value.find(L"ignoredups") != std::wstring::npos ||
        value.find(L"erasedups") != std::wstring::npos ||
        value.find(L"dedupe") != std::wstring::npos;
}

size_t history_size_limit() {
    std::wstring value = trim_copy(get_history_control_value(L"HISTSIZE"));
    if (value.empty()) {
        return kDefaultHistoryLimit;
    }

    try {
        size_t index = 0;
        unsigned long long parsed = std::stoull(value, &index);
        if (index != value.size()) {
            return kDefaultHistoryLimit;
        }
        if (parsed == 0) {
            return 0;
        }
        return static_cast<size_t>(parsed);
    } catch (...) {
        return kDefaultHistoryLimit;
    }
}

std::wstring format_history_timestamp() {
    SYSTEMTIME local_time;
    GetLocalTime(&local_time);

    wchar_t buffer[32];
    _snwprintf_s(
        buffer,
        _countof(buffer),
        _TRUNCATE,
        L"%04u-%02u-%02u %02u:%02u:%02u",
        static_cast<unsigned>(local_time.wYear),
        static_cast<unsigned>(local_time.wMonth),
        static_cast<unsigned>(local_time.wDay),
        static_cast<unsigned>(local_time.wHour),
        static_cast<unsigned>(local_time.wMinute),
        static_cast<unsigned>(local_time.wSecond));
    return buffer;
}

std::wstring serialize_history_entry(const HistoryEntry& entry) {
    std::wstring timestamp = entry.timestamp;
    if (timestamp.empty()) {
        timestamp = format_history_timestamp();
    }

    if (timestamp.empty()) {
        return entry.command;
    }

    return timestamp + L"\t" + entry.command;
}

void rewrite_history_file();


std::wstring resolve_history_file_path() {
    auto read_env_var = [](const wchar_t* name) -> std::wstring {
        if (name == nullptr || *name == L'\0') {
            return L"";
        }
        return get_system_env_var(name);
    };

    // Prefer HOME first for POSIX-style workflows, then USERPROFILE and HOMEDRIVE+HOMEPATH.
    std::wstring home_path = trim_copy(read_env_var(L"HOME"));
    if (home_path.empty()) {
        home_path = trim_copy(read_env_var(L"USERPROFILE"));
    }
    if (home_path.empty()) {
        std::wstring home_drive = trim_copy(read_env_var(L"HOMEDRIVE"));
        std::wstring home_path_part = trim_copy(read_env_var(L"HOMEPATH"));
        if (!home_drive.empty() && !home_path_part.empty()) {
            home_path = home_drive + home_path_part;
        }
    }

    if (!home_path.empty()) {
        while (!home_path.empty() && (home_path.back() == L'\\' || home_path.back() == L'/')) {
            home_path.pop_back();
        }
        if (!home_path.empty()) {
            return home_path + L"\\.ksh_history";
        }
    }

    return L"";
}
void enforce_history_size_limit(bool rewrite_file) {
    size_t limit = history_size_limit();
    if (limit == 0 || g_command_history.size() <= limit) {
        return;
    }

    size_t remove_count = g_command_history.size() - limit;
    std::vector<HistoryEntry>::difference_type offset = static_cast<std::vector<HistoryEntry>::difference_type>(remove_count);
    g_command_history.erase(g_command_history.begin(), g_command_history.begin() + offset);
    if (rewrite_file && g_is_interactive_session) {
        rewrite_history_file();
    }
}

bool append_history_entry(const std::wstring& command_line) {
    if (command_line.empty()) {
        return false;
    }

    if (history_dedupe_enabled() && !g_command_history.empty() && g_command_history.back().command == command_line) {
        return false;
    }

    HistoryEntry entry;
    entry.command = command_line;
    entry.timestamp = format_history_timestamp();
    g_command_history.push_back(entry);

    bool rewrote_history = false;
    size_t limit = history_size_limit();
    if (limit > 0 && g_command_history.size() > limit) {
        enforce_history_size_limit(false);
        rewrote_history = true;
    }

    if (g_is_interactive_session) {
        if (rewrote_history) {
            rewrite_history_file();
        } else {
            if (g_history_file_path.empty()) {
                g_history_file_path = resolve_history_file_path();
            }

            std::wofstream history_file(g_history_file_path.c_str(), std::ios::app);
            if (history_file.is_open()) {
                history_file << serialize_history_entry(entry) << L"\n";
            }
        }
    }

    return true;
}

void load_command_history_from_file() {
    g_history_file_path = resolve_history_file_path();

    if (g_history_file_path.empty()) {
        return;
    }

    std::wifstream history_file(g_history_file_path.c_str());
    if (!history_file.is_open()) {
        return;
    }

    const size_t configured_limit = history_size_limit();
    size_t history_load_cap = configured_limit;
    if (history_load_cap == 0) {
        history_load_cap = kMaxHistoryLoadEntries;
    } else {
        if (history_load_cap > kMaxHistoryLoadEntries) {
            history_load_cap = kMaxHistoryLoadEntries;
        }
    }

    std::deque<HistoryEntry> loaded_history;
    if (history_load_cap > 0) {
        loaded_history.clear();
    }

    std::wstring line;
    bool saw_legacy_untimestamped_entry = false;
    while (std::getline(history_file, line)) {
        // Skip lines that exceed the per-line limit to avoid memory exhaustion from
        // a malformed or adversarially crafted history file.
        if (line.size() > kMaxHistoryLineBytes) {
            continue;
        }
        std::wstring trimmed = trim_copy(line);
        if (!trimmed.empty()) {
            HistoryEntry entry;
            size_t separator = trimmed.find(L'\t');
            if (separator != std::wstring::npos) {
                entry.timestamp = trim_copy(trimmed.substr(0, separator));
                entry.command = trim_copy(trimmed.substr(separator + 1));
            } else {
                entry.timestamp = format_history_timestamp();
                entry.command = trimmed;
                saw_legacy_untimestamped_entry = true;
            }

            if (!entry.command.empty()) {
                if (history_load_cap > 0 && loaded_history.size() == history_load_cap) {
                    loaded_history.pop_front();
                }
                loaded_history.push_back(std::move(entry));
            }
        }
    }

    g_command_history.assign(loaded_history.begin(), loaded_history.end());

    // Normalize pre-timestamp history files once loaded so all persisted entries use
    // the timestamped on-disk format.
    if (saw_legacy_untimestamped_entry && g_is_interactive_session) {
        rewrite_history_file();
    }
}

void append_history_entry_to_file(const std::wstring& command_line) {
    (void)append_history_entry(command_line);
}

void rewrite_history_file() {
    if (g_history_file_path.empty()) {
        g_history_file_path = resolve_history_file_path();
    }

    std::wofstream history_file(g_history_file_path.c_str(), std::ios::trunc);
    if (!history_file.is_open()) {
        return;
    }

    for (const HistoryEntry& entry : g_command_history) {
        history_file << serialize_history_entry(entry) << L"\n";
    }
}

std::wstring format_bytes_iec(ULONGLONG bytes) {
    static const wchar_t* units[] = { L"B", L"KiB", L"MiB", L"GiB", L"TiB" };
    double value = static_cast<double>(bytes);
    size_t unit_index = 0;
    while (value >= 1024.0 && unit_index + 1 < _countof(units)) {
        value /= 1024.0;
        unit_index++;
    }

    wchar_t buffer[64];
    if (unit_index == 0) {
        _snwprintf_s(buffer, _countof(buffer), _TRUNCATE, L"%llu %s", bytes, units[unit_index]);
    } else {
        _snwprintf_s(buffer, _countof(buffer), _TRUNCATE, L"%.2f %s", value, units[unit_index]);
    }
    return buffer;
}

std::wstring get_registry_string(HKEY root, const wchar_t* subkey, const wchar_t* value_name) {
    wchar_t buffer[256];
    DWORD size = static_cast<DWORD>(sizeof(buffer));
    LONG status = RegGetValueW(root, subkey, value_name, RRF_RT_REG_SZ, nullptr, buffer, &size);
    if (status != ERROR_SUCCESS) {
        return L"";
    }
    return std::wstring(buffer);
}

std::wstring get_windows_release_text() {
    std::wstring product_name = get_registry_string(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"ProductName");
    std::wstring display_version = get_registry_string(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"DisplayVersion");

    typedef LONG(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    RtlGetVersionPtr rtl_get_version = (ntdll != nullptr)
        ? reinterpret_cast<RtlGetVersionPtr>(GetProcAddress(ntdll, "RtlGetVersion"))
        : nullptr;

    RTL_OSVERSIONINFOEXW os = {};
    os.dwOSVersionInfoSize = sizeof(os);

    wchar_t version_text[96] = L"unknown";
    if (rtl_get_version != nullptr && rtl_get_version(reinterpret_cast<PRTL_OSVERSIONINFOW>(&os)) == 0) {
        _snwprintf_s(version_text, _countof(version_text), _TRUNCATE, L"%lu.%lu.%lu", os.dwMajorVersion, os.dwMinorVersion, os.dwBuildNumber);
    }

    std::wstring release = product_name.empty() ? L"Windows" : product_name;
    release += L" (";
    release += version_text;
    if (!display_version.empty()) {
        release += L", ";
        release += display_version;
    }
    release += L")";
    return release;
}

void print_version_text(const std::function<bool(const std::wstring&)>& write_output) {
    std::wstring out =
        L"\n"
        L"  CrossShellKSH 3.1.16-2026 Copyright (C) 2026, Roberto J Dohnert\n"
        L"         All Rights Reserved. License: BSD-3-Clause        \n"
        L"\n"
        L"Microsoft Windows is a registered trademark of Microsoft Corporation.\n"
        L"KornShell is a registered trademark of AT&T Research released under the\n"
        L"Eclipse Public License.\n"
        L"\n"
        L"This program is licensed under the BSD-3 Clause License.\n"
        L"\n"
        L"OS Release: Microsoft " + get_windows_release_text() + L"\n\n";
    write_output(out);
}

void initialize_default_shell_aliases() {
    if (g_main_aliases.empty()) {
        g_main_aliases[L"integer"] = L"typeset -i";
        g_main_aliases[L"float"] = L"typeset -E";
        g_main_aliases[L"nameref"] = L"typeset -n";
        g_main_aliases[L"functions"] = L"typeset -f";
        g_main_aliases[L"autoload"] = L"typeset -fu";
        g_main_aliases[L"compound"] = L"typeset -C";
        g_main_aliases[L"history"] = L"fc -l";
        g_main_aliases[L"r"] = L"fc -s";
        g_main_aliases[L"type"] = L"whence -v";
    }
}

void print_startup_system_info() {
    std::wcout << L"CrossShellKSH v3.1.16-2026" << std::endl;
}

std::wstring longest_common_prefix_case_insensitive(const std::vector<std::wstring>& values) {
    if (values.empty()) {
        return L"";
    }

    std::wstring prefix = values[0];
    for (size_t i = 1; i < values.size() && !prefix.empty(); ++i) {
        size_t common = 0;
        size_t limit = (prefix.size() < values[i].size()) ? prefix.size() : values[i].size();
        while (common < limit && std::towlower(prefix[common]) == std::towlower(values[i][common])) {
            common++;
        }
        prefix = prefix.substr(0, common);
    }
    return prefix;
}

std::wstring to_lower_copy(const std::wstring& value) {
    return to_lower_copy(std::wstring_view(value));
}

std::wstring to_lower_copy(std::wstring_view value) {
    std::wstring lower;
    lower.reserve(value.size());
    for (wchar_t ch : value) {
        lower.push_back(static_cast<wchar_t>(std::towlower(ch)));
    }
    return lower;
}

std::wstring to_upper_copy(const std::wstring& value) {
    std::wstring upper;
    upper.reserve(value.size());
    for (wchar_t ch : value) {
        upper.push_back(static_cast<wchar_t>(std::towupper(ch)));
    }
    return upper;
}

bool starts_with_case_insensitive(std::wstring_view value, std::wstring_view prefix) {
    if (prefix.size() > value.size()) {
        return false;
    }
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (std::towlower(value[i]) != std::towlower(prefix[i])) {
            return false;
        }
    }
    return true;
}

std::wstring get_status_value(const std::wstring& default_value) {
    std::map<std::wstring, std::wstring>::const_iterator status_it = ksh_env.variables.find(L"?");
    if (status_it != ksh_env.variables.end()) {
        return status_it->second;
    }
    return default_value;
}

bool contains_whitespace(const std::wstring& value) {
    for (wchar_t ch : value) {
        if (std::iswspace(ch)) {
            return true;
        }
    }
    return false;
}

struct CompletionCandidate {
    std::wstring value;
    int rank = 0;
};

struct CompletionQuery {
    std::wstring normalized_prefix;
    bool quoted = false;
    wchar_t quote_char = 0;
    bool path_completion = false;
    std::wstring path_directory;
};

struct CompletionCache {
    std::wstring cwd;
    std::wstring path_value;
    ULONGLONG cached_at_ms = 0;
    std::vector<CompletionCandidate> candidates;
};

constexpr ULONGLONG kCompletionCacheTtlMs = 1000;

struct BuiltinCommandHelp {
    const wchar_t* name;
    const wchar_t* description;
};

struct BuiltinCommandGroup {
    const wchar_t* title;
    std::vector<const wchar_t*> names;
};

const std::vector<BuiltinCommandHelp>& builtin_command_help_entries() {
    static const std::vector<BuiltinCommandHelp> entries = {
        {L"exit", L"Exit the shell."},
        {L"logout", L"Exit the shell session."},
        {L"source", L"Run commands from a file in the current shell."},
        {L".", L"Alias for source."},
        {L"builtin", L"Invoke a shell builtin directly."},
        {L"set", L"Set shell options/variables or print current variables (supports set -A name values...)."},
        {L"unset", L"Unset variables or functions."},
        {L"export", L"Mark variables for export to child processes."},
        {L"readonly", L"Mark variables as read-only."},
        {L"alias", L"Define or show command aliases."},
        {L"unalias", L"Remove command aliases."},
        {L"command", L"Run command lookup or execute without function lookup."},
        {L"whence", L"Report how command names are resolved."},
        {L"type", L"Describe command type (builtin/function/alias/external)."},
        {L"hash", L"Show or refresh command resolution results."},
        {L"eval", L"Evaluate and execute constructed command text."},
        {L"getopts", L"Parse positional options into shell variables."},
        {L"typeset", L"Declare or inspect shell variables, including -L/-R/-Z justification."},
        {L"enum", L"Define an integer enumeration: enum Name=(value ...)."},
        {L"[[", L"Evaluate a ksh-style conditional expression."},
        {L"test", L"Evaluate a conditional expression."},
        {L"[", L"Alias form of test requiring closing ]."},
        {L"cd", L"Change the current working directory. Accepts Unix-style /path and Windows-style C:\\path."},
        {L"pwd", L"Print the current working directory."},
        {L"dirs", L"Print the current directory stack."},
        {L"pushd", L"Push the current directory onto the stack and change to another directory."},
        {L"popd", L"Pop the directory stack and change to the saved directory."},
        {L"print", L"Write arguments to standard output."},
        {L"echo", L"Write arguments to standard output."},
        {L"printf", L"Format and write arguments to standard output, including %q shell escaping and %T local time output."},
        {L"true", L"Return success status 0."},
        {L"false", L"Return failure status 1."},
        {L"let", L"Evaluate arithmetic expressions and return success when the final value is non-zero."},
        {L"return", L"Return from the current function."},
        {L"clear", L"Clear the terminal screen."},
        {L"history", L"Show the current command history."},
        {L"complete", L"Show command completion candidates."},
        {L"math", L"Evaluate an arithmetic expression."},
        {L"trap", L"Install, clear, or show trap handlers for EXIT/HUP/INT/BREAK/TERM/CHLD and related signal names."},
        {L"jobs", L"List background jobs (jobs [-l|-p] [job...])."},
        {L"fg", L"Bring a background job to the foreground."},
        {L"bg", L"Continue a background job."},
        {L"disown", L"Remove background jobs from the job table without terminating them."},
        {L"kill", L"Send a signal to a process ID or shell job (%job, -p for pid, -j for job)."},
        {L"wait", L"Wait for one or more jobs to finish."},
        {L"coproc", L"Run a command as a bidirectional co-process; use read -p and print -p for I/O."},
        {L"find", L"Search for files in a directory hierarchy."},
        {L"read", L"Read a line from standard input and assign fields to variables or -A arrays."},
        {L"exec", L"Replace the shell process with the specified command, or redirect shell handles."},
        {L"umask", L"Set or print the file mode creation mask."},
        {L"fc", L"List, edit, and re-execute historical shell commands."},
        {L"stty", L"Set or print terminal line configuration settings."},
        {L"times", L"Print accumulated user and system CPU times for shell and children."},
        {L"sleep", L"Pause for a decimal number of seconds; pending console signals interrupt the wait."},
        {L"shift", L"Shift positional parameters left by N places (default 1)."},
        {L":", L"Null command (returns success)."},
        {L"getconf", L"Query system configuration variables (getconf system_var [pathname] or getconf -a)."},
        {L"pathchk", L"Check that pathnames are valid and portable (pathchk [-p] [-P] pathname...)."},
        {L"help", L"Display help information about built-in commands and user-defined functions."},
        {L"version", L"Display shell version information and OS details."}
    };
    return entries;
}

const BuiltinCommandHelp* find_builtin_command_help(std::wstring_view name) {
    const std::vector<BuiltinCommandHelp>& entries = builtin_command_help_entries();
    for (const BuiltinCommandHelp& entry : entries) {
        if (name == entry.name) {
            return &entry;
        }
    }
    return nullptr;
}

const std::vector<BuiltinCommandGroup>& builtin_command_help_groups() {
    static const std::vector<BuiltinCommandGroup> groups = {
        { L"Shell control", { L"exit", L"logout", L"builtin", L"command", L"whence", L"type", L"eval", L"source", L".", L"exec", L"stty" } },
        { L"Directory navigation", { L"cd", L"pwd", L"dirs", L"pushd", L"popd" } },
        { L"Variables and expansion", { L"set", L"unset", L"export", L"readonly", L"typeset", L"enum", L"getopts", L"alias", L"unalias", L"hash", L"let", L"return", L"[[", L"test", L"[" } },
        { L"Jobs and signals", { L"jobs", L"fg", L"bg", L"disown", L"kill", L"wait", L"trap", L"coproc" } },
        { L"Files and system", { L"find", L"pathchk", L"getconf", L"umask", L"times", L"sleep", L"shift", L":" } },
        { L"Interactive and diagnostics", { L"print", L"echo", L"printf", L"true", L"false", L"history", L"complete", L"fc", L"clear", L"math", L"read" } }
    };
    return groups;
}

const std::array<const wchar_t*, 13> kHelpControlFlowKeywordLines = {
    L"\nControl flow keywords:\n",
    L"  break     Exit the nearest enclosing loop.\n",
    L"  continue  Skip to the next loop iteration.\n",
    L"  if / elif / else / fi  Conditional command blocks.\n",
    L"  for / do / done        Iterate over words or positional parameters.\n",
    L"  while / do / done     Repeat while a command succeeds.\n",
    L"  until / do / done     Repeat until a command succeeds.\n",
    L"  select / do / done    Present a numbered selection menu.\n",
    L"  case / in / esac      Match a word against shell patterns.\n",
    L"\nExamples:\n",
    L"  if test -n \"$x\"; then echo set; else echo unset; fi\n",
    L"  for x in 1 2 3; do echo \"$x\"; done\n",
    L"  case \"$x\" in yes) echo true ;; *) echo false ;; esac\n"
};

const std::array<const wchar_t*, 25> kHelpShellSyntaxLines = {
    L"\nShell syntax and execution:\n",
    L"  command1 | command2       Pipe standard output into the next command.\n",
    L"  command &                 Run a command or pipeline in the background.\n",
    L"  <(command) / >(command)   Windows named-pipe process substitution.\n",
    L"  < file, > file, >> file   Redirect standard input or output.\n",
    L"  2> file, 2>> file         Redirect standard error.\n",
    L"  2>&1, 1>&2               Duplicate standard output/error streams.\n",
    L"  command1; command2        Execute top-level commands in sequence.\n",
    L"  command |                 Continue a pipeline on the next script line.\n",
    L"  backslash at line end     Continue a logical command on the next line.\n",
    L"  # comment                Begins a comment outside quotes.\n",
    L"\nExpansion:\n",
    L"  $name, ${name}            Expand shell variables.\n",
    L"  ${array[index]}           Expand indexed or associative array elements.\n",
    L"  $(command)                Capture command substitution output.\n",
    L"  $((expression))           Evaluate arithmetic expansion.\n",
    L"  ${HKLM.Key.Path.Value}    Read a Windows Registry value.\n",
    L"  ${HKLM.Key/With.Dots.Value} Use / for Registry paths containing dots.\n",
    L"\nRegistry namespaces:\n",
    L"  HKLM and HKCU expose dynamic Registry properties.\n",
    L"  HKLM.Key.Path.Value=TEXT writes existing REG_SZ/REG_EXPAND_SZ values.\n",
    L"  Existing REG_DWORD and REG_QWORD values accept numeric assignments.\n",
    L"  Unsupported Registry types are rejected instead of converted silently.\n",
    L"\nStatus and errors:\n",
    L"  $? holds the most recent command status; 0 means success.\n"
};

const std::array<const wchar_t*, 12> kHelpRuntimeBehaviorLines = {
    L"\nScripts and runtime behavior:\n",
    L"  ksh script.ksh [args...]   Run a script with positional parameters.\n",
    L"  ksh -c \"command\" [name [args...]] Execute a command string.\n",
    L"  ksh --script-test FILE     Run FILE without a startup profile and report PASS/FAIL.\n",
    L"  source FILE or . FILE      Run a file in the current shell state.\n",
    L"  Functions use name() { ... } or function name { ... } syntax.\n",
    L"  Pipeline builtin/function stages use isolated shell state.\n",
    L"  Exported variables are inherited by external child processes.\n",
    L"  Startup profile: ~/.kshrc, or the path supplied by --profile.\n",
    L"  --no-profile disables startup profile loading.\n",
    L"  Script file limit is 64 MiB; variable/function/alias tables have bounded capacities.\n",
    L"  Process-substitution connection timeout: ksh_PROC_SUB_CONNECT_TIMEOUT_MS (1000..600000 ms).\n"
};

const std::array<const wchar_t*, 3> kHelpExternalUtilitiesLines = {
    L"\nExternal utilities:\n",
    L"  suspend   Suspend or resume processes by PID.\n",
    L"  ulimit    Report resource usage or run a child under Windows job limits.\n"
};

const std::array<const wchar_t*, 4> kHelpPathHandlingLines = {
    L"\nPath handling:\n",
    L"  cd accepts both Windows paths and Unix-style rooted paths.\n",
    L"  Example: cd /src/project\n",
    L"  Example: cd C:\\src\\project\n"
};

const std::array<const wchar_t*, 4> kHelpPromptCustomizationLines = {
    L"\nPrompt customization (.kshrc):\n",
    L"  If PS1 is not set, default prompt is '$ ' for users and '# ' for admin.\n",
    L"  Set PS1 in ~/.kshrc to customize order/content.\n",
    L"  Supported prompt tokens: %u user, %d domain, %w cwd, %m host, %# role-char, %% literal-percent.\n"
};

const std::array<const wchar_t*, 2> kHelpPromptCustomizationShortLines = {
    L"  Example: PS1='%u@%d %w %# '\n",
    L"  Example: PS1='%w %# '\n"
};

const std::array<const wchar_t*, 4> kHelpViModeLines = {
    L"\nLine editing mode:\n",
    L"  set -o vi   Enable vi-style command mode in interactive input.\n",
    L"  set +o vi   Return to default insert editing mode.\n",
    L"  vi keys: Esc enter command mode, i/a/A/I switch to insert, 0/^/_ line-start, h/l move, w/W/b/B/e/E/ge/gE jump, | (or N|) column jump, f/F/t/T find, ; repeat, , reverse-repeat, j/k history, x/D delete. Prefix motions with N (e.g., 3w, 4h, 2k).\n"
};

const std::array<const wchar_t*, 7> kHelpCdCommandLines = {
    L"cd\n",
    L"  Change the current working directory.\n",
    L"\nPath handling:\n",
    L"  Accepts Unix-style rooted paths like /src/project.\n",
    L"  Accepts traditional Windows paths like C:\\src\\project.\n",
    L"\nExamples:\n",
    L"  cd /src/project\n"
};

const std::array<const wchar_t*, 1> kHelpCdCommandLinesTail = {
    L"  cd C:\\src\\project\n"
};

const std::array<const wchar_t*, 16> kHelpKillCommandLines = {
    L"kill\n",
    L"  Send a signal to a process ID or shell job.\n",
    L"\nUsage:\n",
    L"  kill [-l] [-s signal] [-p | -j] pid|%job ...\n",
    L"\nTarget selection:\n",
    L"  Bare numeric targets are treated as process IDs.\n",
    L"  Targets beginning with % are treated as shell jobs.\n",
    L"  -p forces process-ID interpretation.\n",
    L"  -j forces shell-job interpretation.\n",
    L"\nSignals:\n",
    L"  TERM/HUP attempt graceful close first, then force termination.\n",
    L"  INT sends CTRL+C, QUIT sends CTRL+BREAK, KILL force-terminates.\n",
    L"\nExamples:\n",
    L"  kill 11576\n",
    L"  kill -p -9 11576\n",
    L"  kill %1\n"
};

const std::array<const wchar_t*, 1> kHelpKillCommandLinesTail = {
    L"  kill -j %1\n"
};

const std::array<const wchar_t*, 9> kHelpPromptCommandLines = {
    L"prompt / PS1\n",
    L"  Customize interactive prompt text from ~/.kshrc using PS1.\n",
    L"\nBehavior:\n",
    L"  If PS1 is unset, default prompt remains '$ ' for users and '# ' for admin.\n",
    L"\nSupported tokens:\n",
    L"  %u user, %d domain, %w cwd, %m host, %# role-char, %% literal-percent\n",
    L"\nExamples:\n",
    L"  PS1='%u@%d %w %# '\n",
    L"  PS1='%w %# '\n"
};

const std::array<const wchar_t*, 1> kHelpPromptCommandLinesTail = {
    L"  PS1='%# '\n"
};

const std::array<const wchar_t*, 2> kHelpLoopKeywordLines = {
    L"  Loop-control keyword recognized inside while, until, for, and select blocks.\n",
    L"  Use an optional positive integer to target outer loops.\n"
};

template <size_t N>
bool write_help_lines(const std::function<bool(const std::wstring&)>& write_output, const std::array<const wchar_t*, N>& lines) {
    for (const wchar_t* line : lines) {
        if (!write_output(line)) {
            return false;
        }
    }
    return true;
}

bool write_grouped_builtin_help(const std::function<bool(const std::wstring&)>& write_output, bool leading_blank_line_per_group) {
    const std::vector<BuiltinCommandGroup>& groups = builtin_command_help_groups();
    bool first_group = true;
    for (const BuiltinCommandGroup& group : groups) {
        if (leading_blank_line_per_group || !first_group) {
            if (!write_output(L"\n")) {
                return false;
            }
        }
        first_group = false;

        if (!write_output(std::wstring(group.title) + L":\n")) {
            return false;
        }

        for (const wchar_t* name : group.names) {
            if (const BuiltinCommandHelp* entry = find_builtin_command_help(name)) {
                if (!write_output(std::wstring(L"  ") + entry->name + L" - " + entry->description + L"\n")) {
                    return false;
                }
            }
        }
    }
    if (!write_output(L"\n")) {
        return false;
    }
    return true;
}

const std::vector<std::wstring>& builtin_commands() {
    static std::vector<std::wstring> commands;
    if (commands.empty()) {
        const std::vector<BuiltinCommandHelp>& entries = builtin_command_help_entries();
        commands.reserve(entries.size());
        for (const BuiltinCommandHelp& entry : entries) {
            commands.push_back(entry.name);
        }
    }
    return commands;
}

void write_help_topic_index(const std::function<bool(const std::wstring&)>& write_output) {
    write_output(L"CrossShellKSH help\n\n");
    write_output(L"Use one of these forms:\n");
    write_output(L"  help                         Show this topic index and command groups.\n");
    write_output(L"  help COMMAND                 Show help for a builtin or shell function.\n");
    write_output(L"  help TOPIC                   Show a help section.\n");
    write_output(L"  help all                     Show the complete reference.\n\n");
    write_output(L"Topics:\n");
    write_output(L"  commands, builtins           Builtin commands grouped by purpose.\n");
    write_output(L"  control, flow                if, for, while, until, case, and select.\n");
    write_output(L"  syntax, expansion            Pipelines, redirection, substitution, and status.\n");
    write_output(L"  scripts, runtime             Scripts, profiles, functions, and limits.\n");
    write_output(L"  utilities, external          External helpers such as suspend and ulimit.\n");
    write_output(L"  paths, cd                    Windows and Unix-style path handling.\n");
    write_output(L"  prompt, ps1                  Prompt tokens and .kshrc customization.\n");
    write_output(L"  vi, editing                  Vi-style interactive line editing.\n");
    write_output(L"  help, overview               Explain help navigation.\n\n");
    write_output(L"Examples:\n");
    write_output(L"  help cd                      Help for the cd builtin.\n");
    write_output(L"  help syntax                  Shell syntax and expansion.\n");
    write_output(L"  help prompt                  Prompt customization.\n");
    write_output(L"  ksh --help                   Complete startup reference.\n");
}

bool write_help_topic(const std::wstring& raw_topic, const std::function<bool(const std::wstring&)>& write_output) {
    const std::wstring topic = to_lower_copy(raw_topic);
    if (topic == L"help" || topic == L"overview" || topic == L"index" || topic == L"topics") {
        write_help_topic_index(write_output);
        return true;
    }
    if (topic == L"commands" || topic == L"builtins" || topic == L"builtin") {
        write_output(L"Builtin commands:\n\n");
        return write_grouped_builtin_help(write_output, false);
    }
    if (topic == L"control" || topic == L"flow" || topic == L"keywords") {
        return write_help_lines(write_output, kHelpControlFlowKeywordLines);
    }
    if (topic == L"syntax" || topic == L"expansion" || topic == L"redirection" || topic == L"shell") {
        return write_help_lines(write_output, kHelpShellSyntaxLines);
    }
    if (topic == L"scripts" || topic == L"runtime" || topic == L"startup") {
        return write_help_lines(write_output, kHelpRuntimeBehaviorLines);
    }
    if (topic == L"utilities" || topic == L"external") {
        return write_help_lines(write_output, kHelpExternalUtilitiesLines);
    }
    if (topic == L"paths" || topic == L"path" || topic == L"cd") {
        return write_help_lines(write_output, kHelpPathHandlingLines);
    }
    if (topic == L"prompt" || topic == L"ps1") {
        return write_help_lines(write_output, kHelpPromptCustomizationLines) &&
            write_help_lines(write_output, kHelpPromptCustomizationShortLines);
    }
    if (topic == L"vi" || topic == L"editing" || topic == L"line-editing") {
        return write_help_lines(write_output, kHelpViModeLines);
    }
    return false;
}

void write_full_help_text(const std::function<bool(const std::wstring&)>& write_output) {
    auto write = [&](const std::wstring& text) -> bool {
        return write_output(text);
    };

    write(L"Usage:\n");
    write(L"  ksh [options] [script [args...]]\n");
    write(L"  ksh -c COMMAND [name [args...]]\n");
    write(L"  ksh --script-test SCRIPT [args...]\n");
    write(L"  ksh -- [script [args...]]\n");
    write(L"\nOptions:\n");
    write(L"  -h, --help       Show this help text and list built-in commands.\n");
    write(L"  --version        Show version information.\n");
    write(L"  --no-profile     Skip loading the startup profile.\n");
    write(L"  --profile FILE   Load the specified startup profile.\n");
    write(L"  -c COMMAND       Execute COMMAND, then exit with its status.\n");
    write(L"  --script-test SCRIPT  Run SCRIPT without a startup profile and report PASS or FAIL.\n");
    write(L"  --               End options; treat remaining arguments as script and arguments.\n");

    write(L"\nBuilt-in commands:\n");
    if (!write_grouped_builtin_help(write, true)) return;
    if (!write_help_lines(write, kHelpControlFlowKeywordLines)) return;
    if (!write_help_lines(write, kHelpShellSyntaxLines)) return;
    if (!write_help_lines(write, kHelpRuntimeBehaviorLines)) return;
    if (!write_help_lines(write, kHelpExternalUtilitiesLines)) return;
    if (!write_help_lines(write, kHelpPathHandlingLines)) return;
    if (!write_help_lines(write, kHelpPromptCustomizationLines)) return;
    if (!write_help_lines(write, kHelpPromptCustomizationShortLines)) return;
    write_help_lines(write, kHelpViModeLines);
}

void print_help_text() {
    auto write_stdout = [](const std::wstring& text) -> bool {
        std::wcout << text;
        return true;
    };
    write_full_help_text(write_stdout);
}

void add_completion_candidate(std::map<std::wstring, CompletionCandidate>& candidates, const std::wstring& candidate, int rank) {
    if (candidate.empty()) {
        return;
    }

    std::wstring key = to_lower_copy(candidate);
    auto it = candidates.find(key);
    if (it == candidates.end() || rank < it->second.rank || (rank == it->second.rank && candidate < it->second.value)) {
        candidates[key] = CompletionCandidate{candidate, rank};
    }
}

CompletionQuery analyze_completion_query(const std::wstring& raw_prefix) {
    CompletionQuery query;
    query.normalized_prefix = raw_prefix;

    if (!query.normalized_prefix.empty() && (query.normalized_prefix.front() == L'"' || query.normalized_prefix.front() == L'\'')) {
        query.quoted = true;
        query.quote_char = query.normalized_prefix.front();
        query.normalized_prefix.erase(0, 1);
    }

    size_t separator = query.normalized_prefix.find_last_of(L"\\/");
    if (separator != std::wstring::npos) {
        query.path_completion = true;
        query.path_directory = query.normalized_prefix.substr(0, separator + 1);
    }

    return query;
}

std::vector<CompletionCandidate> filter_completion_candidates(const std::vector<CompletionCandidate>& candidates, const std::wstring& prefix) {
    std::vector<CompletionCandidate> matches;
    for (const CompletionCandidate& candidate : candidates) {
        if (prefix.empty() || starts_with_case_insensitive(candidate.value, prefix)) {
            matches.push_back(candidate);
        }
    }

    std::sort(matches.begin(), matches.end(), [](const CompletionCandidate& left, const CompletionCandidate& right) {
        if (left.rank != right.rank) {
            return left.rank < right.rank;
        }

        std::wstring left_lower = to_lower_copy(left.value);
        std::wstring right_lower = to_lower_copy(right.value);
        if (left_lower != right_lower) {
            return left_lower < right_lower;
        }
        return left.value < right.value;
    });

    return matches;
}

const std::vector<CompletionCandidate>& collect_cached_command_candidates() {
    static CompletionCache cache;
    const ULONGLONG now_ms = GetTickCount64();

    wchar_t cwd_buffer[MAX_PATH];
    std::wstring cwd_value;
    if (GetCurrentDirectoryW(MAX_PATH, cwd_buffer) > 0) {
        cwd_value = cwd_buffer;
    }

    std::wstring path_value = get_system_env_var(L"PATH");

    if (cache.cwd == cwd_value && cache.path_value == path_value && !cache.candidates.empty() && (now_ms - cache.cached_at_ms) < kCompletionCacheTtlMs) {
        return cache.candidates;
    }

    std::map<std::wstring, CompletionCandidate> candidates;

    for (const std::wstring& builtin : builtin_commands()) {
        add_completion_candidate(candidates, builtin, 0);
    }

    if (!cwd_value.empty()) {
        std::wstring pattern = cwd_value;
        if (!pattern.empty() && pattern.back() != L'\\' && pattern.back() != L'/') {
            pattern += L"\\";
        }
        pattern += L"*";

        WIN32_FIND_DATAW find_data;
        HANDLE handle = FindFirstFileW(pattern.c_str(), &find_data);
        if (handle != INVALID_HANDLE_VALUE) {
            do {
                std::wstring name = find_data.cFileName;
                if (name == L"." || name == L"..") {
                    continue;
                }
                add_completion_candidate(candidates, name, 1);

                if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                    continue;
                }

                size_t dot = name.find_last_of(L'.');
                if (dot != std::wstring::npos) {
                    std::wstring extension = to_lower_copy(name.substr(dot));
                    if (extension == L".exe" || extension == L".cmd" || extension == L".bat" || extension == L".com") {
                        add_completion_candidate(candidates, name.substr(0, dot), 1);
                    }
                }
            } while (FindNextFileW(handle, &find_data));

            FindClose(handle);
        }
    }

    if (!path_value.empty()) {
        size_t start = 0;
        while (start <= path_value.size()) {
            size_t separator = path_value.find(L';', start);
            std::wstring entry = (separator == std::wstring::npos)
                ? path_value.substr(start)
                : path_value.substr(start, separator - start);
            entry = trim_copy(entry);
            if (!entry.empty()) {
                std::wstring pattern = entry;
                if (!pattern.empty() && pattern.back() != L'\\' && pattern.back() != L'/') {
                    pattern += L"\\";
                }
                pattern += L"*";

                WIN32_FIND_DATAW find_data;
                HANDLE handle = FindFirstFileW(pattern.c_str(), &find_data);
                if (handle != INVALID_HANDLE_VALUE) {
                    do {
                        std::wstring name = find_data.cFileName;
                        if (name == L"." || name == L"..") {
                            continue;
                        }
                        add_completion_candidate(candidates, name, 2);

                        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                            continue;
                        }

                        size_t dot = name.find_last_of(L'.');
                        if (dot != std::wstring::npos) {
                            std::wstring extension = to_lower_copy(name.substr(dot));
                            if (extension == L".exe" || extension == L".cmd" || extension == L".bat" || extension == L".com") {
                                add_completion_candidate(candidates, name.substr(0, dot), 2);
                            }
                        }
                    } while (FindNextFileW(handle, &find_data));

                    FindClose(handle);
                }
            }

            if (separator == std::wstring::npos) {
                break;
            }
            start = separator + 1;
        }
    }

    cache.cwd = cwd_value;
    cache.path_value = path_value;
    cache.cached_at_ms = now_ms;
    cache.candidates.clear();
    cache.candidates.reserve(candidates.size());
    for (const auto& kv : candidates) {
        cache.candidates.push_back(kv.second);
    }
    return cache.candidates;
}

std::vector<CompletionCandidate> collect_path_completion_candidates(const CompletionQuery& query) {
    std::vector<CompletionCandidate> candidates;
    std::wstring directory = query.path_directory;
    if (directory.empty()) {
        return candidates;
    }

    std::wstring pattern = directory;
    if (!pattern.empty() && pattern.back() != L'\\' && pattern.back() != L'/') {
        pattern += L"\\";
    }
    pattern += L"*";

    WIN32_FIND_DATAW find_data;
    HANDLE handle = FindFirstFileW(pattern.c_str(), &find_data);
    if (handle == INVALID_HANDLE_VALUE) {
        return candidates;
    }

    do {
        std::wstring name = find_data.cFileName;
        if (name == L"." || name == L"..") {
            continue;
        }
        std::wstring full_name = directory + name;
        candidates.push_back(CompletionCandidate{full_name, 0});

        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }

        size_t dot = name.find_last_of(L'.');
        if (dot != std::wstring::npos) {
            std::wstring extension = to_lower_copy(name.substr(dot));
            if (extension == L".exe" || extension == L".cmd" || extension == L".bat" || extension == L".com") {
                candidates.push_back(CompletionCandidate{directory + name.substr(0, dot), 0});
            }
        }
    } while (FindNextFileW(handle, &find_data));

    FindClose(handle);

    return filter_completion_candidates(candidates, query.normalized_prefix);
}

std::wstring format_completion_replacement(const CompletionQuery& query, const std::wstring& match) {
    bool should_quote = query.quoted || contains_whitespace(match);
    if (!should_quote) {
        return match;
    }

    wchar_t quote_char = query.quote_char != 0 ? query.quote_char : L'"';
    return std::wstring(1, quote_char) + match + std::wstring(1, quote_char);
}

size_t find_completion_token_start(const std::wstring& buffer, size_t cursor) {
    bool in_single_quotes = false;
    bool in_double_quotes = false;
    bool escaping = false;
    size_t token_start = 0;

    for (size_t i = 0; i < cursor; ++i) {
        wchar_t ch = buffer[i];
        if (escaping) {
            escaping = false;
            continue;
        }

        if (ch == L'\\' && !in_single_quotes) {
            escaping = true;
            continue;
        }

        if (ch == L'\'' && !in_double_quotes) {
            in_single_quotes = !in_single_quotes;
            continue;
        }

        if (ch == L'"' && !in_single_quotes) {
            in_double_quotes = !in_double_quotes;
            continue;
        }

        if (!in_single_quotes && !in_double_quotes && std::iswspace(ch)) {
            token_start = i + 1;
        }
    }

    return token_start;
}

std::vector<std::wstring> collect_completion_candidates(const std::wstring& raw_prefix) {
    CompletionQuery query = analyze_completion_query(raw_prefix);
    std::vector<CompletionCandidate> candidates;

    if (query.path_completion) {
        candidates = collect_path_completion_candidates(query);
    } else {
        const std::vector<CompletionCandidate>& cached = collect_cached_command_candidates();
        candidates = filter_completion_candidates(cached, query.normalized_prefix);
    }

    std::vector<std::wstring> matches;
    matches.reserve(candidates.size());
    for (const CompletionCandidate& candidate : candidates) {
        matches.push_back(candidate.value);
    }
    return matches;
}

struct InputLineRenderState {
    bool initialized = false;
    COORD line_start = { 0, 0 };
    size_t rendered_width = 0;
};

InputLineRenderState& input_line_render_state() {
    static InputLineRenderState state;
    return state;
}

void reset_input_line_render_state() {
    input_line_render_state() = InputLineRenderState();
}

void initialize_input_line_render_state() {
    HANDLE output_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (output_handle == nullptr || output_handle == INVALID_HANDLE_VALUE) {
        reset_input_line_render_state();
        return;
    }

    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!GetConsoleScreenBufferInfo(output_handle, &info)) {
        reset_input_line_render_state();
        return;
    }

    InputLineRenderState& state = input_line_render_state();
    state.initialized = true;
    state.line_start = info.dwCursorPosition;
    state.line_start.Y = info.dwCursorPosition.Y;
    state.rendered_width = 0;
}

void render_input_line(const std::wstring& prompt, const std::wstring& buffer, size_t cursor_pos) {
    InputLineRenderState& state = input_line_render_state();
    HANDLE output_handle = GetStdHandle(STD_OUTPUT_HANDLE);

    if (state.initialized && output_handle != nullptr && output_handle != INVALID_HANDLE_VALUE) {
        const size_t rendered_width = prompt.size() + buffer.size();
        const size_t clear_width = (state.rendered_width > rendered_width) ? (state.rendered_width - rendered_width) : 0;
        DWORD written = 0;
        CONSOLE_CURSOR_INFO cursor_info;
        const bool have_cursor_info = GetConsoleCursorInfo(output_handle, &cursor_info) != FALSE;

        if (have_cursor_info && cursor_info.bVisible) {
            CONSOLE_CURSOR_INFO hidden_cursor_info = cursor_info;
            hidden_cursor_info.bVisible = FALSE;
            SetConsoleCursorInfo(output_handle, &hidden_cursor_info);
        }

        SetConsoleCursorPosition(output_handle, state.line_start);
        std::wcout << prompt << buffer;
        if (clear_width > 0) {
            COORD clear_position = state.line_start;
            clear_position.X = static_cast<SHORT>(clear_position.X + static_cast<SHORT>(prompt.size() + buffer.size()));
            FillConsoleOutputCharacterW(output_handle, L' ', static_cast<DWORD>(clear_width), clear_position, &written);
        }

        COORD cursor_position = state.line_start;
        cursor_position.X = static_cast<SHORT>(cursor_position.X + static_cast<SHORT>(prompt.size() + cursor_pos));
        SetConsoleCursorPosition(output_handle, cursor_position);

        if (have_cursor_info && cursor_info.bVisible) {
            SetConsoleCursorInfo(output_handle, &cursor_info);
        }

        std::wcout.flush();
        state.rendered_width = rendered_width;
        return;
    }

    std::wcout << L"\r" << prompt << buffer;
    if (state.rendered_width > prompt.size() + buffer.size()) {
        std::wcout << std::wstring(state.rendered_width - (prompt.size() + buffer.size()), L' ');
    }
    std::wcout << L"\r" << prompt << buffer.substr(0, cursor_pos);
    std::wcout.flush();
    state.rendered_width = prompt.size() + buffer.size();
}

bool read_interactive_line(const std::wstring& prompt, std::wstring& output) {
    struct InteractiveConsoleModeGuard {
        HANDLE input = INVALID_HANDLE_VALUE;
        DWORD original_mode = 0;
        bool changed = false;

        InteractiveConsoleModeGuard() {
            input = GetStdHandle(STD_INPUT_HANDLE);
            if (input != nullptr && input != INVALID_HANDLE_VALUE && GetConsoleMode(input, &original_mode)) {
                const DWORD editor_mode = original_mode &
                    ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
                changed = SetConsoleMode(input, editor_mode) != FALSE;
            }
        }

        ~InteractiveConsoleModeGuard() {
            if (changed) {
                SetConsoleMode(input, original_mode);
            }
        }
    } console_mode_guard;

    enum class ViState {
        Insert,
        Command
    };

    std::wstring buffer;
    size_t cursor = 0;
    size_t history_index = g_command_history.size();
    std::wstring saved_current_line;
    ViState vi_state = ViState::Insert;
    wint_t last_find_motion = 0;
    wint_t last_find_target = 0;
    int vi_pending_count = 0;
    bool vi_has_pending_count = false;

    auto invert_find_motion = [&](wint_t motion) -> wint_t {
        switch (motion) {
            case L'f': return L'F';
            case L'F': return L'f';
            case L't': return L'T';
            case L'T': return L't';
            default: return 0;
        }
    };

    auto apply_find_motion = [&](wint_t motion, wint_t target) -> bool {
        if (buffer.empty()) {
            return false;
        }
        if (target == 0 || target == 224) {
            return false;
        }

        const wchar_t needle = static_cast<wchar_t>(target);

        if (motion == L'f' || motion == L't') {
            const size_t start = (cursor < buffer.size()) ? (cursor + 1) : buffer.size();
            for (size_t i = start; i < buffer.size(); ++i) {
                if (buffer[i] == needle) {
                    cursor = (motion == L't' && i > 0) ? (i - 1) : i;
                    return true;
                }
            }
            return false;
        }

        if (motion == L'F' || motion == L'T') {
            if (cursor == 0) {
                return false;
            }

            const size_t start = cursor - 1;
            for (size_t i = start + 1; i > 0; --i) {
                const size_t index = i - 1;
                if (buffer[index] == needle) {
                    cursor = (motion == L'T' && (index + 1) < buffer.size()) ? (index + 1) : index;
                    return true;
                }
            }
            return false;
        }

        return false;
    };

    auto clamp_insert_cursor = [&]() {
        if (cursor > buffer.size()) {
            cursor = buffer.size();
        }
    };

    auto clamp_command_cursor = [&]() {
        if (buffer.empty()) {
            cursor = 0;
            return;
        }
        if (cursor >= buffer.size()) {
            cursor = buffer.size() - 1;
        }
    };

    auto move_to_first_non_blank = [&]() {
        size_t pos = 0;
        while (pos < buffer.size() && std::iswspace(buffer[pos])) {
            pos++;
        }
        cursor = (pos < buffer.size()) ? pos : 0;
    };

    auto history_up = [&]() {
        if (g_command_history.empty()) {
            return;
        }
        if (history_index == g_command_history.size()) {
            saved_current_line = buffer;
        }
        if (history_index > 0) {
            history_index--;
            buffer = g_command_history[history_index].command;
            cursor = buffer.size();
            if (g_vi_mode_enabled && vi_state == ViState::Command) {
                clamp_command_cursor();
            }
            render_input_line(prompt, buffer, cursor);
        }
    };

    auto history_down = [&]() {
        if (history_index < g_command_history.size()) {
            history_index++;
            if (history_index == g_command_history.size()) {
                buffer = saved_current_line;
            } else {
                buffer = g_command_history[history_index].command;
            }
            cursor = buffer.size();
            if (g_vi_mode_enabled && vi_state == ViState::Command) {
                clamp_command_cursor();
            }
            render_input_line(prompt, buffer, cursor);
        }
    };

    initialize_input_line_render_state();
    std::wcout << prompt;
    std::wcout.flush();

    while (true) {
        wint_t ch = _getwch();

        if (ch == 26) {
            if (buffer.empty()) {
                std::wcout << L"\n";
                return false;
            }
            continue;
        }

        if (ch == 13) {
            std::wcout << L"\n";
            output = buffer;
            return true;
        }

        if (ch == 3) {
            std::wcout << L"^C\n";
            output.clear();
            return true;
        }

        if (g_vi_mode_enabled) {
            if (ch == 27) {
                if (vi_state == ViState::Insert) {
                    vi_state = ViState::Command;
                    vi_pending_count = 0;
                    vi_has_pending_count = false;
                    if (cursor > 0) {
                        cursor--;
                    }
                    clamp_command_cursor();
                    render_input_line(prompt, buffer, cursor);
                }
                continue;
            }

            if (vi_state == ViState::Command && ch != 0 && ch != 224) {
                if (ch >= L'1' && ch <= L'9') {
                    const int digit = static_cast<int>(ch - L'0');
                    if (!vi_has_pending_count) {
                        vi_has_pending_count = true;
                        vi_pending_count = 0;
                    }
                    if (vi_pending_count <= 100000000) {
                        vi_pending_count = (vi_pending_count * 10) + digit;
                    }
                    continue;
                }
                if (ch == L'0' && vi_has_pending_count) {
                    if (vi_pending_count <= 100000000) {
                        vi_pending_count *= 10;
                    }
                    continue;
                }

                const int command_count = vi_has_pending_count ? vi_pending_count : 0;
                vi_pending_count = 0;
                vi_has_pending_count = false;
                const int repeat_count = (command_count > 0) ? command_count : 1;

                switch (ch) {
                    case L'i':
                        vi_state = ViState::Insert;
                        clamp_insert_cursor();
                        continue;
                    case L'a':
                        if (cursor < buffer.size()) {
                            cursor++;
                        }
                        vi_state = ViState::Insert;
                        clamp_insert_cursor();
                        render_input_line(prompt, buffer, cursor);
                        continue;
                    case L'A':
                        cursor = buffer.size();
                        vi_state = ViState::Insert;
                        render_input_line(prompt, buffer, cursor);
                        continue;
                    case L'I':
                        move_to_first_non_blank();
                        vi_state = ViState::Insert;
                        render_input_line(prompt, buffer, cursor);
                        continue;
                    case L'^':
                    case L'_':
                        move_to_first_non_blank();
                        render_input_line(prompt, buffer, cursor);
                        continue;
                    case L'0':
                        cursor = 0;
                        render_input_line(prompt, buffer, cursor);
                        continue;
                    case L'$':
                        cursor = buffer.empty() ? 0 : (buffer.size() - 1);
                        render_input_line(prompt, buffer, cursor);
                        continue;
                    case L'|': {
                        if (buffer.empty()) {
                            cursor = 0;
                        } else {
                            size_t target_column = 0;
                            if (command_count > 0) {
                                target_column = static_cast<size_t>(command_count - 1);
                            }
                            cursor = (target_column >= buffer.size()) ? (buffer.size() - 1) : target_column;
                        }
                        render_input_line(prompt, buffer, cursor);
                        continue;
                    }
                    case L'h':
                        for (int i = 0; i < repeat_count; ++i) {
                            if (cursor > 0) {
                                cursor--;
                            }
                        }
                        if (!buffer.empty()) {
                            render_input_line(prompt, buffer, cursor);
                        }
                        continue;
                    case L'l':
                        for (int i = 0; i < repeat_count; ++i) {
                            if (!buffer.empty() && cursor + 1 < buffer.size()) {
                                cursor++;
                            }
                        }
                        if (!buffer.empty()) {
                            render_input_line(prompt, buffer, cursor);
                        }
                        continue;
                    case L'w':
                    case L'W': {
                        for (int i = 0; i < repeat_count; ++i) {
                            size_t pos = cursor;
                            while (pos < buffer.size() && !std::iswspace(buffer[pos])) {
                                pos++;
                            }
                            while (pos < buffer.size() && std::iswspace(buffer[pos])) {
                                pos++;
                            }
                            if (!buffer.empty()) {
                                cursor = (pos >= buffer.size()) ? (buffer.size() - 1) : pos;
                            } else {
                                cursor = 0;
                            }
                        }
                        render_input_line(prompt, buffer, cursor);
                        continue;
                    }
                    case L'e':
                    case L'E': {
                        if (!buffer.empty()) {
                            for (int i = 0; i < repeat_count; ++i) {
                                size_t pos = (cursor >= buffer.size()) ? (buffer.size() - 1) : cursor;

                                while (pos < buffer.size() && std::iswspace(buffer[pos])) {
                                    pos++;
                                }

                                if (pos >= buffer.size()) {
                                    cursor = buffer.size() - 1;
                                } else {
                                    while ((pos + 1) < buffer.size() && !std::iswspace(buffer[pos + 1])) {
                                        pos++;
                                    }
                                    cursor = pos;
                                }
                            }

                            render_input_line(prompt, buffer, cursor);
                        }
                        continue;
                    }
                    case L'g': {
                        wint_t next = _getwch();
                        if (next == L'e' || next == L'E') {
                            if (!buffer.empty()) {
                                size_t pos = (cursor >= buffer.size()) ? (buffer.size() - 1) : cursor;

                                if (std::iswspace(buffer[pos])) {
                                    while (pos > 0 && std::iswspace(buffer[pos])) {
                                        pos--;
                                    }
                                } else {
                                    while (pos > 0 && !std::iswspace(buffer[pos - 1])) {
                                        pos--;
                                    }
                                    if (pos > 0) {
                                        pos--;
                                        while (pos > 0 && std::iswspace(buffer[pos])) {
                                            pos--;
                                        }
                                    }
                                }

                                while (pos > 0 && std::iswspace(buffer[pos])) {
                                    pos--;
                                }
                                while (pos > 0 && !std::iswspace(buffer[pos - 1])) {
                                    pos--;
                                }
                                size_t end_pos = pos;
                                while ((end_pos + 1) < buffer.size() && !std::iswspace(buffer[end_pos + 1])) {
                                    end_pos++;
                                }

                                cursor = end_pos;
                                render_input_line(prompt, buffer, cursor);
                            }
                            continue;
                        }
                        continue;
                    }
                    case L'f':
                    case L'F':
                    case L't':
                    case L'T': {
                        wint_t target = _getwch();
                        bool moved = false;
                        for (int i = 0; i < repeat_count; ++i) {
                            if (!apply_find_motion(ch, target)) {
                                break;
                            }
                            moved = true;
                        }
                        if (moved) {
                            last_find_motion = ch;
                            last_find_target = target;
                            render_input_line(prompt, buffer, cursor);
                        }
                        continue;
                    }
                    case L';': {
                        bool moved = false;
                        for (int i = 0; i < repeat_count; ++i) {
                            if (!apply_find_motion(last_find_motion, last_find_target)) {
                                break;
                            }
                            moved = true;
                        }
                        if (moved) {
                            render_input_line(prompt, buffer, cursor);
                        }
                        continue;
                    }
                    case L',': {
                        wint_t reverse_motion = invert_find_motion(last_find_motion);
                        bool moved = false;
                        for (int i = 0; i < repeat_count; ++i) {
                            if (!apply_find_motion(reverse_motion, last_find_target)) {
                                break;
                            }
                            moved = true;
                        }
                        if (moved) {
                            render_input_line(prompt, buffer, cursor);
                        }
                        continue;
                    }
                    case L'b':
                    case L'B': {
                        if (!buffer.empty()) {
                            for (int i = 0; i < repeat_count; ++i) {
                                size_t pos = cursor;
                                if (pos > 0) {
                                    pos--;
                                }
                                while (pos > 0 && std::iswspace(buffer[pos])) {
                                    pos--;
                                }
                                while (pos > 0 && !std::iswspace(buffer[pos - 1])) {
                                    pos--;
                                }
                                cursor = pos;
                            }
                            render_input_line(prompt, buffer, cursor);
                        }
                        continue;
                    }
                    case L'k':
                        for (int i = 0; i < repeat_count; ++i) {
                            history_up();
                        }
                        continue;
                    case L'j':
                        for (int i = 0; i < repeat_count; ++i) {
                            history_down();
                        }
                        continue;
                    case L'x':
                        if (cursor < buffer.size()) {
                            buffer.erase(cursor, 1);
                            clamp_command_cursor();
                            render_input_line(prompt, buffer, cursor);
                        }
                        continue;
                    case L'D':
                        if (cursor < buffer.size()) {
                            buffer.erase(cursor);
                            clamp_command_cursor();
                            render_input_line(prompt, buffer, cursor);
                        }
                        continue;
                    default:
                        continue;
                }
            }
        }

        if (ch == 9 && (!g_vi_mode_enabled || vi_state == ViState::Insert)) {
            size_t token_start = find_completion_token_start(buffer, cursor);
            std::wstring prefix = buffer.substr(token_start, cursor - token_start);
            CompletionQuery query = analyze_completion_query(prefix);
            std::vector<std::wstring> matches = collect_completion_candidates(prefix);

            if (matches.empty()) {
                std::wcout << L'\a';
                std::wcout.flush();
                continue;
            }

            std::wstring replacement = format_completion_replacement(query, matches[0]);
            if (matches.size() > 1) {
                std::wstring common_prefix = longest_common_prefix_case_insensitive(matches);
                if (common_prefix.size() > query.normalized_prefix.size()) {
                    replacement = format_completion_replacement(query, common_prefix);
                } else {
                    std::wcout << L"\n";
                    for (const std::wstring& match : matches) {
                        std::wcout << match << L"\n";
                    }
                    std::wcout.flush();
                    initialize_input_line_render_state(); // re-anchor after candidate list shifts the cursor
                    render_input_line(prompt, buffer, cursor);
                    continue;
                }
            }

            buffer.replace(token_start, cursor - token_start, replacement);
            cursor = token_start + replacement.size();
            if (matches.size() == 1 && (cursor == buffer.size() || !std::iswspace(buffer[cursor]))) {
                buffer.insert(cursor, 1, L' ');
                cursor++;
            }
            render_input_line(prompt, buffer, cursor);
            continue;
        }

        if (ch == 8 && (!g_vi_mode_enabled || vi_state == ViState::Insert)) {
            if (cursor > 0) {
                buffer.erase(cursor - 1, 1);
                cursor--;
                render_input_line(prompt, buffer, cursor);
            }
            continue;
        }

        if (ch == 0 || ch == 224) {
            wint_t key = _getwch();
            if (key == 72) {
                history_up();
            } else if (key == 80) {
                history_down();
            } else if (key == 75) {
                if (cursor > 0) {
                    cursor--;
                    render_input_line(prompt, buffer, cursor);
                }
            } else if (key == 77) {
                if (!g_vi_mode_enabled || vi_state == ViState::Insert) {
                    if (cursor < buffer.size()) {
                        cursor++;
                        render_input_line(prompt, buffer, cursor);
                    }
                } else if (!buffer.empty() && cursor + 1 < buffer.size()) {
                    cursor++;
                    render_input_line(prompt, buffer, cursor);
                }
            } else if (key == 71) {
                cursor = 0;
                render_input_line(prompt, buffer, cursor);
            } else if (key == 79) {
                cursor = (!g_vi_mode_enabled || vi_state == ViState::Insert)
                    ? buffer.size()
                    : (buffer.empty() ? 0 : (buffer.size() - 1));
                render_input_line(prompt, buffer, cursor);
            } else if (key == 83) {
                if (cursor < buffer.size()) {
                    buffer.erase(cursor, 1);
                    if (g_vi_mode_enabled && vi_state == ViState::Command) {
                        clamp_command_cursor();
                    }
                    render_input_line(prompt, buffer, cursor);
                }
            }
            continue;
        }

        if (ch >= 32 && (!g_vi_mode_enabled || vi_state == ViState::Insert)) {
            buffer.insert(cursor, 1, static_cast<wchar_t>(ch));
            cursor++;
            render_input_line(prompt, buffer, cursor);
        }
    }
}

bool try_parse_unsigned_long_strict(const std::wstring& raw, unsigned long& parsed_value);
bool try_parse_positive_int_strict(const std::wstring& raw, int& parsed_value);
bool is_executing_function_scope();
bool is_function_return_requested();
void request_function_return(int status_code);
void restore_function_scope_variables(FunctionScopeContext& context);
void snapshot_local_variable_if_needed(const std::wstring& name);
void set_flag_value(std::map<std::wstring, bool>& flags, const std::wstring& name, bool value);
void sync_exported_environment_variable(const std::wstring& name);

// History section: event lookup and recall resolution.
bool resolve_history_recall(const std::wstring& input, std::wstring& resolved) {
    resolved = input;
    if (input.empty() || input[0] != L'!') {
        return true;
    }

    if (input == L"!!") {
        if (g_command_history.empty()) {
            std::wcerr << L"ksh: history empty\n";
            return false;
        }
        resolved = g_command_history.back().command;
        return true;
    }

    if (input.size() > 1 && input[1] == L'-') {
        unsigned long rel = 0;
        if (!try_parse_unsigned_long_strict(input.substr(2), rel)) {
            std::wcerr << L"ksh: invalid history reference\n";
            return false;
        }
        if (rel == 0 || rel > g_command_history.size()) {
            std::wcerr << L"ksh: history event not found\n";
            return false;
        }
        resolved = g_command_history[g_command_history.size() - static_cast<size_t>(rel)].command;
        return true;
    }

    bool numeric = input.size() > 1;
    for (size_t i = 1; i < input.size(); ++i) {
        if (!std::iswdigit(input[i])) {
            numeric = false;
            break;
        }
    }

    if (numeric) {
        unsigned long index = 0;
        if (!try_parse_unsigned_long_strict(input.substr(1), index)) {
            std::wcerr << L"ksh: invalid history reference\n";
            return false;
        }
        if (index == 0 || index > g_command_history.size()) {
            std::wcerr << L"ksh: history event not found\n";
            return false;
        }
        resolved = g_command_history[static_cast<size_t>(index - 1)].command;
        return true;
    }

    std::wstring prefix = input.substr(1);
    for (size_t i = g_command_history.size(); i > 0; --i) {
        if (starts_with_case_insensitive(g_command_history[i - 1].command, prefix)) {
            resolved = g_command_history[i - 1].command;
            return true;
        }
    }

    std::wcerr << L"ksh: history event not found\n";
    return false;
}

std::wstring format_background_job_line(const BackgroundJob& job);
void close_background_job_handles(BackgroundJob& job);

std::wstring normalize_trap_event_name(const std::wstring& raw) {
    std::wstring token = trim_copy(raw);
    if (token.empty()) {
        return L"";
    }

    if (token.size() >= 3) {
        std::wstring prefix;
        prefix.reserve(3);
        prefix.push_back(static_cast<wchar_t>(std::towupper(token[0])));
        prefix.push_back(static_cast<wchar_t>(std::towupper(token[1])));
        prefix.push_back(static_cast<wchar_t>(std::towupper(token[2])));
        if (prefix == L"SIG") {
            token = token.substr(3);
        }
    }

    std::wstring upper;
    upper.reserve(token.size());
    for (wchar_t ch : token) {
        upper.push_back(static_cast<wchar_t>(std::towupper(ch)));
    }

    if (upper == L"0" || upper == L"EXIT") return L"EXIT";
    if (upper == L"1" || upper == L"HUP") return L"HUP";
    if (upper == L"2" || upper == L"INT") return L"INT";
    if (upper == L"3" || upper == L"BREAK") return L"BREAK";
    if (upper == L"QUIT") return L"QUIT";
    if (upper == L"8" || upper == L"FPE") return L"FPE";
    if (upper == L"10" || upper == L"USR1") return L"USR1";
    if (upper == L"11" || upper == L"SEGV") return L"SEGV";
    if (upper == L"12" || upper == L"USR2") return L"USR2";
    if (upper == L"14" || upper == L"ALRM") return L"ALRM";
    if (upper == L"15" || upper == L"TERM") return L"TERM";
    if (upper == L"17" || upper == L"CHLD" || upper == L"CLD") return L"CHLD";

    return L"";
}

bool has_trap_handler(const std::wstring& event_name) {
    return g_trap_handlers.find(event_name) != g_trap_handlers.end();
}

BOOL WINAPI ksh_console_ctrl_handler(DWORD ctrl_type) {
    switch (ctrl_type) {
    case CTRL_C_EVENT: {
        const bool int_active = InterlockedCompareExchange(&g_int_trap_active, 0, 0) != 0;
        if (int_active) {
            InterlockedExchange(&g_pending_int_trap, 1);
            return TRUE;
        }
        const bool fg_active = InterlockedCompareExchange(&g_foreground_process_active, 0, 0) != 0;
        const bool interactive = InterlockedCompareExchange(&g_in_interactive_loop, 0, 0) != 0;
        if (fg_active || interactive) {
            InterlockedExchange(&g_pending_int_trap, 1);
            return TRUE;
        }
        return FALSE;
    }
    case CTRL_BREAK_EVENT: {
        const bool break_active = InterlockedCompareExchange(&g_break_trap_active, 0, 0) != 0;
        if (break_active) {
            InterlockedExchange(&g_pending_break_trap, 1);
            return TRUE;
        }
        const bool fg_active = InterlockedCompareExchange(&g_foreground_process_active, 0, 0) != 0;
        const bool interactive = InterlockedCompareExchange(&g_in_interactive_loop, 0, 0) != 0;
        if (fg_active || interactive) {
            InterlockedExchange(&g_pending_break_trap, 1);
            return TRUE;
        }
        return FALSE;
    }
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT: {
        const bool hup_active = InterlockedCompareExchange(&g_hup_trap_active, 0, 0) != 0;
        const bool term_active = InterlockedCompareExchange(&g_term_trap_active, 0, 0) != 0;
        if (!hup_active && !term_active) {
            return FALSE;
        }
        if (hup_active) {
            InterlockedExchange(&g_pending_hup_trap, 1);
        }
        InterlockedExchange(&g_pending_term_trap, 1);
        return TRUE;
    }
    default:
        return FALSE;
    }
}

class ScopedDebugPrivilege {
public:
    ScopedDebugPrivilege() {
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token_handle_)) {
            return;
        }

        LUID privilege_luid;
        if (!LookupPrivilegeValueW(nullptr, L"SeDebugPrivilege", &privilege_luid)) {
            return;
        }

        TOKEN_PRIVILEGES requested_state;
        ZeroMemory(&requested_state, sizeof(requested_state));
        requested_state.PrivilegeCount = 1;
        requested_state.Privileges[0].Luid = privilege_luid;
        requested_state.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        DWORD previous_state_size = sizeof(previous_state_);
        const BOOL adjusted = AdjustTokenPrivileges(
            token_handle_,
            FALSE,
            &requested_state,
            sizeof(previous_state_),
            &previous_state_,
            &previous_state_size);
        const DWORD adjust_error = GetLastError();
        if (!adjusted || adjust_error == ERROR_NOT_ALL_ASSIGNED) {
            ZeroMemory(&previous_state_, sizeof(previous_state_));
            return;
        }

        has_previous_state_ = true;
    }

    ~ScopedDebugPrivilege() {
        if (token_handle_ == nullptr) {
            return;
        }

        if (has_previous_state_) {
            AdjustTokenPrivileges(token_handle_, FALSE, &previous_state_, 0, nullptr, nullptr);
        }

        CloseHandle(token_handle_);
    }

private:
    HANDLE token_handle_ = nullptr;
    TOKEN_PRIVILEGES previous_state_ = {};
    bool has_previous_state_ = false;
};

enum class KillBuiltinSignal {
    Hup,
    Int,
    Quit,
    Kill,
    Term
};

struct KillWindowEnumData {
    DWORD pid;
    bool window_found;
};

BOOL CALLBACK kill_window_enum_proc(HWND window_handle, LPARAM parameter) {
    KillWindowEnumData* data = reinterpret_cast<KillWindowEnumData*>(parameter);
    DWORD window_pid = 0;
    GetWindowThreadProcessId(window_handle, &window_pid);

    if (window_pid == data->pid && IsWindowVisible(window_handle)) {
        PostMessageW(window_handle, WM_CLOSE, 0, 0);
        data->window_found = true;
    }

    return TRUE;
}

bool parse_kill_builtin_signal(const std::wstring& raw_signal, KillBuiltinSignal& signal) {
    std::wstring normalized_signal = normalize_trap_event_name(raw_signal);
    if (normalized_signal == L"HUP") {
        signal = KillBuiltinSignal::Hup;
        return true;
    }
    if (normalized_signal == L"INT") {
        signal = KillBuiltinSignal::Int;
        return true;
    }
    if (normalized_signal == L"BREAK" || normalized_signal == L"QUIT") {
        signal = KillBuiltinSignal::Quit;
        return true;
    }
    if (normalized_signal == L"TERM") {
        signal = KillBuiltinSignal::Term;
        return true;
    }

    std::wstring upper_signal;
    upper_signal.reserve(raw_signal.size());
    for (wchar_t ch : raw_signal) {
        upper_signal.push_back(static_cast<wchar_t>(std::towupper(ch)));
    }
    if (upper_signal.rfind(L"SIG", 0) == 0) {
        upper_signal = upper_signal.substr(3);
    }

    if (upper_signal == L"9" || upper_signal == L"KILL") {
        signal = KillBuiltinSignal::Kill;
        return true;
    }

    return false;
}

std::wstring signal_spec_to_trap_event(const std::wstring& raw_signal) {
    std::wstring normalized_signal = normalize_trap_event_name(raw_signal);
    if (normalized_signal == L"QUIT") {
        return L"BREAK";
    }
    return normalized_signal;
}

bool force_kill_process_by_pid(DWORD pid) {
    HANDLE process_handle = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (process_handle == nullptr) {
        return false;
    }

    const BOOL terminated = TerminateProcess(process_handle, 1);
    CloseHandle(process_handle);
    return terminated != FALSE;
}

bool graceful_kill_process_by_pid(DWORD pid) {
    KillWindowEnumData enum_data = { pid, false };
    EnumWindows(kill_window_enum_proc, reinterpret_cast<LPARAM>(&enum_data));
    if (!enum_data.window_found) {
        return force_kill_process_by_pid(pid);
    }
    return true;
}

bool send_console_event_to_pid(DWORD pid, DWORD control_event) {
    if (!AttachConsole(pid)) {
        return false;
    }

    SetConsoleCtrlHandler(nullptr, TRUE);
    const BOOL generated = GenerateConsoleCtrlEvent(control_event, 0);
    FreeConsole();
    SetConsoleCtrlHandler(nullptr, FALSE);
    return generated != FALSE;
}

bool execute_registered_trap(const std::wstring& event_name, bool& should_exit_shell) {
    std::map<std::wstring, std::wstring>::const_iterator it = g_trap_handlers.find(event_name);
    if (it == g_trap_handlers.end()) {
        return true;
    }

    const std::wstring command = it->second;
    if (command.empty()) {
        return true;
    }

    if (g_running_trap_handler) {
        return true;
    }

    ScopedTrapExecutionFlag trap_guard(g_running_trap_handler);
    bool trap_exit_requested = false;
    bool ok = execute_command_line(command, trap_exit_requested);
    if (trap_exit_requested) {
        should_exit_shell = true;
    }
    return ok;
}

bool queue_pending_trap_event(const std::wstring& event_name) {
    if (event_name == L"INT") {
        InterlockedExchange(&g_pending_int_trap, 1);
        return true;
    }
    if (event_name == L"BREAK") {
        InterlockedExchange(&g_pending_break_trap, 1);
        return true;
    }
    if (event_name == L"HUP") {
        InterlockedExchange(&g_pending_hup_trap, 1);
        return true;
    }
    if (event_name == L"ALRM") {
        InterlockedExchange(&g_pending_alrm_trap, 1);
        return true;
    }
    if (event_name == L"USR1") {
        InterlockedExchange(&g_pending_usr1_trap, 1);
        return true;
    }
    if (event_name == L"USR2") {
        InterlockedExchange(&g_pending_usr2_trap, 1);
        return true;
    }
    if (event_name == L"SEGV") {
        InterlockedExchange(&g_pending_segv_trap, 1);
        return true;
    }
    if (event_name == L"FPE") {
        InterlockedExchange(&g_pending_fpe_trap, 1);
        return true;
    }
    if (event_name == L"TERM") {
        InterlockedExchange(&g_pending_term_trap, 1);
        return true;
    }
    if (event_name == L"CHLD") {
        InterlockedExchange(&g_pending_chld_trap, 1);
        return true;
    }
    return false;
}

void process_pending_traps(bool& should_exit_shell) {
    if (InterlockedExchange(&g_pending_int_trap, 0) != 0) {
        if (!has_trap_handler(L"INT")) {
            ksh_env.variables[L"?"] = L"130";
        }
        execute_registered_trap(L"INT", should_exit_shell);
    }

    if (InterlockedExchange(&g_pending_break_trap, 0) != 0) {
        if (!has_trap_handler(L"BREAK")) {
            ksh_env.variables[L"?"] = L"131";
        }
        execute_registered_trap(L"BREAK", should_exit_shell);
    }

    if (InterlockedExchange(&g_pending_hup_trap, 0) != 0) {
        if (!has_trap_handler(L"HUP")) {
            ksh_env.variables[L"?"] = L"129";
            should_exit_shell = true;
        }
        execute_registered_trap(L"HUP", should_exit_shell);
    }

    if (InterlockedExchange(&g_pending_alrm_trap, 0) != 0) {
        execute_registered_trap(L"ALRM", should_exit_shell);
    }

    if (InterlockedExchange(&g_pending_usr1_trap, 0) != 0) {
        execute_registered_trap(L"USR1", should_exit_shell);
    }

    if (InterlockedExchange(&g_pending_usr2_trap, 0) != 0) {
        execute_registered_trap(L"USR2", should_exit_shell);
    }

    if (InterlockedExchange(&g_pending_segv_trap, 0) != 0) {
        execute_registered_trap(L"SEGV", should_exit_shell);
    }

    if (InterlockedExchange(&g_pending_fpe_trap, 0) != 0) {
        execute_registered_trap(L"FPE", should_exit_shell);
    }

    if (InterlockedExchange(&g_pending_term_trap, 0) != 0) {
        if (!has_trap_handler(L"TERM")) {
            ksh_env.variables[L"?"] = L"143";
            should_exit_shell = true;
        }
        execute_registered_trap(L"TERM", should_exit_shell);
    }

    if (InterlockedExchange(&g_pending_chld_trap, 0) != 0) {
        execute_registered_trap(L"CHLD", should_exit_shell);
    }
}

void run_exit_trap_once(bool& should_exit_shell) {
    if (g_exit_trap_executed) {
        return;
    }

    g_exit_trap_executed = true;
    execute_registered_trap(L"EXIT", should_exit_shell);
}

void update_background_jobs(bool print_completions) {
    auto ensure_job_handle_views = [](BackgroundJob& job) {
        if (job.process_handles.empty() && job.process_handle != nullptr) {
            job.process_handles.push_back(job.process_handle);
        }
        if (job.pids.empty() && job.pid != 0) {
            job.pids.push_back(job.pid);
        }
        if (!job.process_handles.empty()) {
            job.process_handle = job.process_handles.back();
        }
        if (!job.pids.empty()) {
            job.pid = job.pids.back();
        }
    };

    for (BackgroundJob& job : g_background_jobs) {
        if (job.completed) {
            continue;
        }

        ensure_job_handle_views(job);

        if (job.process_handles.empty()) {
            job.completed = true;
            job.exit_code = 1;
            job.completion_reported = true;
            continue;
        }

        bool all_complete = true;
        DWORD last_code = 0;
        for (size_t i = 0; i < job.process_handles.size(); ++i) {
            HANDLE handle = job.process_handles[i];
            if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
                last_code = 1;
                continue;
            }

            DWORD code = STILL_ACTIVE;
            bool is_pipeline_thread = false;
            {
                std::lock_guard<std::mutex> lock(g_pipeline_threads_mutex);
                is_pipeline_thread = (g_pipeline_thread_handles.find(handle) != g_pipeline_thread_handles.end());
            }
            if (is_pipeline_thread) {
                if (!GetExitCodeThread(handle, &code)) code = 1;
            } else if (!GetExitCodeProcess(handle, &code)) {
                code = 1;
            }

            if (i + 1 == job.process_handles.size()) {
                last_code = code;
            }

            if (code == STILL_ACTIVE) {
                all_complete = false;
            }
        }

        if (all_complete) {
            for (HANDLE handle : job.process_handles) {
                register_child_process_times(handle);
            }
            job.completed = true;
            job.exit_code = (last_code == STILL_ACTIVE) ? 1 : last_code;
            if (print_completions && !job.completion_reported) {
                std::wcout << format_background_job_line(job) << L"\n";
            }
            job.completion_reported = true;
            // Release process handles as soon as the job is done to avoid handle buildup.
            close_background_job_handles(job);
        }
    }
}

BackgroundJob* find_background_job(int job_id) {
    for (BackgroundJob& job : g_background_jobs) {
        if (job.id == job_id) {
            return &job;
        }
    }
    return nullptr;
}

BackgroundJob* find_default_background_job_for_resume_or_foreground() {
    for (size_t i = g_background_jobs.size(); i > 0; --i) {
        BackgroundJob& job = g_background_jobs[i - 1];
        if (!job.completed) {
            return &job;
        }
    }
    return nullptr;
}

bool resolve_job_reference(const std::wstring& raw, int& job_id) {
    if (raw.empty()) {
        return false;
    }

    if (raw == L"%+" || raw == L"%%") {
        if (g_background_jobs.empty()) {
            return false;
        }
        job_id = g_background_jobs.back().id;
        return true;
    }

    if (raw == L"%-") {
        if (g_background_jobs.size() < 2) {
            return false;
        }
        job_id = g_background_jobs[g_background_jobs.size() - 2].id;
        return true;
    }

    std::wstring value = raw;
    if (!value.empty() && value[0] == L'%') {
        value = value.substr(1);
    }
    if (value.empty()) {
        return false;
    }

    int parsed = 0;
    if (!try_parse_positive_int_strict(value, parsed)) {
        return false;
    }
    job_id = parsed;
    return true;
}

std::wstring format_background_job_line(const BackgroundJob& job) {
    std::wstring line = L"[" + std::to_wstring(job.id) + L"] ";
    if (job.completed) {
        line += L"Done (exit=" + std::to_wstring(job.exit_code) + L") ";
    } else {
        line += L"Running pid=" + std::to_wstring(job.pid) + L" ";
    }
    line += job.command;
    return line;
}

bool parse_job_id(const std::wstring& raw, int& job_id) {
    if (raw.empty()) {
        return false;
    }

    std::wstring value = raw;
    if (!value.empty() && value[0] == L'%') {
        value = value.substr(1);
    }
    if (value.empty()) {
        return false;
    }

    int parsed = 0;
    if (!try_parse_positive_int_strict(value, parsed)) {
        return false;
    }
    job_id = parsed;
    return true;
}

bool try_parse_int_strict(const std::wstring& raw, int& parsed_value) {
    if (raw.empty()) {
        return false;
    }

    wchar_t* end_ptr = nullptr;
    errno = 0;
    long parsed = std::wcstol(raw.c_str(), &end_ptr, 10);
    if (errno != 0 || end_ptr == raw.c_str() || (end_ptr != nullptr && *end_ptr != L'\0')) {
        return false;
    }
    if (parsed < static_cast<long>(INT_MIN) || parsed > static_cast<long>(INT_MAX)) {
        return false;
    }

    parsed_value = static_cast<int>(parsed);
    return true;
}

bool try_parse_unsigned_long_strict(const std::wstring& raw, unsigned long& parsed_value) {
    if (raw.empty()) {
        return false;
    }

    wchar_t* end_ptr = nullptr;
    errno = 0;
    unsigned long parsed = std::wcstoul(raw.c_str(), &end_ptr, 10);
    if (errno != 0 || end_ptr == raw.c_str() || (end_ptr != nullptr && *end_ptr != L'\0')) {
        return false;
    }

    parsed_value = parsed;
    return true;
}

bool try_parse_positive_int_strict(const std::wstring& raw, int& parsed_value) {
    if (!try_parse_int_strict(raw, parsed_value)) {
        return false;
    }
    return parsed_value > 0;
}

void close_background_job_handles(BackgroundJob& job) {
    std::vector<HANDLE> closed;
    closed.reserve(job.process_handles.size() + 1);

    auto close_once = [&](HANDLE handle) {
        if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
            return;
        }
        if (std::find(closed.begin(), closed.end(), handle) != closed.end()) {
            return;
        }
        {
            std::lock_guard<std::mutex> lock(g_pipeline_threads_mutex);
            g_pipeline_thread_handles.erase(handle);
        }
        CloseHandle(handle);
        closed.push_back(handle);
    };

    for (HANDLE handle : job.process_handles) {
        close_once(handle);
    }
    close_once(job.process_handle);

    for (const std::wstring& path : job.temp_file_paths) {
        if (!path.empty()) {
            DeleteFileW(path.c_str());
        }
    }
    job.temp_file_paths.clear();

    job.process_handle = nullptr;
    job.process_handles.clear();
}

bool wait_for_background_job(BackgroundJob& job, DWORD& exit_code) {
    update_background_jobs(false);

    if (job.completed) {
        close_background_job_handles(job);
        exit_code = job.exit_code;
        return true;
    }

    if (job.process_handles.empty() && job.process_handle != nullptr) {
        job.process_handles.push_back(job.process_handle);
    }

    if (job.process_handles.empty()) {
        exit_code = 1;
        job.completed = true;
        job.exit_code = 1;
        return false;
    }

    DWORD last_code = 1;
    bool ok = true;
    for (size_t i = 0; i < job.process_handles.size(); ++i) {
        HANDLE handle = job.process_handles[i];
        if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
            ok = false;
            if (i + 1 == job.process_handles.size()) {
                last_code = 1;
            }
            continue;
        }

        WaitForSingleObject(handle, INFINITE);
        register_child_process_times(handle);
        DWORD code = 1;
        if (!GetExitCodeProcess(handle, &code)) {
            code = 1;
            ok = false;
        }

        if (i + 1 == job.process_handles.size()) {
            last_code = code;
        }
    }

    job.completed = true;
    job.exit_code = last_code;
    job.completion_reported = true;
    close_background_job_handles(job);
    exit_code = last_code;
    return ok;
}

void remove_background_job(int job_id) {
    for (size_t i = 0; i < g_background_jobs.size(); ++i) {
        if (g_background_jobs[i].id == job_id) {
            close_background_job_handles(g_background_jobs[i]);
            g_background_jobs.erase(g_background_jobs.begin() + static_cast<long long>(i));
            return;
        }
    }
}

void cleanup_all_background_jobs() {
    for (BackgroundJob& job : g_background_jobs) {
        close_background_job_handles(job);
    }
    g_background_jobs.clear();
}

const std::vector<std::wstring>& current_script_args() {
    static const std::vector<std::wstring> empty_args;
    if (g_script_context_stack.empty()) {
        return empty_args;
    }
    return g_script_context_stack.back().args;
}

// Script context section: current script name and argument helpers.
std::wstring current_script_name() {
    if (g_script_context_stack.empty()) {
        return L"";
    }
    return g_script_context_stack.back().script_name;
}

std::wstring join_script_args(const std::vector<std::wstring>& args) {
    std::wstring joined;
    for (size_t i = 0; i < args.size(); ++i) {
        joined += args[i];
        if (i + 1 < args.size()) {
            joined += L" ";
        }
    }
    return joined;
}

std::wstring trim_copy(const std::wstring& value) {
    return trim_copy(std::wstring_view(value));
}

std::wstring trim_copy(std::wstring_view value) {
    const std::wstring_view whitespace = L" \t\r\n";
    const size_t start = value.find_first_not_of(whitespace);
    if (start == std::wstring::npos) {
        return L"";
    }
    const size_t end = value.find_last_not_of(whitespace);
    return std::wstring(value.substr(start, end - start + 1));
}

bool ends_with_case_insensitive(const std::wstring& value, const std::wstring& suffix) {
    if (suffix.size() > value.size()) {
        return false;
    }

    const size_t offset = value.size() - suffix.size();
    for (size_t i = 0; i < suffix.size(); ++i) {
        if (std::towlower(value[offset + i]) != std::towlower(suffix[i])) {
            return false;
        }
    }
    return true;
}

std::wstring quote_command_argument(const std::wstring& arg) {
    if (arg.empty()) {
        return L"\"\"";
    }

    bool needs_quotes = false;
    for (wchar_t ch : arg) {
        if (std::iswspace(ch) || ch == L'"') {
            needs_quotes = true;
            break;
        }
    }

    if (!needs_quotes) {
        return arg;
    }

    std::wstring escaped;
    escaped.reserve(arg.size() + 8);
    escaped.push_back(L'"');

    size_t backslash_run = 0;
    for (wchar_t ch : arg) {
        if (ch == L'\\') {
            backslash_run++;
            continue;
        }

        if (ch == L'"') {
            escaped.append(backslash_run * 2 + 1, L'\\');
            escaped.push_back(L'"');
            backslash_run = 0;
            continue;
        }

        if (backslash_run > 0) {
            escaped.append(backslash_run, L'\\');
            backslash_run = 0;
        }

        escaped.push_back(ch);
    }

    if (backslash_run > 0) {
        escaped.append(backslash_run * 2, L'\\');
    }

    escaped.push_back(L'"');
    return escaped;
}

std::wstring quote_shell_argument(const std::wstring& arg) {
    std::wstring quoted = L"'";
    for (wchar_t ch : arg) {
        if (ch == L'\'') {
            quoted += L"'\\''";
        } else {
            quoted.push_back(ch);
        }
    }
    quoted.push_back(L'\'');
    return quoted;
}

bool build_cmd_shell_command_line(const std::wstring& command, std::wstring& command_line) {
    // Pass command as a single argv item to cmd so spaces/quotes do not split unexpectedly.
    std::wstring cmd_path;
    if (!resolve_cmd_exe_path(cmd_path)) {
        return false;
    }

    command_line = quote_command_argument(cmd_path) + L" /d /s /c \"" + command + L"\"";
    return true;
}

std::wstring join_tokens_as_command_line(const std::vector<std::wstring>& tokens) {
    std::wstring command;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (!command.empty()) {
            command += L" ";
        }
        command += quote_command_argument(tokens[i]);
    }
    return command;
}

std::wstring join_tokens_with_spaces(const std::vector<std::wstring>& tokens, size_t start_index = 0) {
    size_t estimated_size = 0;
    if (start_index < tokens.size()) {
        for (size_t i = start_index; i < tokens.size(); ++i) {
            estimated_size += tokens[i].size();
            if (i + 1 < tokens.size()) {
                estimated_size += 1;
            }
        }
    }

    std::wstring joined;
    joined.reserve(estimated_size);
    for (size_t i = start_index; i < tokens.size(); ++i) {
        if (!joined.empty()) {
            joined += L" ";
        }
        joined += tokens[i];
    }
    return joined;
}

std::wstring quote_for_single_quoted_shell_literal(const std::wstring& value) {
    std::wstring quoted = L"'";
    for (wchar_t ch : value) {
        if (ch == L'\'') {
            quoted += L"'\\''";
            continue;
        }
        quoted.push_back(ch);
    }
    quoted += L"'";
    return quoted;
}

bool resolve_external_command_path(const std::wstring& command_name, std::wstring& resolved_path) {
    resolved_path.clear();
    if (command_name.empty()) {
        return false;
    }

    if (command_name.find_first_of(L"/\\") != std::wstring::npos) {
        DWORD attrs = GetFileAttributesW(command_name.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            resolved_path = command_name;
            return true;
        }
        return false;
    }

    auto hash_it = g_command_hash_table.find(command_name);
    if (hash_it != g_command_hash_table.end()) {
        DWORD attrs = GetFileAttributesW(hash_it->second.path.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            hash_it->second.hits++;
            resolved_path = hash_it->second.path;
            return true;
        } else {
            g_command_hash_table.erase(hash_it);
        }
    }

    wchar_t buffer[MAX_PATH];
    DWORD length = SearchPathW(nullptr, command_name.c_str(), nullptr, MAX_PATH, buffer, nullptr);
    if (length > 0 && length < MAX_PATH) {
        resolved_path = buffer;
        g_command_hash_table[command_name] = { resolved_path, 1 };
        return true;
    }

    std::wstring pathext = get_system_env_var(L"PATHEXT");
    if (pathext.empty()) {
        pathext = L".COM;.EXE;.BAT;.CMD";
    }

    std::vector<std::wstring> extensions;
    size_t ext_start = 0;
    while (ext_start < pathext.size()) {
        size_t semi = pathext.find(L';', ext_start);
        std::wstring ext = (semi != std::wstring::npos) ? pathext.substr(ext_start, semi - ext_start) : pathext.substr(ext_start);
        ext = trim_copy(ext);
        if (!ext.empty()) {
            if (ext[0] != L'.') {
                ext = L"." + ext;
            }
            extensions.push_back(to_lower_copy(ext));
        }
        if (semi == std::wstring::npos) break;
        ext_start = semi + 1;
    }
    if (extensions.empty()) {
        extensions = { L".com", L".exe", L".bat", L".cmd" };
    }

    for (const auto& ext : extensions) {
        length = SearchPathW(nullptr, command_name.c_str(), ext.c_str(), MAX_PATH, buffer, nullptr);
        if (length > 0 && length < MAX_PATH) {
            resolved_path = buffer;
            g_command_hash_table[command_name] = { resolved_path, 1 };
            return true;
        }
    }

    return false;
}

enum class CommandResolutionKind {
    Alias,
    Function,
    Builtin,
    External,
    Missing
};

bool can_capture_builtin_command_substitution(const std::vector<std::wstring>& tokens) {
    if (tokens.empty()) {
        return false;
    }

    // Only allow builtins that are expected to be stateless with respect to
    // shell globals so command substitution can safely avoid full env snapshots.
    static const std::vector<std::wstring> capture_safe_builtins = {
        L":",
        L"echo",
        L"getconf",
        L"help",
        L"math",
        L"pathchk",
        L"print",
        L"printf",
        L"pwd",
        L"times",
    };

    if (std::find(capture_safe_builtins.begin(), capture_safe_builtins.end(), tokens[0]) == capture_safe_builtins.end()) {
        return false;
    }

    // Ensure it is not shadowed by an alias or user-defined function
    if (g_aliases.find(tokens[0]) != g_aliases.end()) {
        return false;
    }
    if (g_shell_functions.find(tokens[0]) != g_shell_functions.end()) {
        return false;
    }

    return true;
}

bool is_snapshot_safe_external_segment(const std::wstring& segment_text) {
    const std::wstring trimmed = trim_copy(segment_text);
    if (trimmed.empty()) {
        return false;
    }

    std::vector<std::wstring> tokens = ksh_tokenize_preserve_quotes(trimmed);
    if (tokens.empty()) {
        return false;
    }

    RedirectionSpec redir;
    std::wstring redirection_error;
    if (!parse_redirections(tokens, redir, redirection_error)) {
        return false;
    }

    if (tokens.empty()) {
        return false;
    }

    const std::wstring residual = join_tokens_with_spaces(tokens);
    if (has_unquoted_shell_metacharacters(residual)) {
        return false;
    }

    // Leading NAME=VALUE assignment mutates shell state.
    const std::wstring& first = tokens[0];
    size_t eq_pos = first.find(L'=');
    if (eq_pos != std::wstring::npos && eq_pos > 0) {
        std::wstring lhs = first.substr(0, eq_pos);
        if (is_valid_shell_identifier(lhs)) {
            return false;
        }
    }

    std::wstring detail;
    CommandResolutionKind kind = resolve_command_kind(first, detail);
    return kind == CommandResolutionKind::External;
}

bool command_requires_environment_snapshot(const std::wstring& command) {
    const std::wstring trimmed = trim_copy(command);
    if (trimmed.empty()) {
        return true;
    }

    if (!has_unquoted_shell_metacharacters(trimmed)) {
        return !is_snapshot_safe_external_segment(trimmed);
    }

    // Allow external-only pipelines (with optional redirections) to bypass
    // full environment snapshots; they do not mutate shell variable state.
    std::vector<std::wstring> pipeline_segments = split_pipeline_segments(trimmed);
    if (pipeline_segments.size() <= 1) {
        return true;
    }

    for (const std::wstring& segment : pipeline_segments) {
        if (!is_snapshot_safe_external_segment(segment)) {
            return true;
        }
    }

    return false;
}

std::wstring execute_command_substitution(const std::wstring& command, bool use_capture_sink) {
    const bool perf_trace = is_performance_telemetry_enabled();
    const ULONGLONG substitution_start = GetTickCount64();

    const bool require_state_snapshot = !use_capture_sink && command_requires_environment_snapshot(command);
    ScopedSubshellDepthGuard subshell_depth_guard;
    std::optional<ShellStateSnapshot> saved_snapshot;
    ScopedSnapshotRestoreGuard snapshot_guard(saved_snapshot, L"0");

    std::wstring sub_output;

    if (use_capture_sink) {
        struct CaptureSinkGuard {
            std::wstring*& slot;
            std::wstring* previous;
            CaptureSinkGuard(std::wstring*& slot_ref, std::wstring* current) : slot(slot_ref), previous(slot_ref) {
                slot = current;
            }
            ~CaptureSinkGuard() {
                slot = previous;
            }
        } capture_guard(g_builtin_capture_output, &sub_output);

        bool should_exit = false;
        execute_command_line(command, should_exit);
    } else {
        HANDLE read_pipe = INVALID_HANDLE_VALUE;
        HANDLE write_pipe = INVALID_HANDLE_VALUE;
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = nullptr;
        sa.bInheritHandle = TRUE;

        if (CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
            if (!SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0)) {
                CloseHandle(read_pipe);
                CloseHandle(write_pipe);
            } else {
                HANDLE old_subshell_stdout = g_subshell_stdout;
                g_subshell_stdout = write_pipe;
                std::string bytes;
                bool output_limit_hit = false;

                bool reader_started = false;
                std::thread reader_thread;
                try {
                    reader_thread = std::thread([&]() {
                        char buffer[4096];
                        DWORD bytesRead = 0;
                        while (ReadFile(read_pipe, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
                            if (output_limit_hit) {
                                continue;
                            }

                            const size_t current_size = bytes.size();
                            if (current_size + bytesRead > kMaxCommandSubstitutionOutputBytes) {
                                const size_t remaining = kMaxCommandSubstitutionOutputBytes - current_size;
                                if (remaining > 0) {
                                    bytes.append(buffer, remaining);
                                }
                                output_limit_hit = true;
                                continue;
                            }

                            bytes.append(buffer, bytesRead);
                        }
                    });
                    reader_started = true;
                } catch (...) {
                    g_subshell_stdout = old_subshell_stdout;
                    CloseHandle(write_pipe);
                    CloseHandle(read_pipe);
                }

                if (reader_started) {
                    try {
                        if (require_state_snapshot) {
                            saved_snapshot.emplace(capture_shell_state_snapshot());
                        }

                        bool should_exit = false;
                        execute_command_line(command, should_exit);
                    } catch (...) {
                        if (write_pipe != INVALID_HANDLE_VALUE) {
                            CloseHandle(write_pipe);
                            write_pipe = INVALID_HANDLE_VALUE;
                        }
                        g_subshell_stdout = old_subshell_stdout;
                        if (reader_thread.joinable()) {
                            reader_thread.join();
                        }
                        if (read_pipe != INVALID_HANDLE_VALUE) {
                            CloseHandle(read_pipe);
                            read_pipe = INVALID_HANDLE_VALUE;
                        }
                        throw;
                    }

                    if (write_pipe != INVALID_HANDLE_VALUE) {
                        CloseHandle(write_pipe);
                        write_pipe = INVALID_HANDLE_VALUE;
                    }
                    g_subshell_stdout = old_subshell_stdout;

                    if (reader_thread.joinable()) {
                        reader_thread.join();
                    }
                    if (read_pipe != INVALID_HANDLE_VALUE) {
                        CloseHandle(read_pipe);
                        read_pipe = INVALID_HANDLE_VALUE;
                    }
                }

                if (reader_started && output_limit_hit) {
                    std::wcerr << L"ksh: command substitution output exceeded limit\n";
                    g_expansion_error = true;
                    ksh_env.variables[L"?"] = L"1";
                } else if (reader_started && !bytes.empty()) {
                    int wlen = MultiByteToWideChar(CP_UTF8, 0, bytes.data(), (int)bytes.size(), NULL, 0);
                    if (wlen > 0) {
                        std::vector<wchar_t> wbuf(wlen);
                        MultiByteToWideChar(CP_UTF8, 0, bytes.data(), (int)bytes.size(), wbuf.data(), wlen);
                        sub_output = std::wstring(wbuf.data(), wlen);
                    } else {
                        wlen = MultiByteToWideChar(CP_ACP, 0, bytes.data(), (int)bytes.size(), NULL, 0);
                        if (wlen > 0) {
                            std::vector<wchar_t> wbuf(wlen);
                            MultiByteToWideChar(CP_ACP, 0, bytes.data(), (int)bytes.size(), wbuf.data(), wlen);
                            sub_output = std::wstring(wbuf.data(), wlen);
                        }
                    }
                }
            }
        }
    }

    const bool used_snapshot_restore = saved_snapshot.has_value();
    restore_shell_state_snapshot_if_present(saved_snapshot, L"0");
    snapshot_guard.dismiss();

    while (!sub_output.empty() && (sub_output.back() == L'\n' || sub_output.back() == L'\r')) {
        sub_output.pop_back();
    }

    const ULONGLONG elapsed_ms = GetTickCount64() - substitution_start;
    record_command_substitution_path_perf(use_capture_sink, used_snapshot_restore, elapsed_ms);
    if (perf_trace) {
        std::wstring detail;
        if (use_capture_sink) {
            detail = L"path=fast_capture count=" + std::to_wstring(g_subshell_path_perf_counters.command_subst_fast_count) +
                L" total_ms=" + std::to_wstring(g_subshell_path_perf_counters.command_subst_fast_ms);
        } else if (used_snapshot_restore) {
            detail = L"path=snapshot_restore count=" + std::to_wstring(g_subshell_path_perf_counters.command_subst_snapshot_count) +
                L" total_ms=" + std::to_wstring(g_subshell_path_perf_counters.command_subst_snapshot_ms);
        } else {
            detail = L"path=direct_no_snapshot count=" + std::to_wstring(g_subshell_path_perf_counters.command_subst_direct_count) +
                L" total_ms=" + std::to_wstring(g_subshell_path_perf_counters.command_subst_direct_ms);
        }
        log_performance_trace(L"substitution.command", elapsed_ms, detail);
    }

    return sub_output;
}

CommandResolutionKind resolve_command_kind(const std::wstring& name, std::wstring& detail) {
    detail.clear();
    if (name.empty()) {
        return CommandResolutionKind::Missing;
    }

    std::map<std::wstring, std::wstring>::const_iterator alias_it = g_aliases.find(name);
    if (alias_it != g_aliases.end()) {
        detail = alias_it->second;
        return CommandResolutionKind::Alias;
    }

    if (g_shell_functions.find(name) != g_shell_functions.end()) {
        return CommandResolutionKind::Function;
    }

    const std::vector<std::wstring>& builtins = builtin_commands();
    if (std::find(builtins.begin(), builtins.end(), name) != builtins.end()) {
        return CommandResolutionKind::Builtin;
    }

    std::wstring external_path;
    if (resolve_external_command_path(name, external_path)) {
        detail = external_path;
        return CommandResolutionKind::External;
    }

    return CommandResolutionKind::Missing;
}

std::wstring command_kind_description(const std::wstring& name, bool verbose, bool ksh_style) {
    std::wstring detail;
    CommandResolutionKind kind = resolve_command_kind(name, detail);

    if (ksh_style) {
        if (kind == CommandResolutionKind::Alias) {
            return name + L" is an alias for " + detail;
        }
        if (kind == CommandResolutionKind::Function) {
            return name + L" is a function";
        }
        if (kind == CommandResolutionKind::Builtin) {
            return name + L" is a shell builtin";
        }
        if (kind == CommandResolutionKind::External) {
            return name + L" is " + detail;
        }
        return name + L" not found";
    }

    if (verbose) {
        if (kind == CommandResolutionKind::Alias) {
            return name + L"\talias\t" + detail;
        }
        if (kind == CommandResolutionKind::Function) {
            return name + L"\tfunction";
        }
        if (kind == CommandResolutionKind::Builtin) {
            return name + L"\tbuiltin";
        }
        if (kind == CommandResolutionKind::External) {
            return name + L"\texternal\t" + detail;
        }
        return name + L"\tnot found";
    }

    if (kind == CommandResolutionKind::Alias) {
        return L"alias";
    }
    if (kind == CommandResolutionKind::Function) {
        return L"function";
    }
    if (kind == CommandResolutionKind::Builtin) {
        return L"builtin";
    }
    if (kind == CommandResolutionKind::External) {
        return detail;
    }
    return L"";
}

bool parse_positive_int(const std::wstring& text, int& value) {
    if (!try_parse_positive_int_strict(text, value)) {
        return false;
    }

    return true;
}

bool is_internal_shell_state_variable(const std::wstring& name) {
    return name == kGetoptsCursorVar || name == kGetoptsOptindMirrorVar;
}

bool has_any_redirection(const RedirectionSpec& redir) {
    return redir.has_stdin || redir.has_stdout || redir.stdout_to_stderr || redir.has_stderr || redir.stderr_to_stdout;
}

bool try_parse_custom_fd_redirection_token(
    const std::wstring& token,
    std::vector<std::wstring>& tokens,
    size_t& token_index,
    RedirectionSpec& redir,
    std::wstring& error_message,
    bool& handled) {
    handled = false;
    if (token.size() < 2 || !std::iswdigit(token[0])) {
        return true;
    }

    const int fd = token[0] - L'0';
    if (!is_custom_fd_in_range(fd)) {
        return true;
    }

    std::wstring suffix = token.substr(1);
    if (suffix.empty() || (suffix[0] != L'>' && suffix[0] != L'<')) {
        return true;
    }
    handled = true;

    auto consume_target = [&](size_t op_len, std::wstring& target_out) -> bool {
        if (suffix.size() > op_len) {
            target_out = suffix.substr(op_len);
            return true;
        }
        if (token_index + 1 >= tokens.size()) {
            return false;
        }
        token_index++;
        target_out = tokens[token_index];
        return true;
    };

    if (suffix == L">&-" || suffix == L"<&-") {
        RedirectionSpec::CustomFdAction action;
        action.fd = fd;
        action.kind = RedirectionSpec::CustomFdAction::Kind::Close;
        redir.custom_fd_actions.push_back(action);
        return true;
    }

    if (suffix.rfind(L">&", 0) == 0) {
        std::wstring source_text;
        if (!consume_target(2, source_text) || source_text.size() != 1 || !std::iswdigit(source_text[0])) {
            error_message = L"ksh: invalid custom fd duplication: " + token;
            return false;
        }

        RedirectionSpec::CustomFdAction action;
        action.fd = fd;
        action.kind = RedirectionSpec::CustomFdAction::Kind::Duplicate;
        action.duplicate_source_fd = source_text[0] - L'0';
        redir.custom_fd_actions.push_back(action);
        return true;
    }

    std::wstring target_path;
    if (suffix.rfind(L">>", 0) == 0) {
        if (!consume_target(2, target_path) || target_path.empty()) {
            error_message = L"ksh: missing file for custom fd append redirection";
            return false;
        }
        RedirectionSpec::CustomFdAction action;
        action.fd = fd;
        action.kind = RedirectionSpec::CustomFdAction::Kind::OpenWriteAppend;
        action.path = target_path;
        redir.custom_fd_actions.push_back(action);
        return true;
    }

    if (suffix.rfind(L">", 0) == 0) {
        if (!consume_target(1, target_path) || target_path.empty()) {
            error_message = L"ksh: missing file for custom fd output redirection";
            return false;
        }
        RedirectionSpec::CustomFdAction action;
        action.fd = fd;
        action.kind = RedirectionSpec::CustomFdAction::Kind::OpenWriteTruncate;
        action.path = target_path;
        redir.custom_fd_actions.push_back(action);
        return true;
    }

    if (suffix.rfind(L"<", 0) == 0) {
        if (!consume_target(1, target_path) || target_path.empty()) {
            error_message = L"ksh: missing file for custom fd input redirection";
            return false;
        }
        RedirectionSpec::CustomFdAction action;
        action.fd = fd;
        action.kind = RedirectionSpec::CustomFdAction::Kind::OpenRead;
        action.path = target_path;
        redir.custom_fd_actions.push_back(action);
        return true;
    }

    error_message = L"ksh: unsupported custom fd redirection construct: " + token;
    return false;
}

// Parser section: redirection normalization and token filtering.
bool parse_redirections(std::vector<std::wstring>& tokens, RedirectionSpec& redir, std::wstring& error_message) {
    std::vector<std::wstring> filtered;
    filtered.reserve(tokens.size());

    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::wstring& token = tokens[i];

        bool handled_custom_fd = false;
        if (!try_parse_custom_fd_redirection_token(token, tokens, i, redir, error_message, handled_custom_fd)) {
            return false;
        }
        if (handled_custom_fd) {
            continue;
        }

        enum class RedirKind {
            None,
            In,
            OutTrunc,
            OutAppend,
            OutToErr,
            ErrTrunc,
            ErrAppend,
            ErrToOut
        };

        auto classify_token = [&](const std::wstring& value, size_t& operator_len, RedirKind& kind) -> bool {
            operator_len = 0;
            kind = RedirKind::None;

            if (value == L"2>&1") {
                operator_len = value.size();
                kind = RedirKind::ErrToOut;
                return true;
            }
            if (value == L"1>&2" || value == L">&2") {
                operator_len = value.size();
                kind = RedirKind::OutToErr;
                return true;
            }

            size_t dup_pos = value.find(L">&");
            if (dup_pos != std::wstring::npos) {
                int source_fd = 1;
                if (dup_pos > 0) {
                    if (dup_pos > 1 || !std::iswdigit(value[0])) {
                        return false;
                    }
                    source_fd = value[0] - L'0';
                }

                if (dup_pos + 2 >= value.size() || (dup_pos + 3) != value.size() || !std::iswdigit(value[dup_pos + 2])) {
                    return false;
                }

                int target_fd = value[dup_pos + 2] - L'0';
                if (source_fd == 1 && target_fd == 2) {
                    operator_len = value.size();
                    kind = RedirKind::OutToErr;
                    return true;
                }
                if (source_fd == 2 && target_fd == 1) {
                    operator_len = value.size();
                    kind = RedirKind::ErrToOut;
                    return true;
                }

                // Allow no-op fd self-duplication forms to parse without effect.
                if (source_fd == target_fd && (source_fd == 1 || source_fd == 2)) {
                    operator_len = value.size();
                    kind = RedirKind::None;
                    return true;
                }

                return false;
            }

            // Parse optional leading FD only when at the very start.
            int fd = -1;
            size_t pos = 0;
            if (!value.empty() && std::iswdigit(value[0])) {
                fd = value[0] - L'0';
                pos = 1;
            }

            if (pos >= value.size()) {
                return false;
            }

            // Strict classification: only treat prefix operators as redirection.
            if (value.compare(pos, 2, L">>") == 0) {
                if (fd == -1 || fd == 1) {
                    operator_len = pos + 2;
                    kind = RedirKind::OutAppend;
                    return true;
                }
                if (fd == 2) {
                    operator_len = pos + 2;
                    kind = RedirKind::ErrAppend;
                    return true;
                }
                return false;
            }

            if (value[pos] == L'>') {
                if (fd == -1 || fd == 1) {
                    operator_len = pos + 1;
                    kind = RedirKind::OutTrunc;
                    return true;
                }
                if (fd == 2) {
                    operator_len = pos + 1;
                    kind = RedirKind::ErrTrunc;
                    return true;
                }
                return false;
            }

            if (value[pos] == L'<') {
                // Support <, 0< and 1< forms.
                if (fd == -1 || fd == 0 || fd == 1) {
                    operator_len = pos + 1;
                    kind = RedirKind::In;
                    return true;
                }
                return false;
            }

            return false;
        };

        auto consume_target = [&](size_t operator_len, std::wstring& target_out) -> bool {
            if (token.size() > operator_len) {
                target_out = token.substr(operator_len);
                return true;
            }
            if (i + 1 >= tokens.size()) {
                return false;
            }
            i++;
            target_out = tokens[i];
            return true;
        };

        size_t op_len = 0;
        RedirKind kind = RedirKind::None;
        if (!classify_token(token, op_len, kind)) {
            size_t redir_pos = 0;
            if (!token.empty() && std::iswdigit(token[0])) {
                redir_pos = 1;
            }

            if (redir_pos < token.size() && (token[redir_pos] == L'>' || token[redir_pos] == L'<')) {
                error_message = L"ksh: unsupported or invalid redirection construct: " + token;
                return false;
            }

            // Mixed tokens like "a>b" are arguments, not redirection operators.
            filtered.push_back(token);
            continue;
        }

        std::wstring target;
        if (kind == RedirKind::None) {
            continue;
        }

        if (kind == RedirKind::ErrToOut) {
            redir.stderr_to_stdout = true;
            redir.has_stderr = false;
            redir.append_stderr = false;
            redir.stderr_path.clear();
            continue;
        }

        if (kind == RedirKind::OutToErr) {
            redir.stdout_to_stderr = true;
            redir.has_stdout = false;
            redir.append_stdout = false;
            redir.stdout_path.clear();
            continue;
        }

        if (kind == RedirKind::In) {
            if (!consume_target(op_len, target)) {
                error_message = L"ksh: missing file for input redirection";
                return false;
            }
            redir.has_stdin = true;
            redir.stdin_path = target;
            continue;
        }

        if (kind == RedirKind::OutAppend) {
            if (!consume_target(op_len, target)) {
                error_message = L"ksh: missing file for output redirection";
                return false;
            }
            redir.has_stdout = true;
            redir.stdout_to_stderr = false;
            redir.append_stdout = true;
            redir.stdout_path = target;
            continue;
        }

        if (kind == RedirKind::OutTrunc) {
            if (!consume_target(op_len, target)) {
                error_message = L"ksh: missing file for output redirection";
                return false;
            }
            redir.has_stdout = true;
            redir.stdout_to_stderr = false;
            redir.append_stdout = false;
            redir.stdout_path = target;
            continue;
        }

        if (kind == RedirKind::ErrAppend) {
            if (!consume_target(op_len, target)) {
                error_message = L"ksh: missing file for stderr redirection";
                return false;
            }
            redir.has_stderr = true;
            redir.stderr_to_stdout = false;
            redir.append_stderr = true;
            redir.stderr_path = target;
            continue;
        }

        if (kind == RedirKind::ErrTrunc) {
            if (!consume_target(op_len, target)) {
                error_message = L"ksh: missing file for stderr redirection";
                return false;
            }
            redir.has_stderr = true;
            redir.stderr_to_stdout = false;
            redir.append_stderr = false;
            redir.stderr_path = target;
            continue;
        }
    }

    // Ordered fd duplication semantics (e.g. 1>&2 combined with 2>file) are
    // not modeled by this parser; reject ambiguous mixes instead of misrouting.
    if (redir.stdout_to_stderr && (redir.has_stderr || redir.stderr_to_stdout)) {
        error_message = L"ksh: unsupported mixed fd redirection requiring ordered duplication semantics";
        return false;
    }
    if (redir.stderr_to_stdout && (redir.has_stdout || redir.stdout_to_stderr)) {
        error_message = L"ksh: unsupported mixed fd redirection requiring ordered duplication semantics";
        return false;
    }

    tokens = filtered;
    return true;
}

std::vector<std::wstring> split_pipeline_segments(const std::wstring& input) {
    std::vector<std::wstring> segments;
    std::wstring current;
    bool in_single_quotes = false;
    bool in_double_quotes = false;
    bool escaped = false;
    int paren_depth = 0;

    for (size_t i = 0; i < input.size(); ++i) {
        wchar_t ch = input[i];

        if (escaped) {
            current += ch;
            escaped = false;
            continue;
        }

        if (ch == L'\\' && !in_single_quotes) {
            current += ch;
            escaped = true;
            continue;
        }

        if (ch == L'\'' && !in_double_quotes) {
            in_single_quotes = !in_single_quotes;
            current += ch;
            continue;
        }

        if (ch == L'"' && !in_single_quotes) {
            in_double_quotes = !in_double_quotes;
            current += ch;
            continue;
        }

        if (!in_single_quotes && !in_double_quotes) {
            if (ch == L'(') {
                paren_depth++;
            } else if (ch == L')') {
                if (paren_depth > 0) paren_depth--;
            }
        }

        if (ch == L'|' && !in_single_quotes && !in_double_quotes && paren_depth == 0) {
            // Do not split logical OR (||) as a pipeline segment delimiter.
            if ((i + 1) < input.size() && input[i + 1] == L'|') {
                current += L"||";
                ++i;
                continue;
            }

            segments.push_back(trim_copy(current));
            current.clear();
            continue;
        }

        current += ch;
    }

    segments.push_back(trim_copy(current));
    return segments;
}

bool validate_shell_lexical_state(const std::wstring& input, std::wstring& error_message) {
    bool in_single_quotes = false;
    bool in_double_quotes = false;
    bool escaped = false;

    for (wchar_t ch : input) {
        if (escaped) {
            escaped = false;
            continue;
        }
        if (ch == L'\\' && !in_single_quotes) {
            escaped = true;
            continue;
        }
        if (ch == L'\'' && !in_double_quotes) {
            in_single_quotes = !in_single_quotes;
        } else if (ch == L'"' && !in_single_quotes) {
            in_double_quotes = !in_double_quotes;
        }
    }

    if (in_single_quotes || in_double_quotes) {
        error_message = L"ksh: syntax error: unterminated quote";
        return false;
    }
    if (escaped) {
        const std::wstring trimmed = trim_copy(input);
        const size_t command_end = trimmed.find_first_of(L" \t");
        const std::wstring command = trimmed.substr(0, command_end);
        if ((command == L"cd" || command == L"cp" || command == L"mv" || command == L"ls") &&
            trimmed.size() > command.size() && trimmed.back() == L'\\') {
            return true;
        }
        error_message = L"ksh: syntax error: trailing escape";
        return false;
    }
    return true;
}

bool has_unquoted_shell_metacharacters(const std::wstring& input) {
    bool in_single_quotes = false;
    bool in_double_quotes = false;
    bool escaped = false;
    int paren_depth = 0;

    for (wchar_t ch : input) {
        if (escaped) {
            escaped = false;
            continue;
        }

        if (ch == L'\\' && !in_single_quotes) {
            escaped = true;
            continue;
        }

        if (ch == L'\'' && !in_double_quotes) {
            in_single_quotes = !in_single_quotes;
            continue;
        }

        if (ch == L'"' && !in_single_quotes) {
            in_double_quotes = !in_double_quotes;
            continue;
        }

        if (!in_single_quotes && !in_double_quotes) {
            if (ch == L'(') {
                paren_depth++;
            } else if (ch == L')') {
                if (paren_depth > 0) paren_depth--;
            } else if (paren_depth == 0 && (ch == L'>' || ch == L'<' || ch == L'&' || ch == L'|' || ch == L';')) {
                return true;
            }
        }
    }

    return false;
}

bool create_process_with_handle_list(
    const std::wstring& command_line,
    const STARTUPINFOW& startup,
    const std::vector<HANDLE>& handles_to_inherit,
    DWORD creation_flags,
    PROCESS_INFORMATION& process_info) {
    std::vector<wchar_t> mutable_cmd(command_line.begin(), command_line.end());
    mutable_cmd.push_back(L'\0');

    if (handles_to_inherit.empty()) {
        STARTUPINFOW si = startup;
        return CreateProcessW(
            nullptr,
            mutable_cmd.data(),
            nullptr,
            nullptr,
            FALSE,
            creation_flags,
            nullptr,
            nullptr,
            &si,
            &process_info) != FALSE;
    }

    std::vector<HANDLE> filtered_handles;
    filtered_handles.reserve(handles_to_inherit.size());
    for (HANDLE handle : handles_to_inherit) {
        if (handle != nullptr && handle != INVALID_HANDLE_VALUE &&
            std::find(filtered_handles.begin(), filtered_handles.end(), handle) == filtered_handles.end()) {
            filtered_handles.push_back(handle);
        }
    }

    if (filtered_handles.empty()) {
        STARTUPINFOW si = startup;
        return CreateProcessW(
            nullptr,
            mutable_cmd.data(),
            nullptr,
            nullptr,
            FALSE,
            creation_flags,
            nullptr,
            nullptr,
            &si,
            &process_info) != FALSE;
    }

    SIZE_T attribute_list_size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attribute_list_size);
    std::vector<unsigned char> attribute_list_buffer(attribute_list_size);

    STARTUPINFOEXW startup_ex;
    ZeroMemory(&startup_ex, sizeof(startup_ex));
    startup_ex.StartupInfo = startup;
    startup_ex.lpAttributeList = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(attribute_list_buffer.data());

    if (!InitializeProcThreadAttributeList(startup_ex.lpAttributeList, 1, 0, &attribute_list_size)) {
        return false;
    }

    std::vector<HANDLE> launch_handles;
    launch_handles.reserve(filtered_handles.size());
    for (HANDLE handle : filtered_handles) {
        HANDLE duplicate = nullptr;
        if (!DuplicateHandle(
                GetCurrentProcess(),
                handle,
                GetCurrentProcess(),
                &duplicate,
                0,
                TRUE,
                DUPLICATE_SAME_ACCESS)) {
            DeleteProcThreadAttributeList(startup_ex.lpAttributeList);
            for (HANDLE launch_handle : launch_handles) {
                CloseHandle(launch_handle);
            }
            return false;
        }
        launch_handles.push_back(duplicate);
    }

    auto remap_standard_handle = [&](HANDLE handle) -> HANDLE {
        for (size_t index = 0; index < filtered_handles.size(); ++index) {
            if (filtered_handles[index] == handle) {
                return launch_handles[index];
            }
        }
        return handle;
    };
    startup_ex.StartupInfo.hStdInput = remap_standard_handle(startup_ex.StartupInfo.hStdInput);
    startup_ex.StartupInfo.hStdOutput = remap_standard_handle(startup_ex.StartupInfo.hStdOutput);
    startup_ex.StartupInfo.hStdError = remap_standard_handle(startup_ex.StartupInfo.hStdError);
    startup_ex.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
    startup_ex.StartupInfo.cb = sizeof(STARTUPINFOEXW);

    bool updated = UpdateProcThreadAttribute(
        startup_ex.lpAttributeList,
        0,
        PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
        launch_handles.data(),
        launch_handles.size() * sizeof(HANDLE),
        nullptr,
        nullptr) != FALSE;

    if (!updated) {
        DeleteProcThreadAttributeList(startup_ex.lpAttributeList);
        for (HANDLE handle : launch_handles) {
            CloseHandle(handle);
        }
        return false;
    }

    bool created = CreateProcessW(
        nullptr,
        mutable_cmd.data(),
        nullptr,
        nullptr,
        TRUE,
        creation_flags | EXTENDED_STARTUPINFO_PRESENT,
        nullptr,
        nullptr,
        &startup_ex.StartupInfo,
        &process_info) != FALSE;

    DeleteProcThreadAttributeList(startup_ex.lpAttributeList);
    for (HANDLE handle : launch_handles) {
        CloseHandle(handle);
    }
    return created;
}

// Pipeline section: stage launch and process wiring.
struct InProcessPipelineStage {
    std::wstring command;
    HANDLE input = INVALID_HANDLE_VALUE;
    HANDLE output = INVALID_HANDLE_VALUE;
    HANDLE error = INVALID_HANDLE_VALUE;
    KshEnvironment environment;
    std::map<std::wstring, CustomTypeDefinition> custom_types;
    std::vector<ScriptContext> script_context_stack;
    std::map<std::wstring, ShellFunctionDefinition> shell_functions;
    std::vector<FunctionScopeContext> function_scope_stack;
    std::map<std::wstring, std::wstring> aliases;
    std::map<std::wstring, std::wstring> trap_handlers;
};

DWORD WINAPI run_inprocess_pipeline_stage(LPVOID raw_context) {
    std::unique_ptr<InProcessPipelineStage> context(static_cast<InProcessPipelineStage*>(raw_context));
    g_current_env = &context->environment;
    g_current_custom_types = &context->custom_types;
    g_current_script_context_stack = &context->script_context_stack;
    g_current_shell_functions = &context->shell_functions;
    g_current_function_scope_stack = &context->function_scope_stack;
    g_current_aliases = &context->aliases;
    g_trap_handlers = context->trap_handlers;
    g_pipeline_stdin = context->input;
    g_pipeline_stdout = context->output;
    g_pipeline_stderr = context->error;
    g_inprocess_pipeline_stage = true;
    bool should_exit = false;
    const bool ok = execute_command_line(context->command, should_exit);
    DWORD exit_code = 1;
    std::map<std::wstring, std::wstring>::const_iterator status_it = ksh_env.variables.find(L"?");
    if (status_it != ksh_env.variables.end()) {
        int parsed_status = 1;
        if (try_parse_int_strict(status_it->second, parsed_status)) {
            exit_code = static_cast<DWORD>(parsed_status);
        }
    } else if (ok) {
        exit_code = 0;
    }
    if (context->input != INVALID_HANDLE_VALUE) CloseHandle(context->input);
    if (context->output != INVALID_HANDLE_VALUE) CloseHandle(context->output);
    if (context->error != INVALID_HANDLE_VALUE) CloseHandle(context->error);
    g_pipeline_stdin = INVALID_HANDLE_VALUE;
    g_pipeline_stdout = INVALID_HANDLE_VALUE;
    g_pipeline_stderr = INVALID_HANDLE_VALUE;
    g_inprocess_pipeline_stage = false;
    return exit_code;
}

bool launch_pipeline_stage_process(
    const std::wstring& segment,
    HANDLE segment_stdin,
    HANDLE segment_stdout,
    HANDLE segment_stderr,
    PROCESS_INFORMATION& pi,
    std::wstring& temp_file_path_out,
    bool allow_inprocess) {
    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = segment_stdin;
    si.hStdOutput = segment_stdout;
    si.hStdError = segment_stderr;
    temp_file_path_out.clear();

    bool needs_shell = has_unquoted_shell_metacharacters(segment);
    if (!needs_shell) {
        std::wstring_view trimmed = trim_copy(std::wstring_view(segment));
        std::wstring first_token;
        size_t first_space = trimmed.find_first_of(L" \t\r\n");
        if (first_space != std::wstring::npos) {
            first_token = std::wstring(trimmed.substr(0, first_space));
        } else {
            first_token = std::wstring(trimmed);
        }
        const std::vector<std::wstring>& builtins = builtin_commands();
        bool is_builtin = (std::find(builtins.begin(), builtins.end(), first_token) != builtins.end());
        bool is_function = (g_shell_functions.find(first_token) != g_shell_functions.end());
        if (is_builtin || is_function) {
            needs_shell = true;
        }
    }

    if (allow_inprocess && needs_shell) {
        auto duplicate_for_thread = [](HANDLE source, HANDLE& duplicate) {
            duplicate = INVALID_HANDLE_VALUE;
            return DuplicateHandle(GetCurrentProcess(), source, GetCurrentProcess(), &duplicate, 0, FALSE, DUPLICATE_SAME_ACCESS) != FALSE;
        };
        std::unique_ptr<InProcessPipelineStage> context(new InProcessPipelineStage());
        context->command = segment;
        context->environment = ksh_env;
        context->custom_types = g_custom_types;
        context->script_context_stack = g_script_context_stack;
        context->shell_functions = g_shell_functions;
        context->function_scope_stack = g_function_scope_stack;
        context->aliases = g_aliases;
        context->trap_handlers = g_trap_handlers;
        if (!duplicate_for_thread(segment_stdin, context->input) ||
            !duplicate_for_thread(segment_stdout, context->output) ||
            !duplicate_for_thread(segment_stderr, context->error)) {
            if (context->input != INVALID_HANDLE_VALUE) CloseHandle(context->input);
            if (context->output != INVALID_HANDLE_VALUE) CloseHandle(context->output);
            if (context->error != INVALID_HANDLE_VALUE) CloseHandle(context->error);
            SetLastError(ERROR_DUPLICATE_TAG);
            return false;
        }
        InProcessPipelineStage* stage_ptr = context.release();
        HANDLE thread = CreateThread(nullptr, 0, run_inprocess_pipeline_stage, stage_ptr, 0, &pi.dwProcessId);
        if (thread != nullptr) {
            pi.hProcess = thread;
            {
                std::lock_guard<std::mutex> lock(g_pipeline_threads_mutex);
                g_pipeline_thread_handles.insert(thread);
            }
            return true;
        }
        delete stage_ptr;
    }
    if (!needs_shell) {
        std::vector<HANDLE> stage_handles = { segment_stdin, segment_stdout, segment_stderr };
        if (create_process_with_handle_list(segment, si, stage_handles, 0, pi)) {
            return true;
        }
    }

    wchar_t exe_path[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return false;
    }

    std::wstring command_to_run;
    size_t reserve_size = segment.size() + 1024;
    for (const auto& pair : g_shell_functions) {
        reserve_size += pair.first.size() + 32;
        for (const auto& line : pair.second.body_lines) {
            reserve_size += line.size() + 1;
        }
    }
    for (const auto& pair : ksh_env.variables) {
        reserve_size += pair.first.size() + pair.second.size() + 16;
    }
    for (const auto& arr_pair : ksh_env.arrays) {
        reserve_size += arr_pair.first.size() + 32;
        for (const auto& elem : arr_pair.second) {
            reserve_size += arr_pair.first.size() + elem.first.size() + elem.second.size() + 16;
        }
    }
    for (const auto& pair : g_aliases) {
        reserve_size += pair.first.size() + pair.second.size() + 16;
    }
    command_to_run.reserve(reserve_size);

    for (const auto& pair : ksh_env.variables) {
        const std::wstring& name = pair.first;
        if (!is_valid_shell_identifier(name)) {
            continue;
        }
        if (name == L"RANDOM" || name == L"SECONDS" || name == L"LINENO" || name == L"PPID") {
            continue;
        }

        std::wstring options;
        if (get_flag_value(ksh_env.integer_flags, name)) options += L"i";
        if (get_flag_value(ksh_env.uppercase_flags, name)) options += L"u";
        if (get_flag_value(ksh_env.lowercase_flags, name)) options += L"l";
        if (get_flag_value(ksh_env.readonly_flags, name)) options += L"r";
        if (get_flag_value(ksh_env.exported, name)) options += L"x";

        command_to_run += name + L"=" + quote_for_single_quoted_shell_literal(pair.second) + L";\n";
        if (!options.empty()) {
            command_to_run += L"typeset -" + options + L" " + name + L";\n";
        }
    }

    for (const auto& arr_pair : ksh_env.arrays) {
        const std::wstring& name = arr_pair.first;
        if (!is_valid_shell_identifier(name)) {
            continue;
        }

        bool is_assoc = get_flag_value(ksh_env.associative_flags, name);
        if (is_assoc) {
            command_to_run += L"typeset -A " + name + L";\n";
        } else {
            command_to_run += L"typeset -a " + name + L";\n";
        }

        for (const auto& elem : arr_pair.second) {
            command_to_run += name + L"[" + elem.first + L"]=" + quote_for_single_quoted_shell_literal(elem.second) + L";\n";
        }
    }

    for (const auto& pair : g_aliases) {
        command_to_run += L"alias " + pair.first + L"=" + quote_for_single_quoted_shell_literal(pair.second) + L";\n";
    }

    for (const auto& pair : g_shell_functions) {
        command_to_run += L"function " + pair.first + L" { ";
        for (const auto& line : pair.second.body_lines) {
            command_to_run += line + L"\n";
        }
        command_to_run += L" };\n";
    }

    command_to_run += segment;

    std::wstring cmd_line;
    if (command_to_run.size() > 4000) {
        wchar_t temp_path[MAX_PATH];
        wchar_t temp_file[MAX_PATH];
        if (GetTempPathW(MAX_PATH, temp_path) != 0 && GetTempFileNameW(temp_path, L"ksh", 0, temp_file) != 0) {
            HANDLE hFile = CreateFileW(
                temp_file,
                GENERIC_WRITE,
                FILE_SHARE_READ,
                NULL,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                NULL
            );
            if (hFile != INVALID_HANDLE_VALUE) {
                std::string utf8;
                int utf8_len = WideCharToMultiByte(CP_UTF8, 0, command_to_run.c_str(), (int)command_to_run.size(), NULL, 0, NULL, NULL);
                if (utf8_len > 0) {
                    utf8.resize(utf8_len);
                    WideCharToMultiByte(CP_UTF8, 0, command_to_run.c_str(), (int)command_to_run.size(), &utf8[0], utf8_len, NULL, NULL);
                }
                DWORD written = 0;
                if (WriteFile(hFile, utf8.data(), (DWORD)utf8.size(), &written, NULL)) {
                    FlushFileBuffers(hFile);
                    CloseHandle(hFile);
                    temp_file_path_out = temp_file;
                    cmd_line = quote_command_argument(exe_path) + L" " + quote_command_argument(temp_file);
                } else {
                    CloseHandle(hFile);
                    DeleteFileW(temp_file);
                }
            }
        }
    }

    if (temp_file_path_out.empty()) {
        cmd_line = quote_command_argument(exe_path) + L" -c " + quote_command_argument(command_to_run);
    }

    std::vector<HANDLE> stage_handles = { segment_stdin, segment_stdout, segment_stderr };
    return create_process_with_handle_list(cmd_line, si, stage_handles, 0, pi);
}

bool strip_trailing_unquoted_background_marker(const std::wstring& input, std::wstring& stripped_output) {
    bool in_single_quotes = false;
    bool in_double_quotes = false;
    bool escaped = false;
    size_t last_non_space_index = std::wstring::npos;
    bool last_non_space_is_background_amp = false;

    for (size_t i = 0; i < input.size(); ++i) {
        wchar_t ch = input[i];

        if (escaped) {
            escaped = false;
        } else if (ch == L'\\' && !in_single_quotes) {
            escaped = true;
        } else if (ch == L'\'' && !in_double_quotes) {
            in_single_quotes = !in_single_quotes;
        } else if (ch == L'"' && !in_single_quotes) {
            in_double_quotes = !in_double_quotes;
        }

        if (!std::iswspace(ch)) {
            last_non_space_index = i;
            last_non_space_is_background_amp = (!in_single_quotes && !in_double_quotes && !escaped && ch == L'&');
        }
    }

    if (last_non_space_index != std::wstring::npos && last_non_space_is_background_amp) {
        stripped_output = trim_copy(std::wstring_view(input).substr(0, last_non_space_index));
        return true;
    }

    stripped_output = input;
    return false;
}

bool strip_trailing_unquoted_coprocess_marker(const std::wstring& input, std::wstring& stripped_output) {
    const std::wstring trimmed = trim_copy(input);
    if (trimmed.size() < 2 || trimmed[trimmed.size() - 2] != L'|' || trimmed.back() != L'&') {
        stripped_output = input;
        return false;
    }

    bool in_single_quotes = false;
    bool in_double_quotes = false;
    bool escaped = false;
    const size_t marker_start = trimmed.size() - 2;
    for (size_t i = 0; i < marker_start; ++i) {
        const wchar_t ch = trimmed[i];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (ch == L'\\' && !in_single_quotes) {
            escaped = true;
        } else if (ch == L'\'' && !in_double_quotes) {
            in_single_quotes = !in_single_quotes;
        } else if (ch == L'"' && !in_single_quotes) {
            in_double_quotes = !in_double_quotes;
        }
    }

    if (in_single_quotes || in_double_quotes || escaped) {
        stripped_output = input;
        return false;
    }

    stripped_output = trim_copy(trimmed.substr(0, marker_start));
    return true;
}

std::wstring format_win32_error_message(DWORD error_code) {
    if (error_code == 0) {
        return L"success";
    }

    wchar_t* buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER |
                        FORMAT_MESSAGE_FROM_SYSTEM |
                        FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD length = FormatMessageW(
        flags,
        nullptr,
        error_code,
        0,
        reinterpret_cast<LPWSTR>(&buffer),
        0,
        nullptr);

    if (length == 0 || buffer == nullptr) {
        return L"unknown error";
    }

    std::wstring message(buffer, length);
    LocalFree(buffer);
    while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n' || message.back() == L' ')) {
        message.pop_back();
    }
    return message;
}

bool execute_pipeline_segments(
    const std::vector<std::wstring>& segments,
    bool run_in_background,
    DWORD& last_exit_code,
    HANDLE& background_handle,
    DWORD& background_pid,
    std::vector<HANDLE>* background_handles,
    std::vector<DWORD>* background_pids,
    std::vector<std::wstring>* temp_files_out = nullptr) {
    last_exit_code = 1;
    background_handle = nullptr;
    background_pid = 0;
    if (background_handles != nullptr) {
        background_handles->clear();
    }
    if (background_pids != nullptr) {
        background_pids->clear();
    }
    g_last_pipeline_error_detail.clear();
    if (segments.size() < 2) {
        g_last_pipeline_error_detail = L"ksh: pipeline requires at least 2 stages";
        return false;
    }

    std::vector<HANDLE> process_handles;
    std::vector<DWORD> process_pids;
    std::vector<std::wstring> stage_temp_paths;
    HANDLE prev_read = nullptr;
    bool run_last_stage_in_parent = false;

    for (size_t i = 0; i < segments.size(); ++i) {
        const std::wstring segment = trim_copy(segments[i]);
        if (segment.empty()) {
            std::wcerr << L"ksh: invalid null command in pipeline\n";
            g_last_pipeline_error_detail = L"ksh: invalid null command in pipeline";
            last_exit_code = 2;
            if (prev_read != nullptr) {
                CloseHandle(prev_read);
            }
            for (HANDLE h : process_handles) {
                if (h != nullptr) {
                    WaitForSingleObject(h, INFINITE);
                    CloseHandle(h);
                }
            }
            for (const std::wstring& path : stage_temp_paths) {
                if (!path.empty()) {
                    DeleteFileW(path.c_str());
                }
            }
            return false;
        }

        HANDLE segment_stdin = GetStdHandle(STD_INPUT_HANDLE);
        HANDLE segment_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
        HANDLE segment_stderr = GetStdHandle(STD_ERROR_HANDLE);

        HANDLE stdin_dup = nullptr;
        if (prev_read != nullptr) {
            if (!DuplicateHandle(GetCurrentProcess(), prev_read, GetCurrentProcess(), &stdin_dup, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
                DWORD duplicate_error = GetLastError();
                g_last_pipeline_error_detail =
                    L"ksh: pipeline stage " + std::to_wstring(i + 1) + L"/" + std::to_wstring(segments.size()) +
                    L" failed duplicating stdin handle (" + std::to_wstring(duplicate_error) + L": " +
                    format_win32_error_message(duplicate_error) + L")";
                last_exit_code = 1;
                if (prev_read != nullptr) {
                    CloseHandle(prev_read);
                }
                for (HANDLE h : process_handles) {
                    if (h != nullptr) {
                        WaitForSingleObject(h, INFINITE);
                        CloseHandle(h);
                    }
                }
                for (const std::wstring& path : stage_temp_paths) {
                    if (!path.empty()) {
                        DeleteFileW(path.c_str());
                    }
                }
                return false;
            }
            segment_stdin = stdin_dup;
        }

        HANDLE next_read = nullptr;
        HANDLE next_write = nullptr;
        const bool is_last = (i + 1 == segments.size());
        if (!is_last) {
            SECURITY_ATTRIBUTES sa;
            sa.nLength = sizeof(sa);
            sa.lpSecurityDescriptor = nullptr;
            sa.bInheritHandle = TRUE;

            if (!CreatePipe(&next_read, &next_write, &sa, 0)) {
                DWORD pipe_error = GetLastError();
                g_last_pipeline_error_detail =
                    L"ksh: pipeline stage " + std::to_wstring(i + 1) + L"/" + std::to_wstring(segments.size()) +
                    L" failed creating pipe (" + std::to_wstring(pipe_error) + L": " +
                    format_win32_error_message(pipe_error) + L")";
                last_exit_code = 1;
                if (stdin_dup != nullptr) CloseHandle(stdin_dup);
                if (prev_read != nullptr) CloseHandle(prev_read);
                for (HANDLE h : process_handles) {
                    if (h != nullptr) {
                        WaitForSingleObject(h, INFINITE);
                        CloseHandle(h);
                    }
                }
                for (const std::wstring& path : stage_temp_paths) {
                    if (!path.empty()) {
                        DeleteFileW(path.c_str());
                    }
                }
                return false;
            }

            if (!SetHandleInformation(next_read, HANDLE_FLAG_INHERIT, 0)) {
                DWORD inherit_error = GetLastError();
                g_last_pipeline_error_detail =
                    L"ksh: pipeline stage " + std::to_wstring(i + 1) + L"/" + std::to_wstring(segments.size()) +
                    L" failed setting pipe inheritance (" + std::to_wstring(inherit_error) + L": " +
                    format_win32_error_message(inherit_error) + L")";
                last_exit_code = 1;
                if (stdin_dup != nullptr) CloseHandle(stdin_dup);
                if (next_read != nullptr) CloseHandle(next_read);
                if (next_write != nullptr) CloseHandle(next_write);
                if (prev_read != nullptr) CloseHandle(prev_read);
                for (HANDLE h : process_handles) {
                    if (h != nullptr) {
                        WaitForSingleObject(h, INFINITE);
                        CloseHandle(h);
                    }
                }
                for (const std::wstring& path : stage_temp_paths) {
                    if (!path.empty()) {
                        DeleteFileW(path.c_str());
                    }
                }
                return false;
            }
            segment_stdout = next_write;
        }

        if (is_last && !run_in_background) {
            bool needs_shell = has_unquoted_shell_metacharacters(segment);
            if (!needs_shell) {
                std::wstring_view trimmed_stage = trim_copy(std::wstring_view(segment));
                std::wstring first_token;
                size_t first_space = trimmed_stage.find_first_of(L" \t\r\n");
                if (first_space != std::wstring::npos) {
                    first_token = std::wstring(trimmed_stage.substr(0, first_space));
                } else {
                    first_token = std::wstring(trimmed_stage);
                }
                const std::vector<std::wstring>& builtins = builtin_commands();
                bool is_builtin = (std::find(builtins.begin(), builtins.end(), first_token) != builtins.end());
                bool is_function = (g_shell_functions.find(first_token) != g_shell_functions.end());
                if (is_builtin || is_function) {
                    needs_shell = true;
                }
            }
            if (needs_shell) {
                run_last_stage_in_parent = true;
            }
        }

        if (run_last_stage_in_parent) {
            // AT&T KornShell native model: Execute rightmost pipeline stage in parent process
            HANDLE old_pipe_in = g_pipeline_stdin;
            HANDLE old_pipe_out = g_pipeline_stdout;
            HANDLE old_pipe_err = g_pipeline_stderr;

            g_pipeline_stdin = segment_stdin;
            g_pipeline_stdout = segment_stdout;
            g_pipeline_stderr = segment_stderr;

            bool should_exit = false;
            const bool ok = execute_command_line(segment, should_exit);
            DWORD last_stage_code = ok ? 0 : 1;
            std::map<std::wstring, std::wstring>::const_iterator status_it = ksh_env.variables.find(L"?");
            if (status_it != ksh_env.variables.end()) {
                int parsed_status = ok ? 0 : 1;
                if (try_parse_int_strict(status_it->second, parsed_status)) {
                    last_stage_code = static_cast<DWORD>(parsed_status);
                }
            }

            g_pipeline_stdin = old_pipe_in;
            g_pipeline_stdout = old_pipe_out;
            g_pipeline_stderr = old_pipe_err;

            if (stdin_dup != nullptr) {
                CloseHandle(stdin_dup);
                stdin_dup = nullptr;
            }
            if (prev_read != nullptr) {
                CloseHandle(prev_read);
                prev_read = nullptr;
            }

            last_exit_code = last_stage_code;
            break;
        }

        PROCESS_INFORMATION pi;
        std::wstring temp_file_path;
        if (!launch_pipeline_stage_process(segment, segment_stdin, segment_stdout, segment_stderr, pi, temp_file_path, true)) {
            DWORD launch_error = GetLastError();
            g_last_pipeline_error_detail =
                L"ksh: pipeline stage " + std::to_wstring(i + 1) + L"/" + std::to_wstring(segments.size()) +
                L" failed launching command: " + segment +
                L" (" + std::to_wstring(launch_error) + L": " +
                format_win32_error_message(launch_error) + L")";
            last_exit_code = 127;
            if (stdin_dup != nullptr) CloseHandle(stdin_dup);
            if (next_read != nullptr) CloseHandle(next_read);
            if (next_write != nullptr) CloseHandle(next_write);
            if (prev_read != nullptr) CloseHandle(prev_read);
            for (HANDLE h : process_handles) {
                if (h != nullptr) {
                    WaitForSingleObject(h, INFINITE);
                    CloseHandle(h);
                }
            }
            if (!temp_file_path.empty()) {
                DeleteFileW(temp_file_path.c_str());
            }
            for (const std::wstring& path : stage_temp_paths) {
                if (!path.empty()) {
                    DeleteFileW(path.c_str());
                }
            }
            return false;
        }

        if (!temp_file_path.empty()) {
            stage_temp_paths.push_back(temp_file_path);
        }

        if (pi.hThread != nullptr && pi.hThread != INVALID_HANDLE_VALUE) {
            CloseHandle(pi.hThread);
        }
        process_handles.push_back(pi.hProcess);
        process_pids.push_back(pi.dwProcessId);

        if (stdin_dup != nullptr) {
            CloseHandle(stdin_dup);
        }
        if (prev_read != nullptr) {
            CloseHandle(prev_read);
            prev_read = nullptr;
        }
        if (next_write != nullptr) {
            CloseHandle(next_write);
        }
        prev_read = next_read;
    }

    if (prev_read != nullptr) {
        CloseHandle(prev_read);
    }

    if (run_in_background) {
        if (!process_handles.empty()) {
            if (background_handles != nullptr) {
                *background_handles = process_handles;
            }
            if (background_pids != nullptr) {
                *background_pids = process_pids;
            }
            background_handle = process_handles.back();
            background_pid = process_pids.back();
            last_exit_code = 0;
            if (temp_files_out != nullptr) {
                *temp_files_out = std::move(stage_temp_paths);
            } else {
                for (const std::wstring& path : stage_temp_paths) {
                    if (!path.empty()) {
                        DeleteFileW(path.c_str());
                    }
                }
            }
            return true;
        }
        for (const std::wstring& path : stage_temp_paths) {
            if (!path.empty()) {
                DeleteFileW(path.c_str());
            }
        }
        return false;
    }

    DWORD rightmost_failure = (run_last_stage_in_parent && last_exit_code != 0) ? last_exit_code : 0;
    for (size_t i = 0; i < process_handles.size(); ++i) {
        WaitForSingleObject(process_handles[i], INFINITE);
        DWORD code = 1;
        bool is_pipe_thread = false;
        {
            std::lock_guard<std::mutex> lock(g_pipeline_threads_mutex);
            auto it = g_pipeline_thread_handles.find(process_handles[i]);
            if (it != g_pipeline_thread_handles.end()) {
                is_pipe_thread = true;
                g_pipeline_thread_handles.erase(it);
            }
        }
        if (is_pipe_thread) {
            GetExitCodeThread(process_handles[i], &code);
        } else {
            register_child_process_times(process_handles[i]);
            GetExitCodeProcess(process_handles[i], &code);
        }
        if (code != 0) {
            rightmost_failure = code;
        }
        if (!run_last_stage_in_parent && i + 1 == process_handles.size()) {
            last_exit_code = code;
        }
        CloseHandle(process_handles[i]);
    }

    if (g_pipefail_enabled && rightmost_failure != 0) {
        last_exit_code = rightmost_failure;
    }

    for (const std::wstring& path : stage_temp_paths) {
        if (!path.empty()) {
            DeleteFileW(path.c_str());
        }
    }

    return true;
}

bool open_redirection_file(const std::wstring& path, DWORD access, DWORD creation, HANDLE& handle) {
    std::wstring norm_path = path;
    bool is_pipe = (norm_path.rfind(L"\\\\.\\", 0) == 0 || norm_path.rfind(L"//./", 0) == 0 ||
                    norm_path.find(L"\\pipe\\") != std::wstring::npos || norm_path.find(L"/pipe/") != std::wstring::npos);
    if (!is_pipe) {
        for (auto& ch : norm_path) {
            if (ch == L'\\') ch = L'/';
        }
    } else {
        for (auto& ch : norm_path) {
            if (ch == L'/') ch = L'\\';
        }
    }

    if (norm_path == L"/dev/null") {
        norm_path = L"NUL";
    } else {
        HANDLE source = INVALID_HANDLE_VALUE;
        if (norm_path == L"/dev/stdin") {
            source = GetStdHandle(STD_INPUT_HANDLE);
        } else if (norm_path == L"/dev/stdout") {
            source = GetStdHandle(STD_OUTPUT_HANDLE);
        } else if (norm_path == L"/dev/stderr") {
            source = GetStdHandle(STD_ERROR_HANDLE);
        }

        if (source != INVALID_HANDLE_VALUE && source != nullptr) {
            return DuplicateHandle(
                GetCurrentProcess(),
                source,
                GetCurrentProcess(),
                &handle,
                0,
                TRUE,
                DUPLICATE_SAME_ACCESS
            ) != 0;
        }
    }

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = nullptr;
    sa.bInheritHandle = TRUE;

    handle = CreateFileW(
        norm_path.c_str(),
        access,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        &sa,
        creation,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    return handle != INVALID_HANDLE_VALUE;
}

bool set_persistent_custom_fd(int fd, HANDLE new_handle, std::wstring& error_message) {
    if (!is_custom_fd_in_range(fd)) {
        error_message = L"ksh: custom fd out of range (expected 3-9)";
        return false;
    }

    HANDLE& slot = g_custom_fd_table[static_cast<size_t>(fd)];
    if (is_valid_handle_value(slot)) {
        CloseHandle(slot);
    }

    if (is_valid_handle_value(new_handle)) {
        slot = new_handle;
    } else {
        slot = INVALID_HANDLE_VALUE;
    }
    return true;
}

bool duplicate_handle_for_custom_fd_source(int source_fd, HANDLE& duplicated_handle, std::wstring& error_message) {
    duplicated_handle = INVALID_HANDLE_VALUE;

    HANDLE source_handle = INVALID_HANDLE_VALUE;
    if (source_fd == 0) {
        source_handle = GetStdHandle(STD_INPUT_HANDLE);
    } else if (source_fd == 1) {
        source_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    } else if (source_fd == 2) {
        source_handle = GetStdHandle(STD_ERROR_HANDLE);
    } else if (is_custom_fd_in_range(source_fd)) {
        HANDLE persistent = get_persistent_custom_fd(source_fd);
        if (!is_valid_handle_value(persistent)) {
            error_message = L"ksh: source custom fd is not open: " + std::to_wstring(source_fd);
            return false;
        }
        source_handle = persistent;
    } else {
        error_message = L"ksh: unsupported source fd: " + std::to_wstring(source_fd);
        return false;
    }

    if (!is_valid_handle_value(source_handle)) {
        error_message = L"ksh: source fd is not available: " + std::to_wstring(source_fd);
        return false;
    }

    if (!DuplicateHandle(
            GetCurrentProcess(),
            source_handle,
            GetCurrentProcess(),
            &duplicated_handle,
            0,
            TRUE,
            DUPLICATE_SAME_ACCESS)) {
        error_message = L"ksh: failed to duplicate fd: " + std::to_wstring(source_fd);
        return false;
    }

    return true;
}

class ScopedCustomFdTransaction {
public:
    ScopedCustomFdTransaction(
        const std::vector<RedirectionSpec::CustomFdAction>& actions,
        std::wstring& error_message)
        : committed_(false), ready_(true) {
        for (const RedirectionSpec::CustomFdAction& action : actions) {
            if (!is_custom_fd_in_range(action.fd)) {
                error_message = L"ksh: custom fd out of range (expected 3-9)";
                ready_ = false;
                cleanup_saved_handles();
                return;
            }

            const size_t slot = static_cast<size_t>(action.fd);
            if (has_snapshot_[slot]) {
                continue;
            }
            has_snapshot_[slot] = true;

            Snapshot snapshot;
            snapshot.had_entry = false;
            snapshot.saved_handle = INVALID_HANDLE_VALUE;

            HANDLE current = g_custom_fd_table[slot];
            if (is_valid_handle_value(current)) {
                HANDLE duplicated = INVALID_HANDLE_VALUE;
                if (!DuplicateHandle(
                        GetCurrentProcess(),
                        current,
                        GetCurrentProcess(),
                        &duplicated,
                        0,
                        TRUE,
                        DUPLICATE_SAME_ACCESS)) {
                    error_message = L"ksh: failed to snapshot custom fd: " + std::to_wstring(action.fd);
                    ready_ = false;
                    cleanup_saved_handles();
                    return;
                }

                snapshot.had_entry = true;
                snapshot.saved_handle = duplicated;
            }

            snapshots_[slot] = snapshot;
        }
    }

    ~ScopedCustomFdTransaction() {
        if (!ready_) {
            return;
        }

        if (!committed_) {
            rollback();
            return;
        }

        cleanup_saved_handles();
    }

    bool ready() const {
        return ready_;
    }

    void commit() {
        committed_ = true;
    }

private:
    struct Snapshot {
        bool had_entry;
        HANDLE saved_handle;
    };

    void rollback() {
        for (int fd = kMinCustomFd; fd <= kMaxCustomFd; ++fd) {
            const size_t slot = static_cast<size_t>(fd);
            if (!has_snapshot_[slot]) {
                continue;
            }

            Snapshot& snapshot = snapshots_[slot];
            if (is_valid_handle_value(g_custom_fd_table[slot])) {
                CloseHandle(g_custom_fd_table[slot]);
            }

            if (snapshot.had_entry && is_valid_handle_value(snapshot.saved_handle)) {
                g_custom_fd_table[slot] = snapshot.saved_handle;
                snapshot.saved_handle = INVALID_HANDLE_VALUE;
            } else {
                g_custom_fd_table[slot] = INVALID_HANDLE_VALUE;
            }
        }
    }

    void cleanup_saved_handles() {
        for (int fd = kMinCustomFd; fd <= kMaxCustomFd; ++fd) {
            const size_t slot = static_cast<size_t>(fd);
            if (!has_snapshot_[slot]) {
                continue;
            }

            HANDLE& handle = snapshots_[slot].saved_handle;
            if (is_valid_handle_value(handle)) {
                CloseHandle(handle);
                handle = INVALID_HANDLE_VALUE;
            }
        }
    }

    std::array<Snapshot, kCustomFdTableSize> snapshots_ = {};
    std::array<bool, kCustomFdTableSize> has_snapshot_ = {};
    bool committed_;
    bool ready_;
};

bool is_custom_fd_source_available_preflight(int source_fd, const std::array<bool, kCustomFdTableSize>& simulated_custom_open) {
    if (is_custom_fd_in_range(source_fd)) {
        return simulated_custom_open[static_cast<size_t>(source_fd)];
    }

    HANDLE source_handle = INVALID_HANDLE_VALUE;
    if (source_fd == 0) {
        source_handle = GetStdHandle(STD_INPUT_HANDLE);
    } else if (source_fd == 1) {
        source_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    } else if (source_fd == 2) {
        source_handle = GetStdHandle(STD_ERROR_HANDLE);
    } else {
        return false;
    }

    return source_handle != nullptr && source_handle != INVALID_HANDLE_VALUE;
}

bool preflight_validate_persistent_custom_fd_actions(
    const std::vector<RedirectionSpec::CustomFdAction>& actions,
    std::wstring& error_message) {
    std::array<bool, kCustomFdTableSize> simulated_custom_open = {};
    for (int fd = kMinCustomFd; fd <= kMaxCustomFd; ++fd) {
        simulated_custom_open[static_cast<size_t>(fd)] =
            is_valid_handle_value(g_custom_fd_table[static_cast<size_t>(fd)]);
    }

    for (const RedirectionSpec::CustomFdAction& action : actions) {
        if (!is_custom_fd_in_range(action.fd)) {
            error_message = L"ksh: custom fd out of range (expected 3-9)";
            return false;
        }

        if (action.kind == RedirectionSpec::CustomFdAction::Kind::Close) {
            simulated_custom_open[static_cast<size_t>(action.fd)] = false;
            continue;
        }

        if (action.kind == RedirectionSpec::CustomFdAction::Kind::Duplicate) {
            if (!is_custom_fd_source_available_preflight(action.duplicate_source_fd, simulated_custom_open)) {
                if (action.duplicate_source_fd >= 3 && action.duplicate_source_fd <= 9) {
                    error_message = L"ksh: source custom fd is not open: " + std::to_wstring(action.duplicate_source_fd);
                } else {
                    error_message = L"ksh: source fd is not available: " + std::to_wstring(action.duplicate_source_fd);
                }
                return false;
            }
            simulated_custom_open[static_cast<size_t>(action.fd)] = true;
            continue;
        }

        // Open actions conceptually make the destination fd available once applied.
        simulated_custom_open[static_cast<size_t>(action.fd)] = true;
    }

    return true;
}

bool apply_persistent_custom_fd_actions(const RedirectionSpec& redir, std::wstring& error_message) {
    if (!preflight_validate_persistent_custom_fd_actions(redir.custom_fd_actions, error_message)) {
        return false;
    }

    ScopedCustomFdTransaction transaction(redir.custom_fd_actions, error_message);
    if (!transaction.ready()) {
        return false;
    }

    for (const RedirectionSpec::CustomFdAction& action : redir.custom_fd_actions) {
        if (action.fd < 3 || action.fd > 9) {
            error_message = L"ksh: custom fd out of range (expected 3-9)";
            return false;
        }

        if (action.kind == RedirectionSpec::CustomFdAction::Kind::Close) {
            if (!set_persistent_custom_fd(action.fd, INVALID_HANDLE_VALUE, error_message)) {
                return false;
            }
            continue;
        }

        HANDLE new_handle = INVALID_HANDLE_VALUE;
        if (action.kind == RedirectionSpec::CustomFdAction::Kind::OpenRead) {
            if (!open_redirection_file(action.path, GENERIC_READ, OPEN_EXISTING, new_handle)) {
                error_message = L"ksh: failed opening custom fd input file: " + action.path;
                return false;
            }
        } else if (action.kind == RedirectionSpec::CustomFdAction::Kind::OpenWriteTruncate) {
            if (!open_redirection_file(action.path, GENERIC_WRITE, CREATE_ALWAYS, new_handle)) {
                error_message = L"ksh: failed opening custom fd output file: " + action.path;
                return false;
            }
        } else if (action.kind == RedirectionSpec::CustomFdAction::Kind::OpenWriteAppend) {
            if (!open_redirection_file(action.path, GENERIC_WRITE, OPEN_ALWAYS, new_handle)) {
                error_message = L"ksh: failed opening custom fd append file: " + action.path;
                return false;
            }
            SetFilePointer(new_handle, 0, nullptr, FILE_END);
        } else if (action.kind == RedirectionSpec::CustomFdAction::Kind::Duplicate) {
            if (!duplicate_handle_for_custom_fd_source(action.duplicate_source_fd, new_handle, error_message)) {
                return false;
            }
        }

        if (!set_persistent_custom_fd(action.fd, new_handle, error_message)) {
            if (new_handle != nullptr && new_handle != INVALID_HANDLE_VALUE) {
                CloseHandle(new_handle);
            }
            return false;
        }
    }

    transaction.commit();
    return true;
}

// Redirection section: std-handle setup and cleanup for command execution.
bool setup_redirection_handles(
    const RedirectionSpec* redir,
    HANDLE& std_in,
    HANDLE& std_out,
    HANDLE& std_err,
    bool& close_in,
    bool& close_out,
    bool& close_err) {
    std_in = (g_pipeline_stdin != INVALID_HANDLE_VALUE) ? g_pipeline_stdin : GetStdHandle(STD_INPUT_HANDLE);
    std_out = (g_pipeline_stdout != INVALID_HANDLE_VALUE)
        ? g_pipeline_stdout
        : ((g_subshell_stdout != INVALID_HANDLE_VALUE) ? g_subshell_stdout : GetStdHandle(STD_OUTPUT_HANDLE));
    std_err = (g_pipeline_stderr != INVALID_HANDLE_VALUE) ? g_pipeline_stderr : GetStdHandle(STD_ERROR_HANDLE);
    close_in = false;
    close_out = false;
    close_err = false;

    if (redir == nullptr || !has_any_redirection(*redir)) {
        return true;
    }

    // Defense-in-depth: reject conflicting redirection state combinations.
    if ((redir->stderr_to_stdout && redir->has_stderr) ||
        (redir->stdout_to_stderr && redir->has_stdout) ||
        (redir->stderr_to_stdout && redir->stdout_to_stderr)) {
        return false;
    }

    if (redir->has_stdin) {
        if (!open_redirection_file(redir->stdin_path, GENERIC_READ, OPEN_EXISTING, std_in)) {
            return false;
        }
        close_in = true;
    }

    if (redir->has_stdout) {
        const DWORD creation = redir->append_stdout ? OPEN_ALWAYS : CREATE_ALWAYS;
        if (!open_redirection_file(redir->stdout_path, GENERIC_WRITE, creation, std_out)) {
            if (close_in) CloseHandle(std_in);
            return false;
        }
        if (redir->append_stdout) {
            SetFilePointer(std_out, 0, nullptr, FILE_END);
        }
        close_out = true;
    }

    if (redir->has_stderr) {
        const DWORD creation = redir->append_stderr ? OPEN_ALWAYS : CREATE_ALWAYS;
        if (!open_redirection_file(redir->stderr_path, GENERIC_WRITE, creation, std_err)) {
            if (close_in) CloseHandle(std_in);
            if (close_out) CloseHandle(std_out);
            return false;
        }
        if (redir->append_stderr) {
            SetFilePointer(std_err, 0, nullptr, FILE_END);
        }
        close_err = true;
    }

    if (redir->stderr_to_stdout) {
        std_err = std_out;
        close_err = false;
    }

    if (redir->stdout_to_stderr) {
        std_out = std_err;
        close_out = false;
    }

    return true;
}

void close_redirection_handles(HANDLE std_in, HANDLE std_out, HANDLE std_err, bool close_in, bool close_out, bool close_err) {
    if (close_in && std_in != nullptr && std_in != INVALID_HANDLE_VALUE) {
        CloseHandle(std_in);
    }

    if (std_out == std_err && std_out != nullptr && std_out != INVALID_HANDLE_VALUE) {
        if (close_out || close_err) {
            CloseHandle(std_out);
        }
        return;
    }

    if (close_out && std_out != nullptr && std_out != INVALID_HANDLE_VALUE) {
        CloseHandle(std_out);
    }
    if (close_err && std_err != nullptr && std_err != INVALID_HANDLE_VALUE) {
        CloseHandle(std_err);
    }
}

std::wstring translate_device_path(const std::wstring& path) {
    std::wstring norm_path = path;
    for (auto& ch : norm_path) {
        if (ch == L'\\') ch = L'/';
    }
    if (norm_path == L"/dev/null") {
        return L"NUL";
    }
    if (norm_path == L"/dev/stdout" || norm_path == L"/dev/stderr") {
        return L"CONOUT$";
    }
    if (norm_path == L"/dev/stdin") {
        return L"CONIN$";
    }
    return path;
}

bool write_text_with_stdout_redirection(const std::wstring& text, const RedirectionSpec& redir) {
    if (redir.stdout_to_stderr) {
        if (g_pipeline_stderr != INVALID_HANDLE_VALUE) {
            int byte_count = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
            if (byte_count > 0) {
                std::string bytes(static_cast<size_t>(byte_count), '\0');
                WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), &bytes[0], byte_count, nullptr, nullptr);
                DWORD written = 0;
                return WriteFile(g_pipeline_stderr, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) != FALSE;
            }
        }
        std::wcerr << text;
        return true;
    }

    if (!redir.has_stdout) {
        if (g_pipeline_stdout != INVALID_HANDLE_VALUE) {
            int byte_count = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
            if (byte_count > 0) {
                std::string bytes(static_cast<size_t>(byte_count), '\0');
                WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), &bytes[0], byte_count, nullptr, nullptr);
                DWORD written = 0;
                return WriteFile(g_pipeline_stdout, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) != FALSE;
            }
        }
        std::wcout << text;
        return true;
    }

    std::wstring target_path = translate_device_path(redir.stdout_path);
    std::wofstream out_file;
    if (redir.append_stdout) {
        out_file.open(target_path.c_str(), std::ios::app);
    } else {
        out_file.open(target_path.c_str(), std::ios::trunc);
    }

    if (!out_file.is_open()) {
        std::wcerr << L"ksh: failed to open output file: " << redir.stdout_path << L"\n";
        return false;
    }

    out_file << text;
    return true;
}

bool get_system_directory_path(std::wstring& system_directory) {
    wchar_t buffer[MAX_PATH];
    UINT length = GetSystemDirectoryW(buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return false;
    }

    system_directory = buffer;
    return true;
}

bool resolve_cmd_exe_path(std::wstring& cmd_path) {
    std::wstring system_directory;
    if (!get_system_directory_path(system_directory)) {
        return false;
    }

    cmd_path = system_directory + L"\\cmd.exe";
    return file_exists_regular(cmd_path);
}

bool resolve_powershell_exe_path(std::wstring& powershell_path) {
    std::wstring system_directory;
    if (!get_system_directory_path(system_directory)) {
        return false;
    }

    powershell_path = system_directory + L"\\WindowsPowerShell\\v1.0\\powershell.exe";
    return file_exists_regular(powershell_path);
}

bool is_absolute_windows_path(const std::wstring& path) {
    if (path.size() >= 3 && std::iswalpha(path[0]) && path[1] == L':' && (path[2] == L'\\' || path[2] == L'/')) {
        return true;
    }

    return path.rfind(L"\\\\", 0) == 0;
}

std::wstring normalize_cd_path(const std::wstring& input_path) {
    if (input_path.empty()) {
        return input_path;
    }

    std::wstring path = input_path;
    std::replace(path.begin(), path.end(), L'/', L'\\');

    if (is_absolute_windows_path(path)) {
        return path;
    }

    if (!path.empty() && path[0] == L'\\') {
        wchar_t cwd[MAX_PATH];
        if (GetCurrentDirectoryW(MAX_PATH, cwd) > 0) {
            std::wstring drive_root = cwd;
            if (drive_root.size() >= 2 && drive_root[1] == L':') {
                return drive_root.substr(0, 2) + path;
            }
        }
    }

    return path;
}

bool resolve_bash_exe_path(std::wstring& bash_path) {
    const wchar_t* candidates[] = {
        L"C:\\Program Files\\Git\\bin\\bash.exe",
        L"C:\\Program Files\\Git\\usr\\bin\\bash.exe",
        L"C:\\Program Files (x86)\\Git\\bin\\bash.exe",
        L"C:\\Program Files (x86)\\Git\\usr\\bin\\bash.exe"
    };

    for (const wchar_t* candidate : candidates) {
        if (file_exists_regular(candidate)) {
            bash_path = candidate;
            return true;
        }
    }

    return false;
}

bool is_powershell_bypass_enabled() {
    if (g_powershell_bypass_enabled) {
        return true;
    }
    std::wstring val = trim_copy(get_environment_value(L"KSH_POWERSHELL_BYPASS"));
    if (val.empty()) {
        return false;
    }
    for (wchar_t ch : val) {
        if (ch == L'1' || ch == L'y' || ch == L'Y' || ch == L't' || ch == L'T') {
            return true;
        }
    }
    return false;
}

enum class ScriptInterpreterResolution {
    NotScript,
    Resolved,
    Failed
};

ScriptInterpreterResolution build_script_interpreter_command(const std::vector<std::wstring>& tokens, std::wstring& command_line) {
    if (tokens.empty()) {
        return ScriptInterpreterResolution::NotScript;
    }

    const std::wstring& script_path = tokens[0];
    if (script_path.empty() || script_path.find(L'=') != std::wstring::npos) {
        return ScriptInterpreterResolution::NotScript;
    }

    auto append_script_arguments = [&]() {
        for (size_t i = 1; i < tokens.size(); ++i) {
            command_line += L" ";
            command_line += quote_command_argument(tokens[i]);
        }
    };

    if (ends_with_case_insensitive(script_path, L".ps1")) {
        std::wstring powershell_path;
        if (!resolve_powershell_exe_path(powershell_path)) {
            return ScriptInterpreterResolution::Failed;
        }
        command_line = quote_command_argument(powershell_path);
        command_line += L" -NoProfile ";
        if (is_powershell_bypass_enabled()) {
            command_line += L"-ExecutionPolicy Bypass ";
        }
        command_line += L"-File ";
        command_line += quote_command_argument(script_path);
        append_script_arguments();
        return ScriptInterpreterResolution::Resolved;
    } else if (ends_with_case_insensitive(script_path, L".cmd") || ends_with_case_insensitive(script_path, L".bat")) {
        std::wstring cmd_path;
        if (!resolve_cmd_exe_path(cmd_path)) {
            return ScriptInterpreterResolution::Failed;
        }
        command_line = quote_command_argument(cmd_path);
        command_line += L" /d /c ";
        command_line += quote_command_argument(script_path);
        append_script_arguments();
        return ScriptInterpreterResolution::Resolved;
    } else if (ends_with_case_insensitive(script_path, L".sh")) {
        std::wstring bash_path;
        if (!resolve_bash_exe_path(bash_path)) {
            return ScriptInterpreterResolution::Failed;
        }
        command_line = quote_command_argument(bash_path);
        command_line += L" ";
        command_line += quote_command_argument(script_path);
        append_script_arguments();
        return ScriptInterpreterResolution::Resolved;
    } else {
        return ScriptInterpreterResolution::NotScript;
    }
}

bool is_ksh_script_path(const std::wstring& path) {
    return ends_with_case_insensitive(path, L".ksh");
}

bool build_self_script_command(const std::wstring& script_path, const std::vector<std::wstring>& script_args, std::wstring& command_line) {
    wchar_t exe_path[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return false;
    }

    command_line = quote_command_argument(exe_path);
    command_line += L" ";
    command_line += quote_command_argument(script_path);
    for (const std::wstring& arg : script_args) {
        command_line += L" ";
        command_line += quote_command_argument(arg);
    }
    return true;
}

std::wstring decode_multibyte(const std::string& input, UINT code_page) {
    if (input.empty()) {
        return L"";
    }

    if (input.size() > static_cast<size_t>(INT_MAX)) {
        return L"";
    }

    const int input_len = static_cast<int>(input.size());
    DWORD flags = (code_page == CP_UTF8) ? MB_ERR_INVALID_CHARS : 0;

    int wide_len = MultiByteToWideChar(code_page, flags, input.data(), input_len, nullptr, 0);
    if (wide_len <= 0 && flags != 0) {
        wide_len = MultiByteToWideChar(code_page, 0, input.data(), input_len, nullptr, 0);
    }
    if (wide_len <= 0) {
        return L"";
    }

    std::wstring output(static_cast<size_t>(wide_len), L'\0');
    if (!MultiByteToWideChar(code_page, 0, input.data(), input_len, &output[0], wide_len)) {
        return L"";
    }
    return output;
}

std::wstring decode_script_text(const std::vector<char>& bytes) {
    if (bytes.empty()) {
        return L"";
    }

    if (bytes.size() >= 3 &&
        static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB &&
        static_cast<unsigned char>(bytes[2]) == 0xBF) {
        return decode_multibyte(std::string(bytes.begin() + 3, bytes.end()), CP_UTF8);
    }

    if (bytes.size() >= 2 &&
        static_cast<unsigned char>(bytes[0]) == 0xFF &&
        static_cast<unsigned char>(bytes[1]) == 0xFE) {
        const size_t byte_count = bytes.size() - 2;
        const size_t wchar_count = byte_count / sizeof(wchar_t);
        std::wstring text(wchar_count, L'\0');
        if (wchar_count > 0) {
            std::memcpy(&text[0], bytes.data() + 2, wchar_count * sizeof(wchar_t));
        }
        return text;
    }

    if (bytes.size() >= 2 &&
        static_cast<unsigned char>(bytes[0]) == 0xFE &&
        static_cast<unsigned char>(bytes[1]) == 0xFF) {
        std::wstring text;
        for (size_t i = 2; i + 1 < bytes.size(); i += 2) {
            const unsigned char hi = static_cast<unsigned char>(bytes[i]);
            const unsigned char lo = static_cast<unsigned char>(bytes[i + 1]);
            wchar_t ch = static_cast<wchar_t>((hi << 8) | lo);
            text.push_back(ch);
        }
        return text;
    }

    std::wstring utf8 = decode_multibyte(std::string(bytes.begin(), bytes.end()), CP_UTF8);
    if (!utf8.empty()) {
        return utf8;
    }
    return decode_multibyte(std::string(bytes.begin(), bytes.end()), CP_ACP);
}

std::vector<std::wstring> split_script_lines(const std::wstring& text) {
    std::vector<std::wstring> lines;
    std::wstring current;
    for (wchar_t ch : text) {
        if (ch == L'\r') {
            continue;
        }
        if (ch == L'\n') {
            lines.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty()) {
        lines.push_back(current);
    }
    return lines;
}

std::vector<std::wstring> split_top_level_script_commands(const std::wstring& line) {
    std::vector<std::wstring> commands;
    std::wstring current;
    bool in_single_quotes = false;
    bool in_double_quotes = false;
    bool escaped = false;
    int paren_depth = 0;

    for (size_t i = 0; i < line.size(); ++i) {
        const wchar_t ch = line[i];
        if (escaped) {
            current += ch;
            escaped = false;
            continue;
        }
        if (ch == L'\\' && !in_single_quotes) {
            current += ch;
            escaped = true;
            continue;
        }
        if (ch == L'\'' && !in_double_quotes) {
            in_single_quotes = !in_single_quotes;
            current += ch;
            continue;
        }
        if (ch == L'"' && !in_single_quotes) {
            in_double_quotes = !in_double_quotes;
            current += ch;
            continue;
        }
        if (!in_single_quotes && !in_double_quotes) {
            if (ch == L'(') {
                ++paren_depth;
            } else if (ch == L')' && paren_depth > 0) {
                --paren_depth;
            }
            if (ch == L';' && paren_depth == 0) {
                if (i + 2 < line.size() && line[i + 1] == L';' && line[i + 2] == L'&') {
                    current += L";;&";
                    i += 2;
                    continue;
                }
                if (i + 1 < line.size() && (line[i + 1] == L';' || line[i + 1] == L'&')) {
                    current += L';';
                    current += line[i + 1];
                    ++i;
                    continue;
                }
                const std::wstring command = trim_copy(current);
                if (!command.empty()) {
                    commands.push_back(command);
                }
                current.clear();
                continue;
            }
        }
        current += ch;
    }

    const std::wstring command = trim_copy(current);
    if (!command.empty()) {
        commands.push_back(command);
    }
    return commands;
}

std::vector<std::wstring> prepare_script_lines(const std::wstring& text) {
    const std::vector<std::wstring> physical_lines = split_script_lines(text);
    std::vector<std::wstring> logical_lines;
    std::wstring continued;

    for (const std::wstring& physical_line : physical_lines) {
        std::wstring line = physical_line;
        size_t trailing = line.size();
        while (trailing > 0 && (line[trailing - 1] == L' ' || line[trailing - 1] == L'\t')) {
            --trailing;
        }
        const bool escaped_newline = trailing > 0 && line[trailing - 1] == L'\\';
        const std::wstring line_trimmed = trim_copy(line);
        const size_t line_command_end = line_trimmed.find_first_of(L" \t");
        const bool is_cd_path = line_trimmed.substr(0, line_command_end) == L"cd" &&
            line_trimmed.size() > line_command_end;
        if (escaped_newline && !is_cd_path) {
            line.erase(trailing - 1);
            continued += line;
            continue;
        }

        if (!continued.empty()) {
            line = continued + line;
            continued.clear();
        }

        const std::wstring trimmed = trim_copy(line);
        const bool trailing_pipe = !trimmed.empty() && trimmed.back() == L'|';
        if (trailing_pipe) {
            continued = line + L' ';
            continue;
        }

        const std::wstring lowered_line = to_lower_copy(trimmed);
        if (starts_with_for_control_keyword(trimmed)) {
            const size_t do_pos = lowered_line.find(L"; do");
            const size_t done_pos = lowered_line.rfind(L"; done");
            if (do_pos != std::wstring::npos && done_pos != std::wstring::npos && done_pos > do_pos) {
                logical_lines.push_back(trim_copy(trimmed.substr(0, do_pos)));
                logical_lines.push_back(L"do");
                const std::wstring body = trim_copy(trimmed.substr(do_pos + 5, done_pos - (do_pos + 5)));
                const std::vector<std::wstring> body_commands = split_top_level_script_commands(body);
                logical_lines.insert(logical_lines.end(), body_commands.begin(), body_commands.end());
                logical_lines.push_back(L"done" + trimmed.substr(done_pos + 6));
                continue;
            }
        }

        const std::vector<std::wstring> commands = split_top_level_script_commands(line);
        for (const std::wstring& command : commands) {
            std::wstring compact = trim_copy(command);
            std::wstring lowered = to_lower_copy(compact);
            if (starts_with_for_control_keyword(compact)) {
                const size_t do_pos = lowered.find(L"; do");
                const size_t done_pos = lowered.rfind(L"; done");
                if (do_pos != std::wstring::npos && done_pos != std::wstring::npos && done_pos > do_pos) {
                    logical_lines.push_back(trim_copy(compact.substr(0, do_pos)));
                    logical_lines.push_back(L"do");
                    const std::wstring body = trim_copy(compact.substr(do_pos + 5, done_pos - (do_pos + 5)));
                    const std::vector<std::wstring> body_commands = split_top_level_script_commands(body);
                    logical_lines.insert(logical_lines.end(), body_commands.begin(), body_commands.end());
                    logical_lines.push_back(L"done" + compact.substr(done_pos + 6));
                    continue;
                }
            }
            logical_lines.push_back(compact);
        }
    }

    if (!continued.empty()) {
        const std::vector<std::wstring> commands = split_top_level_script_commands(continued);
        logical_lines.insert(logical_lines.end(), commands.begin(), commands.end());
    }
    return logical_lines;
}

// Evaluate ksh93 arithmetic expansion: $(( expr ))
double evaluate_math_function(const std::wstring& function_name, const std::vector<double>& args) {
    std::wstring name = to_lower_copy(function_name);

    if (name == L"abs" && args.size() == 1) return std::fabs(args[0]);
    if (name == L"sqrt" && args.size() == 1) return std::sqrt(args[0]);
    if (name == L"pow" && args.size() == 2) return std::pow(args[0], args[1]);
    if (name == L"min" && args.size() == 2) return (args[0] < args[1]) ? args[0] : args[1];
    if (name == L"max" && args.size() == 2) return (args[0] > args[1]) ? args[0] : args[1];
    if (name == L"sin" && args.size() == 1) return std::sin(args[0]);
    if (name == L"cos" && args.size() == 1) return std::cos(args[0]);
    if (name == L"tan" && args.size() == 1) return std::tan(args[0]);
    if (name == L"asin" && args.size() == 1) return std::asin(args[0]);
    if (name == L"acos" && args.size() == 1) return std::acos(args[0]);
    if (name == L"atan" && args.size() == 1) return std::atan(args[0]);
    if (name == L"log" && args.size() == 1) return std::log(args[0]);
    if (name == L"log10" && args.size() == 1) return std::log10(args[0]);
    if (name == L"exp" && args.size() == 1) return std::exp(args[0]);
    if (name == L"floor" && args.size() == 1) return std::floor(args[0]);
    if (name == L"ceil" && args.size() == 1) return std::ceil(args[0]);
    if (name == L"round" && args.size() == 1) return std::round(args[0]);

    throw std::runtime_error("unsupported function");
}

std::wstring get_variable_value_simple(const std::wstring& name) {
    std::wstring resolved = resolve_variable_name(name);
    std::wstring base_name;
    std::wstring index_expr;
    bool has_index = false;
    if (parse_array_reference_expression(resolved, base_name, index_expr, has_index)) {
        auto arr_it = ksh_env.arrays.find(base_name);
        if (arr_it != ksh_env.arrays.end()) {
            if (!has_index) {
                auto val_it = arr_it->second.find(L"0");
                return (val_it != arr_it->second.end()) ? val_it->second : L"0";
            }
            bool is_assoc = get_flag_value(ksh_env.associative_flags, base_name);
            std::wstring key = index_expr;
            if (!is_assoc) {
                size_t idx = 0;
                if (parse_non_negative_index(index_expr, idx)) {
                    key = std::to_wstring(idx);
                }
            }
            auto val_it = arr_it->second.find(key);
            return (val_it != arr_it->second.end()) ? val_it->second : L"0";
        }
    }
    auto it = ksh_env.variables.find(resolved);
    if (it != ksh_env.variables.end()) {
        return it->second;
    }
    return L"0";
}

class ArithmeticParser {
public:
    explicit ArithmeticParser(const std::wstring& expression) : expr(expression), pos(0), depth(0), has_float_operands(false) {}

    bool is_float() const { return has_float_operands; }

    static long long to_safe_int64(double v) {
        if (!std::isfinite(v)) return 0;
        if (v >= static_cast<double>(LLONG_MAX)) return LLONG_MAX;
        if (v <= static_cast<double>(LLONG_MIN)) return LLONG_MIN;
        return static_cast<long long>(v);
    }

    double parse() {
        double value = parse_expression();
        skip_spaces();
        if (pos != expr.size()) {
            throw std::runtime_error("unexpected trailing characters");
        }
        return value;
    }

private:
    const std::wstring& expr;
    size_t pos;
    int depth;
    bool has_float_operands;

    void check_depth() {
        if (depth >= kMaxArithmeticParseDepth) {
            throw std::runtime_error("arithmetic expression nesting too deep");
        }
    }

    void skip_spaces() {
        while (pos < expr.size() && std::iswspace(expr[pos])) {
            pos++;
        }
    }

    bool consume(wchar_t token) {
        skip_spaces();
        if (pos < expr.size() && expr[pos] == token) {
            pos++;
            return true;
        }
        return false;
    }

    bool consume(const wchar_t* token) {
        skip_spaces();
        const size_t length = wcslen(token);
        if (expr.compare(pos, length, token) == 0) {
            pos += length;
            return true;
        }
        return false;
    }

    double parse_expression() {
        check_depth();
        depth++;

        // Try parsing assignment target: identifier followed by optional [...]
        size_t saved_pos = pos;
        skip_spaces();
        if (pos < expr.size() && (std::iswalpha(expr[pos]) || expr[pos] == L'_')) {
            std::wstring var = parse_identifier();
            skip_spaces();
            bool parsed_subscript = true;
            std::wstring subscript_expr;
            if (pos < expr.size() && expr[pos] == L'[') {
                pos++; // consume '['
                int bracket_depth = 1;
                while (pos < expr.size() && bracket_depth > 0) {
                    if (expr[pos] == L'[') {
                        bracket_depth++;
                    } else if (expr[pos] == L']') {
                        bracket_depth--;
                        if (bracket_depth == 0) {
                            pos++; // consume ']'
                            break;
                        }
                    }
                    subscript_expr.push_back(expr[pos]);
                    pos++;
                }
                if (bracket_depth > 0) {
                    parsed_subscript = false;
                }
            }

            if (parsed_subscript) {
                skip_spaces();
                bool is_plus_eq = (pos + 1 < expr.size() && expr[pos] == L'+' && expr[pos + 1] == L'=');
                bool is_minus_eq = (pos + 1 < expr.size() && expr[pos] == L'-' && expr[pos + 1] == L'=');
                bool is_eq = (pos < expr.size() && expr[pos] == L'=' && (pos + 1 >= expr.size() || expr[pos + 1] != L'='));

                if (is_eq || is_plus_eq || is_minus_eq) {
                    if (is_eq) pos++;
                    else pos += 2;

                    double rhs = parse_expression();

                    std::wstring lhs_target = var;
                    if (!subscript_expr.empty()) {
                        ArithmeticParser sub_parser(subscript_expr);
                        double idx_val = sub_parser.parse();
                        std::wstring idx_str;
                        if (idx_val == std::floor(idx_val)) {
                            idx_str = std::to_wstring(static_cast<long long>(idx_val));
                        } else {
                            idx_str = std::to_wstring(idx_val);
                            size_t dot_or_comma = idx_str.find_first_of(L".,");
                            if (dot_or_comma != std::wstring::npos) {
                                while (!idx_str.empty() && idx_str.back() == L'0') {
                                    idx_str.pop_back();
                                }
                                if (!idx_str.empty() && (idx_str.back() == L'.' || idx_str.back() == L',')) {
                                    idx_str.pop_back();
                                }
                            }
                        }
                        lhs_target += L"[" + idx_str + L"]";
                    }

                    double lhs_val = 0.0;
                    if (is_plus_eq || is_minus_eq) {
                        std::wstring current_str = get_variable_value_simple(lhs_target);
                        try {
                            lhs_val = std::stod(current_str);
                        } catch (...) {
                            lhs_val = 0.0;
                        }
                    }

                    double final_val = rhs;
                    if (is_plus_eq) final_val = lhs_val + rhs;
                    else if (is_minus_eq) final_val = lhs_val - rhs;

                    std::wstring val_str;
                    if (final_val == std::floor(final_val)) {
                        val_str = std::to_wstring(static_cast<long long>(final_val));
                    } else {
                        val_str = std::to_wstring(final_val);
                        size_t dot_or_comma = val_str.find_first_of(L".,");
                        if (dot_or_comma != std::wstring::npos) {
                            while (!val_str.empty() && val_str.back() == L'0') {
                                val_str.pop_back();
                            }
                            if (!val_str.empty() && (val_str.back() == L'.' || val_str.back() == L',')) {
                                val_str.pop_back();
                            }
                        }
                    }

                    std::wstring err;
                    assign_parameter_value(lhs_target, val_str, false, false, err, false);
                    depth--;
                    return final_val;
                }
            }
        }

        // Restore pos if not an assignment expression
        pos = saved_pos;

        double value = parse_ternary();
        depth--;
        return value;
    }

    double parse_ternary() {
        check_depth();
        depth++;
        double condition = parse_logical_or();
        if (!consume(L'?')) {
            depth--;
            return condition;
        }
        const double when_true = parse_expression();
        if (!consume(L':')) {
            depth--;
            throw std::runtime_error("missing ':' in ternary expression");
        }
        const double when_false = parse_ternary();
        depth--;
        return condition != 0.0 ? when_true : when_false;
    }

    double parse_logical_or() {
        double value = parse_logical_and();
        while (consume(L"||")) {
            const double right = parse_logical_and();
            value = (value != 0.0 || right != 0.0) ? 1.0 : 0.0;
        }
        return value;
    }

    double parse_logical_and() {
        double value = parse_bitwise_or();
        while (consume(L"&&")) {
            const double right = parse_bitwise_or();
            value = (value != 0.0 && right != 0.0) ? 1.0 : 0.0;
        }
        return value;
    }

    double parse_bitwise_or() {
        double value = parse_bitwise_xor();
        while (true) {
            skip_spaces();
            if (expr.compare(pos, 2, L"||") == 0 || !consume(L'|')) break;
            value = static_cast<double>(to_safe_int64(value) | to_safe_int64(parse_bitwise_xor()));
        }
        return value;
    }

    double parse_bitwise_xor() {
        double value = parse_bitwise_and();
        while (consume(L'^')) value = static_cast<double>(to_safe_int64(value) ^ to_safe_int64(parse_bitwise_and()));
        return value;
    }

    double parse_bitwise_and() {
        double value = parse_equality();
        while (true) {
            skip_spaces();
            if (expr.compare(pos, 2, L"&&") == 0 || !consume(L'&')) break;
            value = static_cast<double>(to_safe_int64(value) & to_safe_int64(parse_equality()));
        }
        return value;
    }

    double parse_equality() {
        double value = parse_relational();
        while (true) {
            if (consume(L"==")) value = value == parse_relational() ? 1.0 : 0.0;
            else if (consume(L"!=")) value = value != parse_relational() ? 1.0 : 0.0;
            else break;
        }
        return value;
    }

    double parse_relational() {
        double value = parse_shift();
        while (true) {
            if (consume(L"<=")) value = value <= parse_shift() ? 1.0 : 0.0;
            else if (consume(L">=")) value = value >= parse_shift() ? 1.0 : 0.0;
            else if (consume(L'<')) value = value < parse_shift() ? 1.0 : 0.0;
            else if (consume(L'>')) value = value > parse_shift() ? 1.0 : 0.0;
            else break;
        }
        return value;
    }

    double parse_shift() {
        double value = parse_additive();
        while (true) {
            if (consume(L"<<")) {
                double rhs = parse_additive();
                long long lhs_i = to_safe_int64(value);
                long long shift_cnt = to_safe_int64(rhs);
                if (shift_cnt < 0 || shift_cnt >= 64) {
                    value = 0.0;
                } else {
                    value = static_cast<double>(static_cast<unsigned long long>(lhs_i) << shift_cnt);
                }
            } else if (consume(L">>")) {
                double rhs = parse_additive();
                long long lhs_i = to_safe_int64(value);
                long long shift_cnt = to_safe_int64(rhs);
                if (shift_cnt < 0) {
                    value = 0.0;
                } else if (shift_cnt >= 64) {
                    value = (lhs_i < 0) ? -1.0 : 0.0;
                } else {
                    value = static_cast<double>(lhs_i >> shift_cnt);
                }
            } else {
                break;
            }
        }
        return value;
    }

    double parse_additive() {
        double value = parse_term();
        while (true) {
            if (consume(L'+')) value += parse_term();
            else if (consume(L'-')) value -= parse_term();
            else break;
        }
        return value;
    }

    double parse_term() {
        double value = parse_power();
        while (true) {
            if (consume(L'*')) {
                value *= parse_power();
            } else if (consume(L'/')) {
                double right = parse_power();
                if (right == 0.0) {
                    throw std::runtime_error("division by zero");
                }
                if (!has_float_operands && std::floor(value) == value && std::floor(right) == right) {
                    long long i_val = to_safe_int64(value);
                    long long i_right = to_safe_int64(right);
                    value = static_cast<double>(i_val / i_right);
                } else {
                    has_float_operands = true;
                    value /= right;
                }
            } else if (consume(L'%')) {
                double right = parse_power();
                if (right == 0.0) {
                    throw std::runtime_error("modulo by zero");
                }
                value = std::fmod(value, right);
            } else {
                break;
            }
        }
        return value;
    }

    double parse_power() {
        double left = parse_unary();
        if (consume(L"**")) {
            check_depth();
            depth++;
            double right = parse_power();
            depth--;
            return std::pow(left, right);
        }
        return left;
    }

    double parse_unary() {
        check_depth();
        depth++;
        double result;
        if (consume(L'+')) {
            result = parse_unary();
        } else if (consume(L'-')) {
            result = -parse_unary();
        } else {
            result = parse_primary();
        }
        depth--;
        return result;
    }

    std::wstring parse_identifier() {
        skip_spaces();
        std::wstring identifier;
        while (pos < expr.size() && (std::iswalnum(expr[pos]) || expr[pos] == L'_')) {
            identifier.push_back(expr[pos]);
            pos++;
        }
        return identifier;
    }

    double parse_number() {
        skip_spaces();
        size_t start = pos;
        
        // Let's first read a sequence of digits
        size_t digit_start = pos;
        while (pos < expr.size() && std::iswdigit(expr[pos])) {
            pos++;
        }
        
        if (pos < expr.size() && expr[pos] == L'#') {
            // We have base#number!
            std::wstring base_str = expr.substr(digit_start, pos - digit_start);
            if (base_str.empty()) {
                throw std::runtime_error("missing base for arithmetic conversion");
            }
            int base = 0;
            if (!try_parse_int_strict(base_str, base)) {
                throw std::runtime_error("invalid arithmetic base");
            }
            if (base < 2 || base > 64) {
                throw std::runtime_error("invalid arithmetic base (must be 2-64)");
            }
            
            pos++; // consume '#'
            double value = 0;
            bool read_any = false;
            while (pos < expr.size()) {
                wchar_t ch = expr[pos];
                int digit_val = -1;
                if (ch >= L'0' && ch <= L'9') {
                    digit_val = ch - L'0';
                } else if (ch >= L'a' && ch <= L'z') {
                    digit_val = (base <= 36) ? (ch - L'a' + 10) : (ch - L'a' + 10);
                } else if (ch >= L'A' && ch <= L'Z') {
                    digit_val = (base <= 36) ? (ch - L'A' + 10) : (ch - L'A' + 36);
                } else if (ch == L'_') {
                    digit_val = 62;
                } else if (ch == L'@') {
                    digit_val = 63;
                }
                
                if (digit_val >= 0 && digit_val < base) {
                    value = value * base + digit_val;
                    pos++;
                    read_any = true;
                } else {
                    break;
                }
            }
            
            if (!read_any) {
                throw std::runtime_error("invalid character for base arithmetic");
            }
            return value;
        }
        
        // Otherwise, fall back to standard decimal parsing!
        pos = start;
        bool seen_dot = false;
        while (pos < expr.size()) {
            wchar_t ch = expr[pos];
            if (std::iswdigit(ch)) {
                pos++;
                continue;
            }
            if (ch == L'.' && !seen_dot) {
                seen_dot = true;
                pos++;
                continue;
            }
            break;
        }
        if (start == pos) {
            throw std::runtime_error("number expected");
        }
        if (seen_dot) {
            has_float_operands = true;
        }
        return std::stod(expr.substr(start, pos - start));
    }

    double parse_primary() {
        skip_spaces();
        if (consume(L'(')) {
            double inner = parse_expression();
            if (!consume(L')')) {
                throw std::runtime_error("missing closing parenthesis");
            }
            return inner;
        }

        if (pos < expr.size() && (std::iswalpha(expr[pos]) || expr[pos] == L'_')) {
            std::wstring identifier = parse_identifier();
            std::wstring lowered = to_lower_copy(identifier);
            if (lowered == L"pi") {
                has_float_operands = true;
                return std::acos(-1.0);
            }
            if (lowered == L"e") {
                has_float_operands = true;
                return std::exp(1.0);
            }

            if (consume(L'(')) {
                std::vector<double> args;
                skip_spaces();
                if (!consume(L')')) {
                    while (true) {
                        args.push_back(parse_expression());
                        if (consume(L')')) {
                            break;
                        }
                        if (!consume(L',')) {
                            throw std::runtime_error("comma expected");
                        }
                    }
                }
                has_float_operands = true;
                return evaluate_math_function(identifier, args);
            }

            bool is_set = false;
            std::wstring val = get_variable_value_with_hooks(identifier, is_set);
            if (is_set) {
                if (val.find(L'.') != std::wstring::npos) {
                    has_float_operands = true;
                }
                return std::stod(val);
            }
            return 0.0;
        }

        return parse_number();
    }
};

std::wstring evaluate_arithmetic(const std::wstring& expr) {
    try {
        ArithmeticParser parser(expr);
        double value = parser.parse();
        if (!std::isfinite(value)) {
            return L"0";
        }

        if (!parser.is_float()) {
            return std::to_wstring(ArithmeticParser::to_safe_int64(value));
        }

        double nearest = std::round(value);
        if (std::fabs(value - nearest) < 1e-9) {
            return std::to_wstring(static_cast<long long>(nearest));
        }

        wchar_t buffer[64];
        _snwprintf_s(buffer, _countof(buffer), _TRUNCATE, L"%.10f", value);
        std::wstring result(buffer);
        while (!result.empty() && result.back() == L'0') {
            result.pop_back();
        }
        if (!result.empty() && (result.back() == L'.' || result.back() == L',')) {
            result.pop_back();
        }
        return result.empty() ? L"0" : result;
    } catch (...) {
        return L"0";
    }
}

std::wstring trim_trailing_line_endings(const std::wstring& value) {
    size_t end = value.size();
    while (end > 0 && (value[end - 1] == L'\n' || value[end - 1] == L'\r')) {
        end--;
    }
    return value.substr(0, end);
}

std::wstring run_command_and_capture_stdout(const std::wstring& command, DWORD& exit_code) {
    exit_code = 1;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = nullptr;
    sa.bInheritHandle = TRUE;

    HANDLE read_pipe = nullptr;
    HANDLE write_pipe = nullptr;
    if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
        return L"";
    }

    if (!SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(read_pipe);
        CloseHandle(write_pipe);
        return L"";
    }

    std::wstring command_line;
    if (!build_cmd_shell_command_line(command, command_line)) {
        CloseHandle(read_pipe);
        CloseHandle(write_pipe);
        return L"";
    }
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    HANDLE inherited_stdin = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE inherited_stderr = GetStdHandle(STD_ERROR_HANDLE);
    HANDLE dup_stdin = nullptr;
    HANDLE dup_stderr = nullptr;

    if (inherited_stdin != nullptr && inherited_stdin != INVALID_HANDLE_VALUE) {
        if (!DuplicateHandle(GetCurrentProcess(), inherited_stdin, GetCurrentProcess(), &dup_stdin, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
            CloseHandle(read_pipe);
            CloseHandle(write_pipe);
            return L"";
        }
        inherited_stdin = dup_stdin;
    }

    if (inherited_stderr != nullptr && inherited_stderr != INVALID_HANDLE_VALUE) {
        if (!DuplicateHandle(GetCurrentProcess(), inherited_stderr, GetCurrentProcess(), &dup_stderr, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
            if (dup_stdin != nullptr) {
                CloseHandle(dup_stdin);
            }
            CloseHandle(read_pipe);
            CloseHandle(write_pipe);
            return L"";
        }
        inherited_stderr = dup_stderr;
    }

    si.hStdInput = inherited_stdin;
    si.hStdOutput = write_pipe;
    si.hStdError = inherited_stderr;

    std::vector<HANDLE> inherited_handles = { si.hStdInput, si.hStdOutput, si.hStdError };
    BOOL created = create_process_with_handle_list(command_line, si, inherited_handles, CREATE_NO_WINDOW, pi) ? TRUE : FALSE;

    CloseHandle(write_pipe);
    if (dup_stdin != nullptr) {
        CloseHandle(dup_stdin);
    }
    if (dup_stderr != nullptr) {
        CloseHandle(dup_stderr);
    }

    if (!created) {
        CloseHandle(read_pipe);
        return L"";
    }

    std::string stdout_bytes;
    char buffer[4096];
    DWORD bytes_read = 0;
    bool output_limit_hit = false;
    while (ReadFile(read_pipe, buffer, static_cast<DWORD>(sizeof(buffer)), &bytes_read, nullptr) && bytes_read > 0) {
        if (stdout_bytes.size() + bytes_read > kMaxCommandSubstitutionOutputBytes) {
            output_limit_hit = true;
            TerminateProcess(pi.hProcess, 1);
            break;
        }
        stdout_bytes.append(buffer, buffer + bytes_read);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    register_child_process_times(pi.hProcess);
    GetExitCodeProcess(pi.hProcess, &exit_code);

    if (output_limit_hit) {
        std::wcerr << L"ksh: command substitution output exceeded limit\n";
        exit_code = 1;
        g_expansion_error = true;
        ksh_env.variables[L"?"] = L"1";
    }

    CloseHandle(read_pipe);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    UINT cp = GetConsoleOutputCP();
    if (cp == 0) {
        cp = CP_OEMCP;
    }

    std::wstring text = decode_multibyte(stdout_bytes, CP_UTF8);
    if (text.empty() && !stdout_bytes.empty()) {
        text = decode_multibyte(stdout_bytes, cp);
    }
    if (text.empty() && !stdout_bytes.empty()) {
        text = decode_multibyte(stdout_bytes, CP_ACP);
    }

    return trim_trailing_line_endings(text);
}

bool parse_command_substitution_content(const std::wstring& input, size_t start_index, std::wstring& content, size_t& next_index) {
    content.clear();
    next_index = start_index;

    int depth = 1;
    bool in_single_quotes = false;
    bool in_double_quotes = false;
    bool escaped = false;

    size_t i = start_index;
    while (i < input.size()) {
        wchar_t ch = input[i];

        if (escaped) {
            content += ch;
            escaped = false;
            i++;
            continue;
        }

        if (ch == L'\\' && !in_single_quotes) {
            content += ch;
            escaped = true;
            i++;
            continue;
        }

        if (ch == L'\'' && !in_double_quotes) {
            in_single_quotes = !in_single_quotes;
            content += ch;
            i++;
            continue;
        }

        if (ch == L'"' && !in_single_quotes) {
            in_double_quotes = !in_double_quotes;
            content += ch;
            i++;
            continue;
        }

        if (!in_single_quotes && !in_double_quotes) {
            if (ch == L'$' && (i + 1) < input.size() && input[i + 1] == L'(' && !((i + 2) < input.size() && input[i + 2] == L'(')) {
                depth++;
                if (depth > kMaxSubstitutionParseDepth) {
                    return false;
                }
                content += L"$(";
                i += 2;
                continue;
            }

            if (ch == L'(') {
                depth++;
                if (depth > kMaxSubstitutionParseDepth) {
                    return false;
                }
                content += ch;
                i++;
                continue;
            }

            if (ch == L')') {
                depth--;
                if (depth == 0) {
                    next_index = i + 1;
                    return true;
                }
                content += ch;
                i++;
                continue;
            }
        }

        content += ch;
        i++;
    }

    return false;
}

bool parse_arithmetic_substitution_content(const std::wstring& input, size_t start_index, std::wstring& content, size_t& next_index) {
    content.clear();
    next_index = start_index;

    int paren_depth = 0;
    bool in_single_quotes = false;
    bool in_double_quotes = false;
    bool escaped = false;

    size_t i = start_index;
    while (i < input.size()) {
        wchar_t ch = input[i];

        if (escaped) {
            content += ch;
            escaped = false;
            i++;
            continue;
        }

        if (ch == L'\\' && !in_single_quotes) {
            content += ch;
            escaped = true;
            i++;
            continue;
        }

        if (ch == L'\'' && !in_double_quotes) {
            in_single_quotes = !in_single_quotes;
            content += ch;
            i++;
            continue;
        }

        if (ch == L'"' && !in_single_quotes) {
            in_double_quotes = !in_double_quotes;
            content += ch;
            i++;
            continue;
        }

        if (!in_single_quotes && !in_double_quotes) {
            if (ch == L'(') {
                paren_depth++;
                if (paren_depth > kMaxSubstitutionParseDepth) {
                    return false;
                }
                content += ch;
                i++;
                continue;
            }

            if (ch == L')') {
                if (paren_depth > 0) {
                    paren_depth--;
                    content += ch;
                    i++;
                    continue;
                }

                if ((i + 1) < input.size() && input[i + 1] == L')') {
                    next_index = i + 2;
                    return true;
                }
            }
        }

        content += ch;
        i++;
    }

    return false;
}

std::wstring expand_variable_reference(const std::wstring& input, size_t dollar_index, size_t& next_index, bool in_double_quotes) {
    auto is_assignable_parameter_name = [](const std::wstring& name) -> bool {
        if (name.empty()) {
            return false;
        }
        if (!(std::iswalpha(name[0]) || name[0] == L'_')) {
            return false;
        }
        return std::all_of(name.begin(), name.end(), [](wchar_t ch) {
            return std::iswalnum(ch) != 0 || ch == L'_' || ch == L'.';
        });
    };

    auto is_valid_variable_name = [](const std::wstring& name) -> bool {
        if (name.empty()) {
            return false;
        }
        if (name[0] == L'.') {
            if (name.size() < 2) return false;
            return std::all_of(name.begin() + 1, name.end(), [](wchar_t ch) {
                return std::iswalnum(ch) != 0 || ch == L'_' || ch == L'.' || ch == L'/';
            });
        }
        if (!(std::iswalpha(name[0]) || name[0] == L'_')) {
            return false;
        }
        return std::all_of(name.begin(), name.end(), [](wchar_t ch) {
            return std::iswalnum(ch) != 0 || ch == L'_' || ch == L'.' || ch == L'/';
        });
    };

    auto parse_array_reference = [&](const std::wstring& text, std::wstring& array_name, std::wstring& index_expr, bool& has_index) -> bool {
        array_name.clear();
        index_expr.clear();
        has_index = false;

        size_t open = text.find(L'[');
        if (open == std::wstring::npos) {
            array_name = text;
            return is_valid_variable_name(array_name);
        }

        if (open == 0 || text.back() != L']') {
            return false;
        }

        array_name = text.substr(0, open);
        if (!is_valid_variable_name(array_name)) {
            return false;
        }

        index_expr = text.substr(open + 1, text.size() - open - 2);
        if (index_expr.empty()) {
            return false;
        }

        has_index = true;
        return true;
    };

    auto parse_non_negative_index = [](const std::wstring& text, size_t& out_index) -> bool {
        if (text.empty()) {
            return false;
        }

        try {
            size_t consumed = 0;
            unsigned long long parsed = std::stoull(text, &consumed);
            if (consumed != text.size()) {
                return false;
            }
            out_index = static_cast<size_t>(parsed);
            return true;
        } catch (...) {
            return false;
        }
    };

    auto join_array_values = [](const std::vector<std::wstring>& values, wchar_t separator) -> std::wstring {
        std::wstring joined;
        for (size_t i = 0; i < values.size(); ++i) {
            joined += values[i];
            if (i + 1 < values.size()) {
                joined.push_back(separator);
            }
        }
        return joined;
    };

    auto try_get_parameter_value = [&](const std::wstring& name, std::wstring& out_value, bool& is_set) {
        out_value.clear();
        is_set = false;

        const std::vector<std::wstring>& script_args = current_script_args();
        const std::wstring script_name = current_script_name();

        if (name == L"#") {
            out_value = std::to_wstring(script_args.size());
            is_set = true;
            return;
        }

        if (name == L"@") {
            if (in_double_quotes) {
                out_value = join_array_values(script_args, k_array_at_quoted_separator);
            } else {
                out_value = join_script_args(script_args);
            }
            is_set = true;
            return;
        }

        if (name == L"*") {
            wchar_t separator = L' ';
            std::map<std::wstring, std::wstring>::const_iterator ifs_it = ksh_env.variables.find(L"IFS");
            if (ifs_it != ksh_env.variables.end()) {
                if (ifs_it->second.empty()) {
                    separator = L'\0';
                } else {
                    separator = ifs_it->second[0];
                }
            }

            if (separator == L'\0') {
                out_value.clear();
                for (const std::wstring& arg : script_args) {
                    out_value += arg;
                }
            } else {
                out_value = join_array_values(script_args, separator);
            }
            is_set = true;
            return;
        }

        if (name == L"$") {
            out_value = std::to_wstring(GetCurrentProcessId());
            is_set = true;
            return;
        }

        if (!name.empty() && std::all_of(name.begin(), name.end(), [](wchar_t ch) { return std::iswdigit(ch) != 0; })) {
            unsigned long arg_index = 0;
            if (!try_parse_unsigned_long_strict(name, arg_index)) {
                return;
            }
            if (arg_index == 0) {
                out_value = script_name;
                is_set = true;
                return;
            }
            if (arg_index <= script_args.size()) {
                out_value = script_args[static_cast<size_t>(arg_index - 1)];
                is_set = true;
                return;
            }
            return;
        }

        std::wstring array_name;
        std::wstring index_expr;
        bool has_index = false;
        bool is_key_expansion = false;
        std::wstring name_to_use = name;
        if (name.size() > 1 && name.front() == L'!') {
            is_key_expansion = true;
            name_to_use = name.substr(1);
        }

        bool handled_nameref_key = false;
        std::wstring array_name_tmp;
        std::wstring index_expr_tmp;
        bool has_index_tmp = false;
        if (parse_array_reference(name_to_use, array_name_tmp, index_expr_tmp, has_index_tmp)) {
            if (is_key_expansion && !has_index_tmp) {
                if (get_flag_value(ksh_env.nameref_flags, array_name_tmp)) {
                    std::map<std::wstring, std::wstring>::const_iterator val_it = ksh_env.variables.find(array_name_tmp);
                    if (val_it != ksh_env.variables.end()) {
                        out_value = val_it->second;
                        is_set = true;
                        handled_nameref_key = true;
                    }
                }
            }
        }

        if (!handled_nameref_key) {
            name_to_use = resolve_variable_name(name_to_use);
            if (parse_array_reference(name_to_use, array_name, index_expr, has_index)) {
                std::map<std::wstring, std::map<std::wstring, std::wstring>>::const_iterator arr_it = ksh_env.arrays.find(array_name);
                if (arr_it != ksh_env.arrays.end()) {
                    is_set = true;

                    if (is_key_expansion) {
                        if (has_index && (index_expr == L"@" || index_expr == L"*")) {
                            std::vector<std::wstring> keys;
                            for (const auto& pair : arr_it->second) {
                                keys.push_back(pair.first);
                            }
                            if (index_expr == L"@") {
                                out_value = in_double_quotes
                                    ? join_array_values(keys, k_array_at_quoted_separator)
                                    : join_array_values(keys, L' ');
                            } else {
                                out_value = join_array_values(keys, L' ');
                            }
                        } else {
                            out_value = array_name;
                        }
                        return;
                    }

                    if (!has_index) {
                        std::map<std::wstring, std::wstring>::const_iterator val_it = arr_it->second.find(L"0");
                        out_value = (val_it != arr_it->second.end()) ? val_it->second : L"";
                        return;
                    }

                    if (index_expr == L"@") {
                        std::vector<std::wstring> values = get_array_values_vector(array_name);
                        out_value = in_double_quotes
                            ? join_array_values(values, k_array_at_quoted_separator)
                            : join_array_values(values, L' ');
                        return;
                    }

                    if (index_expr == L"*") {
                        std::vector<std::wstring> values = get_array_values_vector(array_name);
                        out_value = join_array_values(values, L' ');
                        return;
                    }

                    bool is_assoc = false;
                    std::map<std::wstring, bool>::const_iterator assoc_it = ksh_env.associative_flags.find(array_name);
                    if (assoc_it != ksh_env.associative_flags.end()) {
                        is_assoc = assoc_it->second;
                    }

                    if (is_assoc) {
                        std::map<std::wstring, std::wstring>::const_iterator val_it = arr_it->second.find(index_expr);
                        if (val_it != arr_it->second.end()) {
                            out_value = val_it->second;
                        } else {
                            out_value.clear();
                        }
                    } else {
                        size_t idx = 0;
                        if (parse_non_negative_index(index_expr, idx)) {
                            std::wstring key = std::to_wstring(idx);
                            std::map<std::wstring, std::wstring>::const_iterator val_it = arr_it->second.find(key);
                            if (val_it != arr_it->second.end()) {
                                out_value = val_it->second;
                            } else {
                                out_value.clear();
                            }
                        } else {
                            is_set = false;
                            out_value.clear();
                        }
                    }
                    return;
                }
            }
        }

        std::wstring hook_val = get_variable_value_with_hooks(name_to_use, is_set);
        if (is_set) {
            out_value = hook_val;
            return;
        }

        wchar_t small_buf[256];
        DWORD ret = GetEnvironmentVariableW(name_to_use.c_str(), small_buf, _countof(small_buf));
        if (ret > 0 && ret < _countof(small_buf)) {
            out_value.assign(small_buf, ret);
            is_set = true;
        } else if (ret >= _countof(small_buf)) {
            std::vector<wchar_t> dyn_buf(ret + 1);
            DWORD dyn_ret = GetEnvironmentVariableW(name_to_use.c_str(), dyn_buf.data(), static_cast<DWORD>(dyn_buf.size()));
            if (dyn_ret > 0 && dyn_ret < dyn_buf.size()) {
                out_value.assign(dyn_buf.data(), dyn_ret);
                is_set = true;
            }
        } else if (GetLastError() == ERROR_SUCCESS) {
            out_value.clear();
            is_set = true;
        }
    };

    auto parse_braced_parameter_word = [](const std::wstring& text, size_t start_index, std::wstring& word, size_t& after_closing_brace) -> bool {
        word.clear();
        after_closing_brace = start_index;

        int brace_depth = 0;
        bool in_single_quotes = false;
        bool in_double_quotes = false;
        bool escaped = false;

        size_t i = start_index;
        while (i < text.size()) {
            wchar_t ch = text[i];

            if (escaped) {
                word += ch;
                escaped = false;
                i++;
                continue;
            }

            if (ch == L'\\' && !in_single_quotes) {
                word += ch;
                escaped = true;
                i++;
                continue;
            }

            if (ch == L'\'' && !in_double_quotes) {
                in_single_quotes = !in_single_quotes;
                word += ch;
                i++;
                continue;
            }

            if (ch == L'"' && !in_single_quotes) {
                in_double_quotes = !in_double_quotes;
                word += ch;
                i++;
                continue;
            }

            if (!in_single_quotes && !in_double_quotes) {
                if (ch == L'{') {
                    brace_depth++;
                    word += ch;
                    i++;
                    continue;
                }

                if (ch == L'}') {
                    if (brace_depth == 0) {
                        after_closing_brace = i + 1;
                        return true;
                    }
                    brace_depth--;
                    word += ch;
                    i++;
                    continue;
                }
            }

            word += ch;
            i++;
        }

        return false;
    };

    auto match_parameter_pattern = [](const std::wstring& pattern, const std::wstring& candidate) -> bool {
        const bool case_sensitive = pattern_match_case_sensitive();
        unsigned long long step_budget = kMaxPatternMatchSteps;
        return match_glob_pattern_recursive(pattern, 0, candidate, 0, case_sensitive, step_budget);
    };

    auto apply_parameter_pattern_removal = [&](const std::wstring& value, const std::wstring& raw_pattern, bool prefix_mode, bool longest_match) -> std::wstring {
        std::wstring expanded_pattern = expand_substitutions_left_to_right(raw_pattern);
        if (g_expansion_error) {
            return L"";
        }

        if (expanded_pattern.empty()) {
            return value;
        }

        if (prefix_mode) {
            if (longest_match) {
                for (size_t len = value.size() + 1; len > 0; --len) {
                    size_t prefix_len = len - 1;
                    if (match_parameter_pattern(expanded_pattern, value.substr(0, prefix_len))) {
                        return value.substr(prefix_len);
                    }
                }
            } else {
                for (size_t prefix_len = 0; prefix_len <= value.size(); ++prefix_len) {
                    if (match_parameter_pattern(expanded_pattern, value.substr(0, prefix_len))) {
                        return value.substr(prefix_len);
                    }
                }
            }
            return value;
        }

        if (longest_match) {
            for (size_t start = 0; start <= value.size(); ++start) {
                if (match_parameter_pattern(expanded_pattern, value.substr(start))) {
                    return value.substr(0, start);
                }
            }
        } else {
            for (size_t start = value.size() + 1; start > 0; --start) {
                size_t suffix_start = start - 1;
                if (match_parameter_pattern(expanded_pattern, value.substr(suffix_start))) {
                    return value.substr(0, suffix_start);
                }
            }
        }

        return value;
    };

    auto apply_parameter_substitution = [&](const std::wstring& value, const std::wstring& raw_pattern, const std::wstring& raw_replacement, bool replace_all) -> std::wstring {
        std::wstring expanded_pattern = expand_substitutions_left_to_right(raw_pattern);
        if (g_expansion_error) {
            return L"";
        }

        std::wstring replacement = expand_substitutions_left_to_right(raw_replacement);
        if (g_expansion_error) {
            return L"";
        }

        if (expanded_pattern.empty()) {
            return value;
        }

        auto find_match = [&](size_t start_index, size_t& match_start, size_t& match_length) -> bool {
            for (size_t begin = start_index; begin <= value.size(); ++begin) {
                for (size_t end = begin; end <= value.size(); ++end) {
                    if (match_parameter_pattern(expanded_pattern, value.substr(begin, end - begin))) {
                        match_start = begin;
                        match_length = end - begin;
                        return true;
                    }
                }
            }
            return false;
        };

        std::wstring result;
        size_t cursor = 0;
        bool replaced = false;
        while (cursor <= value.size()) {
            size_t match_start = 0;
            size_t match_length = 0;
            if (!find_match(cursor, match_start, match_length)) {
                result += value.substr(cursor);
                break;
            }

            replaced = true;
            result += value.substr(cursor, match_start - cursor);
            result += replacement;

            if (match_length == 0) {
                if (match_start < value.size()) {
                    result.push_back(value[match_start]);
                }
                cursor = match_start + 1;
            } else {
                cursor = match_start + match_length;
            }

            if (!replace_all) {
                result += value.substr(cursor);
                break;
            }
        }

        return replaced ? result : value;
    };

    next_index = dollar_index + 1;

    auto fail_bad_substitution = [&]() -> std::wstring {
        std::wcerr << L"ksh: bad substitution: missing }\n";
        ksh_env.variables[L"?"] = L"1";
        g_expansion_error = true;
        return L"";
    };

    if (next_index >= input.length()) {
        return L"$";
    }

    wchar_t marker = input[next_index];
    if (marker == L'#') {
        next_index++;
        std::wstring value;
        bool is_set = false;
        try_get_parameter_value(L"#", value, is_set);
        return value;
    }
    if (marker == L'@') {
        next_index++;
        std::wstring value;
        bool is_set = false;
        try_get_parameter_value(L"@", value, is_set);
        return value;
    }
    if (marker == L'*') {
        next_index++;
        std::wstring value;
        bool is_set = false;
        try_get_parameter_value(L"*", value, is_set);
        return value;
    }
    if (marker == L'$') {
        next_index++;
        std::wstring value;
        bool is_set = false;
        try_get_parameter_value(L"$", value, is_set);
        return value;
    }
    if (marker == L'?') {
        next_index++;
        std::wstring value;
        bool is_set = false;
        try_get_parameter_value(L"?", value, is_set);
        if (is_set) {
            return value;
        }
        return L"0";
    }
    if (std::iswdigit(marker)) {
        std::wstring index_text;
        while (next_index < input.length() && std::iswdigit(input[next_index])) {
            index_text += input[next_index];
            next_index++;
        }
        std::wstring value;
        bool is_set = false;
        try_get_parameter_value(index_text, value, is_set);
        if (is_set) {
            return value;
        }
        return L"";
    }

    bool braced = false;
    bool closing_brace_consumed = false;
    bool length_operator = false;
    std::wstring var_name;
    if (marker == L'{') {
        braced = true;
        next_index++;

        if (next_index < input.length() && input[next_index] == L'#') {
            length_operator = true;
            next_index++;
        }

        if (next_index < input.length() && input[next_index] == L'!') {
            var_name += L'!';
            next_index++;
        }
    }

    if (next_index < input.length() && (input[next_index] == L'@' || input[next_index] == L'#' || input[next_index] == L'*')) {
        var_name += input[next_index];
        next_index++;
    } else if (next_index < input.length() && input[next_index] == L'.') {
        var_name += input[next_index];
        next_index++;
    }

    while (next_index < input.length() && (std::iswalnum(input[next_index]) || input[next_index] == L'_' || input[next_index] == L'.')) {
        var_name += input[next_index];
        next_index++;
    }

    if (braced && next_index < input.length() && input[next_index] == L'[') {
        size_t bracket_end = input.find(L']', next_index + 1);
        if (bracket_end == std::wstring::npos) {
            return fail_bad_substitution();
        }

        var_name += input.substr(next_index, bracket_end - next_index + 1);
        next_index = bracket_end + 1;
    }

    if (length_operator) {
        std::wstring resolved_value;
        bool resolved_is_set = false;

        if (var_name.empty()) {
            return fail_bad_substitution();
        }

        std::wstring resolved_var_name = resolve_variable_name(var_name);

        std::wstring array_name;
        std::wstring index_expr;
        bool has_index = false;
        if (parse_array_reference(resolved_var_name, array_name, index_expr, has_index)) {
            std::map<std::wstring, std::map<std::wstring, std::wstring>>::const_iterator arr_it = ksh_env.arrays.find(array_name);
            if (arr_it != ksh_env.arrays.end()) {
                if (has_index && (index_expr == L"@" || index_expr == L"*")) {
                    resolved_value = std::to_wstring(arr_it->second.size());
                    resolved_is_set = true;
                } else if (has_index) {
                    bool is_assoc = false;
                    std::map<std::wstring, bool>::const_iterator assoc_it = ksh_env.associative_flags.find(array_name);
                    if (assoc_it != ksh_env.associative_flags.end()) {
                        is_assoc = assoc_it->second;
                    }

                    if (is_assoc) {
                        std::map<std::wstring, std::wstring>::const_iterator val_it = arr_it->second.find(index_expr);
                        if (val_it != arr_it->second.end()) {
                            resolved_value = std::to_wstring(val_it->second.size());
                        } else {
                            resolved_value = L"0";
                        }
                        resolved_is_set = true;
                    } else {
                        size_t idx = 0;
                        if (parse_non_negative_index(index_expr, idx)) {
                            std::wstring key = std::to_wstring(idx);
                            std::map<std::wstring, std::wstring>::const_iterator val_it = arr_it->second.find(key);
                            if (val_it != arr_it->second.end()) {
                                resolved_value = std::to_wstring(val_it->second.size());
                            } else {
                                resolved_value = L"0";
                            }
                            resolved_is_set = true;
                        } else {
                            resolved_value = L"0";
                            resolved_is_set = true;
                        }
                    }
                } else {
                    std::map<std::wstring, std::wstring>::const_iterator val_it = arr_it->second.find(L"0");
                    resolved_value = (val_it != arr_it->second.end()) ? std::to_wstring(val_it->second.size()) : L"0";
                    resolved_is_set = true;
                }
            }
        }

        if (!resolved_is_set) {
            try_get_parameter_value(resolved_var_name, resolved_value, resolved_is_set);
            if (resolved_is_set) {
                resolved_value = std::to_wstring(resolved_value.size());
            }
        }

        if (!resolved_is_set) {
            resolved_value = L"0";
        }

        if (braced) {
            if (next_index >= input.length() || input[next_index] != L'}') {
                return fail_bad_substitution();
            }
            next_index++;
            closing_brace_consumed = true;
        }

        return resolved_value;
    }

    if (braced && next_index < input.length() && input[next_index] == L':' && (next_index + 1) < input.length()) {
        wchar_t op = input[next_index + 1];
        if (op != L'-' && op != L'=' && op != L'+' && op != L'?') {
            size_t after_offset = next_index + 1;
            size_t colon_or_brace = after_offset;
            int inner_brace_depth = 0;
            while (colon_or_brace < input.length()) {
                if (input[colon_or_brace] == L'{') inner_brace_depth++;
                if (input[colon_or_brace] == L'}') {
                    if (inner_brace_depth == 0) break;
                    inner_brace_depth--;
                }
                if (input[colon_or_brace] == L':' && inner_brace_depth == 0) {
                    break;
                }
                colon_or_brace++;
            }

            if (colon_or_brace == input.length()) {
                return fail_bad_substitution();
            }

            std::wstring offset_str = input.substr(after_offset, colon_or_brace - after_offset);
            std::wstring length_str;
            bool has_length = false;

            if (input[colon_or_brace] == L':') {
                has_length = true;
                size_t length_start = colon_or_brace + 1;
                size_t closing_brace = length_start;
                int inner_brace_depth2 = 0;
                while (closing_brace < input.length()) {
                    if (input[closing_brace] == L'{') inner_brace_depth2++;
                    if (input[closing_brace] == L'}') {
                        if (inner_brace_depth2 == 0) break;
                        inner_brace_depth2--;
                    }
                    closing_brace++;
                }
                if (closing_brace == input.length()) {
                    return fail_bad_substitution();
                }
                length_str = input.substr(length_start, closing_brace - length_start);
                next_index = closing_brace + 1;
            } else {
                next_index = colon_or_brace + 1;
            }

            closing_brace_consumed = true;

            std::wstring expanded_offset = expand_substitutions_left_to_right(offset_str);
            if (g_expansion_error) return L"";
            std::wstring evaluated_offset = evaluate_arithmetic(expanded_offset);
            int offset = 0;
            if (!try_parse_int_strict(evaluated_offset, offset)) {
                std::wcerr << L"ksh: bad substitution: invalid offset\n";
                ksh_env.variables[L"?"] = L"1";
                g_expansion_error = true;
                return L"";
            }

            int length = -1;
            if (has_length) {
                std::wstring expanded_length = expand_substitutions_left_to_right(length_str);
                if (g_expansion_error) return L"";
                std::wstring evaluated_length = evaluate_arithmetic(expanded_length);
                if (!try_parse_int_strict(evaluated_length, length)) {
                    std::wcerr << L"ksh: bad substitution: invalid length\n";
                    ksh_env.variables[L"?"] = L"1";
                    g_expansion_error = true;
                    return L"";
                }
            }

            std::wstring existing_value;
            bool is_set = false;
            try_get_parameter_value(var_name, existing_value, is_set);
            if (!is_set) {
                if (g_nounset_enabled) {
                    std::wcerr << L"ksh: " << var_name << L": parameter not set\n";
                    ksh_env.variables[L"?"] = L"1";
                    g_expansion_error = true;
                }
                return L"";
            }

            int val_len = static_cast<int>(existing_value.length());
            if (offset < 0) {
                offset = val_len + offset;
                if (offset < 0) offset = 0;
            }
            if (offset > val_len) {
                return L"";
            }

            if (length < 0) {
                return existing_value.substr(offset);
            } else {
                if (offset + length > val_len) {
                    length = val_len - offset;
                }
                return existing_value.substr(offset, length);
            }
        }
        if (op == L'-' || op == L'=' || op == L'+' || op == L'?') {
            std::wstring parameter_word;
            size_t after_word = next_index + 2;
            if (!parse_braced_parameter_word(input, next_index + 2, parameter_word, after_word)) {
                return fail_bad_substitution();
            }

            next_index = after_word;
            closing_brace_consumed = true;

            std::wstring existing_value;
            bool is_set = false;
            try_get_parameter_value(var_name, existing_value, is_set);
            bool is_null_or_unset = (!is_set || existing_value.empty());

            if (op == L'-') {
                if (is_null_or_unset) {
                    return expand_substitutions_left_to_right(parameter_word);
                }
                return existing_value;
            }

            if (op == L'=') {
                if (!is_null_or_unset) {
                    return existing_value;
                }

                std::wstring assigned_value = expand_substitutions_left_to_right(parameter_word);
                if (g_expansion_error) {
                    return L"";
                }

                if (!is_assignable_parameter_name(var_name)) {
                    std::wcerr << L"ksh: bad substitution for := operator\n";
                    ksh_env.variables[L"?"] = L"1";
                    g_expansion_error = true;
                    return L"";
                }

                ksh_env.variables[var_name] = assigned_value;
                SetEnvironmentVariableW(var_name.c_str(), assigned_value.c_str());
                return assigned_value;
            }

            if (op == L'+') {
                if (is_null_or_unset) {
                    return L"";
                }
                return expand_substitutions_left_to_right(parameter_word);
            }

            if (is_null_or_unset) {
                std::wstring message = expand_substitutions_left_to_right(parameter_word);
                if (message.empty()) {
                    message = L"parameter '" + var_name + L"' is unset or empty";
                }
                std::wcerr << L"ksh: " << message << L"\n";
                ksh_env.variables[L"?"] = L"1";
                g_expansion_error = true;
                return L"";
            }

            return existing_value;
        }
    }

    if (braced && next_index < input.length() && (input[next_index] == L'#' || input[next_index] == L'%')) {
        const bool prefix_mode = input[next_index] == L'#';
        bool longest_match = false;
        size_t pattern_start = next_index + 1;
        if (pattern_start < input.length() && input[pattern_start] == input[next_index]) {
            longest_match = true;
            pattern_start++;
        }

        std::wstring pattern_word;
        size_t after_word = pattern_start;
        if (!parse_braced_parameter_word(input, pattern_start, pattern_word, after_word)) {
            return fail_bad_substitution();
        }

        next_index = after_word;
        closing_brace_consumed = true;

        std::wstring existing_value;
        bool is_set = false;
        try_get_parameter_value(var_name, existing_value, is_set);
        if (!is_set) {
            return L"";
        }

        return apply_parameter_pattern_removal(existing_value, pattern_word, prefix_mode, longest_match);
    }

    if (braced && next_index < input.length() && input[next_index] == L'/') {
        bool replace_all = false;
        size_t pattern_start = next_index + 1;
        if (pattern_start < input.length() && input[pattern_start] == L'/') {
            replace_all = true;
            pattern_start++;
        }

        std::wstring pattern_word;
        std::wstring replacement_word;
        size_t i = pattern_start;
        int brace_depth = 0;
        bool in_single_quotes = false;
        bool inner_in_double_quotes = false;
        bool escaped = false;
        bool saw_separator = false;

        while (i < input.length()) {
            wchar_t ch = input[i];

            if (escaped) {
                (saw_separator ? replacement_word : pattern_word) += ch;
                escaped = false;
                i++;
                continue;
            }

            if (ch == L'\\' && !in_single_quotes) {
                (saw_separator ? replacement_word : pattern_word) += ch;
                escaped = true;
                i++;
                continue;
            }

            if (ch == L'\'' && !inner_in_double_quotes) {
                in_single_quotes = !in_single_quotes;
                (saw_separator ? replacement_word : pattern_word) += ch;
                i++;
                continue;
            }

            if (ch == L'"' && !in_single_quotes) {
                inner_in_double_quotes = !inner_in_double_quotes;
                (saw_separator ? replacement_word : pattern_word) += ch;
                i++;
                continue;
            }

            if (!in_single_quotes && !inner_in_double_quotes) {
                if (ch == L'{') {
                    brace_depth++;
                    (saw_separator ? replacement_word : pattern_word) += ch;
                    i++;
                    continue;
                }

                if (ch == L'}') {
                    if (brace_depth == 0) {
                        next_index = i + 1;
                        closing_brace_consumed = true;

                        std::wstring existing_value;
                        bool is_set = false;
                        try_get_parameter_value(var_name, existing_value, is_set);
                        if (!is_set) {
                            return L"";
                        }

                        return apply_parameter_substitution(existing_value, pattern_word, replacement_word, replace_all);
                    }

                    brace_depth--;
                    (saw_separator ? replacement_word : pattern_word) += ch;
                    i++;
                    continue;
                }

                if (ch == L'/' && brace_depth == 0 && !saw_separator) {
                    saw_separator = true;
                    i++;
                    continue;
                }
            }

            (saw_separator ? replacement_word : pattern_word) += ch;
            i++;
        }

        return fail_bad_substitution();
    }

    if (braced && !closing_brace_consumed) {
        if (next_index >= input.length() || input[next_index] != L'}') {
            return fail_bad_substitution();
        }
        next_index++;
        closing_brace_consumed = true;
    }

    if (var_name.empty()) {
        if (braced) {
            std::wcerr << L"ksh: bad substitution\n";
            ksh_env.variables[L"?"] = L"1";
            g_expansion_error = true;
            return L"";
        }
        return L"$";
    }

    std::wstring resolved_value;
    bool resolved_is_set = false;
    try_get_parameter_value(var_name, resolved_value, resolved_is_set);
    if (resolved_is_set) {
        return resolved_value;
    }
    if (g_nounset_enabled) {
        std::wcerr << L"ksh: " << var_name << L": parameter not set\n";
        ksh_env.variables[L"?"] = L"1";
        g_expansion_error = true;
    }
    return L"";
}

std::wstring expand_substitutions_left_to_right(const std::wstring& input) {
    ScopedExpansionDepth depth_guard;
    if (!depth_guard.active()) {
        std::wcerr << L"ksh: expansion nesting depth exceeded limit\n";
        ksh_env.variables[L"?"] = L"1";
        g_expansion_error = true;
        return L"";
    }

    std::wstring result;
    size_t i = 0;
    bool in_single_quotes = false;
    bool in_double_quotes = false;
    bool escaped = false;

    while (i < input.length()) {
        wchar_t ch = input[i];

        if (escaped) {
            result += ch;
            escaped = false;
            i++;
            continue;
        }

        if (ch == L'\\' && !in_single_quotes) {
            result += ch;
            escaped = true;
            i++;
            continue;
        }

        if (ch == L'\'' && !in_double_quotes) {
            in_single_quotes = !in_single_quotes;
            result += ch;
            i++;
            continue;
        }

        if (ch == L'"' && !in_single_quotes) {
            in_double_quotes = !in_double_quotes;
            result += ch;
            i++;
            continue;
        }

        if (ch != L'$' || in_single_quotes) {
            result += ch;
            i++;
            continue;
        }

        if ((i + 1) < input.length() && input[i + 1] == L'(') {
            if ((i + 2) < input.length() && input[i + 2] == L'(') {
                std::wstring arith_content;
                size_t next_index = i + 3;
                if (!parse_arithmetic_substitution_content(input, i + 3, arith_content, next_index)) {
                    result += L"$((";
                    i += 3;
                    continue;
                }

                std::wstring expanded_arith = expand_substitutions_left_to_right(arith_content);
                result += evaluate_arithmetic(expanded_arith);
                i = next_index;
                continue;
            }

            std::wstring cmd_content;
            size_t next_index = i + 2;
            if (!parse_command_substitution_content(input, i + 2, cmd_content, next_index)) {
                result += L"$(";
                i += 2;
                continue;
            }

            std::wstring expanded_command = expand_substitutions_left_to_right(cmd_content);
            std::vector<std::wstring> substitution_tokens = ksh_tokenize_preserve_quotes(expanded_command);
            const bool use_capture_sink = can_capture_builtin_command_substitution(substitution_tokens);
            std::wstring sub_output = execute_command_substitution(expanded_command, use_capture_sink);

            result += sub_output;
            i = next_index;
            continue;
        }

        size_t next_index = i + 1;
        result += expand_variable_reference(input, i, next_index, in_double_quotes);
        if (next_index <= i) {
            next_index = i + 1;
        }
        i = next_index;
    }

    return result;
}

std::wstring evaluate_command_substitutions(const std::wstring& input) {
    ScopedExpansionDepth depth_guard;
    if (!depth_guard.active()) {
        g_expansion_error = true;
        std::wcerr << L"ksh: maximum expansion nesting depth exceeded\n";
        return L"";
    }

    std::wstring output;
    size_t i = 0;

    while (i < input.size()) {
        if (input[i] == L'$' && (i + 1) < input.size() && input[i + 1] == L'(' && !((i + 2) < input.size() && input[i + 2] == L'(')) {
            std::wstring cmd_content;
            size_t next_index = i + 2;
            if (!parse_command_substitution_content(input, i + 2, cmd_content, next_index)) {
                output += L"$(";
                i += 2;
                continue;
            }

            std::wstring nested = evaluate_command_substitutions(cmd_content);
            if (g_expansion_error) {
                return L"";
            }
            std::vector<std::wstring> substitution_tokens = ksh_tokenize_preserve_quotes(nested);
            const bool use_capture_sink = can_capture_builtin_command_substitution(substitution_tokens);
            std::wstring sub_output = execute_command_substitution(nested, use_capture_sink);

            output += sub_output;
            i = next_index;
            continue;
        }

        output += input[i];
        i++;
    }

    return output;
}

// Expand ksh93 variables, arrays, and $((arithmetic))
std::vector<std::wstring> expand_globs_for_tokens(const std::vector<std::wstring>& tokens, const std::vector<std::wstring>& preserved_tokens);

std::wstring join_fields_with_internal_separator(const std::vector<std::wstring>& fields) {
    std::wstring joined;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) {
            joined.push_back(k_array_at_quoted_separator);
        }
        joined += fields[i];
    }
    return joined;
}

std::wstring effective_ifs_value() {
    std::map<std::wstring, std::wstring>::const_iterator it = ksh_env.variables.find(L"IFS");
    if (it != ksh_env.variables.end()) {
        return it->second;
    }
    return L" \t\n";
}

std::vector<std::wstring> split_fields_by_ifs(const std::wstring& input) {
    std::vector<std::wstring> fields;
    std::wstring current;
    const std::wstring ifs = effective_ifs_value();

    bool in_quotes = false;
    bool in_single_quotes = false;
    bool in_array_def = false;
    bool escaped = false;
    bool ended_with_internal_separator = false;

    auto is_ifs_delimiter = [&](wchar_t ch) {
        return ifs.find(ch) != std::wstring::npos;
    };

    for (size_t i = 0; i < input.length(); ++i) {
        wchar_t c = input[i];

        if (c == k_array_at_quoted_separator) {
            fields.push_back(current);
            current.clear();
            ended_with_internal_separator = true;
            continue;
        }

        ended_with_internal_separator = false;

        if (escaped) {
            current += c;
            escaped = false;
            continue;
        }

        if (c == L'\\' && !in_single_quotes) {
            current += c;
            escaped = true;
            continue;
        }

        if (c == L'"' && !in_array_def && !in_single_quotes) {
            in_quotes = !in_quotes;
            current += c;
            continue;
        }

        if (c == L'\'' && !in_array_def && !in_quotes) {
            in_single_quotes = !in_single_quotes;
            current += c;
            continue;
        }

        if (c == L'(' && i > 0 && input[i - 1] == L'=') {
            in_array_def = true;
            current += c;
            continue;
        }

        if (c == L')' && in_array_def) {
            in_array_def = false;
            current += c;
            continue;
        }

        if (!in_quotes && !in_single_quotes && !in_array_def && is_ifs_delimiter(c)) {
            if (!current.empty()) {
                fields.push_back(current);
                current.clear();
            }
            continue;
        }

        current += c;
    }

    if (!current.empty() || ended_with_internal_separator) {
        fields.push_back(current);
    }

    return fields;
}

std::wstring remove_quotes_and_escapes_from_token(const std::wstring& token) {
    std::wstring result;
    bool in_double_quotes = false;
    bool in_single_quotes = false;

    for (size_t i = 0; i < token.size(); ++i) {
        wchar_t ch = token[i];

        if (ch == L'"' && !in_single_quotes) {
            in_double_quotes = !in_double_quotes;
            continue;
        }

        if (ch == L'\'' && !in_double_quotes) {
            in_single_quotes = !in_single_quotes;
            continue;
        }

        if (ch == L'\\' && !in_single_quotes) {
            if (i + 1 >= token.size()) {
                result += ch;
                continue;
            }

            wchar_t next = token[i + 1];
            if (!in_double_quotes) {
                if (std::iswspace(next) || next == L'\\' || next == L'"' || next == L'\'' || next == L'$' ||
                    next == L'`' || next == L'|' || next == L'&' || next == L';' || next == L'<' ||
                    next == L'>' || next == L'(' || next == L')') {
                    result += next;
                    i++;
                    continue;
                }

                result += ch;
                continue;
            }

            if (next == L'\\' || next == L'"' || next == L'$' || next == L'`' || next == L'\n') {
                result += next;
                i++;
                continue;
            }

            result += ch;
            continue;
        }

        result += ch;
    }

    return result;
}

std::vector<std::wstring> remove_quotes_and_escapes_from_tokens(const std::vector<std::wstring>& tokens) {
    std::vector<std::wstring> stripped;
    stripped.reserve(tokens.size());
    for (const std::wstring& token : tokens) {
        stripped.push_back(remove_quotes_and_escapes_from_token(token));
    }
    return stripped;
}

std::wstring expand_tilde(const std::wstring& input) {
    if (input.empty() || input[0] != L'~') {
        return input;
    }
    
    size_t slash_pos = input.find_first_of(L"/\\");
    std::wstring username = (slash_pos == std::wstring::npos) ? input.substr(1) : input.substr(1, slash_pos - 1);
    std::wstring rest = (slash_pos == std::wstring::npos) ? L"" : input.substr(slash_pos);
    
    std::wstring home_dir;
    if (username.empty()) {
        wchar_t user_profile[MAX_PATH];
        if (GetEnvironmentVariableW(L"USERPROFILE", user_profile, MAX_PATH) > 0) {
            home_dir = user_profile;
        } else if (GetEnvironmentVariableW(L"HOME", user_profile, MAX_PATH) > 0) {
            home_dir = user_profile;
        } else {
            home_dir = L"C:\\";
        }
    } else {
        std::wstring target = L"C:\\Users\\" + username;
        DWORD attr = GetFileAttributesW(target.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
            home_dir = target;
        } else {
            return input;
        }
    }
    return home_dir + rest;
}

std::vector<std::wstring> expand_tilde_for_tokens(const std::vector<std::wstring>& tokens) {
    std::vector<std::wstring> res;
    res.reserve(tokens.size());
    for (const std::wstring& t : tokens) {
        res.push_back(expand_tilde(t));
    }
    return res;
}

std::wstring ksh_expand(const std::wstring& input) {
    g_expansion_error = false;
    std::wstring tilde_expanded = expand_tilde(input);
    std::wstring expanded = expand_substitutions_left_to_right(tilde_expanded);
    if (g_expansion_error) {
        return L"";
    }

    std::vector<std::wstring> fields = split_fields_by_ifs(expanded);
    fields = expand_globs_for_tokens(fields, fields);
    std::vector<std::wstring> stripped = remove_quotes_and_escapes_from_tokens(fields);
    return join_fields_with_internal_separator(stripped);
}

std::wstring expand_ansi_c_quoting(const std::wstring& input, size_t& i) {
    std::wstring result;
    // input[i] is '$', input[i+1] is '\''
    i += 2; // consume '$' and '\''
    while (i < input.length()) {
        wchar_t c = input[i];
        if (c == L'\'') {
            i++; // consume closing '\''
            break;
        }
        if (c == L'\\') {
            i++; // consume '\\'
            if (i >= input.length()) {
                result += L'\\';
                break;
            }
            wchar_t escape_char = input[i];
            if (escape_char == L'a') {
                result += L'\a';
                i++;
            } else if (escape_char == L'b') {
                result += L'\b';
                i++;
            } else if (escape_char == L'e' || escape_char == L'E') {
                result += L'\x1b';
                i++;
            } else if (escape_char == L'f') {
                result += L'\f';
                i++;
            } else if (escape_char == L'n') {
                result += L'\n';
                i++;
            } else if (escape_char == L'r') {
                result += L'\r';
                i++;
            } else if (escape_char == L't') {
                result += L'\t';
                i++;
            } else if (escape_char == L'v') {
                result += L'\v';
                i++;
            } else if (escape_char == L'\\') {
                result += L'\\';
                i++;
            } else if (escape_char == L'\'') {
                result += L'\'';
                i++;
            } else if (escape_char == L'"') {
                result += L'"';
                i++;
            } else if (escape_char == L'?') {
                result += L'?';
                i++;
            } else if (escape_char == L'x') {
                i++; // consume 'x'
                std::wstring hex_str;
                for (int count = 0; count < 2 && i < input.length(); ++count) {
                    if (std::iswxdigit(input[i])) {
                        hex_str.push_back(input[i]);
                        i++;
                    } else {
                        break;
                    }
                }
                if (!hex_str.empty()) {
                    unsigned long val = 0;
                    for (wchar_t digit : hex_str) {
                        val *= 16;
                        if (digit >= L'0' && digit <= L'9') val += (digit - L'0');
                        else if (digit >= L'a' && digit <= L'f') val += (digit - L'a' + 10);
                        else if (digit >= L'A' && digit <= L'F') val += (digit - L'A' + 10);
                    }
                    result += static_cast<wchar_t>(val);
                } else {
                    result += L'x';
                }
            } else if (escape_char == L'u') {
                i++; // consume 'u'
                std::wstring hex_str;
                for (int count = 0; count < 4 && i < input.length(); ++count) {
                    if (std::iswxdigit(input[i])) {
                        hex_str.push_back(input[i]);
                        i++;
                    } else {
                        break;
                    }
                }
                if (!hex_str.empty()) {
                    unsigned long val = 0;
                    for (wchar_t digit : hex_str) {
                        val *= 16;
                        if (digit >= L'0' && digit <= L'9') val += (digit - L'0');
                        else if (digit >= L'a' && digit <= L'f') val += (digit - L'a' + 10);
                        else if (digit >= L'A' && digit <= L'F') val += (digit - L'A' + 10);
                    }
                    result += static_cast<wchar_t>(val);
                } else {
                    result += L'u';
                }
            } else if (escape_char == L'U') {
                i++; // consume 'U'
                std::wstring hex_str;
                for (int count = 0; count < 8 && i < input.length(); ++count) {
                    if (std::iswxdigit(input[i])) {
                        hex_str.push_back(input[i]);
                        i++;
                    } else {
                        break;
                    }
                }
                if (!hex_str.empty()) {
                    unsigned long val = 0;
                    for (wchar_t digit : hex_str) {
                        val *= 16;
                        if (digit >= L'0' && digit <= L'9') val += (digit - L'0');
                        else if (digit >= L'a' && digit <= L'f') val += (digit - L'a' + 10);
                        else if (digit >= L'A' && digit <= L'F') val += (digit - L'A' + 10);
                    }
                    result += static_cast<wchar_t>(val);
                } else {
                    result += L'U';
                }
            } else if (escape_char == L'c') {
                i++; // consume 'c'
                if (i < input.length()) {
                    wchar_t ctrl_char = input[i];
                    result += static_cast<wchar_t>(ctrl_char & 0x1F);
                    i++;
                } else {
                    result += L'c';
                }
            } else if (escape_char >= L'0' && escape_char <= L'7') {
                std::wstring oct_str;
                for (int count = 0; count < 3 && i < input.length(); ++count) {
                    if (input[i] >= L'0' && input[i] <= L'7') {
                        oct_str.push_back(input[i]);
                        i++;
                    } else {
                        break;
                    }
                }
                unsigned long val = 0;
                for (wchar_t digit : oct_str) {
                    val = val * 8 + (digit - L'0');
                }
                result += static_cast<wchar_t>(val);
            } else {
                result += escape_char;
                i++;
            }
        } else {
            result += c;
            i++;
        }
    }
    return result;
}

std::wstring escape_for_double_quotes(const std::wstring& str) {
    std::wstring result;
    result += L'"';
    for (wchar_t ch : str) {
        if (ch == L'\\' || ch == L'"' || ch == L'$' || ch == L'`' || ch == L'\n') {
            result += L'\\';
        }
        result += ch;
    }
    result += L'"';
    return result;
}

// Advanced ksh93 Tokenizer handling quotes and compound array blocks
std::vector<std::wstring> ksh_tokenize(const std::wstring& input) {
    std::vector<std::wstring> tokens;
    std::wstring current;
    bool in_quotes = false;
    bool in_single_quotes = false;
    bool in_array_def = false;
    bool escaped = false;

    for (size_t i = 0; i < input.length(); ) {
        wchar_t c = input[i];

        if (c == k_array_at_quoted_separator) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            i++;
            continue;
        }

        if (escaped) {
            current += c;
            escaped = false;
            i++;
            continue;
        }

        if (c == L'\\' && !in_single_quotes) {
            current += c;
            escaped = true;
            i++;
            continue;
        }

        if (c == L'$' && i + 1 < input.length() && input[i + 1] == L'\'' && !in_quotes && !in_single_quotes && !in_array_def) {
            current += expand_ansi_c_quoting(input, i);
            continue;
        }

        if (c == L'"' && !in_array_def && !in_single_quotes) {
            in_quotes = !in_quotes;
            i++;
        } else if (c == L'\'' && !in_array_def && !in_quotes) {
            in_single_quotes = !in_single_quotes;
            i++;
        } else if (c == L'(' && i > 0 && input[i-1] == L'=') {
            in_array_def = true;
            current += c;
            i++;
        } else if (c == L')' && in_array_def) {
            in_array_def = false;
            current += c;
            i++;
        } else if ((c == L' ' || c == L'\t') && !in_quotes && !in_single_quotes && !in_array_def) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            i++;
        } else {
            current += c;
            i++;
        }
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

std::vector<std::wstring> ksh_tokenize_preserve_quotes(const std::wstring& input) {
    std::vector<std::wstring> tokens;
    std::wstring current;
    bool in_quotes = false;
    bool in_single_quotes = false;
    bool in_array_def = false;
    bool escaped = false;

    for (size_t i = 0; i < input.length(); ) {
        wchar_t c = input[i];

        if (c == k_array_at_quoted_separator) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            i++;
            continue;
        }

        if (escaped) {
            current += c;
            escaped = false;
            i++;
            continue;
        }

        if (c == L'\\' && !in_single_quotes) {
            current += c;
            escaped = true;
            i++;
            continue;
        }

        if (c == L'$' && i + 1 < input.length() && input[i + 1] == L'\'' && !in_quotes && !in_single_quotes && !in_array_def) {
            current += escape_for_double_quotes(expand_ansi_c_quoting(input, i));
            continue;
        }

        if (c == L'"' && !in_array_def && !in_single_quotes) {
            in_quotes = !in_quotes;
            current += c;
            i++;
            continue;
        }

        if (c == L'\'' && !in_array_def && !in_quotes) {
            in_single_quotes = !in_single_quotes;
            current += c;
            i++;
            continue;
        }

        if (c == L'(' && i > 0 && input[i - 1] == L'=') {
            in_array_def = true;
            current += c;
            i++;
            continue;
        }

        if (c == L')' && in_array_def) {
            in_array_def = false;
            current += c;
            i++;
            continue;
        }

        if ((c == L' ' || c == L'\t') && !in_quotes && !in_single_quotes && !in_array_def) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            i++;
            continue;
        }

        current += c;
        i++;
    }

    if (!current.empty()) {
        tokens.push_back(current);
    }

    return tokens;
}

bool is_path_separator(wchar_t ch) {
    return ch == L'/' || ch == L'\\';
}

bool has_glob_metacharacters(const std::wstring& value) {
    if (value.find_first_of(L"*?[") != std::wstring::npos) {
        return true;
    }
    return value.find(L"+(") != std::wstring::npos ||
           value.find(L"@(") != std::wstring::npos ||
           value.find(L"!(") != std::wstring::npos;
}

bool has_unquoted_glob_metacharacters(const std::wstring& token) {
    bool in_double_quotes = false;
    bool in_single_quotes = false;
    bool escaped = false;

    for (size_t i = 0; i < token.size(); ++i) {
        wchar_t ch = token[i];
        if (escaped) {
            escaped = false;
            continue;
        }

        if (ch == L'\\' && !in_single_quotes) {
            escaped = true;
            continue;
        }

        if (ch == L'"' && !in_single_quotes) {
            in_double_quotes = !in_double_quotes;
            continue;
        }

        if (ch == L'\'' && !in_double_quotes) {
            in_single_quotes = !in_single_quotes;
            continue;
        }

        if (!in_double_quotes && !in_single_quotes) {
            if (ch == L'*' || ch == L'?' || ch == L'[') {
                return true;
            }
            if (ch == L'+' || ch == L'@' || ch == L'!') {
                if (i + 1 < token.size() && token[i + 1] == L'(') {
                    return true;
                }
            }
        }
    }

    return false;
}

bool match_glob_character_class(const std::wstring& pattern, size_t class_start, wchar_t candidate, size_t& consumed_length, bool case_sensitive) {
    consumed_length = 0;
    if (class_start >= pattern.size() || pattern[class_start] != L'[') {
        return false;
    }

    size_t i = class_start + 1;
    bool negate = false;
    if (i < pattern.size() && (pattern[i] == L'!' || pattern[i] == L'^')) {
        negate = true;
        i++;
    }

    bool matched = false;
    bool saw_item = false;
    while (i < pattern.size() && pattern[i] != L']') {
        wchar_t left = pattern[i];
        if ((i + 2) < pattern.size() && pattern[i + 1] == L'-' && pattern[i + 2] != L']') {
            wchar_t right = pattern[i + 2];
            if (case_sensitive) {
                if (left <= candidate && candidate <= right) {
                    matched = true;
                }
            } else {
                wchar_t lower_candidate = static_cast<wchar_t>(std::towlower(candidate));
                wchar_t lower_left = static_cast<wchar_t>(std::towlower(left));
                wchar_t lower_right = static_cast<wchar_t>(std::towlower(right));
                if (lower_left <= lower_candidate && lower_candidate <= lower_right) {
                    matched = true;
                }
            }
            i += 3;
            saw_item = true;
            continue;
        }

        if (case_sensitive ? (left == candidate) : (std::towlower(left) == std::towlower(candidate))) {
            matched = true;
        }
        i++;
        saw_item = true;
    }

    if (i >= pattern.size() || pattern[i] != L']' || !saw_item) {
        return false;
    }

    consumed_length = (i - class_start) + 1;
    return negate ? !matched : matched;
}

bool parse_extended_glob(const std::wstring& pattern, size_t ext_start, size_t& ext_end, std::vector<std::wstring>& sub_patterns) {
    if (ext_start + 2 >= pattern.size() || pattern[ext_start + 1] != L'(') {
        return false;
    }

    size_t i = ext_start + 2;
    int paren_level = 1;
    std::wstring current;
    std::vector<std::wstring> parts;

    bool in_double_quotes = false;
    bool in_single_quotes = false;
    bool escaped = false;

    while (i < pattern.size()) {
        wchar_t ch = pattern[i];

        if (escaped) {
            current += L'\\';
            current += ch;
            escaped = false;
            i++;
            continue;
        }

        if (ch == L'\\' && !in_single_quotes) {
            escaped = true;
            i++;
            continue;
        }

        if (ch == L'"' && !in_single_quotes) {
            in_double_quotes = !in_double_quotes;
            current += ch;
            i++;
            continue;
        }

        if (ch == L'\'' && !in_double_quotes) {
            in_single_quotes = !in_single_quotes;
            current += ch;
            i++;
            continue;
        }

        if (!in_double_quotes && !in_single_quotes) {
            if (ch == L'(') {
                paren_level++;
            } else if (ch == L')') {
                paren_level--;
                if (paren_level == 0) {
                    parts.push_back(current);
                    ext_end = i;
                    sub_patterns = parts;
                    return true;
                }
            } else if (ch == L'|' && paren_level == 1) {
                parts.push_back(current);
                current.clear();
                i++;
                continue;
            }
        }

        current += ch;
        i++;
    }

    return false;
}

bool match_glob_pattern_recursive(
    const std::wstring& pattern,
    size_t pattern_index,
    const std::wstring& candidate,
    size_t candidate_index,
    bool case_sensitive,
    unsigned long long& step_budget) {
    if (step_budget == 0) {
        return false;
    }
    step_budget--;

    while (pattern_index < pattern.size()) {
        wchar_t p = pattern[pattern_index];

        bool is_ext_glob = false;
        if (pattern_index + 1 < pattern.size() && pattern[pattern_index + 1] == L'(') {
            if (p == L'+' || p == L'*' || p == L'?' || p == L'@' || p == L'!') {
                is_ext_glob = true;
            }
        }

        if (is_ext_glob) {
            size_t ext_end = 0;
            std::vector<std::wstring> sub_patterns;
            if (parse_extended_glob(pattern, pattern_index, ext_end, sub_patterns)) {
                std::wstring remaining_pattern = pattern.substr(ext_end + 1);
                wchar_t op = p;

                if (op == L'?') {
                    if (match_glob_pattern_recursive(remaining_pattern, 0, candidate, candidate_index, case_sensitive, step_budget)) {
                        return true;
                    }
                    for (const auto& sub : sub_patterns) {
                        std::wstring temp = sub + remaining_pattern;
                        if (match_glob_pattern_recursive(temp, 0, candidate, candidate_index, case_sensitive, step_budget)) {
                            return true;
                        }
                    }
                    return false;
                }

                if (op == L'@') {
                    for (const auto& sub : sub_patterns) {
                        std::wstring temp = sub + remaining_pattern;
                        if (match_glob_pattern_recursive(temp, 0, candidate, candidate_index, case_sensitive, step_budget)) {
                            return true;
                        }
                    }
                    return false;
                }

                if (op == L'*') {
                    if (match_glob_pattern_recursive(remaining_pattern, 0, candidate, candidate_index, case_sensitive, step_budget)) {
                        return true;
                    }
                    std::wstring ext_glob_part = pattern.substr(pattern_index, ext_end - pattern_index + 1);
                    for (const auto& sub : sub_patterns) {
                        std::wstring temp = sub + ext_glob_part + remaining_pattern;
                        if (match_glob_pattern_recursive(temp, 0, candidate, candidate_index, case_sensitive, step_budget)) {
                            return true;
                        }
                    }
                    return false;
                }

                if (op == L'+') {
                    std::wstring star_glob_part = L"*" + pattern.substr(pattern_index + 1, ext_end - pattern_index);
                    for (const auto& sub : sub_patterns) {
                        std::wstring temp = sub + star_glob_part + remaining_pattern;
                        if (match_glob_pattern_recursive(temp, 0, candidate, candidate_index, case_sensitive, step_budget)) {
                            return true;
                        }
                    }
                    return false;
                }

                if (op == L'!') {
                    size_t max_L = candidate.size() - candidate_index;
                    for (size_t L = 0; L <= max_L; ++L) {
                        std::wstring candidate_prefix = candidate.substr(candidate_index, L);
                        bool matches_any = false;
                        for (const auto& sub : sub_patterns) {
                            unsigned long long local_budget = step_budget;
                            if (match_glob_pattern_recursive(sub, 0, candidate_prefix, 0, case_sensitive, local_budget)) {
                                matches_any = true;
                                break;
                            }
                        }

                        if (!matches_any) {
                            if (match_glob_pattern_recursive(remaining_pattern, 0, candidate, candidate_index + L, case_sensitive, step_budget)) {
                                return true;
                            }
                        }
                    }
                    return false;
                }
            }
        }

        if (p == L'*') {
            while (pattern_index < pattern.size() && pattern[pattern_index] == L'*') {
                pattern_index++;
            }
            if (pattern_index == pattern.size()) {
                return true;
            }
            while (candidate_index <= candidate.size()) {
                if (match_glob_pattern_recursive(pattern, pattern_index, candidate, candidate_index, case_sensitive, step_budget)) {
                    return true;
                }
                if (candidate_index == candidate.size()) {
                    break;
                }
                candidate_index++;
            }
            return false;
        }

        if (candidate_index >= candidate.size()) {
            return false;
        }

        if (p == L'?') {
            pattern_index++;
            candidate_index++;
            continue;
        }

        if (p == L'[') {
            size_t consumed = 0;
            if (!match_glob_character_class(pattern, pattern_index, candidate[candidate_index], consumed, case_sensitive)) {
                return false;
            }
            pattern_index += consumed;
            candidate_index++;
            continue;
        }

        if (case_sensitive ? (p != candidate[candidate_index]) : (std::towlower(p) != std::towlower(candidate[candidate_index]))) {
            return false;
        }
        pattern_index++;
        candidate_index++;
    }

    return candidate_index == candidate.size();
}

bool match_glob_pattern(const std::wstring& pattern, const std::wstring& candidate) {
    if (!candidate.empty() && candidate[0] == L'.' && (pattern.empty() || pattern[0] != L'.')) {
        return false;
    }

    const bool case_sensitive = pattern_match_case_sensitive();
    unsigned long long step_budget = kMaxPatternMatchSteps;
    return match_glob_pattern_recursive(pattern, 0, candidate, 0, case_sensitive, step_budget);
}

std::wstring join_path_for_glob(const std::wstring& base, const std::wstring& name) {
    if (base.empty()) {
        return name;
    }
    if (base == L".") {
        return L".\\" + name;
    }
    if (base.back() == L'\\' || base.back() == L'/') {
        return base + name;
    }
    return base + L"\\" + name;
}

std::wstring normalize_relative_glob_result(const std::wstring& value) {
    if (value.rfind(L".\\", 0) == 0) {
        return value.substr(2);
    }
    return value;
}

void expand_glob_token(const std::wstring& pattern, std::vector<std::wstring>& matches) {
    matches.clear();

    std::wstring normalized = pattern;
    std::replace(normalized.begin(), normalized.end(), L'/', L'\\');

    std::vector<std::wstring> segments;
    std::wstring current_segment;
    for (wchar_t ch : normalized) {
        if (is_path_separator(ch)) {
            if (!current_segment.empty()) {
                segments.push_back(current_segment);
                current_segment.clear();
            }
            continue;
        }
        current_segment += ch;
    }
    if (!current_segment.empty()) {
        segments.push_back(current_segment);
    }

    if (segments.empty()) {
        return;
    }

    bool absolute_drive = normalized.size() >= 2 && std::iswalpha(normalized[0]) && normalized[1] == L':';
    bool rooted = !normalized.empty() && is_path_separator(normalized[0]);

    std::vector<std::wstring> candidates;
    if (absolute_drive) {
        std::wstring base;
        base += normalized[0];
        base += L':';
        base += L'\\';
        candidates.push_back(base);
    } else if (rooted) {
        candidates.push_back(L"\\");
    } else {
        candidates.push_back(L".");
    }

    for (size_t segment_index = 0; segment_index < segments.size(); ++segment_index) {
        const std::wstring& segment = segments[segment_index];
        const bool needs_glob = has_glob_metacharacters(segment);
        const bool is_last = segment_index + 1 == segments.size();

        std::vector<std::wstring> next_candidates;
        for (const std::wstring& base : candidates) {
            if (!needs_glob) {
                std::wstring next_path = join_path_for_glob(base, segment);
                DWORD attrs = GetFileAttributesW(next_path.c_str());
                if (attrs == INVALID_FILE_ATTRIBUTES) {
                    continue;
                }
                if (!is_last && ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0)) {
                    continue;
                }
                next_candidates.push_back(next_path);
                continue;
            }

            std::wstring probe = join_path_for_glob(base, L"*");
            WIN32_FIND_DATAW find_data;
            HANDLE handle = FindFirstFileW(probe.c_str(), &find_data);
            if (handle == INVALID_HANDLE_VALUE) {
                continue;
            }

            do {
                std::wstring name = find_data.cFileName;
                if (name == L"." || name == L"..") {
                    continue;
                }
                if (!match_glob_pattern(segment, name)) {
                    continue;
                }
                if (!is_last && ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)) {
                    continue;
                }

                next_candidates.push_back(join_path_for_glob(base, name));
            } while (FindNextFileW(handle, &find_data));

            FindClose(handle);
        }

        candidates.swap(next_candidates);
        if (candidates.empty()) {
            return;
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const std::wstring& left, const std::wstring& right) {
        return _wcsicmp(left.c_str(), right.c_str()) < 0;
    });

    for (const std::wstring& candidate : candidates) {
        matches.push_back(normalize_relative_glob_result(candidate));
    }
}

std::vector<std::wstring> expand_globs_for_tokens(const std::vector<std::wstring>& tokens, const std::vector<std::wstring>& preserved_tokens) {
    std::vector<std::wstring> expanded;
    expanded.reserve(tokens.size());

    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::wstring& token = tokens[i];
        const std::wstring& preserved = (i < preserved_tokens.size()) ? preserved_tokens[i] : token;

        if (!has_unquoted_glob_metacharacters(preserved) || !has_glob_metacharacters(token)) {
            expanded.push_back(token);
            continue;
        }

        std::vector<std::wstring> matches;
        expand_glob_token(token, matches);
        if (matches.empty()) {
            // Keep unmatched wildcard tokens literal for compatibility.
            expanded.push_back(token);
            continue;
        }

        expanded.insert(expanded.end(), std::make_move_iterator(matches.begin()), std::make_move_iterator(matches.end()));
    }

    return expanded;
}

bool is_valid_shell_identifier(const std::wstring& name) {
    if (name.empty()) {
        return false;
    }
    if (name[0] == L'.') {
        if (name.size() < 2) return false;
        return std::all_of(name.begin() + 1, name.end(), [](wchar_t ch) {
            return std::iswalnum(ch) != 0 || ch == L'_' || ch == L'.';
        });
    }
    if (!(std::iswalpha(name[0]) || name[0] == L'_')) {
        return false;
    }
    return std::all_of(name.begin(), name.end(), [](wchar_t ch) {
        return std::iswalnum(ch) != 0 || ch == L'_' || ch == L'.';
    });
}

bool parse_array_reference_expression(const std::wstring& expression, std::wstring& base_name, std::wstring& index_text, bool& has_index) {
    base_name.clear();
    index_text.clear();
    has_index = false;

    size_t open = expression.find(L'[');
    if (open == std::wstring::npos) {
        base_name = expression;
        return is_valid_shell_identifier(base_name);
    }

    if (open == 0) {
        return false;
    }

    int depth = 0;
    size_t match_pos = std::wstring::npos;
    for (size_t i = open; i < expression.size(); ++i) {
        if (expression[i] == L'[') {
            depth++;
        } else if (expression[i] == L']') {
            depth--;
            if (depth == 0) {
                match_pos = i;
                break;
            }
        }
    }

    if (match_pos == std::wstring::npos || match_pos != expression.size() - 1) {
        return false;
    }

    base_name = expression.substr(0, open);
    if (!is_valid_shell_identifier(base_name)) {
        return false;
    }

    index_text = expression.substr(open + 1, expression.size() - open - 2);
    if (index_text.empty()) {
        return false;
    }

    has_index = true;
    return true;
}

bool parse_non_negative_index(const std::wstring& text, size_t& out_index) {
    if (text.empty()) {
        return false;
    }

    try {
        size_t consumed = 0;
        unsigned long long parsed = std::stoull(text, &consumed);
        if (consumed != text.size()) {
            return false;
        }
        out_index = static_cast<size_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_array_literal(const std::wstring& value, std::vector<std::wstring>& elements) {
    elements.clear();
    std::wstring trimmed = trim_copy(value);
    if (trimmed.size() < 2 || trimmed.front() != L'(' || trimmed.back() != L')') {
        return false;
    }

    std::wstring inner = trim_copy(trimmed.substr(1, trimmed.size() - 2));
    if (inner.empty()) {
        return true;
    }

    std::vector<std::wstring> tokens = ksh_tokenize(inner);
    size_t next_positional_index = 0;

    for (const std::wstring& token : tokens) {
        if (token.empty()) {
            continue;
        }

        size_t close_bracket = token.find(L']');
        if (token[0] == L'[' && close_bracket != std::wstring::npos && (close_bracket + 1) < token.size() && token[close_bracket + 1] == L'=') {
            std::wstring index_text = token.substr(1, close_bracket - 1);
            size_t explicit_index = 0;
            if (!parse_non_negative_index(index_text, explicit_index)) {
                return false;
            }
            if (explicit_index >= kMaxArrayIndex) {
                return false;
            }

            std::wstring explicit_value = token.substr(close_bracket + 2);
            if (elements.size() <= explicit_index) {
                elements.resize(explicit_index + 1);
            }
            elements[explicit_index] = explicit_value;
            if (next_positional_index <= explicit_index) {
                next_positional_index = explicit_index + 1;
            }
            continue;
        }

        if (elements.size() <= next_positional_index) {
            elements.resize(next_positional_index + 1);
        }
        elements[next_positional_index] = token;
        next_positional_index++;
    }

    return true;
}

std::vector<std::wstring> get_array_values_vector(const std::wstring& name) {
    std::vector<std::wstring> result;
    std::map<std::wstring, std::map<std::wstring, std::wstring>>::const_iterator it = ksh_env.arrays.find(name);
    if (it == ksh_env.arrays.end()) {
        return result;
    }

    const std::map<std::wstring, std::wstring>& m = it->second;
    bool is_assoc = false;
    std::map<std::wstring, bool>::const_iterator assoc_it = ksh_env.associative_flags.find(name);
    if (assoc_it != ksh_env.associative_flags.end()) {
        is_assoc = assoc_it->second;
    }

    if (is_assoc) {
        for (const auto& pair : m) {
            result.push_back(pair.second);
        }
    } else {
        size_t max_idx = 0;
        bool has_any = false;
        std::vector<std::pair<size_t, std::wstring>> parsed;
        for (const auto& pair : m) {
            size_t idx = 0;
            if (parse_non_negative_index(pair.first, idx)) {
                parsed.push_back({idx, pair.second});
                if (idx > max_idx) {
                    max_idx = idx;
                }
                has_any = true;
            }
        }
        if (has_any) {
            result.resize(max_idx + 1);
            for (const auto& p : parsed) {
                result[p.first] = p.second;
            }
        }
    }
    return result;
}

bool parse_associative_array_literal(const std::wstring& value, std::map<std::wstring, std::wstring>& elements) {
    elements.clear();
    std::wstring trimmed = trim_copy(value);
    if (trimmed.size() < 2 || trimmed.front() != L'(' || trimmed.back() != L')') {
        return false;
    }

    std::wstring inner = trim_copy(trimmed.substr(1, trimmed.size() - 2));
    if (inner.empty()) {
        return true;
    }

    std::vector<std::wstring> tokens = ksh_tokenize(inner);

    for (const std::wstring& token : tokens) {
        if (token.empty()) {
            continue;
        }

        size_t close_bracket = token.find(L']');
        if (token[0] == L'[' && close_bracket != std::wstring::npos && (close_bracket + 1) < token.size() && token[close_bracket + 1] == L'=') {
            std::wstring key = token.substr(1, close_bracket - 1);
            std::wstring explicit_value = token.substr(close_bracket + 2);
            elements[key] = explicit_value;
        } else {
            return false;
        }
    }
    return true;
}

void assign_scalar_parameter(const std::wstring& name, const std::wstring& value) {
    ensure_registry_namespaces_registered();
    const size_t namespace_separator = name.find(L'.');
    if (namespace_separator != std::wstring::npos) {
        const std::wstring namespace_name = name.substr(0, namespace_separator);
        std::map<std::wstring, CustomTypeDefinition>::const_iterator namespace_it = g_custom_types.find(namespace_name);
        if (namespace_it != g_custom_types.end() && namespace_it->second.is_dynamic_namespace && namespace_it->second.property_setter) {
            if (!namespace_it->second.property_setter(name, value)) {
                ksh_env.variables[L"?"] = L"1";
            } else {
                ksh_env.variables[L"?"] = L"0";
            }
            return;
        }
    }

    bool exists = ksh_env.variables.find(name) != ksh_env.variables.end();
    if (!exists && g_shell_functions.find(name + L".init") != g_shell_functions.end() &&
        g_active_init_hooks.find(name) == g_active_init_hooks.end()) {
        
        g_active_init_hooks.insert(name);
        bool dummy = false;
        execute_defined_function(name + L".init", {}, dummy);
        g_active_init_hooks.erase(name);
    }

    std::wstring assigned_value = value;
    if (g_shell_functions.find(name + L".set") != g_shell_functions.end() &&
        g_active_set_hooks.find(name) == g_active_set_hooks.end()) {
        
        g_active_set_hooks.insert(name);
        
        std::wstring old_sh_val;
        bool had_sh_val = false;
        auto sh_it = ksh_env.variables.find(L".sh.value");
        if (sh_it != ksh_env.variables.end()) {
            old_sh_val = sh_it->second;
            had_sh_val = true;
        }
        
        ksh_env.variables[L".sh.value"] = value;
        
        bool dummy = false;
        execute_defined_function(name + L".set", {}, dummy);
        
        assigned_value = ksh_env.variables[L".sh.value"];
        
        if (had_sh_val) {
            ksh_env.variables[L".sh.value"] = old_sh_val;
        } else {
            ksh_env.variables.erase(L".sh.value");
        }
        
        g_active_set_hooks.erase(name);
    }

    const bool uppercase_mode = get_flag_value(ksh_env.uppercase_flags, name);
    const bool lowercase_mode = get_flag_value(ksh_env.lowercase_flags, name);
    if (uppercase_mode) {
        assigned_value = to_upper_copy(assigned_value);
    } else if (lowercase_mode) {
        assigned_value = to_lower_copy(assigned_value);
    }

    std::map<std::wstring, wchar_t>::const_iterator justify_mode_it = ksh_env.justify_modes.find(name);
    if (justify_mode_it != ksh_env.justify_modes.end()) {
        std::map<std::wstring, int>::const_iterator justify_width_it = ksh_env.justify_widths.find(name);
        int justify_width = (justify_width_it != ksh_env.justify_widths.end()) ? justify_width_it->second : 0;
        if (justify_width > 0) {
            const size_t width = static_cast<size_t>(justify_width);
            if (justify_mode_it->second == L'L') {
                if (assigned_value.size() > width) {
                    assigned_value = assigned_value.substr(0, width);
                } else if (assigned_value.size() < width) {
                    assigned_value.append(width - assigned_value.size(), L' ');
                }
            } else if (justify_mode_it->second == L'R') {
                if (assigned_value.size() > width) {
                    assigned_value = assigned_value.substr(assigned_value.size() - width);
                } else if (assigned_value.size() < width) {
                    assigned_value.insert(0, width - assigned_value.size(), L' ');
                }
            } else if (justify_mode_it->second == L'Z') {
                if (assigned_value.size() > width) {
                    assigned_value = assigned_value.substr(assigned_value.size() - width);
                } else if (assigned_value.size() < width) {
                    size_t pad_count = width - assigned_value.size();
                    if (!assigned_value.empty() && (assigned_value[0] == L'+' || assigned_value[0] == L'-')) {
                        assigned_value.insert(1, pad_count, L'0');
                    } else {
                        assigned_value.insert(0, pad_count, L'0');
                    }
                }
            }
        }
    }

    ksh_env.variables[name] = assigned_value;
    ksh_env.registry_metadata.erase(name);
    ksh_env.arrays.erase(name);
    ksh_env.associative_flags.erase(name);
    SetEnvironmentVariableW(name.c_str(), assigned_value.c_str());
}

void assign_array_parameter(const std::wstring& name, const std::vector<std::wstring>& values) {
    std::map<std::wstring, std::wstring> map_values;
    for (size_t i = 0; i < values.size(); ++i) {
        if (!values[i].empty()) {
            map_values[std::to_wstring(i)] = values[i];
        }
    }
    ksh_env.arrays[name] = std::move(map_values);
    ksh_env.variables.erase(name);
    ksh_env.registry_metadata.erase(name);
    ksh_env.justify_modes.erase(name);
    ksh_env.justify_widths.erase(name);
    SetEnvironmentVariableW(name.c_str(), nullptr);
}

bool try_read_environment_variable(const std::wstring& name, std::wstring& value) {
    value.clear();
    DWORD required = GetEnvironmentVariableW(name.c_str(), nullptr, 0);
    if (required == 0) {
        return false;
    }

    std::vector<wchar_t> buffer(required);
    DWORD written = GetEnvironmentVariableW(name.c_str(), buffer.data(), required);
    if (written == 0 || written >= required) {
        return false;
    }

    value.assign(buffer.data(), written);
    return true;
}

std::wstring resolve_variable_name(const std::wstring& name, int depth) {
    if (name.empty() || depth > 10) {
        return name;
    }

    std::wstring base_name;
    std::wstring index_text;
    bool has_index = false;
    if (parse_array_reference_expression(name, base_name, index_text, has_index)) {
        if (has_index) {
            std::wstring resolved_base = resolve_variable_name(base_name, depth + 1);
            return resolved_base + L"[" + index_text + L"]";
        }
    }

    std::map<std::wstring, bool>::const_iterator nameref_it = ksh_env.nameref_flags.find(name);
    if (nameref_it != ksh_env.nameref_flags.end() && nameref_it->second) {
        std::map<std::wstring, std::wstring>::const_iterator val_it = ksh_env.variables.find(name);
        if (val_it != ksh_env.variables.end()) {
            return resolve_variable_name(val_it->second, depth + 1);
        }
    }

    return name;
}

int count_unquoted_brace_delta(const std::wstring& line) {
    int delta = 0;
    bool in_single_quotes = false;
    bool in_double_quotes = false;
    bool escaped = false;

    for (wchar_t ch : line) {
        if (escaped) {
            escaped = false;
            continue;
        }

        if (ch == L'\\' && !in_single_quotes) {
            escaped = true;
            continue;
        }

        if (ch == L'\'' && !in_double_quotes) {
            in_single_quotes = !in_single_quotes;
            continue;
        }

        if (ch == L'"' && !in_single_quotes) {
            in_double_quotes = !in_double_quotes;
            continue;
        }

        if (in_single_quotes || in_double_quotes) {
            continue;
        }

        if (ch == L'#') {
            break;
        }

        if (ch == L'{') {
            delta++;
        } else if (ch == L'}') {
            delta--;
        }
    }

    return delta;
}

bool parse_function_header(const std::wstring& trimmed_line, std::wstring& function_name, bool& has_open_brace) {
    function_name.clear();
    has_open_brace = false;

    if (trimmed_line.empty() || trimmed_line.size() > kMaxFunctionHeaderLineLength) {
        return false;
    }

    std::wstring_view sv(trimmed_line);
    size_t pos = 0;
    while (pos < sv.size() && (sv[pos] == L' ' || sv[pos] == L'\t')) pos++;
    if (pos >= sv.size()) return false;

    bool keyword_mode = false;
    if (sv.size() - pos >= 8 && starts_with_case_insensitive(sv.substr(pos), L"function")) {
        size_t after_fn = pos + 8;
        if (after_fn < sv.size() && (sv[after_fn] == L' ' || sv[after_fn] == L'\t')) {
            keyword_mode = true;
            pos = after_fn;
            while (pos < sv.size() && (sv[pos] == L' ' || sv[pos] == L'\t')) pos++;
        }
    }

    if (pos >= sv.size()) return false;

    wchar_t first_char = sv[pos];
    if (!(std::iswalpha(first_char) || first_char == L'_')) {
        return false;
    }

    size_t ident_start = pos;
    pos++;
    while (pos < sv.size()) {
        wchar_t ch = sv[pos];
        if (std::iswalnum(ch) || ch == L'_' || ch == L'.') {
            pos++;
        } else {
            break;
        }
    }
    std::wstring_view ident_sv = sv.substr(ident_start, pos - ident_start);

    while (pos < sv.size() && (sv[pos] == L' ' || sv[pos] == L'\t')) pos++;

    bool has_parens = false;
    if (pos < sv.size() && sv[pos] == L'(') {
        pos++;
        while (pos < sv.size() && (sv[pos] == L' ' || sv[pos] == L'\t')) pos++;
        if (pos >= sv.size() || sv[pos] != L')') {
            return false;
        }
        pos++;
        has_parens = true;
        while (pos < sv.size() && (sv[pos] == L' ' || sv[pos] == L'\t')) pos++;
    }

    if (!keyword_mode && !has_parens) {
        return false;
    }

    if (pos < sv.size() && sv[pos] == L'{') {
        has_open_brace = true;
        pos++;
        while (pos < sv.size() && (sv[pos] == L' ' || sv[pos] == L'\t')) pos++;
    }

    if (pos < sv.size()) {
        return false;
    }

    function_name = std::wstring(ident_sv);
    return true;
}

bool capture_function_definition(
    const std::vector<std::wstring>& lines,
    size_t& line_index,
    size_t end_index,
    std::wstring& error_message,
    size_t& error_line_index) {
    error_message.clear();
    error_line_index = line_index;

    std::wstring function_name;
    bool has_open_brace = false;
    const std::wstring trimmed = trim_copy(lines[line_index]);
    if (!parse_function_header(trimmed, function_name, has_open_brace)) {
        return false;
    }

    size_t body_start = line_index + 1;
    if (!has_open_brace) {
        size_t probe = line_index + 1;
        bool found_open = false;
        while (probe < end_index) {
            std::wstring probe_trimmed = trim_copy(lines[probe]);
            if (probe_trimmed.empty() || probe_trimmed[0] == L'#') {
                probe++;
                continue;
            }

            if (probe_trimmed == L"{") {
                found_open = true;
                body_start = probe + 1;
                break;
            }

            error_message = L"expected '{' to start function body";
            error_line_index = probe;
            return false;
        }

        if (!found_open) {
            error_message = L"missing '{' for function body";
            error_line_index = line_index;
            return false;
        }
    }

    std::vector<std::wstring> body_lines;
    int brace_depth = 1;
    size_t probe = body_start;
    bool found_close = false;

    while (probe < end_index) {
        const std::wstring current_line = lines[probe];
        const std::wstring current_trimmed = trim_copy(current_line);
        const int delta = count_unquoted_brace_delta(current_line);

        if (brace_depth == 1 && current_trimmed == L"}" && delta == -1) {
            brace_depth = 0;
            found_close = true;
            break;
        }

        body_lines.push_back(current_line);
        brace_depth += delta;
        if (brace_depth < 0) {
            error_message = L"unexpected '}' while parsing function";
            error_line_index = probe;
            return false;
        }

        if (brace_depth == 0) {
            body_lines.pop_back();
            found_close = true;
            break;
        }

        probe++;
    }

    if (!found_close) {
        error_message = L"missing '}' for function body";
        error_line_index = line_index;
        return false;
    }

    ShellFunctionDefinition definition;
    definition.body_lines = body_lines;

    // Enforce per-session function table cap (new entries only).
    if (g_shell_functions.find(function_name) == g_shell_functions.end() &&
        g_shell_functions.size() >= kMaxShellFunctions) {
        std::wcerr << L"ksh: function table limit reached, cannot define: " << function_name << L"\n";
        return false;
    }
    g_shell_functions[function_name] = definition;

    line_index = probe + 1;
    return true;
}

bool assign_parameter_value(const std::wstring& lhs, const std::wstring& rhs, bool array_hint, bool local_scope, std::wstring& error_message, bool is_nameref) {
    error_message.clear();

    if (is_nameref) {
        std::wstring base_name;
        std::wstring index_text;
        bool has_index = false;
        if (!parse_array_reference_expression(lhs, base_name, index_text, has_index)) {
            error_message = L"invalid assignment target: " + lhs;
            return false;
        }

        if (has_index) {
            error_message = L"cannot define array element as a nameref: " + lhs;
            return false;
        }

        if (local_scope) {
            snapshot_local_variable_if_needed(base_name);
        }

        if (get_flag_value(ksh_env.readonly_flags, base_name)) {
            error_message = L"variable is read-only: " + base_name;
            return false;
        }

        ksh_env.variables[base_name] = rhs;
        ksh_env.arrays.erase(base_name);
        ksh_env.associative_flags.erase(base_name);
        set_flag_value(ksh_env.nameref_flags, base_name, true);
        return true;
    }

    std::wstring resolved_lhs = resolve_variable_name(lhs);

    std::wstring base_name;
    std::wstring index_text;
    bool has_index = false;
    if (!parse_array_reference_expression(resolved_lhs, base_name, index_text, has_index)) {
        error_message = L"invalid assignment target: " + lhs;
        return false;
    }

    if (!has_index) {
        ensure_registry_namespaces_registered();
        const size_t namespace_separator = base_name.find(L'.');
        if (namespace_separator != std::wstring::npos) {
            const std::wstring namespace_name = base_name.substr(0, namespace_separator);
            std::map<std::wstring, CustomTypeDefinition>::const_iterator namespace_it = g_custom_types.find(namespace_name);
            if (namespace_it != g_custom_types.end() && namespace_it->second.is_dynamic_namespace && namespace_it->second.property_setter) {
                if (!namespace_it->second.property_setter(base_name, rhs)) {
                    error_message = L"failed to set Registry property: " + base_name;
                    return false;
                }
                ksh_env.variables[L"?"] = L"0";
                return true;
            }
        }
    }

    if (local_scope) {
        snapshot_local_variable_if_needed(base_name);
    }

    if (get_flag_value(ksh_env.readonly_flags, base_name)) {
        error_message = L"variable is read-only: " + base_name;
        return false;
    }

    const bool integer_mode = get_flag_value(ksh_env.integer_flags, base_name);

    if (integer_mode && (has_index || array_hint)) {
        error_message = L"integer variable cannot be assigned as an array: " + base_name;
        return false;
    }

    const bool is_assoc = get_flag_value(ksh_env.associative_flags, base_name);

    if (is_assoc) {
        if (integer_mode) {
            error_message = L"integer variable cannot be assigned as an array: " + base_name;
            return false;
        }

        if (has_index) {
            if (index_text.empty()) {
                error_message = L"associative array key cannot be empty";
                return false;
            }
            ksh_env.arrays[base_name][index_text] = rhs;
            ksh_env.variables.erase(base_name);
            return true;
        }

        std::wstring trimmed_rhs = trim_copy(rhs);
        if (!trimmed_rhs.empty() && trimmed_rhs.front() == L'(' && trimmed_rhs.back() == L')') {
            std::map<std::wstring, std::wstring> assoc_elements;
            if (parse_associative_array_literal(rhs, assoc_elements)) {
                ksh_env.arrays[base_name] = std::move(assoc_elements);
                ksh_env.variables.erase(base_name);
                return true;
            } else {
                error_message = L"invalid associative array literal assignment";
                return false;
            }
        }

        ksh_env.arrays[base_name][L"0"] = rhs;
        ksh_env.variables.erase(base_name);
        return true;
    }

    std::vector<std::wstring> literal_values;
    const bool rhs_is_array_literal = parse_array_literal(rhs, literal_values);

    if (integer_mode && rhs_is_array_literal) {
        error_message = L"integer variable cannot be assigned an array literal: " + base_name;
        return false;
    }

    if (has_index) {
        size_t index = 0;
        if (!parse_non_negative_index(index_text, index)) {
            error_message = L"array index must be a non-negative integer";
            return false;
        }
        if (index >= kMaxArrayIndex) {
            error_message = L"array index exceeds maximum allowed value";
            return false;
        }

        std::map<std::wstring, std::wstring>& target_array = ksh_env.arrays[base_name];
        target_array[std::to_wstring(index)] = rhs;
        ksh_env.variables.erase(base_name);
        return true;
    }

    if (array_hint || rhs_is_array_literal) {
        if (!rhs_is_array_literal && array_hint) {
            literal_values.push_back(rhs);
        }
        assign_array_parameter(base_name, literal_values);
        sync_exported_environment_variable(base_name);
        return true;
    }

    std::wstring scalar_value = rhs;
    if (integer_mode) {
        scalar_value = evaluate_arithmetic(rhs);
    }

    // Enforce per-session variable table cap (new entries only).
    const bool scalar_exists = ksh_env.variables.find(base_name) != ksh_env.variables.end();
    const bool array_exists  = ksh_env.arrays.find(base_name)    != ksh_env.arrays.end();
    if (!scalar_exists && !array_exists) {
        const size_t total = ksh_env.variables.size() + ksh_env.arrays.size();
        if (total >= kMaxShellVariables) {
            error_message = L"shell variable table limit reached, cannot create: " + base_name;
            return false;
        }
    }

    assign_scalar_parameter(base_name, scalar_value);
    sync_exported_environment_variable(base_name);
    return true;
}

bool extract_ksh_conditional_block(const std::wstring& expanded_input, std::wstring& condition, std::wstring& error_message) {
    condition.clear();
    error_message.clear();

    size_t start = expanded_input.find(L"[[");
    if (start == std::wstring::npos) {
        error_message = L"missing opening [[";
        return false;
    }

    bool in_single_quotes = false;
    bool in_double_quotes = false;
    bool escaped = false;
    size_t i = start + 2;
    size_t closing = std::wstring::npos;

    while (i < expanded_input.size()) {
        wchar_t ch = expanded_input[i];

        if (escaped) {
            escaped = false;
            i++;
            continue;
        }

        if (ch == L'\\' && !in_single_quotes) {
            escaped = true;
            i++;
            continue;
        }

        if (ch == L'\'' && !in_double_quotes) {
            in_single_quotes = !in_single_quotes;
            i++;
            continue;
        }

        if (ch == L'"' && !in_single_quotes) {
            in_double_quotes = !in_double_quotes;
            i++;
            continue;
        }

        if (!in_single_quotes && !in_double_quotes && ch == L']' && (i + 1) < expanded_input.size() && expanded_input[i + 1] == L']') {
            closing = i;
            break;
        }

        i++;
    }

    if (closing == std::wstring::npos) {
        error_message = L"missing closing ]]";
        return false;
    }

    condition = trim_copy(expanded_input.substr(start + 2, closing - (start + 2)));

    std::wstring trailing = trim_copy(expanded_input.substr(closing + 2));
    if (!trailing.empty()) {
        error_message = L"unexpected tokens after ]]";
        return false;
    }

    return true;
}

bool tokenize_ksh_conditional(const std::wstring& condition, std::vector<std::wstring>& tokens, std::wstring& error_message) {
    tokens.clear();
    error_message.clear();

    std::wstring current;
    bool in_single_quotes = false;
    bool in_double_quotes = false;
    bool escaped = false;
    bool in_token = false;

    auto flush_current = [&]() {
        if (in_token) {
            tokens.push_back(current);
            current.clear();
            in_token = false;
        }
    };

    for (size_t i = 0; i < condition.size(); ) {
        wchar_t ch = condition[i];

        if (escaped) {
            current += ch;
            in_token = true;
            escaped = false;
            i++;
            continue;
        }

        if (ch == L'\\' && !in_single_quotes) {
            escaped = true;
            i++;
            continue;
        }

        if (ch == L'$' && i + 1 < condition.length() && condition[i + 1] == L'\'' && !in_double_quotes && !in_single_quotes) {
            current += expand_ansi_c_quoting(condition, i);
            in_token = true;
            continue;
        }

        if (ch == L'\'' && !in_double_quotes) {
            in_single_quotes = !in_single_quotes;
            in_token = true;
            i++;
            continue;
        }

        if (ch == L'"' && !in_single_quotes) {
            in_double_quotes = !in_double_quotes;
            in_token = true;
            i++;
            continue;
        }

        if (!in_single_quotes && !in_double_quotes && std::iswspace(ch)) {
            flush_current();
            i++;
            continue;
        }

        current += ch;
        in_token = true;
        i++;
    }

    if (escaped) {
        error_message = L"unfinished escape sequence";
        return false;
    }
    if (in_single_quotes || in_double_quotes) {
        error_message = L"unterminated quoted string";
        return false;
    }

    flush_current();
    return true;
}

bool try_parse_strict_double(const std::wstring& token, double& value) {
    if (token.empty()) {
        return false;
    }

    wchar_t* end_ptr = nullptr;
    value = std::wcstod(token.c_str(), &end_ptr);
    return end_ptr != token.c_str() && end_ptr != nullptr && *end_ptr == L'\0';
}

bool evaluate_file_unary_operator(const std::wstring& op, const std::wstring& path, bool& result) {
    result = false;

    DWORD attrs = GetFileAttributesW(path.c_str());
    const bool exists = attrs != INVALID_FILE_ATTRIBUTES;

    if (op == L"-e" || op == L"-a") {
        result = exists;
        return true;
    }

    if (op == L"-f") {
        result = exists && ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0);
        return true;
    }

    if (op == L"-d") {
        result = exists && ((attrs & FILE_ATTRIBUTE_DIRECTORY) != 0);
        return true;
    }

    if (op == L"-h" || op == L"-L") {
        result = exists && ((attrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0);
        return true;
    }

    if (op == L"-s") {
        if (!exists || ((attrs & FILE_ATTRIBUTE_DIRECTORY) != 0)) {
            result = false;
            return true;
        }

        WIN32_FILE_ATTRIBUTE_DATA file_data;
        if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &file_data)) {
            result = false;
            return true;
        }

        ULONGLONG size = (static_cast<ULONGLONG>(file_data.nFileSizeHigh) << 32) | file_data.nFileSizeLow;
        result = size > 0;
        return true;
    }

    if (op == L"-r") {
        result = _waccess(path.c_str(), 4) == 0;
        return true;
    }

    if (op == L"-w") {
        result = _waccess(path.c_str(), 2) == 0;
        return true;
    }

    if (op == L"-x") {
        result = _waccess(path.c_str(), 0) == 0;
        return true;
    }

    return false;
}

bool evaluate_file_comparison_operator(const std::wstring& left, const std::wstring& op, const std::wstring& right, bool& result, std::wstring& error_message) {
    error_message.clear();

    if (op == L"-ef") {
        wchar_t left_full[MAX_PATH];
        wchar_t right_full[MAX_PATH];
        DWORD left_len = GetFullPathNameW(left.c_str(), MAX_PATH, left_full, nullptr);
        DWORD right_len = GetFullPathNameW(right.c_str(), MAX_PATH, right_full, nullptr);
        if (left_len == 0 || right_len == 0) {
            result = false;
            return true;
        }
        result = _wcsicmp(left_full, right_full) == 0;
        return true;
    }

    if (op == L"-nt" || op == L"-ot") {
        WIN32_FILE_ATTRIBUTE_DATA left_data;
        WIN32_FILE_ATTRIBUTE_DATA right_data;
        if (!GetFileAttributesExW(left.c_str(), GetFileExInfoStandard, &left_data) ||
            !GetFileAttributesExW(right.c_str(), GetFileExInfoStandard, &right_data)) {
            result = false;
            return true;
        }

        LONG cmp = CompareFileTime(&left_data.ftLastWriteTime, &right_data.ftLastWriteTime);
        result = (op == L"-nt") ? (cmp > 0) : (cmp < 0);
        return true;
    }

    return false;
}

bool evaluate_ksh_simple_condition(const std::vector<std::wstring>& tokens, size_t begin, size_t end, bool& result, std::wstring& error_message) {
    error_message.clear();
    result = false;

    if (begin >= end) {
        error_message = L"missing conditional expression";
        return false;
    }

    size_t negate_count = 0;
    while (begin < end && tokens[begin] == L"!") {
        negate_count++;
        begin++;
    }

    if (begin >= end) {
        error_message = L"missing conditional expression after !";
        return false;
    }

    size_t count = end - begin;

    if (count == 1) {
        result = !tokens[begin].empty();
    } else if (count == 2) {
        const std::wstring op = tokens[begin];
        const std::wstring operand = tokens[begin + 1];
        if (op == L"-n") {
            result = !operand.empty();
        } else if (op == L"-z") {
            result = operand.empty();
        } else if (evaluate_file_unary_operator(op, operand, result)) {
            // File test operator evaluated.
        } else {
            error_message = L"unsupported unary operator: " + op;
            return false;
        }
    } else if (count == 3) {
        const std::wstring left = tokens[begin];
        const std::wstring op = tokens[begin + 1];
        const std::wstring right = tokens[begin + 2];

        if (op == L"==" || op == L"=") {
            result = match_glob_pattern(right, left);
        } else if (op == L"!=") {
            result = !match_glob_pattern(right, left);
        } else if (op == L"=~") {
            if (!is_regex_enabled()) {
                error_message = L"regex matching is disabled";
                return false;
            }
            if (right.size() > kMaxRegexPatternLength) {
                error_message = L"regex pattern exceeds limit";
                return false;
            }
            if (left.size() > kMaxRegexInputLength) {
                error_message = L"regex input exceeds limit";
                return false;
            }
            try {
                std::wregex rx(right);
                std::wsmatch match;
                result = std::regex_search(left, match, rx);
                if (result) {
                    ksh_env.arrays[L".sh.match"].clear();
                    for (size_t group_idx = 0; group_idx < match.size(); ++group_idx) {
                        std::wstring idx_str = std::to_wstring(group_idx);
                        ksh_env.arrays[L".sh.match"][idx_str] = match[group_idx].str();
                    }
                } else {
                    ksh_env.arrays[L".sh.match"].clear();
                }
            } catch (...) {
                ksh_env.arrays[L".sh.match"].clear();
                error_message = L"invalid regex pattern";
                return false;
            }
        } else if (op == L"<") {
            result = left < right;
        } else if (op == L">") {
            result = left > right;
        } else if (op == L"-eq" || op == L"-ne" || op == L"-gt" || op == L"-ge" || op == L"-lt" || op == L"-le") {
            double left_num = 0.0;
            double right_num = 0.0;
            if (!try_parse_strict_double(left, left_num) || !try_parse_strict_double(right, right_num)) {
                error_message = L"numeric comparison requires numeric operands";
                return false;
            }

            if (op == L"-eq") result = (left_num == right_num);
            if (op == L"-ne") result = (left_num != right_num);
            if (op == L"-gt") result = (left_num > right_num);
            if (op == L"-ge") result = (left_num >= right_num);
            if (op == L"-lt") result = (left_num < right_num);
            if (op == L"-le") result = (left_num <= right_num);
        } else {
            bool file_result = false;
            if (evaluate_file_comparison_operator(left, op, right, file_result, error_message)) {
                result = file_result;
            } else {
                error_message = L"unsupported binary operator: " + op;
                return false;
            }
        }
    } else {
        error_message = L"unable to parse conditional expression";
        return false;
    }

    if ((negate_count % 2) == 1) {
        result = !result;
    }
    return true;
}

bool is_test_open_paren(const std::wstring& token) {
    return token == L"(" || token == L"\\(";
}

bool is_test_close_paren(const std::wstring& token) {
    return token == L")" || token == L"\\)";
}

bool is_supported_unary_condition_operator(const std::wstring& op) {
    return op == L"-n" || op == L"-z" || op == L"-e" || op == L"-a" || op == L"-f" || op == L"-d" ||
           op == L"-h" || op == L"-L" || op == L"-s" || op == L"-r" || op == L"-w" || op == L"-x";
}

bool is_supported_binary_condition_operator(const std::wstring& op) {
    return op == L"==" || op == L"=" || op == L"!=" || op == L"=~" || op == L"<" || op == L">" ||
           op == L"-eq" || op == L"-ne" || op == L"-gt" || op == L"-ge" || op == L"-lt" || op == L"-le" ||
           op == L"-ef" || op == L"-nt" || op == L"-ot";
}

bool evaluate_test_condition_or(const std::vector<std::wstring>& tokens, size_t& index, bool& result, std::wstring& error_message);

bool evaluate_test_condition_primary(const std::vector<std::wstring>& tokens, size_t& index, bool& result, std::wstring& error_message) {
    if (index >= tokens.size()) {
        error_message = L"missing conditional expression";
        return false;
    }

    if (is_test_open_paren(tokens[index])) {
        index++;
        if (!evaluate_test_condition_or(tokens, index, result, error_message)) {
            return false;
        }
        if (index >= tokens.size() || !is_test_close_paren(tokens[index])) {
            error_message = L"missing closing parenthesis in test expression";
            return false;
        }
        index++;
        return true;
    }

    if (is_test_close_paren(tokens[index])) {
        error_message = L"unexpected closing parenthesis in test expression";
        return false;
    }

    size_t start = index;
    size_t consume = 1;

    if ((index + 1) < tokens.size() && is_supported_unary_condition_operator(tokens[index])) {
        consume = 2;
    } else if ((index + 2) < tokens.size() && is_supported_binary_condition_operator(tokens[index + 1])) {
        consume = 3;
    }

    if ((start + consume) > tokens.size()) {
        error_message = L"missing operand in conditional expression";
        return false;
    }

    if (!evaluate_ksh_simple_condition(tokens, start, start + consume, result, error_message)) {
        return false;
    }

    index += consume;
    return true;
}

bool evaluate_test_condition_not(const std::vector<std::wstring>& tokens, size_t& index, bool& result, std::wstring& error_message) {
    if (index < tokens.size() && tokens[index] == L"!") {
        index++;
        if (!evaluate_test_condition_not(tokens, index, result, error_message)) {
            return false;
        }
        result = !result;
        return true;
    }

    return evaluate_test_condition_primary(tokens, index, result, error_message);
}

bool evaluate_test_condition_and(const std::vector<std::wstring>& tokens, size_t& index, bool& result, std::wstring& error_message) {
    if (!evaluate_test_condition_not(tokens, index, result, error_message)) {
        return false;
    }

    while (index < tokens.size() && tokens[index] == L"-a") {
        index++;
        bool rhs = false;
        if (!evaluate_test_condition_not(tokens, index, rhs, error_message)) {
            if (error_message.empty()) {
                error_message = L"missing expression after -a";
            }
            return false;
        }
        result = result && rhs;
    }

    return true;
}

bool evaluate_test_condition_or(const std::vector<std::wstring>& tokens, size_t& index, bool& result, std::wstring& error_message) {
    if (!evaluate_test_condition_and(tokens, index, result, error_message)) {
        return false;
    }

    while (index < tokens.size() && tokens[index] == L"-o") {
        index++;
        bool rhs = false;
        if (!evaluate_test_condition_and(tokens, index, rhs, error_message)) {
            if (error_message.empty()) {
                error_message = L"missing expression after -o";
            }
            return false;
        }
        result = result || rhs;
    }

    return true;
}

bool evaluate_test_condition_expression(const std::vector<std::wstring>& tokens, bool& result, std::wstring& error_message) {
    result = false;
    error_message.clear();

    if (tokens.empty()) {
        error_message = L"missing conditional expression";
        return false;
    }

    size_t index = 0;
    if (!evaluate_test_condition_or(tokens, index, result, error_message)) {
        return false;
    }

    if (index != tokens.size()) {
        error_message = L"unexpected token in test expression: " + tokens[index];
        return false;
    }

    return true;
}

bool evaluate_ksh_conditional(const std::wstring& condition_str, bool& value, std::wstring& error_message) {
    value = false;
    error_message.clear();

    std::vector<std::wstring> tokens;
    if (!tokenize_ksh_conditional(condition_str, tokens, error_message)) {
        return false;
    }

    if (tokens.empty()) {
        error_message = L"empty conditional expression";
        return false;
    }

    bool overall_value = false;
    bool have_or_term = false;
    size_t i = 0;

    while (i < tokens.size()) {
        bool and_value = true;
        bool have_and_value = false;

        while (i < tokens.size() && tokens[i] != L"||") {
            size_t segment_start = i;
            while (i < tokens.size() && tokens[i] != L"&&" && tokens[i] != L"||") {
                i++;
            }

            bool segment_value = false;
            if (!evaluate_ksh_simple_condition(tokens, segment_start, i, segment_value, error_message)) {
                return false;
            }

            if (!have_and_value) {
                and_value = segment_value;
                have_and_value = true;
            } else {
                and_value = and_value && segment_value;
            }

            if (i < tokens.size() && tokens[i] == L"&&") {
                i++;
                if (i >= tokens.size()) {
                    error_message = L"dangling && in conditional expression";
                    return false;
                }
            }
        }

        if (!have_and_value) {
            error_message = L"missing expression between logical operators";
            return false;
        }

        if (!have_or_term) {
            overall_value = and_value;
            have_or_term = true;
        } else {
            overall_value = overall_value || and_value;
        }

        if (i < tokens.size() && tokens[i] == L"||") {
            i++;
            if (i >= tokens.size()) {
                error_message = L"dangling || in conditional expression";
                return false;
            }
        }
    }

    value = overall_value;
    return true;
}

bool execute_native_or_fallback(const std::wstring& full_command, const RedirectionSpec* redir = nullptr, DWORD* exit_code_out = nullptr) {
    struct ScopedForegroundProcessGuard {
        ScopedForegroundProcessGuard() {
            InterlockedExchange(&g_foreground_process_active, 1);
        }
        ~ScopedForegroundProcessGuard() {
            InterlockedExchange(&g_foreground_process_active, 0);
        }
    } fg_guard;

    const bool perf_trace = is_performance_telemetry_enabled();
    const ULONGLONG overall_start = perf_trace ? GetTickCount64() : 0;

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    HANDLE std_in = nullptr;
    HANDLE std_out = nullptr;
    HANDLE std_err = nullptr;
    bool close_in = false;
    bool close_out = false;
    bool close_err = false;

    if (!setup_redirection_handles(redir, std_in, std_out, std_err, close_in, close_out, close_err)) {
        if (exit_code_out != nullptr) {
            *exit_code_out = 1;
        }
        std::wcerr << L"ksh: redirection setup failed\n";
        return false;
    }

    const bool use_redirected_stdio = (redir != nullptr && has_any_redirection(*redir));
    if (use_redirected_stdio) {
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = std_in;
        si.hStdOutput = std_out;
        si.hStdError = std_err;
    }

    std::vector<HANDLE> inherited_handles;
    if (use_redirected_stdio) {
        inherited_handles = { std_in, std_out, std_err };
    }
    if (create_process_with_handle_list(full_command, si, inherited_handles, 0, pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        register_child_process_times(pi.hProcess);
        DWORD code = 0;
        GetExitCodeProcess(pi.hProcess, &code);
        if (exit_code_out != nullptr) {
            *exit_code_out = code;
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        close_redirection_handles(std_in, std_out, std_err, close_in, close_out, close_err);
        if (perf_trace) {
            log_performance_trace(L"process.foreground.native", GetTickCount64() - overall_start, L"exit=" + std::to_wstring(code));
        }
        return true;
    }

    // Fallback to cmd.exe for Windows built-in shell commands
    std::wstring cmd_name = get_command_name(full_command);
    if (!is_windows_internal_command(cmd_name)) {
        if (exit_code_out != nullptr) {
            *exit_code_out = 127;
        }
        std::wcerr << L"ksh: " << cmd_name << L": command not found\n";
        close_redirection_handles(std_in, std_out, std_err, close_in, close_out, close_err);
        return false;
    }

    std::wstring cmd_call;
    if (!build_cmd_shell_command_line(full_command, cmd_call)) {
        if (exit_code_out != nullptr) {
            *exit_code_out = 1;
        }
        std::wcerr << L"ksh: failed to start command\n";
        close_redirection_handles(std_in, std_out, std_err, close_in, close_out, close_err);
        return false;
    }
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    if (use_redirected_stdio) {
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = std_in;
        si.hStdOutput = std_out;
        si.hStdError = std_err;
    }
    if (create_process_with_handle_list(cmd_call, si, inherited_handles, 0, pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        register_child_process_times(pi.hProcess);
        DWORD code = 0;
        GetExitCodeProcess(pi.hProcess, &code);
        if (exit_code_out != nullptr) {
            *exit_code_out = code;
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        close_redirection_handles(std_in, std_out, std_err, close_in, close_out, close_err);
        if (perf_trace) {
            log_performance_trace(L"process.foreground.cmd_fallback", GetTickCount64() - overall_start, L"exit=" + std::to_wstring(code));
        }
        return true;
    }

    if (exit_code_out != nullptr) {
        *exit_code_out = 1;
    }
    std::wcerr << L"ksh: failed to start command\n";

    close_redirection_handles(std_in, std_out, std_err, close_in, close_out, close_err);
    if (perf_trace) {
        log_performance_trace(L"process.foreground.failed", GetTickCount64() - overall_start);
    }
    return false;
}

bool launch_process(const std::wstring& full_command, HANDLE& process_handle, DWORD& pid, const RedirectionSpec* redir = nullptr) {
    const bool perf_trace = is_performance_telemetry_enabled();
    const ULONGLONG overall_start = perf_trace ? GetTickCount64() : 0;

    process_handle = nullptr;
    pid = 0;

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    HANDLE std_in = nullptr;
    HANDLE std_out = nullptr;
    HANDLE std_err = nullptr;
    bool close_in = false;
    bool close_out = false;
    bool close_err = false;

    if (!setup_redirection_handles(redir, std_in, std_out, std_err, close_in, close_out, close_err)) {
        return false;
    }

    const bool use_redirected_stdio = (redir != nullptr && has_any_redirection(*redir));
    if (use_redirected_stdio) {
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = std_in;
        si.hStdOutput = std_out;
        si.hStdError = std_err;
    }

    std::vector<HANDLE> inherited_handles;
    if (use_redirected_stdio) {
        inherited_handles = { std_in, std_out, std_err };
    }
    if (create_process_with_handle_list(full_command, si, inherited_handles, 0, pi)) {
        process_handle = pi.hProcess;
        pid = pi.dwProcessId;
        CloseHandle(pi.hThread);
        close_redirection_handles(std_in, std_out, std_err, close_in, close_out, close_err);
        if (perf_trace) {
            log_performance_trace(L"process.background.native", GetTickCount64() - overall_start, L"pid=" + std::to_wstring(pid));
        }
        return true;
    }

    std::wstring cmd_name = get_command_name(full_command);
    if (!is_windows_internal_command(cmd_name)) {
        std::wcerr << L"ksh: " << cmd_name << L": command not found\n";
        close_redirection_handles(std_in, std_out, std_err, close_in, close_out, close_err);
        return false;
    }

    std::wstring cmd_call;
    if (!build_cmd_shell_command_line(full_command, cmd_call)) {
        close_redirection_handles(std_in, std_out, std_err, close_in, close_out, close_err);
        return false;
    }
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    if (use_redirected_stdio) {
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = std_in;
        si.hStdOutput = std_out;
        si.hStdError = std_err;
    }
    if (create_process_with_handle_list(cmd_call, si, inherited_handles, 0, pi)) {
        process_handle = pi.hProcess;
        pid = pi.dwProcessId;
        CloseHandle(pi.hThread);
        close_redirection_handles(std_in, std_out, std_err, close_in, close_out, close_err);
        if (perf_trace) {
            log_performance_trace(L"process.background.cmd_fallback", GetTickCount64() - overall_start, L"pid=" + std::to_wstring(pid));
        }
        return true;
    }

    close_redirection_handles(std_in, std_out, std_err, close_in, close_out, close_err);
    if (perf_trace) {
        log_performance_trace(L"process.background.failed", GetTickCount64() - overall_start);
    }
    return false;
}

bool is_script_empty_or_comment_line(const std::wstring& trimmed) {
    return trimmed.empty() || trimmed[0] == L'#';
}

bool starts_with_if_control_keyword(const std::wstring& trimmed) {
    if (trimmed.size() < 2) {
        return false;
    }

    if (std::towlower(trimmed[0]) != L'i' || std::towlower(trimmed[1]) != L'f') {
        return false;
    }

    if (trimmed.size() == 2) {
        return true;
    }

    return std::iswspace(trimmed[2]) != 0;
}

bool starts_with_elif_control_keyword(const std::wstring& trimmed) {
    if (trimmed.size() < 4) {
        return false;
    }

    if (std::towlower(trimmed[0]) != L'e' ||
        std::towlower(trimmed[1]) != L'l' ||
        std::towlower(trimmed[2]) != L'i' ||
        std::towlower(trimmed[3]) != L'f') {
        return false;
    }

    if (trimmed.size() == 4) {
        return true;
    }

    return std::iswspace(trimmed[4]) != 0;
}

bool starts_with_while_control_keyword(const std::wstring& trimmed) {
    if (trimmed.size() < 5) {
        return false;
    }

    if (std::towlower(trimmed[0]) != L'w' ||
        std::towlower(trimmed[1]) != L'h' ||
        std::towlower(trimmed[2]) != L'i' ||
        std::towlower(trimmed[3]) != L'l' ||
        std::towlower(trimmed[4]) != L'e') {
        return false;
    }

    if (trimmed.size() == 5) {
        return true;
    }

    return std::iswspace(trimmed[5]) != 0;
}

bool starts_with_until_control_keyword(const std::wstring& trimmed) {
    if (trimmed.size() < 5) {
        return false;
    }

    if (std::towlower(trimmed[0]) != L'u' ||
        std::towlower(trimmed[1]) != L'n' ||
        std::towlower(trimmed[2]) != L't' ||
        std::towlower(trimmed[3]) != L'i' ||
        std::towlower(trimmed[4]) != L'l') {
        return false;
    }

    if (trimmed.size() == 5) {
        return true;
    }

    return std::iswspace(trimmed[5]) != 0;
}

bool starts_with_for_control_keyword(const std::wstring& trimmed) {
    if (trimmed.size() < 3) {
        return false;
    }

    if (std::towlower(trimmed[0]) != L'f' ||
        std::towlower(trimmed[1]) != L'o' ||
        std::towlower(trimmed[2]) != L'r') {
        return false;
    }

    if (trimmed.size() == 3) {
        return true;
    }

    return std::iswspace(trimmed[3]) != 0;
}

bool starts_with_case_control_keyword(const std::wstring& trimmed) {
    if (trimmed.size() < 4) {
        return false;
    }

    if (std::towlower(trimmed[0]) != L'c' ||
        std::towlower(trimmed[1]) != L'a' ||
        std::towlower(trimmed[2]) != L's' ||
        std::towlower(trimmed[3]) != L'e') {
        return false;
    }

    if (trimmed.size() == 4) {
        return true;
    }

    return std::iswspace(trimmed[4]) != 0;
}

bool starts_with_select_control_keyword(const std::wstring& trimmed) {
    if (trimmed.size() < 6) {
        return false;
    }
    if (std::towlower(trimmed[0]) != L's' ||
        std::towlower(trimmed[1]) != L'e' ||
        std::towlower(trimmed[2]) != L'l' ||
        std::towlower(trimmed[3]) != L'e' ||
        std::towlower(trimmed[4]) != L'c' ||
        std::towlower(trimmed[5]) != L't') {
        return false;
    }
    if (trimmed.size() == 6) {
        return true;
    }
    return std::iswspace(trimmed[6]) != 0;
}

bool starts_with_loop_control_keyword(const std::wstring& trimmed) {
    return starts_with_while_control_keyword(trimmed) ||
           starts_with_until_control_keyword(trimmed) ||
           starts_with_for_control_keyword(trimmed) ||
           starts_with_select_control_keyword(trimmed);
}

void report_script_syntax_error(size_t line_index, const std::wstring& message) {
    std::wcerr << L"ksh: script: line " << (line_index + 1) << L": " << message << L"\n";
}

bool parse_if_header_condition(
    const std::vector<std::wstring>& lines,
    size_t header_line_index,
    size_t end_index,
    size_t keyword_length,
    const std::wstring& header_label,
    std::wstring& condition_command,
    size_t& body_start,
    std::wstring& error_message,
    size_t& error_line_index) {
    error_message.clear();
    error_line_index = header_line_index;

    const std::wstring header_line = trim_copy(lines[header_line_index]);
    condition_command = trim_copy(header_line.substr(keyword_length));

    bool inline_then = false;
    if (ends_with_case_insensitive(condition_command, L"; then")) {
        condition_command = trim_copy(condition_command.substr(0, condition_command.size() - 6));
        inline_then = true;
    }

    if (condition_command.empty()) {
        error_message = header_label + L" requires a condition command";
        return false;
    }

    if (inline_then) {
        body_start = header_line_index + 1;
        return true;
    }

    size_t probe = header_line_index + 1;
    while (probe < end_index) {
        const std::wstring probe_trimmed = trim_copy(lines[probe]);
        if (is_script_empty_or_comment_line(probe_trimmed)) {
            probe++;
            continue;
        }

        if (to_lower_copy(probe_trimmed) == L"then") {
            body_start = probe + 1;
            return true;
        }

        error_message = L"expected then";
        error_line_index = probe;
        return false;
    }

    error_message = L"missing then for " + header_label + L" block";
    error_line_index = header_line_index;
    return false;
}

struct ScriptFlowControl {
    int break_levels = 0;
    int continue_levels = 0;
};

bool parse_do_block_start(
    const std::vector<std::wstring>& lines,
    size_t header_line_index,
    size_t end_index,
    std::wstring& trailing,
    const std::wstring& header_label,
    size_t& body_start,
    std::wstring& error_message,
    size_t& error_line_index) {
    error_message.clear();
    error_line_index = header_line_index;

    std::wstring normalized = trim_copy(trailing);
    bool inline_do = false;
    if (ends_with_case_insensitive(normalized, L"; do")) {
        normalized = trim_copy(normalized.substr(0, normalized.size() - 4));
        inline_do = true;
    }

    trailing = normalized;

    if (normalized.empty()) {
        error_message = header_label + L" requires a command";
        return false;
    }

    if (inline_do) {
        body_start = header_line_index + 1;
        return true;
    }

    size_t probe = header_line_index + 1;
    while (probe < end_index) {
        const std::wstring probe_trimmed = trim_copy(lines[probe]);
        if (is_script_empty_or_comment_line(probe_trimmed)) {
            probe++;
            continue;
        }

        if (to_lower_copy(probe_trimmed) == L"do") {
            body_start = probe + 1;
            return true;
        }

        error_message = L"expected do";
        error_line_index = probe;
        return false;
    }

    error_message = L"missing do for " + header_label + L" block";
    error_line_index = header_line_index;
    return false;
}

bool is_loop_done_keyword(const std::wstring& line, std::wstring& redirection_part) {
    std::wstring trimmed = trim_copy(line);
    std::wstring lowered = to_lower_copy(trimmed);
    if (lowered == L"done") {
        redirection_part = L"";
        return true;
    }
    if (lowered.rfind(L"done", 0) == 0 && lowered.size() > 4) {
        wchar_t next = lowered[4];
        if (next == L' ' || next == L'\t' || next == L'<' || next == L'>' || next == L'|' || next == L'&' || next == L';') {
            size_t done_pos = line.find(trimmed);
            redirection_part = trim_copy(line.substr(done_pos + 4));
            return true;
        }
    }
    return false;
}

bool find_matching_done(
    const std::vector<std::wstring>& lines,
    size_t body_start,
    size_t end_index,
    size_t& done_line_index) {
    done_line_index = std::wstring::npos;
    int nested_if_depth = 0;
    int nested_loop_depth = 0;

    for (size_t probe = body_start; probe < end_index; ++probe) {
        const std::wstring probe_trimmed = trim_copy(lines[probe]);
        if (is_script_empty_or_comment_line(probe_trimmed)) {
            continue;
        }

        if (starts_with_if_control_keyword(probe_trimmed)) {
            nested_if_depth++;
            continue;
        }

        if (starts_with_loop_control_keyword(probe_trimmed)) {
            nested_loop_depth++;
            continue;
        }

        const std::wstring lowered = to_lower_copy(probe_trimmed);
        if (lowered == L"fi") {
            if (nested_if_depth > 0) {
                nested_if_depth--;
            }
            continue;
        }

        std::wstring dummy_redir;
        if (is_loop_done_keyword(lines[probe], dummy_redir)) {
            if (nested_if_depth > 0) {
                continue;
            }

            if (nested_loop_depth > 0) {
                nested_loop_depth--;
                continue;
            }

            done_line_index = probe;
            return true;
        }
    }

    return false;
}

bool parse_for_header(
    const std::vector<std::wstring>& lines,
    size_t header_line_index,
    size_t end_index,
    std::wstring& variable_name,
    std::vector<std::wstring>& items,
    size_t& body_start,
    std::wstring& error_message,
    size_t& error_line_index);

void parse_and_replace_process_substitutions(std::wstring& line);

class BlockRedirector {
    HANDLE hSavedIn, hSavedOut;
    HANDLE hNewIn, hNewOut;
    bool activeIn, activeOut;
public:
    BlockRedirector(const RedirectionSpec& r) :
        activeIn(false), activeOut(false),
        hSavedIn(INVALID_HANDLE_VALUE), hSavedOut(INVALID_HANDLE_VALUE),
        hNewIn(INVALID_HANDLE_VALUE), hNewOut(INVALID_HANDLE_VALUE) {
        
        if (r.has_stdin) {
            hSavedIn = GetStdHandle(STD_INPUT_HANDLE);
            if (open_redirection_file(r.stdin_path, GENERIC_READ, OPEN_EXISTING, hNewIn)) {
                SetStdHandle(STD_INPUT_HANDLE, hNewIn);
                activeIn = true;
            }
        }
        if (r.has_stdout) {
            hSavedOut = GetStdHandle(STD_OUTPUT_HANDLE);
            DWORD creation = r.append_stdout ? OPEN_ALWAYS : CREATE_ALWAYS;
            DWORD access = GENERIC_WRITE;
            if (r.append_stdout) access |= FILE_APPEND_DATA;
            if (open_redirection_file(r.stdout_path, access, creation, hNewOut)) {
                if (r.append_stdout) {
                    SetFilePointer(hNewOut, 0, NULL, FILE_END);
                }
                SetStdHandle(STD_OUTPUT_HANDLE, hNewOut);
                activeOut = true;
            }
        }
    }
    ~BlockRedirector() {
        if (activeIn) {
            SetStdHandle(STD_INPUT_HANDLE, hSavedIn);
            CloseHandle(hNewIn);
        }
        if (activeOut) {
            SetStdHandle(STD_OUTPUT_HANDLE, hSavedOut);
            CloseHandle(hNewOut);
        }
    }
    bool is_valid(const RedirectionSpec& r) const {
        if (r.has_stdin && !activeIn) return false;
        if (r.has_stdout && !activeOut) return false;
        return true;
    }
};

bool execute_script_while_until_block(
    const std::vector<std::wstring>& lines,
    size_t& line_index,
    size_t end_index,
    bool until_mode,
    bool& should_exit_shell,
    ScriptFlowControl* flow_control);

bool execute_script_select_block(
    const std::vector<std::wstring>& lines,
    size_t& line_index,
    size_t end_index,
    bool& should_exit_shell,
    ScriptFlowControl* flow_control);

bool execute_script_for_block(
    const std::vector<std::wstring>& lines,
    size_t& line_index,
    size_t end_index,
    bool& should_exit_shell,
    ScriptFlowControl* flow_control);

bool execute_script_case_block(
    const std::vector<std::wstring>& lines,
    size_t& line_index,
    size_t end_index,
    bool& should_exit_shell,
    ScriptFlowControl* flow_control);

bool execute_script_lines_range(
    const std::vector<std::wstring>& lines,
    size_t start_index,
    size_t end_index,
    bool& should_exit_shell,
    ScriptFlowControl* flow_control = nullptr);

bool execute_script_if_block(
    const std::vector<std::wstring>& lines,
    size_t& line_index,
    size_t end_index,
    bool& should_exit_shell,
    ScriptFlowControl* flow_control) {
    struct ConditionalBranch {
        bool has_condition = true;
        std::wstring condition_command;
        size_t marker_line_index = 0;
        size_t body_start = 0;
        size_t body_end = 0;
    };

    const size_t if_line_index = line_index;

    std::wstring if_condition;
    size_t if_body_start = if_line_index + 1;
    std::wstring parse_error;
    size_t parse_error_line = if_line_index;
    if (!parse_if_header_condition(lines, if_line_index, end_index, 2, L"if", if_condition, if_body_start, parse_error, parse_error_line)) {
        report_script_syntax_error(parse_error_line, parse_error);
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    std::vector<size_t> top_level_elif_lines;
    size_t else_line_index = std::wstring::npos;
    size_t fi_line_index = std::wstring::npos;
    int nested_if_depth = 0;

    for (size_t probe = if_body_start; probe < end_index; ++probe) {
        const std::wstring probe_trimmed = trim_copy(lines[probe]);
        if (is_script_empty_or_comment_line(probe_trimmed)) {
            continue;
        }

        if (starts_with_if_control_keyword(probe_trimmed)) {
            nested_if_depth++;
            continue;
        }

        if (nested_if_depth > 0) {
            if (to_lower_copy(probe_trimmed) == L"fi") {
                nested_if_depth--;
            }
            continue;
        }

        if (starts_with_elif_control_keyword(probe_trimmed)) {
            if (else_line_index != std::wstring::npos) {
                report_script_syntax_error(probe, L"unexpected elif after else");
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            top_level_elif_lines.push_back(probe);
            continue;
        }

        const std::wstring lowered = to_lower_copy(probe_trimmed);
        if (lowered == L"else") {
            if (else_line_index != std::wstring::npos) {
                report_script_syntax_error(probe, L"multiple else blocks in if statement");
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            else_line_index = probe;
            continue;
        }

        if (lowered == L"fi") {
            fi_line_index = probe;
            break;
        }
    }

    if (fi_line_index == std::wstring::npos) {
        report_script_syntax_error(if_line_index, L"missing fi for if block");
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    std::vector<ConditionalBranch> branches;

    ConditionalBranch if_branch;
    if_branch.has_condition = true;
    if_branch.condition_command = if_condition;
    if_branch.marker_line_index = if_line_index;
    if_branch.body_start = if_body_start;
    branches.push_back(if_branch);

    for (size_t elif_line : top_level_elif_lines) {
        ConditionalBranch elif_branch;
        elif_branch.has_condition = true;
        elif_branch.marker_line_index = elif_line;
        if (!parse_if_header_condition(lines, elif_line, end_index, 4, L"elif", elif_branch.condition_command, elif_branch.body_start, parse_error, parse_error_line)) {
            report_script_syntax_error(parse_error_line, parse_error);
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        branches.push_back(elif_branch);
    }

    if (else_line_index != std::wstring::npos) {
        ConditionalBranch else_branch;
        else_branch.has_condition = false;
        else_branch.marker_line_index = else_line_index;
        else_branch.body_start = else_line_index + 1;
        branches.push_back(else_branch);
    }

    for (size_t idx = 0; idx < branches.size(); ++idx) {
        size_t end_marker = fi_line_index;
        if (idx + 1 < branches.size()) {
            end_marker = branches[idx + 1].marker_line_index;
        }
        branches[idx].body_end = end_marker;

        if (branches[idx].body_start > branches[idx].body_end) {
            report_script_syntax_error(branches[idx].marker_line_index, L"invalid branch body range");
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
    }

    for (const ConditionalBranch& branch : branches) {
        bool should_run_branch = false;

        if (!branch.has_condition) {
            should_run_branch = true;
        } else {
            g_errexit_disabled_depth++;
            bool cond_ok = execute_command_line(branch.condition_command, should_exit_shell);
            g_errexit_disabled_depth--;
            if (!cond_ok) {
                return false;
            }
            if (should_exit_shell) {
                line_index = fi_line_index + 1;
                return true;
            }
            if (is_function_return_requested()) {
                line_index = fi_line_index + 1;
                return true;
            }

            std::map<std::wstring, std::wstring>::const_iterator status_it = ksh_env.variables.find(L"?");
            should_run_branch = (status_it != ksh_env.variables.end() && status_it->second == L"0");
        }

        if (should_run_branch) {
            if (!execute_script_lines_range(lines, branch.body_start, branch.body_end, should_exit_shell, flow_control)) {
                return false;
            }
            if (is_function_return_requested()) {
                line_index = fi_line_index + 1;
                return true;
            }
            break;
        }
    }

    line_index = fi_line_index + 1;
    return true;
}

bool execute_script_while_until_block(
    const std::vector<std::wstring>& lines,
    size_t& line_index,
    size_t end_index,
    bool until_mode,
    bool& should_exit_shell,
    ScriptFlowControl* flow_control) {
    const size_t header_line_index = line_index;
    const std::wstring header_line = trim_copy(lines[header_line_index]);
    const std::wstring header_label = until_mode ? L"until" : L"while";

    std::wstring condition_command = trim_copy(header_line.substr(5));
    std::wstring parse_error;
    size_t parse_error_line = header_line_index;
    size_t body_start = header_line_index + 1;

    if (!parse_do_block_start(lines, header_line_index, end_index, condition_command, header_label, body_start, parse_error, parse_error_line)) {
        report_script_syntax_error(parse_error_line, parse_error);
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    size_t done_line_index = std::wstring::npos;
    if (!find_matching_done(lines, body_start, end_index, done_line_index)) {
        report_script_syntax_error(header_line_index, L"missing done for " + header_label + L" block");
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    std::wstring redirection_part;
    RedirectionSpec loop_redir;
    bool has_redir = false;
    if (is_loop_done_keyword(lines[done_line_index], redirection_part) && !redirection_part.empty()) {
        parse_and_replace_process_substitutions(redirection_part);
        std::vector<std::wstring> redir_tokens = ksh_tokenize_preserve_quotes(redirection_part);
        redir_tokens = remove_quotes_and_escapes_from_tokens(redir_tokens);
        std::wstring err;
        if (parse_redirections(redir_tokens, loop_redir, err)) {
            has_redir = true;
        } else {
            std::wcerr << L"ksh: while: loop redirection parse error: " << err << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
    }

    std::unique_ptr<BlockRedirector> block_redir;
    if (has_redir) {
        block_redir = std::make_unique<BlockRedirector>(loop_redir);
        if (!block_redir->is_valid(loop_redir)) {
            std::wcerr << L"ksh: while: loop redirection setup failed\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
    }

    while (true) {
        g_errexit_disabled_depth++;
        bool cond_ok = execute_command_line(condition_command, should_exit_shell);
        g_errexit_disabled_depth--;
        if (should_exit_shell) {
            line_index = done_line_index + 1;
            return true;
        }
        if (is_function_return_requested()) {
            line_index = done_line_index + 1;
            return true;
        }

        std::map<std::wstring, std::wstring>::const_iterator status_it = ksh_env.variables.find(L"?");
        const bool condition_true = cond_ok && (status_it != ksh_env.variables.end() && status_it->second == L"0");
        const bool run_body = until_mode ? !condition_true : condition_true;
        if (!run_body) {
            break;
        }

        ScriptFlowControl local_flow;
        ScriptFlowControl* body_flow = (flow_control != nullptr) ? flow_control : &local_flow;
        if (!execute_script_lines_range(lines, body_start, done_line_index, should_exit_shell, body_flow)) {
            return false;
        }
        if (should_exit_shell) {
            line_index = done_line_index + 1;
            return true;
        }
        if (is_function_return_requested()) {
            line_index = done_line_index + 1;
            return true;
        }

        if (body_flow->break_levels > 0) {
            body_flow->break_levels--;
            ksh_env.variables[L"?"] = L"0";
            line_index = done_line_index + 1;
            return true;
        }

        if (body_flow->continue_levels > 0) {
            body_flow->continue_levels--;
            if (body_flow->continue_levels > 0) {
                line_index = done_line_index + 1;
                return true;
            }
            continue;
        }
    }

    ksh_env.variables[L"?"] = L"0";
    line_index = done_line_index + 1;
    return true;
}

bool execute_script_case_block(
    const std::vector<std::wstring>& lines,
    size_t& line_index,
    size_t end_index,
    bool& should_exit_shell,
    ScriptFlowControl* flow_control) {
    const size_t header_line_index = line_index;
    const std::wstring header_line = trim_copy(lines[header_line_index]);
    std::wstring trailing = trim_copy(header_line.substr(4));

    std::wstring case_word;
    size_t body_start = header_line_index + 1;

    size_t in_pos = trailing.find(L" in");
    if (in_pos != std::wstring::npos) {
        case_word = trim_copy(trailing.substr(0, in_pos));
        body_start = header_line_index + 1;
    } else {
        case_word = trim_copy(trailing);
        size_t probe = header_line_index + 1;
        while (probe < end_index) {
            const std::wstring probe_trimmed = trim_copy(lines[probe]);
            if (is_script_empty_or_comment_line(probe_trimmed)) {
                probe++;
                continue;
            }
            if (to_lower_copy(probe_trimmed) == L"in") {
                body_start = probe + 1;
                break;
            }
            report_script_syntax_error(probe, L"expected 'in' for case block");
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
    }

    if (case_word.empty()) {
        report_script_syntax_error(header_line_index, L"case requires a word");
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    std::wstring expanded_case_word = expand_substitutions_left_to_right(case_word);
    if (g_expansion_error) {
        ksh_env.variables[L"?"] = L"1";
        return false;
    }
    expanded_case_word = remove_quotes_and_escapes_from_token(expanded_case_word);

    size_t esac_line_index = std::wstring::npos;
    int case_nesting_depth = 0;
    for (size_t probe = body_start; probe < end_index; ++probe) {
        const std::wstring probe_trimmed = trim_copy(lines[probe]);
        if (is_script_empty_or_comment_line(probe_trimmed)) {
            continue;
        }

        if (starts_with_case_control_keyword(probe_trimmed)) {
            case_nesting_depth++;
            continue;
        }

        if (to_lower_copy(probe_trimmed) == L"esac") {
            if (case_nesting_depth > 0) {
                case_nesting_depth--;
                continue;
            }
            esac_line_index = probe;
            break;
        }
    }

    if (esac_line_index == std::wstring::npos) {
        report_script_syntax_error(header_line_index, L"missing esac for case block");
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    bool execute_following_clauses = false;
    bool matched_any = false;
    size_t probe = body_start;
    while (probe < esac_line_index) {
        const std::wstring clause_line = trim_copy(lines[probe]);
        if (is_script_empty_or_comment_line(clause_line)) {
            probe++;
            continue;
        }

        const size_t close_paren = clause_line.find(L')');
        if (close_paren == std::wstring::npos) {
            report_script_syntax_error(probe, L"invalid case pattern clause");
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        std::wstring patterns_text = trim_copy(clause_line.substr(0, close_paren));
        std::wstring clause_remainder = trim_copy(clause_line.substr(close_paren + 1));

        std::vector<std::wstring> patterns;
        std::wstring current_pattern;
        bool in_single_quotes = false;
        bool in_double_quotes = false;
        bool escaped = false;
        for (wchar_t ch : patterns_text) {
            if (escaped) {
                current_pattern.push_back(ch);
                escaped = false;
                continue;
            }
            if (ch == L'\\' && !in_single_quotes) {
                current_pattern.push_back(ch);
                escaped = true;
                continue;
            }
            if (ch == L'\'' && !in_double_quotes) {
                in_single_quotes = !in_single_quotes;
                current_pattern.push_back(ch);
                continue;
            }
            if (ch == L'"' && !in_single_quotes) {
                in_double_quotes = !in_double_quotes;
                current_pattern.push_back(ch);
                continue;
            }
            if (ch == L'|' && !in_single_quotes && !in_double_quotes) {
                patterns.push_back(trim_copy(current_pattern));
                current_pattern.clear();
                continue;
            }
            current_pattern.push_back(ch);
        }
        if (!current_pattern.empty()) {
            patterns.push_back(trim_copy(current_pattern));
        }

        bool clause_matches = execute_following_clauses;
        if (!clause_matches) {
            for (const std::wstring& raw_pattern : patterns) {
                std::wstring expanded_pattern = expand_substitutions_left_to_right(raw_pattern);
                if (g_expansion_error) {
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
                expanded_pattern = remove_quotes_and_escapes_from_token(expanded_pattern);
                if (match_glob_pattern(expanded_pattern, expanded_case_word)) {
                    clause_matches = true;
                    matched_any = true;
                    break;
                }
            }
        }

        std::vector<std::wstring> clause_body_lines;
        std::wstring inline_terminator;
        if (!clause_remainder.empty()) {
            size_t term_pos = std::wstring::npos;
            std::wstring term;
            if ((term_pos = clause_remainder.find(L";;&")) != std::wstring::npos) {
                term = L";;&";
            } else if ((term_pos = clause_remainder.find(L";&")) != std::wstring::npos) {
                term = L";&";
            } else if ((term_pos = clause_remainder.find(L";;")) != std::wstring::npos) {
                term = L";;";
            }

            if (!term.empty()) {
                std::wstring inline_body = trim_copy(clause_remainder.substr(0, term_pos));
                if (!inline_body.empty()) {
                    clause_body_lines.push_back(inline_body);
                }
                inline_terminator = term;
            } else {
                clause_body_lines.push_back(clause_remainder);
            }
        }

        size_t terminator_line = probe;
        std::wstring terminator = L";;";
        if (!inline_terminator.empty()) {
            terminator = inline_terminator;
        } else {
            bool found_terminator = false;
            size_t scan = probe + 1;
            int nested_case_depth = 0;
            while (scan < esac_line_index) {
                const std::wstring body_line = trim_copy(lines[scan]);
                if (starts_with_case_control_keyword(body_line)) {
                    nested_case_depth++;
                    clause_body_lines.push_back(lines[scan]);
                    scan++;
                    continue;
                }

                const std::wstring lowered_body = to_lower_copy(body_line);
                if (lowered_body == L"esac" && nested_case_depth > 0) {
                    nested_case_depth--;
                    clause_body_lines.push_back(lines[scan]);
                    scan++;
                    continue;
                }

                if (nested_case_depth == 0 && (body_line == L";;" || body_line == L";&" || body_line == L";;&")) {
                    terminator_line = scan;
                    terminator = body_line;
                    found_terminator = true;
                    break;
                }
                clause_body_lines.push_back(lines[scan]);
                scan++;
            }
            if (!found_terminator) {
                report_script_syntax_error(probe, L"missing case clause terminator (;;, ;&, or ;;&)");
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
        }

        if (clause_matches) {
            if (!execute_script_lines_range(clause_body_lines, 0, clause_body_lines.size(), should_exit_shell, flow_control)) {
                return false;
            }
            if (should_exit_shell || is_function_return_requested()) {
                line_index = esac_line_index + 1;
                return true;
            }
        }

        if (terminator == L";;") {
            if (clause_matches) {
                line_index = esac_line_index + 1;
                return true;
            }
            execute_following_clauses = false;
        } else if (terminator == L";&") {
            execute_following_clauses = clause_matches;
        } else {
            // ;;& keeps pattern testing in following clauses.
            execute_following_clauses = false;
        }

        probe = terminator_line + 1;
    }

    if (!matched_any) {
        ksh_env.variables[L"?"] = L"0";
    }

    line_index = esac_line_index + 1;
    return true;
}

bool parse_for_header(
    const std::vector<std::wstring>& lines,
    size_t header_line_index,
    size_t end_index,
    std::wstring& variable_name,
    std::vector<std::wstring>& items,
    size_t& body_start,
    std::wstring& error_message,
    size_t& error_line_index) {
    error_message.clear();
    error_line_index = header_line_index;
    variable_name.clear();
    items.clear();

    const std::wstring header_line = trim_copy(lines[header_line_index]);
    std::wstring trailing = trim_copy(header_line.substr(3));

    std::wstring normalized = trailing;
    bool inline_do = false;
    if (ends_with_case_insensitive(normalized, L"; do")) {
        normalized = trim_copy(normalized.substr(0, normalized.size() - 4));
        inline_do = true;
    }

    std::vector<std::wstring> tokens = ksh_tokenize_preserve_quotes(normalized);
    if (tokens.empty()) {
        error_message = L"for requires a loop variable";
        return false;
    }

    variable_name = tokens[0];
    if (variable_name.empty() ||
        !(std::iswalpha(variable_name[0]) || variable_name[0] == L'_') ||
        !std::all_of(variable_name.begin(), variable_name.end(), [](wchar_t ch) {
            return std::iswalnum(ch) != 0 || ch == L'_';
        })) {
        error_message = L"invalid for loop variable: " + variable_name;
        return false;
    }

    if (tokens.size() == 1) {
        items = current_script_args();
    } else {
        if (to_lower_copy(tokens[1]) != L"in") {
            error_message = L"for header must use 'in' list form";
            return false;
        }

        for (size_t i = 2; i < tokens.size(); ++i) {
            std::wstring expanded_item = expand_substitutions_left_to_right(tokens[i]);
            if (g_expansion_error) {
                error_message = L"failed to expand for-loop item";
                return false;
            }

            std::vector<std::wstring> expanded_tokens = split_fields_by_ifs(expanded_item);
            expanded_tokens = expand_tilde_for_tokens(expanded_tokens);
            expanded_tokens = expand_globs_for_tokens(expanded_tokens, expanded_tokens);
            expanded_tokens = remove_quotes_and_escapes_from_tokens(expanded_tokens);
            if (expanded_tokens.empty()) {
                items.push_back(L"");
            } else {
                items.insert(items.end(), expanded_tokens.begin(), expanded_tokens.end());
            }
        }
    }

    if (inline_do) {
        body_start = header_line_index + 1;
        return true;
    }

    size_t probe = header_line_index + 1;
    while (probe < end_index) {
        const std::wstring probe_trimmed = trim_copy(lines[probe]);
        if (is_script_empty_or_comment_line(probe_trimmed)) {
            probe++;
            continue;
        }

        if (to_lower_copy(probe_trimmed) == L"do") {
            body_start = probe + 1;
            return true;
        }

        error_message = L"expected do";
        error_line_index = probe;
        return false;
    }

    error_message = L"missing do for for block";
    error_line_index = header_line_index;
    return false;
}

bool execute_script_select_block(
    const std::vector<std::wstring>& lines,
    size_t& line_index,
    size_t end_index,
    bool& should_exit_shell,
    ScriptFlowControl* flow_control) {
    const size_t header_line_index = line_index;
    std::wstring variable_name;
    std::vector<std::wstring> items;
    size_t body_start = header_line_index + 1;
    std::wstring parse_error;
    
    const std::wstring header_line = trim_copy(lines[header_line_index]);
    std::wstring trailing = trim_copy(header_line.substr(6));
    
    std::wstring normalized = trailing;
    bool inline_do = false;
    if (ends_with_case_insensitive(normalized, L"; do")) {
        normalized = trim_copy(normalized.substr(0, normalized.size() - 4));
        inline_do = true;
    }
    
    std::vector<std::wstring> tokens = ksh_tokenize_preserve_quotes(normalized);
    if (tokens.empty()) {
        report_script_syntax_error(header_line_index, L"select requires a loop variable");
        ksh_env.variables[L"?"] = L"1";
        return false;
    }
    
    variable_name = tokens[0];
    if (variable_name.empty() ||
        !(std::iswalpha(variable_name[0]) || variable_name[0] == L'_') ||
        !std::all_of(variable_name.begin(), variable_name.end(), [](wchar_t ch) {
            return std::iswalnum(ch) != 0 || ch == L'_';
        })) {
        report_script_syntax_error(header_line_index, L"invalid select loop variable: " + variable_name);
        ksh_env.variables[L"?"] = L"1";
        return false;
    }
    
    if (tokens.size() == 1) {
        items = current_script_args();
    } else {
        if (to_lower_copy(tokens[1]) != L"in") {
            report_script_syntax_error(header_line_index, L"select header must use 'in' list form");
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        
        for (size_t i = 2; i < tokens.size(); ++i) {
            std::wstring expanded_item = expand_substitutions_left_to_right(tokens[i]);
            if (g_expansion_error) {
                report_script_syntax_error(header_line_index, L"failed to expand select-loop item");
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            
            std::vector<std::wstring> expanded_tokens = split_fields_by_ifs(expanded_item);
            expanded_tokens = expand_tilde_for_tokens(expanded_tokens);
            expanded_tokens = expand_globs_for_tokens(expanded_tokens, expanded_tokens);
            expanded_tokens = remove_quotes_and_escapes_from_tokens(expanded_tokens);
            if (expanded_tokens.empty()) {
                items.push_back(L"");
            } else {
                items.insert(items.end(), expanded_tokens.begin(), expanded_tokens.end());
            }
        }
    }
    
    if (inline_do) {
        body_start = header_line_index + 1;
    } else {
        size_t probe = header_line_index + 1;
        bool found_do = false;
        while (probe < end_index) {
            const std::wstring probe_trimmed = trim_copy(lines[probe]);
            if (is_script_empty_or_comment_line(probe_trimmed)) {
                probe++;
                continue;
            }
            if (to_lower_copy(probe_trimmed) == L"do") {
                body_start = probe + 1;
                found_do = true;
                break;
            }
            report_script_syntax_error(probe, L"expected do");
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        if (!found_do) {
            report_script_syntax_error(header_line_index, L"missing do for select block");
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
    }
    
    size_t done_line_index = std::wstring::npos;
    if (!find_matching_done(lines, body_start, end_index, done_line_index)) {
        report_script_syntax_error(header_line_index, L"missing done for select block");
        ksh_env.variables[L"?"] = L"1";
        return false;
    }
    
    auto read_line_from_stdin = [](std::wstring& out_line) -> bool {
        out_line.clear();
        if (_isatty(_fileno(stdin))) {
            if (std::getline(std::wcin, out_line)) {
                return true;
            }
            return false;
        } else {
            HANDLE hStdin = (g_pipeline_stdin != INVALID_HANDLE_VALUE) ? g_pipeline_stdin : GetStdHandle(STD_INPUT_HANDLE);
            std::string line;
            char ch;
            DWORD bytes_read = 0;
            bool read_any = false;
            while (ReadFile(hStdin, &ch, 1, &bytes_read, nullptr) && bytes_read > 0) {
                read_any = true;
                if (ch == '\n') {
                    break;
                }
                if (ch != '\r') {
                    line.push_back(ch);
                }
            }
            if (!read_any) {
                return false;
            }
            int wlen = MultiByteToWideChar(CP_UTF8, 0, line.c_str(), -1, nullptr, 0);
            if (wlen > 0) {
                std::vector<wchar_t> wbuf(wlen);
                MultiByteToWideChar(CP_UTF8, 0, line.c_str(), -1, wbuf.data(), wlen);
                out_line = std::wstring(wbuf.data());
            }
            return true;
        }
    };

    while (true) {
        for (size_t idx = 0; idx < items.size(); ++idx) {
            std::wcerr << (idx + 1) << L") " << items[idx] << L"\n";
        }
        std::wcerr.flush();
        
        std::wstring prompt = L"#? ";
        wchar_t ps3[256];
        if (GetEnvironmentVariableW(L"PS3", ps3, 256) > 0) {
            prompt = ps3;
        } else {
            auto it = ksh_env.variables.find(L"PS3");
            if (it != ksh_env.variables.end()) {
                prompt = it->second;
            }
        }
        std::wcerr << prompt;
        std::wcerr.flush();
        
        std::wstring input;
        if (!read_line_from_stdin(input)) {
            break;
        }
        
        std::wstring reply = trim_copy(input);
        ksh_env.variables[L"REPLY"] = reply;
        SetEnvironmentVariableW(L"REPLY", reply.c_str());
        
        std::wstring selected_value;
        int choice = 0;
        if (try_parse_int_strict(reply, choice) && choice >= 1 && choice <= static_cast<int>(items.size())) {
            selected_value = items[choice - 1];
        }
        
        ksh_env.variables[variable_name] = selected_value;
        SetEnvironmentVariableW(variable_name.c_str(), selected_value.c_str());
        
        ScriptFlowControl local_flow;
        ScriptFlowControl* body_flow = (flow_control != nullptr) ? flow_control : &local_flow;
        if (!execute_script_lines_range(lines, body_start, done_line_index, should_exit_shell, body_flow)) {
            return false;
        }
        
        if (should_exit_shell) {
            line_index = done_line_index + 1;
            return true;
        }
        if (is_function_return_requested()) {
            line_index = done_line_index + 1;
            return true;
        }
        
        if (body_flow->break_levels > 0) {
            body_flow->break_levels--;
            break;
        }
        if (body_flow->continue_levels > 0) {
            body_flow->continue_levels--;
        }
    }
    
    line_index = done_line_index + 1;
    return true;
}

bool execute_script_for_block(
    const std::vector<std::wstring>& lines,
    size_t& line_index,
    size_t end_index,
    bool& should_exit_shell,
    ScriptFlowControl* flow_control) {
    const size_t header_line_index = line_index;
    std::wstring variable_name;
    std::vector<std::wstring> items;
    size_t body_start = header_line_index + 1;
    std::wstring parse_error;
    size_t parse_error_line = header_line_index;

    if (!parse_for_header(lines, header_line_index, end_index, variable_name, items, body_start, parse_error, parse_error_line)) {
        report_script_syntax_error(parse_error_line, parse_error);
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    size_t done_line_index = std::wstring::npos;
    if (!find_matching_done(lines, body_start, end_index, done_line_index)) {
        report_script_syntax_error(header_line_index, L"missing done for for block");
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    std::wstring redirection_part;
    RedirectionSpec loop_redir;
    bool has_redir = false;
    if (is_loop_done_keyword(lines[done_line_index], redirection_part) && !redirection_part.empty()) {
        parse_and_replace_process_substitutions(redirection_part);
        std::vector<std::wstring> redir_tokens = ksh_tokenize_preserve_quotes(redirection_part);
        redir_tokens = remove_quotes_and_escapes_from_tokens(redir_tokens);
        std::wstring err;
        if (parse_redirections(redir_tokens, loop_redir, err)) {
            has_redir = true;
        } else {
            std::wcerr << L"ksh: for: loop redirection parse error: " << err << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
    }

    std::unique_ptr<BlockRedirector> block_redir;
    if (has_redir) {
        block_redir = std::make_unique<BlockRedirector>(loop_redir);
        if (!block_redir->is_valid(loop_redir)) {
            std::wcerr << L"ksh: for: loop redirection setup failed\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
    }

    for (const std::wstring& item : items) {
        ksh_env.variables[variable_name] = item;
        SetEnvironmentVariableW(variable_name.c_str(), item.c_str());

        ScriptFlowControl local_flow;
        ScriptFlowControl* body_flow = (flow_control != nullptr) ? flow_control : &local_flow;
        if (!execute_script_lines_range(lines, body_start, done_line_index, should_exit_shell, body_flow)) {
            return false;
        }
        if (should_exit_shell) {
            line_index = done_line_index + 1;
            return true;
        }
        if (is_function_return_requested()) {
            line_index = done_line_index + 1;
            return true;
        }

        if (body_flow->break_levels > 0) {
            body_flow->break_levels--;
            line_index = done_line_index + 1;
            return true;
        }

        if (body_flow->continue_levels > 0) {
            body_flow->continue_levels--;
            if (body_flow->continue_levels > 0) {
                line_index = done_line_index + 1;
                return true;
            }
            continue;
        }
    }

    line_index = done_line_index + 1;
    return true;
}

bool execute_script_lines_range(
    const std::vector<std::wstring>& lines,
    size_t start_index,
    size_t end_index,
    bool& should_exit_shell,
    ScriptFlowControl* flow_control) {
    size_t line_index = start_index;
    while (line_index < end_index) {
        const std::wstring trimmed = trim_copy(lines[line_index]);
        if (is_script_empty_or_comment_line(trimmed)) {
            line_index++;
            continue;
        }

        std::wstring function_parse_error;
        size_t function_parse_error_line = line_index;
        size_t function_line = line_index;
        if (capture_function_definition(lines, line_index, end_index, function_parse_error, function_parse_error_line)) {
            if (line_index <= function_line) {
                report_script_syntax_error(function_line, L"failed to advance after function definition");
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            continue;
        }
        if (!function_parse_error.empty()) {
            report_script_syntax_error(function_parse_error_line, function_parse_error);
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        if (starts_with_if_control_keyword(trimmed)) {
            if (!execute_script_if_block(lines, line_index, end_index, should_exit_shell, flow_control)) {
                return false;
            }
            if (should_exit_shell) {
                return true;
            }
            if (is_function_return_requested()) {
                return true;
            }
            if (flow_control != nullptr && (flow_control->break_levels > 0 || flow_control->continue_levels > 0)) {
                return true;
            }
            continue;
        }

        if (starts_with_while_control_keyword(trimmed)) {
            if (!execute_script_while_until_block(lines, line_index, end_index, false, should_exit_shell, flow_control)) {
                return false;
            }
            if (should_exit_shell) {
                return true;
            }
            if (is_function_return_requested()) {
                return true;
            }
            if (flow_control != nullptr && (flow_control->break_levels > 0 || flow_control->continue_levels > 0)) {
                return true;
            }
            continue;
        }

        if (starts_with_until_control_keyword(trimmed)) {
            if (!execute_script_while_until_block(lines, line_index, end_index, true, should_exit_shell, flow_control)) {
                return false;
            }
            if (should_exit_shell) {
                return true;
            }
            if (is_function_return_requested()) {
                return true;
            }
            if (flow_control != nullptr && (flow_control->break_levels > 0 || flow_control->continue_levels > 0)) {
                return true;
            }
            continue;
        }

        if (starts_with_select_control_keyword(trimmed)) {
            if (!execute_script_select_block(lines, line_index, end_index, should_exit_shell, flow_control)) {
                return false;
            }
            if (should_exit_shell) {
                return true;
            }
            if (is_function_return_requested()) {
                return true;
            }
            if (flow_control != nullptr && (flow_control->break_levels > 0 || flow_control->continue_levels > 0)) {
                return true;
            }
            continue;
        }

        if (starts_with_for_control_keyword(trimmed)) {
            if (!execute_script_for_block(lines, line_index, end_index, should_exit_shell, flow_control)) {
                return false;
            }
            if (should_exit_shell) {
                return true;
            }
            if (is_function_return_requested()) {
                return true;
            }
            if (flow_control != nullptr && (flow_control->break_levels > 0 || flow_control->continue_levels > 0)) {
                return true;
            }
            continue;
        }

        if (starts_with_case_control_keyword(trimmed)) {
            if (!execute_script_case_block(lines, line_index, end_index, should_exit_shell, flow_control)) {
                return false;
            }
            if (should_exit_shell) {
                return true;
            }
            if (is_function_return_requested()) {
                return true;
            }
            if (flow_control != nullptr && (flow_control->break_levels > 0 || flow_control->continue_levels > 0)) {
                return true;
            }
            continue;
        }

        const std::wstring lowered = to_lower_copy(trimmed);
        if (lowered == L"then" || lowered == L"else" || lowered == L"fi" || lowered == L"do" || lowered == L"done" || lowered == L"elif" || lowered == L"in" || lowered == L"esac") {
            report_script_syntax_error(line_index, L"unexpected control keyword: " + lowered);
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        if (lowered.rfind(L"break", 0) == 0 || lowered.rfind(L"continue", 0) == 0) {
            std::vector<std::wstring> flow_tokens = ksh_tokenize(trimmed);
            if (flow_tokens.empty()) {
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            const std::wstring command = to_lower_copy(flow_tokens[0]);
            if (command == L"break" || command == L"continue") {
                if (flow_control == nullptr) {
                    report_script_syntax_error(line_index, command + L" is only valid inside a loop");
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }

                int level_count = 1;
                if (flow_tokens.size() > 2) {
                    report_script_syntax_error(line_index, command + L" takes at most one numeric argument");
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
                if (flow_tokens.size() == 2) {
                    int parsed = 0;
                    if (!try_parse_positive_int_strict(flow_tokens[1], parsed)) {
                        report_script_syntax_error(line_index, command + L" requires a positive integer argument");
                        ksh_env.variables[L"?"] = L"1";
                        return false;
                    }
                    level_count = parsed;
                }

                if (command == L"break") {
                    flow_control->break_levels = level_count;
                } else {
                    flow_control->continue_levels = level_count;
                }
                ksh_env.variables[L"?"] = L"0";
                return true;
            }
        }

        std::wstring command_to_execute = trimmed;
        std::wstring heredoc_temp_file;
        bool consume_heredoc_lines = false;
        size_t heredoc_end_line = line_index;

        std::vector<std::wstring> heredoc_tokens = ksh_tokenize_preserve_quotes(trimmed);
        size_t heredoc_op_index = std::wstring::npos;
        bool heredoc_strip_tabs = false;
        std::wstring heredoc_delimiter;
        bool heredoc_disable_expansion = false;

        for (size_t t = 0; t < heredoc_tokens.size(); ++t) {
            const std::wstring& token = heredoc_tokens[t];
            bool is_op = false;
            size_t op_len = 0;
            if (token == L"<<") {
                is_op = true;
                op_len = 2;
            } else if (token == L"<<-") {
                is_op = true;
                op_len = 3;
                heredoc_strip_tabs = true;
            } else if (token.rfind(L"<<-", 0) == 0) {
                is_op = true;
                op_len = 3;
                heredoc_strip_tabs = true;
            } else if (token.rfind(L"<<", 0) == 0) {
                is_op = true;
                op_len = 2;
            }

            if (!is_op) {
                continue;
            }

            heredoc_op_index = t;
            if (token.size() > op_len) {
                heredoc_delimiter = token.substr(op_len);
            } else if (t + 1 < heredoc_tokens.size()) {
                heredoc_delimiter = heredoc_tokens[t + 1];
            }
            break;
        }

        if (heredoc_op_index != std::wstring::npos) {
            if (heredoc_delimiter.empty()) {
                report_script_syntax_error(line_index, L"missing here-doc delimiter");
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            std::wstring raw_delimiter = heredoc_delimiter;
            if ((raw_delimiter.size() >= 2) &&
                ((raw_delimiter.front() == L'"' && raw_delimiter.back() == L'"') ||
                 (raw_delimiter.front() == L'\'' && raw_delimiter.back() == L'\''))) {
                heredoc_disable_expansion = true;
                raw_delimiter = raw_delimiter.substr(1, raw_delimiter.size() - 2);
            }

            std::wstring heredoc_content;
            size_t probe = line_index + 1;
            bool found_delimiter = false;
            for (; probe < end_index; ++probe) {
                std::wstring candidate = lines[probe];
                std::wstring compare = candidate;
                if (heredoc_strip_tabs) {
                    while (!compare.empty() && compare.front() == L'\t') {
                        compare.erase(compare.begin());
                    }
                }

                if (compare == raw_delimiter) {
                    found_delimiter = true;
                    break;
                }

                std::wstring content_line = heredoc_strip_tabs ? compare : candidate;
                if (!heredoc_disable_expansion) {
                    content_line = expand_substitutions_left_to_right(content_line);
                    if (g_expansion_error) {
                        ksh_env.variables[L"?"] = L"1";
                        return false;
                    }
                }

                if (heredoc_content.size() + content_line.size() + 1 > kMaxHereDocContentBytes) {
                    report_script_syntax_error(line_index, L"here-doc content exceeds configured limit");
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }

                heredoc_content += content_line;
                heredoc_content += L"\n";
            }

            if (!found_delimiter) {
                report_script_syntax_error(line_index, L"missing here-doc terminator: " + raw_delimiter);
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            wchar_t temp_dir[MAX_PATH];
            DWORD dir_len = GetTempPathW(MAX_PATH, temp_dir);
            if (dir_len == 0 || dir_len >= MAX_PATH) {
                report_script_syntax_error(line_index, L"failed to allocate here-doc temp path");
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            wchar_t temp_name[MAX_PATH];
            if (GetTempFileNameW(temp_dir, L"ksh", 0, temp_name) == 0) {
                report_script_syntax_error(line_index, L"failed to create here-doc temp file");
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            heredoc_temp_file = temp_name;
            std::wofstream out(heredoc_temp_file.c_str(), std::ios::binary | std::ios::trunc);
            if (!out.is_open()) {
                DeleteFileW(heredoc_temp_file.c_str());
                report_script_syntax_error(line_index, L"failed to write here-doc temp file");
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            out << heredoc_content;
            out.close();

            std::vector<std::wstring> rebuilt_tokens;
            for (size_t t = 0; t < heredoc_tokens.size(); ++t) {
                if (t == heredoc_op_index) {
                    const std::wstring& op_token = heredoc_tokens[t];
                    if (op_token == L"<<" || op_token == L"<<-") {
                        if ((t + 1) < heredoc_tokens.size()) {
                            t++;
                        }
                    }
                    continue;
                }

                if ((t == heredoc_op_index + 1) &&
                    (heredoc_tokens[heredoc_op_index] == L"<<" || heredoc_tokens[heredoc_op_index] == L"<<-")) {
                    continue;
                }

                rebuilt_tokens.push_back(heredoc_tokens[t]);
            }
            rebuilt_tokens.push_back(L"<");
            rebuilt_tokens.push_back(L"\"" + heredoc_temp_file + L"\"");
            command_to_execute.clear();
            for (size_t i = 0; i < rebuilt_tokens.size(); ++i) {
                if (!command_to_execute.empty()) {
                    command_to_execute += L" ";
                }
                command_to_execute += rebuilt_tokens[i];
            }
            consume_heredoc_lines = true;
            heredoc_end_line = probe;
        }

        if (!execute_command_line(command_to_execute, should_exit_shell)) {
            if (!heredoc_temp_file.empty()) {
                DeleteFileW(heredoc_temp_file.c_str());
            }
            return false;
        }
        if (!heredoc_temp_file.empty()) {
            DeleteFileW(heredoc_temp_file.c_str());
        }
        if (should_exit_shell) {
            return true;
        }
        if (is_function_return_requested()) {
            return true;
        }

        if (consume_heredoc_lines) {
            line_index = heredoc_end_line;
        }

        line_index++;
    }

    return true;
}

bool execute_script_file(const std::wstring& script_path, const std::vector<std::wstring>& script_args, bool& should_exit_shell) {
    ScriptContext context;
    context.script_name = script_path;
    context.args = script_args;
    g_script_context_stack.push_back(context);

    std::ifstream script_file(script_path.c_str(), std::ios::binary);
    if (!script_file.is_open()) {
        std::wcerr << L"ksh: cannot open script: " << script_path << L"\n";
        ksh_env.variables[L"?"] = L"1";
        g_script_context_stack.pop_back();
        return false;
    }

    script_file.seekg(0, std::ios::end);
    std::streampos script_size = script_file.tellg();
    script_file.seekg(0, std::ios::beg);
    if (script_size < 0 || static_cast<size_t>(script_size) > kMaxScriptFileSizeBytes) {
        std::wcerr << L"ksh: script file too large: " << script_path << L"\n";
        ksh_env.variables[L"?"] = L"1";
        g_script_context_stack.pop_back();
        return false;
    }

    std::vector<char> bytes((std::istreambuf_iterator<char>(script_file)), std::istreambuf_iterator<char>());
    std::wstring script_text = decode_script_text(bytes);
    std::vector<std::wstring> lines = prepare_script_lines(script_text);

    size_t start_index = 0;
    if (!lines.empty() && lines[0].rfind(L"#!", 0) == 0) {
        start_index = 1;
    }

    if (!execute_script_lines_range(lines, start_index, lines.size(), should_exit_shell)) {
        g_script_context_stack.pop_back();
        return false;
    }
    if (should_exit_shell) {
        g_script_context_stack.pop_back();
        return true;
    }

    g_script_context_stack.pop_back();
    return true;
}

// Function execution and scope restore.
bool execute_defined_function(const std::wstring& function_name, const std::vector<std::wstring>& function_args, bool& should_exit_shell) {
    std::map<std::wstring, ShellFunctionDefinition>::const_iterator fn_it = g_shell_functions.find(function_name);
    if (fn_it == g_shell_functions.end()) {
        return false;
    }

    if (static_cast<int>(g_function_scope_stack.size()) >= kMaxFunctionCallDepth) {
        std::wcerr << L"ksh: " << function_name << L": maximum function call depth exceeded\n";
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    g_function_scope_stack.emplace_back();
    g_function_scope_stack.back().locals.reserve(function_args.size() + 8);

    g_script_context_stack.emplace_back();
    g_script_context_stack.back().script_name = function_name;
    g_script_context_stack.back().args = function_args;

    bool ok = execute_script_lines_range(fn_it->second.body_lines, 0, fn_it->second.body_lines.size(), should_exit_shell, nullptr);

    FunctionScopeContext finished_scope = std::move(g_function_scope_stack.back());
    g_function_scope_stack.pop_back();
    restore_function_scope_variables(finished_scope);

    g_script_context_stack.pop_back();

    if (!ok) {
        if (ksh_env.variables.find(L"?") == ksh_env.variables.end() || ksh_env.variables[L"?"] == L"0") {
            ksh_env.variables[L"?"] = L"1";
        }
        return false;
    }

    if (finished_scope.return_requested) {
        ksh_env.variables[L"?"] = std::to_wstring(finished_scope.return_status);
        return true;
    }

    if (ksh_env.variables.find(L"?") == ksh_env.variables.end()) {
        ksh_env.variables[L"?"] = L"0";
    }

    return true;
}

std::wstring decode_raw_bytes(const std::string& bytes) {
    if (bytes.empty()) return L"";

    bool is_utf16 = false;
    if (bytes.size() >= 2) {
        if (static_cast<unsigned char>(bytes[0]) == 0xFF && static_cast<unsigned char>(bytes[1]) == 0xFE) {
            is_utf16 = true;
        } else if (bytes.size() % 2 == 0) {
            size_t null_count = 0;
            for (size_t i = 1; i < bytes.size(); i += 2) {
                if (bytes[i] == '\0') null_count++;
            }
            if (null_count == bytes.size() / 2) {
                is_utf16 = true;
            }
        }
    }

    if (is_utf16) {
        std::wstring wstr;
        size_t start = 0;
        if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF && static_cast<unsigned char>(bytes[1]) == 0xFE) {
            start = 2;
        }
        wstr.resize((bytes.size() - start) / 2);
        if (!wstr.empty()) {
            memcpy(&wstr[0], bytes.data() + start, wstr.size() * 2);
        }
        return wstr;
    }

    int wlen = MultiByteToWideChar(CP_UTF8, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
    if (wlen > 0) {
        std::wstring wstr(wlen, 0);
        MultiByteToWideChar(CP_UTF8, 0, bytes.data(), static_cast<int>(bytes.size()), &wstr[0], wlen);
        return wstr;
    }

    int alen = MultiByteToWideChar(CP_ACP, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
    if (alen > 0) {
        std::wstring wstr(alen, 0);
        MultiByteToWideChar(CP_ACP, 0, bytes.data(), static_cast<int>(bytes.size()), &wstr[0], alen);
        return wstr;
    }

    std::wstring wstr;
    for (char c : bytes) {
        wstr.push_back(static_cast<wchar_t>(c));
    }
    return wstr;
}

// Builtin input and help handlers.
bool execute_builtin_read(const std::vector<std::wstring>& tokens) {
    std::wstring prompt = L"";
    bool silent = false;
    bool raw = false;
    bool array_mode = false;
    bool use_coprocess_input = false;
    int max_chars = -1;
    double timeout_sec = -1.0;
    wchar_t delim = L'\n';
    std::vector<std::wstring> var_names;

    for (size_t i = 1; i < tokens.size(); ++i) {
        std::wstring arg = tokens[i];
        if (arg.size() > 1 && arg[0] == L'-') {
            for (size_t j = 1; j < arg.size(); ++j) {
                wchar_t flag = arg[j];
                bool consumed_option_argument = false;
                if (flag == L'r') {
                    raw = true;
                } else if (flag == L's') {
                    silent = true;
                } else if (flag == L'A') {
                    array_mode = true;
                } else if (flag == L'p') {
                    const bool has_attached_prompt =
                        j + 1 < arg.size() &&
                        arg[j + 1] != L'r' && arg[j + 1] != L's' && arg[j + 1] != L'A' &&
                        arg[j + 1] != L'p' && arg[j + 1] != L'n' && arg[j + 1] != L't' &&
                        arg[j + 1] != L'd';
                    if (!has_attached_prompt) {
                        if (!g_coprocess.active) {
                            std::wcerr << L"ksh: read -p: no active co-process\n";
                            ksh_env.variables[L"?"] = L"1";
                            return false;
                        }
                        use_coprocess_input = true;
                    } else if (j + 1 < arg.size()) {
                        prompt = arg.substr(j + 1);
                        consumed_option_argument = true;
                    } else if (i + 1 < tokens.size()) {
                        prompt = tokens[++i];
                        consumed_option_argument = true;
                    } else {
                        std::wcerr << L"ksh: read: -p requires an argument\n";
                        ksh_env.variables[L"?"] = L"1";
                        return false;
                    }
                } else if (flag == L'n') {
                    std::wstring n_val;
                    if (j + 1 < arg.size()) {
                        n_val = arg.substr(j + 1);
                        consumed_option_argument = true;
                    } else if (i + 1 < tokens.size()) {
                        n_val = tokens[++i];
                        consumed_option_argument = true;
                    } else {
                        std::wcerr << L"ksh: read: -n requires an argument\n";
                        ksh_env.variables[L"?"] = L"1";
                        return false;
                    }
                    if (!try_parse_int_strict(n_val, max_chars)) {
                        std::wcerr << L"ksh: read: invalid nchars\n";
                        ksh_env.variables[L"?"] = L"1";
                        return false;
                    }
                } else if (flag == L't') {
                    std::wstring t_val;
                    if (j + 1 < arg.size()) {
                        t_val = arg.substr(j + 1);
                        consumed_option_argument = true;
                    } else if (i + 1 < tokens.size()) {
                        t_val = tokens[++i];
                        consumed_option_argument = true;
                    } else {
                        std::wcerr << L"ksh: read: -t requires an argument\n";
                        ksh_env.variables[L"?"] = L"1";
                        return false;
                    }
                    try {
                        timeout_sec = std::stod(t_val);
                    } catch (...) {
                        std::wcerr << L"ksh: read: invalid timeout\n";
                        ksh_env.variables[L"?"] = L"1";
                        return false;
                    }
                } else if (flag == L'd') {
                    std::wstring d_val;
                    if (j + 1 < arg.size()) {
                        d_val = arg.substr(j + 1);
                        consumed_option_argument = true;
                    } else if (i + 1 < tokens.size()) {
                        d_val = tokens[++i];
                        consumed_option_argument = true;
                    } else {
                        std::wcerr << L"ksh: read: -d requires an argument\n";
                        ksh_env.variables[L"?"] = L"1";
                        return false;
                    }
                    if (!d_val.empty()) {
                        delim = d_val[0];
                    }
                } else {
                    std::wcerr << L"ksh: read: invalid option: -" << flag << L"\n";
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
                if (consumed_option_argument) {
                    break;
                }
            }
        } else {
            var_names.push_back(arg);
        }
    }

    if (var_names.empty()) {
        if (array_mode) {
            std::wcerr << L"ksh: read: -A requires an array name\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        var_names.push_back(L"REPLY");
    }

    if (array_mode && var_names.size() != 1) {
        std::wcerr << L"ksh: read: -A accepts exactly one array name\n";
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    for (const auto& name : var_names) {
        if (!is_valid_shell_identifier(name)) {
            std::wcerr << L"ksh: read: invalid variable name: " << name << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
    }

    if (!prompt.empty() && !use_coprocess_input) {
        std::wcerr << prompt;
        std::wcerr.flush();
    }

    std::wstring result;
    int read_status = 0;
    HANDLE hIn = use_coprocess_input
        ? g_coprocess.output_read
        : ((g_pipeline_stdin != INVALID_HANDLE_VALUE) ? g_pipeline_stdin : GetStdHandle(STD_INPUT_HANDLE));
    DWORD mode;
    bool is_console = !use_coprocess_input && GetConsoleMode(hIn, &mode) != FALSE;

    auto start_time = std::chrono::steady_clock::now();
    bool escape_next = false;

    if (is_console) {
        DWORD origMode = 0;
        bool mode_set = false;
        if (GetConsoleMode(hIn, &origMode)) {
            DWORD newMode = origMode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
            if (SetConsoleMode(hIn, newMode)) {
                mode_set = true;
            }
        }

        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);

        while (true) {
            if (max_chars > 0 && static_cast<int>(result.length()) >= max_chars) {
                break;
            }

            DWORD wait_ms = INFINITE;
            if (timeout_sec >= 0.0) {
                auto now = std::chrono::steady_clock::now();
                double elapsed = std::chrono::duration<double>(now - start_time).count();
                double remaining = timeout_sec - elapsed;
                if (remaining <= 0.0) {
                    read_status = 142;
                    break;
                }
                wait_ms = static_cast<DWORD>(remaining * 1000.0);
            }

            DWORD wait_res = WaitForSingleObject(hIn, wait_ms);
            if (wait_res == WAIT_TIMEOUT) {
                read_status = 142;
                break;
            }
            if (wait_res != WAIT_OBJECT_0) {
                read_status = 1;
                break;
            }

            INPUT_RECORD ir;
            DWORD records_read = 0;
            if (!ReadConsoleInputW(hIn, &ir, 1, &records_read) || records_read == 0) {
                read_status = 1;
                break;
            }

            if (ir.EventType != KEY_EVENT || !ir.Event.KeyEvent.bKeyDown) {
                continue;
            }

            wchar_t ch = ir.Event.KeyEvent.uChar.UnicodeChar;
            WORD vk = ir.Event.KeyEvent.wVirtualKeyCode;

            if (ch == 0) continue;
            if (ch == 3) {
                read_status = 130;
                break;
            }
            if (ch == 26) {
                break;
            }

            if (vk == VK_BACK || ch == L'\b') {
                if (!result.empty()) {
                    result.pop_back();
                    if (!silent) {
                        DWORD written;
                        WriteConsoleW(hOut, L"\b \b", 3, &written, NULL);
                    }
                }
                continue;
            }

            if (escape_next && (ch == L'\r' || ch == L'\n')) {
                escape_next = false;
                if (!silent) {
                    DWORD written;
                    WriteConsoleW(hOut, L"\r\n> ", 4, &written, NULL);
                }
                continue;
            }

            bool is_delim = false;
            if (delim == L'\n' && (ch == L'\r' || ch == L'\n')) {
                is_delim = true;
            } else if (ch == delim) {
                is_delim = true;
            }

            if (is_delim) {
                if (!silent) {
                    DWORD written;
                    WriteConsoleW(hOut, L"\r\n", 2, &written, NULL);
                }
                break;
            }

            if (!raw && !escape_next && ch == L'\\') {
                escape_next = true;
                continue;
            }

            escape_next = false;
            result.push_back(ch);

            if (!silent) {
                DWORD written;
                WriteConsoleW(hOut, &ch, 1, &written, NULL);
            }
        }

        if (mode_set) {
            SetConsoleMode(hIn, origMode);
        }
    } else {
        std::string raw_bytes;
        while (true) {
            if (max_chars > 0 && static_cast<int>(raw_bytes.length()) >= max_chars) {
                break;
            }

            if (timeout_sec >= 0.0) {
                auto now = std::chrono::steady_clock::now();
                double elapsed = std::chrono::duration<double>(now - start_time).count();
                if (elapsed >= timeout_sec) {
                    read_status = 142;
                    break;
                }

                DWORD avail = 0;
                if (PeekNamedPipe(hIn, NULL, 0, NULL, &avail, NULL) && avail == 0) {
                    Sleep(10);
                    continue;
                }
            }

            char c_char;
            DWORD bytesRead = 0;
            if (!ReadFile(hIn, &c_char, 1, &bytesRead, NULL) || bytesRead == 0) {
                if (use_coprocess_input) {
                    if (!raw_bytes.empty()) {
                        break;
                    }

                    DWORD exit_code = 1;
                    if (g_coprocess.process != INVALID_HANDLE_VALUE) {
                        GetExitCodeProcess(g_coprocess.process, &exit_code);
                    }
                    close_coprocess(false);
                    ksh_env.variables[L"COPROC_ACTIVE"] = L"0";
                    ksh_env.variables[L"?"] = std::to_wstring(exit_code == STILL_ACTIVE ? 1 : exit_code);
                    return false;
                }
                read_status = 1;
                break;
            }

            raw_bytes.push_back(c_char);

            bool is_delim = false;
            if (delim == L'\n') {
                if (c_char == '\n') {
                    is_delim = true;
                }
            } else {
                if (static_cast<wchar_t>(c_char) == delim) {
                    is_delim = true;
                }
            }

            if (is_delim) {
                if (raw_bytes.size() >= 2 && raw_bytes[raw_bytes.size() - 2] == '\n' && raw_bytes.back() == '\0') {
                    // Handled
                } else if (delim == L'\n') {
                    char next_c;
                    DWORD peekRead = 0;
                    if (PeekNamedPipe(hIn, &next_c, 1, &peekRead, NULL, NULL) && peekRead > 0 && next_c == '\0') {
                        ReadFile(hIn, &next_c, 1, &peekRead, NULL);
                        raw_bytes.push_back(next_c);
                    }
                }
                break;
            }
        }
        result = decode_raw_bytes(raw_bytes);
    }

    if (read_status != 0 && read_status != 130) {
        ksh_env.variables[L"?"] = std::to_wstring(read_status);
        return false;
    }
    if (read_status == 130) {
        ksh_env.variables[L"?"] = L"130";
        return false;
    }

    std::wstring ifs = effective_ifs_value();
    auto is_ifs = [&](wchar_t ch) {
        return ifs.find(ch) != std::wstring::npos;
    };

    std::vector<std::wstring> words;
    std::wstring current_word;
    size_t char_idx = 0;

    while (char_idx < result.length() && is_ifs(result[char_idx])) {
        char_idx++;
    }

    while (char_idx < result.length()) {
        if (words.size() + 1 == var_names.size()) {
            std::wstring remainder = result.substr(char_idx);
            while (!remainder.empty() && is_ifs(remainder.back())) {
                remainder.pop_back();
            }
            words.push_back(remainder);
            break;
        }

        if (is_ifs(result[char_idx])) {
            words.push_back(current_word);
            current_word.clear();
            while (char_idx < result.length() && is_ifs(result[char_idx])) {
                char_idx++;
            }
        } else {
            current_word.push_back(result[char_idx]);
            char_idx++;
        }
    }
    if (words.size() < var_names.size() && !current_word.empty()) {
        words.push_back(current_word);
    }

    while (words.size() < var_names.size()) {
        words.push_back(L"");
    }

    if (array_mode) {
        const std::wstring& array_name = var_names[0];
        std::wstring resolved_array_name = resolve_variable_name(array_name);
        if (is_executing_function_scope()) {
            snapshot_local_variable_if_needed(resolved_array_name);
        }
        if (get_flag_value(ksh_env.readonly_flags, resolved_array_name)) {
            std::wcerr << L"ksh: read: variable is read-only: " << resolved_array_name << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        assign_array_parameter(resolved_array_name, words);
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    bool assign_ok = true;
    for (size_t i = 0; i < var_names.size(); ++i) {
        std::wstring err;
        if (!assign_parameter_value(var_names[i], words[i], false, is_executing_function_scope(), err)) {
            std::wcerr << L"ksh: read: " << err << L"\n";
            assign_ok = false;
        }
    }

    ksh_env.variables[L"?"] = assign_ok ? L"0" : L"1";
    return assign_ok;
}

bool execute_builtin_help(const std::vector<std::wstring>& tokens, const std::function<bool(const std::wstring&)>& write_output) {
    if (tokens.size() > 2) {
        std::wcerr << L"ksh: help: too many arguments. Usage: help [command]\n";
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    if (tokens.size() == 2) {
        std::wstring target = tokens[1];

        if (target == L"cd") {
            if (!write_help_lines(write_output, kHelpCdCommandLines) || !write_help_lines(write_output, kHelpCdCommandLinesTail)) {
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        if (target == L"kill") {
            if (!write_help_lines(write_output, kHelpKillCommandLines) || !write_help_lines(write_output, kHelpKillCommandLinesTail)) {
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        if (target == L"prompt" || target == L"PS1") {
            if (!write_help_lines(write_output, kHelpPromptCommandLines) || !write_help_lines(write_output, kHelpPromptCommandLinesTail)) {
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        if (target == L"break" || target == L"continue") {
            if (!write_output(target + L"\n") || !write_help_lines(write_output, kHelpLoopKeywordLines)) {
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        const std::wstring lowered_target = to_lower_copy(target);
        if (lowered_target == L"all" || lowered_target == L"full" || lowered_target == L"reference") {
            write_full_help_text(write_output);
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        if (write_help_topic(target, write_output)) {
            ksh_env.variables[L"?"] = L"0";
            return true;
        }
        
        if (const BuiltinCommandHelp* entry = find_builtin_command_help(target)) {
            write_output(std::wstring(entry->name) + L" - " + entry->description + L"\n");
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        std::map<std::wstring, ShellFunctionDefinition>::const_iterator fn_it = g_shell_functions.find(target);
        if (fn_it != g_shell_functions.end()) {
            std::wstring body = L"function " + target + L"\n{\n";
            for (const auto& line : fn_it->second.body_lines) {
                body += L"    " + line + L"\n";
            }
            body += L"}\n";
            write_output(body);
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        std::wcerr << L"ksh: help: no help found for: " << target << L"\n";
        ksh_env.variables[L"?"] = L"1";
        return false;
    }
    write_output(L"\n");
    write_help_topic_index(write_output);
    write_output(L"\nCrossShellKSH Commands:\n");
    if (!write_grouped_builtin_help(write_output, false)) {
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    write_output(L"\nPrompt customization:\n");
    write_output(L"  Set PS1 in ~/.kshrc to customize prompt order/content.\n");
    write_output(L"  Tokens: %u user, %d domain, %w cwd, %m host, %# role-char, %% literal-percent\n");
    if (!write_help_lines(write_output, kHelpPromptCustomizationShortLines)) {
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    write_output(L"\n");
    
    ksh_env.variables[L"?"] = L"0";
    return true;
}

bool parse_custom_type_definition(const std::wstring& type_name, const std::wstring& body, CustomTypeDefinition& def) {
    def.name = type_name;
    std::vector<std::wstring> tokens = ksh_tokenize_preserve_quotes(body);
    
    size_t i = 0;
    while (i < tokens.size()) {
        if (tokens[i] == L";" || tokens[i] == L"\n") {
            i++;
            continue;
        }
        
        std::wstring type_kw = tokens[i];
        bool is_integer = false;
        bool is_array = false;
        bool is_associative = false;
        std::wstring member_type_name = L"";
        
        if (type_kw == L"integer") {
            is_integer = true;
            i++;
        } else if (type_kw == L"typeset") {
            i++;
            while (i < tokens.size() && tokens[i][0] == L'-') {
                std::wstring opt = tokens[i];
                for (size_t j = 1; j < opt.size(); ++j) {
                    if (opt[j] == L'i') is_integer = true;
                    else if (opt[j] == L'a') is_array = true;
                    else if (opt[j] == L'A') is_associative = true;
                }
                i++;
            }
        } else if (g_custom_types.find(type_kw) != g_custom_types.end()) {
            member_type_name = type_kw;
            i++;
        } else {
            // Default variable type
        }
        
        while (i < tokens.size() && tokens[i] != L";" && tokens[i] != L"\n" &&
               tokens[i] != L"integer" && tokens[i] != L"typeset" &&
               g_custom_types.find(tokens[i]) == g_custom_types.end()) {
            
            std::wstring token = tokens[i];
            i++;
            
            std::wstring name = token;
            std::wstring def_val = L"";
            size_t eq = token.find(L'=');
            if (eq != std::wstring::npos) {
                name = token.substr(0, eq);
                def_val = remove_quotes_and_escapes_from_token(token.substr(eq + 1));
            }
            
            if (name.empty()) continue;
            
            CustomTypeMember m;
            m.name = name;
            m.is_integer = is_integer;
            m.is_array = is_array;
            m.is_associative = is_associative;
            m.type_name = member_type_name;
            m.default_value = def_val;
            
            def.members.push_back(m);
        }
    }
    return true;
}

void instantiate_custom_type(const std::wstring& type_name, const std::wstring& var_prefix) {
    auto it = g_custom_types.find(type_name);
    if (it == g_custom_types.end()) return;
    
    for (const auto& member : it->second.members) {
        std::wstring full_member_name = var_prefix + L"." + member.name;
        if (!member.type_name.empty()) {
            instantiate_custom_type(member.type_name, full_member_name);
        } else {
            std::wstring val = member.default_value;
            if (val.empty() && member.is_integer) {
                val = L"0";
            }
            
            if (member.is_integer) {
                set_flag_value(ksh_env.integer_flags, full_member_name, true);
            }
            if (member.is_array) {
                ksh_env.arrays[full_member_name].clear();
            } else if (member.is_associative) {
                ksh_env.arrays[full_member_name].clear();
                ksh_env.associative_flags[full_member_name] = true;
            } else {
                ksh_env.variables[full_member_name] = val;
                SetEnvironmentVariableW(full_member_name.c_str(), val.c_str());
            }
        }
    }
}

#include <set>
#include <fstream>

thread_local std::set<DWORD> g_registered_child_pids;
thread_local ULONGLONG g_child_user_time = 0;
thread_local ULONGLONG g_child_kernel_time = 0;
thread_local int g_umask = 022; // default umask (octal)

void register_child_process_times(HANDLE hProcess) {
    if (hProcess == nullptr || hProcess == INVALID_HANDLE_VALUE) return;
    DWORD pid = GetProcessId(hProcess);
    if (pid == 0) return;
    
    // Check if already registered
    if (g_registered_child_pids.find(pid) != g_registered_child_pids.end()) {
        return;
    }
    
    FILETIME creationTime, exitTime, kernelTime, userTime;
    if (GetProcessTimes(hProcess, &creationTime, &exitTime, &kernelTime, &userTime)) {
        ULARGE_INTEGER uUser, uKernel;
        uUser.LowPart = userTime.dwLowDateTime;
        uUser.HighPart = userTime.dwHighDateTime;
        uKernel.LowPart = kernelTime.dwLowDateTime;
        uKernel.HighPart = kernelTime.dwHighDateTime;
        
        g_child_user_time += uUser.QuadPart;
        g_child_kernel_time += uKernel.QuadPart;
        g_registered_child_pids.insert(pid);
    }
}

bool format_printf_time_argument(const std::wstring& arg_value, std::wstring& formatted_time) {
    __time64_t timestamp = _time64(nullptr);

    if (!arg_value.empty() && arg_value != L"-1") {
        try {
            timestamp = static_cast<__time64_t>(std::stoll(arg_value, nullptr, 0));
        } catch (...) {
            return false;
        }
    }

    struct tm local_tm;
    if (_localtime64_s(&local_tm, &timestamp) != 0) {
        return false;
    }

    wchar_t time_buffer[128];
    size_t written = wcsftime(time_buffer, _countof(time_buffer), L"%Y-%m-%d %H:%M:%S", &local_tm);
    if (written == 0) {
        return false;
    }

    formatted_time.assign(time_buffer, written);
    return true;
}

bool evaluate_builtin_printf(const std::vector<std::wstring>& tokens, std::wstring& output_str, std::wstring& error_msg) {
    if (tokens.size() < 2) {
        error_msg = L"ksh: printf: usage: printf [-v varname] format [arguments ...]";
        return false;
    }
    
    bool has_var = false;
    std::wstring varname;
    size_t format_idx = 1;
    
    if (tokens[1] == L"-v") {
        if (tokens.size() < 4) {
            error_msg = L"ksh: printf: usage: printf [-v varname] format [arguments ...]";
            return false;
        }
        has_var = true;
        varname = tokens[2];
        format_idx = 3;
    }
    
    std::wstring format = tokens[format_idx];
    size_t arg_idx = format_idx + 1;
    output_str.clear();
    
    bool first_pass = true;
    while (first_pass || arg_idx < tokens.size()) {
        first_pass = false;
        
        size_t last_consumed_arg_idx = arg_idx;
        
        for (size_t i = 0; i < format.size(); ) {
            if (format[i] == L'\\') {
                if (i + 1 < format.size()) {
                    wchar_t next = format[i + 1];
                    if (next == L'n') { output_str += L'\n'; i += 2; }
                    else if (next == L't') { output_str += L'\t'; i += 2; }
                    else if (next == L'v') { output_str += L'\v'; i += 2; }
                    else if (next == L'b') { output_str += L'\b'; i += 2; }
                    else if (next == L'r') { output_str += L'\r'; i += 2; }
                    else if (next == L'f') { output_str += L'\f'; i += 2; }
                    else if (next == L'a') { output_str += L'\a'; i += 2; }
                    else if (next == L'\\') { output_str += L'\\'; i += 2; }
                    else if (next == L'0') {
                        size_t digits = 0;
                        int val = 0;
                        size_t k = i + 2;
                        while (k < format.size() && digits < 3 && format[k] >= L'0' && format[k] <= L'7') {
                            val = val * 8 + (format[k] - L'0');
                            digits++;
                            k++;
                        }
                        output_str += static_cast<wchar_t>(val);
                        i = k;
                    }
                    else {
                        output_str += format[i];
                        i++;
                    }
                } else {
                    output_str += L'\\';
                    i++;
                }
            }
            else if (format[i] == L'%') {
                if (i + 1 < format.size() && format[i + 1] == L'%') {
                    output_str += L'%';
                    i += 2;
                    continue;
                }
                
                std::wstring specifier_expr = L"%";
                size_t k = i + 1;
                while (k < format.size() && (format[k] == L'-' || format[k] == L'+' || format[k] == L' ' || format[k] == L'0' || format[k] == L'#')) {
                    specifier_expr += format[k];
                    k++;
                }
                while (k < format.size() && format[k] >= L'0' && format[k] <= L'9') {
                    specifier_expr += format[k];
                    k++;
                }
                if (k < format.size() && format[k] == L'.') {
                    specifier_expr += L'.';
                    k++;
                    while (k < format.size() && format[k] >= L'0' && format[k] <= L'9') {
                        specifier_expr += format[k];
                        k++;
                    }
                }
                
                if (k >= format.size()) {
                    output_str += specifier_expr;
                    i = k;
                    break;
                }
                
                wchar_t spec = format[k];
                specifier_expr += spec;
                k++;
                i = k;
                
                std::wstring arg_val;
                if (arg_idx < tokens.size()) {
                    arg_val = tokens[arg_idx++];
                }
                
                if (spec == L's') {
                    int len = _scwprintf(specifier_expr.c_str(), arg_val.c_str());
                    if (len >= 0) {
                        std::vector<wchar_t> temp_buf(len + 1);
                        swprintf_s(temp_buf.data(), temp_buf.size(), specifier_expr.c_str(), arg_val.c_str());
                        output_str += temp_buf.data();
                    }
                }
                else if (spec == L'd' || spec == L'i' || spec == L'o' || spec == L'x' || spec == L'X' || spec == L'u') {
                    long long val = 0;
                    if (!arg_val.empty()) {
                        try {
                            if (arg_val[0] == L'\'' || arg_val[0] == L'"') {
                                if (arg_val.size() > 1) {
                                    val = static_cast<long long>(arg_val[1]);
                                }
                            } else {
                                val = std::stoll(arg_val, nullptr, 0);
                            }
                        } catch (...) {
                            val = 0;
                        }
                    }
                    std::wstring mod_spec = specifier_expr;
                    mod_spec.insert(mod_spec.size() - 1, L"ll");
                    int len = _scwprintf(mod_spec.c_str(), val);
                    if (len >= 0) {
                        std::vector<wchar_t> temp_buf(len + 1);
                        swprintf_s(temp_buf.data(), temp_buf.size(), mod_spec.c_str(), val);
                        output_str += temp_buf.data();
                    }
                }
                else if (spec == L'f' || spec == L'e' || spec == L'E' || spec == L'g' || spec == L'G') {
                    double val = 0.0;
                    if (!arg_val.empty()) {
                        try {
                            val = std::stod(arg_val);
                        } catch (...) {
                            val = 0.0;
                        }
                    }
                    int len = _scwprintf(specifier_expr.c_str(), val);
                    if (len >= 0) {
                        std::vector<wchar_t> temp_buf(len + 1);
                        swprintf_s(temp_buf.data(), temp_buf.size(), specifier_expr.c_str(), val);
                        output_str += temp_buf.data();
                    }
                }
                else if (spec == L'c') {
                    wchar_t val = L'\0';
                    if (!arg_val.empty()) {
                        val = arg_val[0];
                    }
                    int len = _scwprintf(specifier_expr.c_str(), val);
                    if (len >= 0) {
                        std::vector<wchar_t> temp_buf(len + 1);
                        swprintf_s(temp_buf.data(), temp_buf.size(), specifier_expr.c_str(), val);
                        output_str += temp_buf.data();
                    }
                }
                else if (spec == L'q') {
                    output_str += quote_shell_argument(arg_val);
                }
                else if (spec == L'b') {
                    std::wstring expanded_arg;
                    for (size_t idx = 0; idx < arg_val.size(); ++idx) {
                        if (arg_val[idx] == L'\\' && idx + 1 < arg_val.size()) {
                            wchar_t next = arg_val[idx + 1];
                            if (next == L'n') { expanded_arg += L'\n'; idx++; }
                            else if (next == L't') { expanded_arg += L'\t'; idx++; }
                            else if (next == L'v') { expanded_arg += L'\v'; idx++; }
                            else if (next == L'b') { expanded_arg += L'\b'; idx++; }
                            else if (next == L'r') { expanded_arg += L'\r'; idx++; }
                            else if (next == L'f') { expanded_arg += L'\f'; idx++; }
                            else if (next == L'a') { expanded_arg += L'\a'; idx++; }
                            else if (next == L'\\') { expanded_arg += L'\\'; idx++; }
                            else { expanded_arg += arg_val[idx]; }
                        } else {
                            expanded_arg += arg_val[idx];
                        }
                    }
                    output_str += expanded_arg;
                }
                else if (spec == L'T') {
                    std::wstring formatted_time;
                    if (!format_printf_time_argument(arg_val, formatted_time)) {
                        error_msg = L"ksh: printf: invalid time argument for %T";
                        return false;
                    }
                    output_str += formatted_time;
                }
                else {
                    output_str += specifier_expr;
                }
            }
            else {
                output_str += format[i];
                i++;
            }
        }
        
        if (arg_idx == last_consumed_arg_idx) {
            break;
        }
    }
    if (has_var) {
        std::wstring assign_err;
        if (!assign_parameter_value(varname, output_str, false, false, assign_err)) {
            error_msg = L"ksh: printf: variable assignment failed: " + assign_err;
            return false;
        }
        output_str.clear();
    }
    return true;
}

bool execute_command_line_impl(const std::wstring& input_line, bool& should_exit_shell, bool bypass_aliases = false, bool bypass_function_lookup = false);

struct PipeRelayParams {
    HANDLE hChild;
    HANDLE hServerPipe;
    HANDLE hEvent;
    OVERLAPPED* ov;
    bool is_output;
    HANDLE hProcess;
    HANDLE hThread;
};

DWORD get_process_substitution_connect_timeout_ms() {
    std::wstring raw_value = trim_copy(get_environment_value(L"ksh_PROC_SUB_CONNECT_TIMEOUT_MS"));
    if (raw_value.empty()) {
        return kDefaultProcessSubstitutionConnectTimeoutMs;
    }

    wchar_t* end_ptr = nullptr;
    errno = 0;
    unsigned long parsed = std::wcstoul(raw_value.c_str(), &end_ptr, 10);
    if (errno != 0 || end_ptr == raw_value.c_str() || (end_ptr != nullptr && *end_ptr != L'\0')) {
        return kDefaultProcessSubstitutionConnectTimeoutMs;
    }

    DWORD timeout_ms = static_cast<DWORD>(parsed);
    if (timeout_ms < kMinProcessSubstitutionConnectTimeoutMs) {
        return kMinProcessSubstitutionConnectTimeoutMs;
    }
    if (timeout_ms > kMaxProcessSubstitutionConnectTimeoutMs) {
        return kMaxProcessSubstitutionConnectTimeoutMs;
    }
    return timeout_ms;
}

std::wstring make_process_substitution_pipe_name(LONG id) {
    LARGE_INTEGER qpc;
    if (!QueryPerformanceCounter(&qpc)) {
        qpc.QuadPart = static_cast<LONGLONG>(GetTickCount64());
    }

    const unsigned long long entropy =
        (static_cast<unsigned long long>(qpc.QuadPart) << 1) ^
        (static_cast<unsigned long long>(GetTickCount64()) << 17) ^
        (static_cast<unsigned long long>(GetCurrentProcessId()) << 33) ^
        (static_cast<unsigned long long>(GetCurrentThreadId()) << 7) ^
        static_cast<unsigned long long>(id);

    return L"\\\\.\\pipe\\ksh_ps_" +
        std::to_wstring(GetCurrentProcessId()) + L"_" +
        std::to_wstring(id) + L"_" +
        std::to_wstring(entropy);
}

DWORD WINAPI PipeRelayThread(LPVOID lpParam) {
    PipeRelayParams* params = static_cast<PipeRelayParams*>(lpParam);
    if (params == nullptr) {
        return 0;
    }

    HANDLE hChild = params->hChild;
    HANDLE hServer = params->hServerPipe;
    HANDLE hEvent = params->hEvent;
    OVERLAPPED* ov = params->ov;
    bool is_output = params->is_output;
    HANDLE hProcess = params->hProcess;
    HANDLE hThread = params->hThread;
    delete params;

    bool child_terminated = false;
    auto wait_for_pipe_io = [&](HANDLE io_event, OVERLAPPED* io_ov, DWORD timeout_ms, DWORD& transferred) -> bool {
        HANDLE wait_handles[2] = { io_event, hProcess };
        DWORD wait_count = (hProcess != nullptr && hProcess != INVALID_HANDLE_VALUE) ? 2 : 1;
        DWORD wait_result = WaitForMultipleObjects(wait_count, wait_handles, FALSE, timeout_ms);
        if (wait_result == WAIT_OBJECT_0) {
            return GetOverlappedResult(hServer, io_ov, &transferred, FALSE) != FALSE;
        }
        if (wait_result == WAIT_OBJECT_0 + 1) {
            child_terminated = true;
            CancelIoEx(hServer, io_ov);
            GetOverlappedResult(hServer, io_ov, &transferred, TRUE);
            return false;
        }
        CancelIoEx(hServer, io_ov);
        GetOverlappedResult(hServer, io_ov, &transferred, TRUE);
        return false;
    };

    DWORD dwWait = WAIT_FAILED;
    if (hEvent != nullptr && hEvent != INVALID_HANDLE_VALUE) {
        HANDLE connect_handles[2] = { hEvent, hProcess };
        DWORD connect_count = (hProcess != nullptr && hProcess != INVALID_HANDLE_VALUE) ? 2 : 1;
        dwWait = WaitForMultipleObjects(connect_count, connect_handles, FALSE, get_process_substitution_connect_timeout_ms());
        if (dwWait == WAIT_OBJECT_0 + 1) {
            child_terminated = true;
            if (is_output) {
                // Child finished producing output; still wait for reader to connect and drain buffer
                dwWait = WaitForSingleObject(hEvent, get_process_substitution_connect_timeout_ms());
            }
        }
        if (dwWait != WAIT_OBJECT_0 && hServer != nullptr && hServer != INVALID_HANDLE_VALUE && ov != nullptr) {
            CancelIoEx(hServer, ov);
            DWORD transferred = 0;
            GetOverlappedResult(hServer, ov, &transferred, TRUE);
        }
        CloseHandle(hEvent);
    }

    if (ov != nullptr) {
        delete ov;
    }

    if (dwWait == WAIT_OBJECT_0) {
        char buffer[65536];
        DWORD bytesRead = 0;
        DWORD bytesWritten = 0;
        HANDLE hIoEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

        if (hIoEvent != nullptr) {
            if (is_output) {
                // <(cmd): child outputs data, relay forwards it to named pipe (hServer)
                bool early_reader_exit = false;
                while (ReadFile(hChild, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
                    OVERLAPPED w_ov;
                    ZeroMemory(&w_ov, sizeof(w_ov));
                    ResetEvent(hIoEvent);
                    w_ov.hEvent = hIoEvent;

                    bytesWritten = 0;
                    BOOL bWrite = WriteFile(hServer, buffer, bytesRead, &bytesWritten, &w_ov);
                    if (!bWrite) {
                        DWORD err = GetLastError();
                        if (err == ERROR_IO_PENDING) {
                            if (!wait_for_pipe_io(hIoEvent, &w_ov, INFINITE, bytesWritten)) {
                                early_reader_exit = true;
                                break;
                            }
                        } else {
                            // ERROR_BROKEN_PIPE, ERROR_PIPE_NOT_CONNECTED, ERROR_NO_DATA, etc.
                            early_reader_exit = true;
                            break;
                        }
                    }

                    if (bytesWritten != bytesRead) {
                        early_reader_exit = true;
                        break;
                    }
                }

                if (!early_reader_exit) {
                    FlushFileBuffers(hServer);
                } else if (hProcess != nullptr && hProcess != INVALID_HANDLE_VALUE) {
                    // Downstream reader exited early (e.g. head -n 1 <(cmd)).
                    // Close hChild immediately to signal broken pipe to child.
                    if (hChild != nullptr && hChild != INVALID_HANDLE_VALUE) {
                        CloseHandle(hChild);
                        hChild = INVALID_HANDLE_VALUE;
                    }
                    // Give child a brief grace period to terminate upon broken pipe, otherwise terminate it
                    if (WaitForSingleObject(hProcess, 50) != WAIT_OBJECT_0) {
                        TerminateProcess(hProcess, 0);
                        WaitForSingleObject(hProcess, 1000);
                    }
                    child_terminated = true;
                }
            } else {
                // >(cmd): main command writes to named pipe (hServer), relay forwards to child stdin (hChild)
                while (true) {
                    OVERLAPPED r_ov;
                    ZeroMemory(&r_ov, sizeof(r_ov));
                    ResetEvent(hIoEvent);
                    r_ov.hEvent = hIoEvent;

                    BOOL bRead = ReadFile(hServer, buffer, sizeof(buffer), &bytesRead, &r_ov);
                    if (!bRead) {
                        DWORD err = GetLastError();
                        if (err == ERROR_IO_PENDING) {
                            bRead = wait_for_pipe_io(hIoEvent, &r_ov, INFINITE, bytesRead);
                        } else {
                            // Writer closed pipe (clean EOF) or error
                            break;
                        }
                    }

                    if (!bRead || bytesRead == 0 || child_terminated) {
                        break;
                    }

                    if (!WriteFile(hChild, buffer, bytesRead, &bytesWritten, NULL) || bytesWritten != bytesRead) {
                        break;
                    }
                }

                // EOF reached on input pipe -> Close child's stdin so child sees EOF and finishes execution
                if (hChild != nullptr && hChild != INVALID_HANDLE_VALUE) {
                    CloseHandle(hChild);
                    hChild = INVALID_HANDLE_VALUE;
                }
                // Allow child process up to 10s to complete processing remaining input
                if (!child_terminated && hProcess != nullptr && hProcess != INVALID_HANDLE_VALUE) {
                    WaitForSingleObject(hProcess, 10000);
                }
            }
            CloseHandle(hIoEvent);
        }
    } else if (!child_terminated && hProcess != nullptr && hProcess != INVALID_HANDLE_VALUE) {
        TerminateProcess(hProcess, 1);
        WaitForSingleObject(hProcess, 5000);
    }

    if (hServer != nullptr && hServer != INVALID_HANDLE_VALUE) {
        DisconnectNamedPipe(hServer);
    }

    if (hChild != nullptr && hChild != INVALID_HANDLE_VALUE) {
        CloseHandle(hChild);
    }
    if (hServer != nullptr && hServer != INVALID_HANDLE_VALUE) {
        CloseHandle(hServer);
    }
    if (hProcess != nullptr && hProcess != INVALID_HANDLE_VALUE) {
        CloseHandle(hProcess);
    }
    if (hThread != nullptr && hThread != INVALID_HANDLE_VALUE) {
        CloseHandle(hThread);
    }
    return 0;
}

std::wstring escape_quotes_for_cmd_line(const std::wstring& str) {
    std::wstring result;
    for (wchar_t ch : str) {
        if (ch == L'"') {
            result += L"\\\"";
        } else {
            result += ch;
        }
    }
    return result;
}

std::wstring handle_single_process_substitution(const std::wstring& inner_cmd, bool is_in) {
    static volatile LONG g_proc_sub_id = 0;
    LONG id = InterlockedIncrement(&g_proc_sub_id);
    std::wstring pipeName = make_process_substitution_pipe_name(id);

    PSECURITY_DESCRIPTOR pipe_sd = nullptr;
    const wchar_t* pipe_sddl = L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;OW)";
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            pipe_sddl,
            SDDL_REVISION_1,
            &pipe_sd,
            nullptr)) {
        pipe_sd = nullptr;
    }

    SECURITY_ATTRIBUTES pipe_sa;
    pipe_sa.nLength = sizeof(pipe_sa);
    pipe_sa.lpSecurityDescriptor = pipe_sd;
    pipe_sa.bInheritHandle = FALSE;
    
    HANDLE hServerPipe = CreateNamedPipeW(
        pipeName.c_str(),
        PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
        1, 65536, 65536, 0, &pipe_sa
    );

    if (pipe_sd != nullptr) {
        LocalFree(pipe_sd);
    }
    
    if (hServerPipe == INVALID_HANDLE_VALUE) {
        return L"";
    }
    
    OVERLAPPED* ov = new OVERLAPPED();
    ZeroMemory(ov, sizeof(OVERLAPPED));
    ov->hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (ov->hEvent == nullptr) {
        delete ov;
        CloseHandle(hServerPipe);
        return L"";
    }
    
    BOOL bConn = ConnectNamedPipe(hServerPipe, ov);
    if (!bConn && GetLastError() != ERROR_IO_PENDING && GetLastError() != ERROR_PIPE_CONNECTED) {
        CloseHandle(ov->hEvent);
        delete ov;
        CloseHandle(hServerPipe);
        return L"";
    }
    
    if (bConn || GetLastError() == ERROR_PIPE_CONNECTED) {
        SetEvent(ov->hEvent);
    }
    
    HANDLE hChildRead = nullptr;
    HANDLE hChildWrite = nullptr;
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;
    
    if (!CreatePipe(&hChildRead, &hChildWrite, &sa, 0)) {
        CloseHandle(ov->hEvent);
        delete ov;
        CloseHandle(hServerPipe);
        return L"";
    }
    
    if (is_in) {
        if (!SetHandleInformation(hChildRead, HANDLE_FLAG_INHERIT, 0)) {
            CloseHandle(hChildRead);
            CloseHandle(hChildWrite);
            CancelIoEx(hServerPipe, ov);
            CloseHandle(ov->hEvent);
            delete ov;
            CloseHandle(hServerPipe);
            return L"";
        }
    } else {
        if (!SetHandleInformation(hChildWrite, HANDLE_FLAG_INHERIT, 0)) {
            CloseHandle(hChildRead);
            CloseHandle(hChildWrite);
            CancelIoEx(hServerPipe, ov);
            CloseHandle(ov->hEvent);
            delete ov;
            CloseHandle(hServerPipe);
            return L"";
        }
    }
    
    wchar_t ksh_path[MAX_PATH];
    DWORD path_len = GetModuleFileNameW(nullptr, ksh_path, MAX_PATH);
    if (path_len == 0 || path_len >= MAX_PATH) {
        CloseHandle(hChildRead);
        CloseHandle(hChildWrite);
        CancelIoEx(hServerPipe, ov);
        CloseHandle(ov->hEvent);
        delete ov;
        CloseHandle(hServerPipe);
        return L"";
    }
    
    std::wstring command_line = quote_command_argument(ksh_path) + L" -c " + quote_command_argument(inner_cmd);
    
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    
    if (is_in) {
        si.hStdOutput = hChildWrite;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    } else {
        si.hStdInput = hChildRead;
        si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    }
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    
    std::vector<wchar_t> cmdBuf(command_line.size() + 1, 0);
    wcsncpy_s(cmdBuf.data(), cmdBuf.size(), command_line.c_str(), command_line.size());
    
    std::vector<HANDLE> inherited_handles = { si.hStdInput, si.hStdOutput, si.hStdError };
    if (create_process_with_handle_list(command_line, si, inherited_handles, 0, pi)) {
        if (is_in) {
            CloseHandle(hChildWrite);
        } else {
            CloseHandle(hChildRead);
        }
        
        PipeRelayParams* params = new PipeRelayParams();
        params->hChild = is_in ? hChildRead : hChildWrite;
        params->hServerPipe = hServerPipe;
        params->hEvent = ov->hEvent;
        params->ov = ov;
        params->is_output = is_in;
        params->hProcess = pi.hProcess;
        params->hThread = pi.hThread;
        
        HANDLE relay_thread = CreateThread(NULL, 0, PipeRelayThread, params, 0, NULL);
        if (relay_thread == nullptr) {
            TerminateProcess(pi.hProcess, 1);
            WaitForSingleObject(pi.hProcess, 5000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            if (params->hChild != nullptr && params->hChild != INVALID_HANDLE_VALUE) {
                CloseHandle(params->hChild);
            }
            if (params->hServerPipe != nullptr && params->hServerPipe != INVALID_HANDLE_VALUE) {
                CancelIoEx(params->hServerPipe, params->ov);
                DisconnectNamedPipe(params->hServerPipe);
                CloseHandle(params->hServerPipe);
            }
            if (params->hEvent != nullptr && params->hEvent != INVALID_HANDLE_VALUE) {
                CloseHandle(params->hEvent);
            }
            delete params;
            delete ov;
            return L"";
        }
        CloseHandle(relay_thread);
    } else {
        CloseHandle(hChildRead);
        CloseHandle(hChildWrite);
        CancelIoEx(hServerPipe, ov);
        CloseHandle(ov->hEvent);
        delete ov;
        CloseHandle(hServerPipe);
    }
    
    return pipeName;
}

void parse_and_replace_process_substitutions(std::wstring& line) {
    if (line.find(L"<(") == std::wstring::npos && line.find(L">(") == std::wstring::npos) {
        return;
    }
    size_t pos = 0;
    while (pos < line.size()) {
        bool in_single = false;
        bool in_double = false;
        bool escaped = false;
        size_t start = std::wstring::npos;
        bool is_in = false;

        for (size_t k = pos; k < line.size(); ++k) {
            wchar_t c = line[k];
            if (escaped) {
                escaped = false;
                continue;
            }
            if (c == L'\\' && !in_single) {
                escaped = true;
                continue;
            }
            if (c == L'\'' && !in_double) {
                in_single = !in_single;
                continue;
            }
            if (c == L'"' && !in_single) {
                in_double = !in_double;
                continue;
            }
            if (!in_single && !in_double) {
                if ((c == L'<' || c == L'>') && k + 1 < line.size() && line[k + 1] == L'(') {
                    start = k;
                    is_in = (c == L'<');
                    break;
                }
            }
        }

        if (start == std::wstring::npos) {
            break;
        }

        size_t i = start + 2;
        int depth = 1;
        bool inner_single = false;
        bool inner_double = false;
        bool inner_escaped = false;

        while (i < line.size() && depth > 0) {
            wchar_t c = line[i];
            if (inner_escaped) {
                inner_escaped = false;
                i++;
                continue;
            }
            if (c == L'\\' && !inner_single) {
                inner_escaped = true;
                i++;
                continue;
            }
            if (c == L'\'' && !inner_double) {
                inner_single = !inner_single;
                i++;
                continue;
            }
            if (c == L'"' && !inner_single) {
                inner_double = !inner_double;
                i++;
                continue;
            }
            if (!inner_single && !inner_double) {
                if (c == L'(') depth++;
                else if (c == L')') {
                    depth--;
                    if (depth == 0) break;
                }
            }
            i++;
        }

        if (depth == 0 && i < line.size()) {
            std::wstring inner = line.substr(start + 2, i - (start + 2));
            parse_and_replace_process_substitutions(inner);

            std::wstring pipePath = handle_single_process_substitution(inner, is_in);
            if (pipePath.empty()) {
                std::wcerr << L"ksh: process substitution setup failed\n";
                g_expansion_error = true;
                return;
            }

            std::wstring escapedPipePath;
            for (wchar_t ch : pipePath) {
                if (ch == L'\\') {
                    escapedPipePath += L"\\\\";
                } else {
                    escapedPipePath += ch;
                }
            }

            line.replace(start, i + 1 - start, escapedPipePath);
            pos = start + escapedPipePath.size();
        } else {
            pos = start + 2;
        }
    }
}

bool close_coprocess(bool terminate_process) {
    if (!g_coprocess.active) {
        return true;
    }

    if (terminate_process && g_coprocess.process != INVALID_HANDLE_VALUE) {
        TerminateProcess(g_coprocess.process, 1);
    }
    if (g_coprocess.process != INVALID_HANDLE_VALUE) {
        WaitForSingleObject(g_coprocess.process, 5000);
        CloseHandle(g_coprocess.process);
    }
    if (g_coprocess.input_write != INVALID_HANDLE_VALUE) {
        CloseHandle(g_coprocess.input_write);
    }
    if (g_coprocess.output_read != INVALID_HANDLE_VALUE) {
        CloseHandle(g_coprocess.output_read);
    }
    if (!g_coprocess.temp_file_path.empty()) {
        DeleteFileW(g_coprocess.temp_file_path.c_str());
    }
    g_coprocess = CoProcessState();
    return true;
}

bool execute_builtin_coproc(const std::vector<std::wstring>& tokens) {
    if (tokens.size() < 2) {
        std::wcerr << L"ksh: coproc: usage: coproc command [argument ...]\n";
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    close_coprocess(true);

    SECURITY_ATTRIBUTES pipe_attributes;
    pipe_attributes.nLength = sizeof(pipe_attributes);
    pipe_attributes.lpSecurityDescriptor = nullptr;
    pipe_attributes.bInheritHandle = TRUE;

    HANDLE child_stdin = INVALID_HANDLE_VALUE;
    HANDLE parent_stdin = INVALID_HANDLE_VALUE;
    HANDLE parent_stdout = INVALID_HANDLE_VALUE;
    HANDLE child_stdout = INVALID_HANDLE_VALUE;
    if (!CreatePipe(&child_stdin, &parent_stdin, &pipe_attributes, 0) ||
        !CreatePipe(&parent_stdout, &child_stdout, &pipe_attributes, 0)) {
        if (child_stdin != INVALID_HANDLE_VALUE) CloseHandle(child_stdin);
        if (parent_stdin != INVALID_HANDLE_VALUE) CloseHandle(parent_stdin);
        if (parent_stdout != INVALID_HANDLE_VALUE) CloseHandle(parent_stdout);
        if (child_stdout != INVALID_HANDLE_VALUE) CloseHandle(child_stdout);
        std::wcerr << L"ksh: coproc: unable to create pipes\n";
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    if (!SetHandleInformation(parent_stdin, HANDLE_FLAG_INHERIT, 0) ||
        !SetHandleInformation(parent_stdout, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(child_stdin);
        CloseHandle(parent_stdin);
        CloseHandle(parent_stdout);
        CloseHandle(child_stdout);
        std::wcerr << L"ksh: coproc: unable to configure pipe inheritance\n";
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    wchar_t ksh_path[MAX_PATH];
    DWORD path_len = GetModuleFileNameW(nullptr, ksh_path, MAX_PATH);
    if (path_len == 0 || path_len >= MAX_PATH) {
        CloseHandle(child_stdin);
        CloseHandle(parent_stdin);
        CloseHandle(parent_stdout);
        CloseHandle(child_stdout);
        std::wcerr << L"ksh: coproc: unable to resolve shell path\n";
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    std::vector<std::wstring> command_tokens(tokens.begin() + 1, tokens.end());
    std::wstring command_to_run;
    command_to_run.reserve(4096);

    for (const auto& pair : ksh_env.variables) {
        const std::wstring& name = pair.first;
        if (!is_valid_shell_identifier(name) || name == L"RANDOM" || name == L"SECONDS" ||
            name == L"LINENO" || name == L"PPID") {
            continue;
        }

        std::wstring options;
        if (get_flag_value(ksh_env.integer_flags, name)) options += L"i";
        if (get_flag_value(ksh_env.uppercase_flags, name)) options += L"u";
        if (get_flag_value(ksh_env.lowercase_flags, name)) options += L"l";
        if (get_flag_value(ksh_env.readonly_flags, name)) options += L"r";
        if (get_flag_value(ksh_env.exported, name)) options += L"x";

        command_to_run += name + L"=" + quote_for_single_quoted_shell_literal(pair.second) + L";\n";
        if (!options.empty()) {
            command_to_run += L"typeset -" + options + L" " + name + L";\n";
        }
    }

    for (const auto& arr_pair : ksh_env.arrays) {
        const std::wstring& name = arr_pair.first;
        if (!is_valid_shell_identifier(name)) {
            continue;
        }

        if (get_flag_value(ksh_env.associative_flags, name)) {
            command_to_run += L"typeset -A " + name + L";\n";
        } else {
            command_to_run += L"typeset -a " + name + L";\n";
        }
        for (const auto& elem : arr_pair.second) {
            command_to_run += name + L"[" + elem.first + L"]=" +
                quote_for_single_quoted_shell_literal(elem.second) + L";\n";
        }
    }

    for (const auto& pair : g_aliases) {
        command_to_run += L"alias " + pair.first + L"=" +
            quote_for_single_quoted_shell_literal(pair.second) + L";\n";
    }

    for (const auto& pair : g_shell_functions) {
        command_to_run += L"function " + pair.first + L" { ";
        for (const auto& line : pair.second.body_lines) {
            command_to_run += line + L"\n";
        }
        command_to_run += L" };\n";
    }

    command_to_run += join_tokens_as_command_line(command_tokens);

    std::wstring temp_file_path;
    std::wstring command_line;
    if (command_to_run.size() > 4000) {
        wchar_t temp_path[MAX_PATH];
        wchar_t temp_file[MAX_PATH];
        if (GetTempPathW(MAX_PATH, temp_path) != 0 &&
            GetTempFileNameW(temp_path, L"ksh", 0, temp_file) != 0) {
            HANDLE temp_handle = CreateFileW(
                temp_file,
                GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_DELETE,
                nullptr,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                nullptr);
            if (temp_handle != INVALID_HANDLE_VALUE) {
                const int utf8_length = WideCharToMultiByte(
                    CP_UTF8,
                    0,
                    command_to_run.c_str(),
                    static_cast<int>(command_to_run.size()),
                    nullptr,
                    0,
                    nullptr,
                    nullptr);
                std::string utf8;
                if (utf8_length > 0) {
                    utf8.resize(static_cast<size_t>(utf8_length));
                    WideCharToMultiByte(
                        CP_UTF8,
                        0,
                        command_to_run.c_str(),
                        static_cast<int>(command_to_run.size()),
                        &utf8[0],
                        utf8_length,
                        nullptr,
                        nullptr);
                }
                DWORD written = 0;
                if (utf8_length > 0 &&
                    WriteFile(temp_handle, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr) &&
                    written == utf8.size() && FlushFileBuffers(temp_handle)) {
                    temp_file_path = temp_file;
                    command_line = quote_command_argument(ksh_path) + L" " + quote_command_argument(temp_file_path);
                }
                CloseHandle(temp_handle);
                if (temp_file_path.empty()) {
                    DeleteFileW(temp_file);
                }
            } else {
                DeleteFileW(temp_file);
            }
        }

        if (command_line.empty()) {
            CloseHandle(child_stdin);
            CloseHandle(parent_stdin);
            CloseHandle(parent_stdout);
            CloseHandle(child_stdout);
            std::wcerr << L"ksh: coproc: unable to create temporary script\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
    } else {
        command_line = quote_command_argument(ksh_path) + L" -c " + quote_command_argument(command_to_run);
    }

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = child_stdin;
    si.hStdOutput = child_stdout;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    std::vector<HANDLE> inherited_handles = { child_stdin, child_stdout, si.hStdError };
    if (!create_process_with_handle_list(command_line, si, inherited_handles, 0, pi)) {
        if (!temp_file_path.empty()) {
            DeleteFileW(temp_file_path.c_str());
        }
        CloseHandle(child_stdin);
        CloseHandle(parent_stdin);
        CloseHandle(parent_stdout);
        CloseHandle(child_stdout);
        std::wcerr << L"ksh: coproc: unable to launch command\n";
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    CloseHandle(child_stdin);
    CloseHandle(child_stdout);
    CloseHandle(pi.hThread);
    g_coprocess.input_write = parent_stdin;
    g_coprocess.output_read = parent_stdout;
    g_coprocess.process = pi.hProcess;
    g_coprocess.pid = pi.dwProcessId;
    g_coprocess.temp_file_path = std::move(temp_file_path);
    g_coprocess.active = true;
    ksh_env.variables[L"COPROC_PID"] = std::to_wstring(pi.dwProcessId);
    ksh_env.variables[L"COPROC_ACTIVE"] = L"1";
    ksh_env.variables[L"?"] = L"0";
    return true;
}

bool execute_builtin_getconf(const std::vector<std::wstring>& tokens, const std::function<bool(const std::wstring&)>& write_output) {
    if (tokens.size() < 2) {
        std::wcerr << L"getconf: usage: getconf [-v specification] system_var [pathname] or getconf -a\n";
        ksh_env.variables[L"?"] = L"1";
        return true;
    }

    static const std::unordered_map<std::wstring, std::wstring> kStaticGetconfValues = {
        {L"CHAR_BIT", L"8"},
        {L"INT_MAX", L"2147483647"},
        {L"LONG_MAX", L"2147483647"},
        {L"ULONG_MAX", L"4294967295"},
        {L"PAGE_SIZE", L"4096"},
        {L"ARG_MAX", L"32767"},
        {L"PATH_MAX", L"260"}
    };

    auto get_runtime_getconf_value = [](const std::wstring& name, std::wstring& value_out) -> bool {
        if (name == L"PATH") {
            wchar_t windir[MAX_PATH];
            DWORD windir_len = GetEnvironmentVariableW(L"windir", windir, MAX_PATH);
            if (windir_len > 0 && windir_len < MAX_PATH) {
                value_out = std::wstring(windir) + L"\\System32;" + std::wstring(windir);
            } else {
                value_out = L"C:\\Windows\\System32;C:\\Windows";
            }
            return true;
        }

        if (name == L"TMPDIR") {
            wchar_t tmp[MAX_PATH];
            DWORD tmp_len = GetTempPathW(MAX_PATH, tmp);
            if (tmp_len > 0 && tmp_len < MAX_PATH) {
                value_out = tmp;
            } else {
                value_out = L"C:\\Temp";
            }
            return true;
        }

        std::unordered_map<std::wstring, std::wstring>::const_iterator it = kStaticGetconfValues.find(name);
        if (it != kStaticGetconfValues.end()) {
            value_out = it->second;
            return true;
        }

        return false;
    };

    static const std::vector<std::wstring> kGetconfListOrder = {
        L"CHAR_BIT",
        L"INT_MAX",
        L"LONG_MAX",
        L"ULONG_MAX",
        L"PAGE_SIZE",
        L"ARG_MAX",
        L"PATH_MAX",
        L"PATH",
        L"TMPDIR"
    };

    if (tokens[1] == L"-a") {
        for (const std::wstring& key : kGetconfListOrder) {
            std::wstring value;
            if (!get_runtime_getconf_value(key, value)) {
                continue;
            }
            if (!write_output(key + L" = " + value + L"\n")) {
                ksh_env.variables[L"?"] = L"1";
                return true;
            }
        }
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    std::wstring var = tokens[1];
    std::wstring value;
    if (get_runtime_getconf_value(var, value)) {
        if (!write_output(value + L"\n")) {
            ksh_env.variables[L"?"] = L"1";
            return true;
        }
        ksh_env.variables[L"?"] = L"0";
        return true;
    } else {
        std::wcerr << L"getconf: unknown variable: " << var << L"\n";
        ksh_env.variables[L"?"] = L"1";
        return true;
    }
}

bool execute_builtin_pathchk(const std::vector<std::wstring>& tokens) {
    if (tokens.size() < 2) {
        std::wcerr << L"pathchk: usage: pathchk [-p] [-P] pathname...\n";
        ksh_env.variables[L"?"] = L"1";
        return true;
    }

    bool posix_portable = false;
    bool check_empty_components = false;
    size_t start_idx = 1;

    while (start_idx < tokens.size() && tokens[start_idx].size() > 1 && tokens[start_idx][0] == L'-') {
        std::wstring arg = tokens[start_idx];
        for (size_t j = 1; j < arg.size(); ++j) {
            if (arg[j] == L'p') posix_portable = true;
            else if (arg[j] == L'P') check_empty_components = true;
            else {
                std::wcerr << L"pathchk: invalid option: -" << arg[j] << L"\n";
                ksh_env.variables[L"?"] = L"1";
                return true;
            }
        }
        start_idx++;
    }

    if (start_idx >= tokens.size()) {
        std::wcerr << L"pathchk: missing pathname\n";
        ksh_env.variables[L"?"] = L"1";
        return true;
    }

    bool all_ok = true;
    for (size_t i = start_idx; i < tokens.size(); ++i) {
        std::wstring path = tokens[i];

        if (path.empty()) {
            std::wcerr << L"pathchk: empty pathname is invalid\n";
            all_ok = false;
            continue;
        }

        std::vector<std::wstring> components;
        std::wstring current;
        for (wchar_t ch : path) {
            if (ch == L'\\' || ch == L'/') {
                if (!current.empty()) {
                    components.push_back(current);
                    current.clear();
                } else if (check_empty_components) {
                    std::wcerr << L"pathchk: '" << path << L"': empty component is not portable\n";
                    all_ok = false;
                }
            } else {
                current.push_back(ch);
            }
        }
        if (!current.empty()) {
            components.push_back(current);
        }

        if (posix_portable) {
            if (path.size() > 256) {
                std::wcerr << L"pathchk: '" << path << L"': pathname length exceeds POSIX limit (256)\n";
                all_ok = false;
            }
            for (const auto& comp : components) {
                if (comp.size() > 14) {
                    std::wcerr << L"pathchk: '" << path << L"': component '" << comp << L"' length exceeds POSIX limit (14)\n";
                    all_ok = false;
                }
                for (wchar_t ch : comp) {
                    bool ok_char = (ch >= L'A' && ch <= L'Z') || (ch >= L'a' && ch <= L'z') ||
                                   (ch >= L'0' && ch <= L'9') || ch == L'.' || ch == L'_' || ch == L'-';
                    if (!ok_char) {
                        std::wcerr << L"pathchk: '" << path << L"': character '" << ch << L"' in '" << comp << L"' is not portable\n";
                        all_ok = false;
                    }
                }
            }
        } else {
            if (path.size() > 260) {
                std::wcerr << L"pathchk: '" << path << L"': pathname length exceeds Windows limit (260)\n";
                all_ok = false;
            }
            for (const auto& comp : components) {
                if (comp.size() > 255) {
                    std::wcerr << L"pathchk: '" << path << L"': component '" << comp << L"' length exceeds Windows limit (255)\n";
                    all_ok = false;
                }
                for (size_t char_idx = 0; char_idx < comp.size(); ++char_idx) {
                    wchar_t ch = comp[char_idx];
                    if (ch == L':' && char_idx == 1 && comp.size() == 2 && i == start_idx) {
                        continue;
                    }
                    if (ch == L'<' || ch == L'>' || ch == L':' || ch == L'"' || ch == L'|' || ch == L'?' || ch == L'*') {
                        std::wcerr << L"pathchk: '" << path << L"': component '" << comp << L"' contains invalid character '" << ch << L"'\n";
                        all_ok = false;
                    }
                }
            }
        }
    }

    ksh_env.variables[L"?"] = all_ok ? L"0" : L"1";
    return true;
}

bool execute_command_line(const std::wstring& input_line, bool& should_exit_shell, bool bypass_aliases, bool bypass_function_lookup) {
    std::wstring line = input_line;
    g_expansion_error = false;
    parse_and_replace_process_substitutions(line);
    if (g_expansion_error) {
        ksh_env.variables[L"?"] = L"1";
        return false;
    }
    
    bool result = execute_command_line_impl(line, should_exit_shell, bypass_aliases, bypass_function_lookup);
    
    if (g_errexit_enabled && !g_is_interactive_session && g_errexit_disabled_depth == 0) {
        std::map<std::wstring, std::wstring>::const_iterator status_it = ksh_env.variables.find(L"?");
        if (status_it != ksh_env.variables.end() && status_it->second != L"0") {
            should_exit_shell = true;
        }
    }
    return result;
}

// Main command execution pipeline.
bool execute_command_line_impl(const std::wstring& input_line, bool& should_exit_shell, bool bypass_aliases, bool bypass_function_lookup) {
    const bool perf_trace = is_performance_telemetry_enabled();
    const ULONGLONG command_start = perf_trace ? GetTickCount64() : 0;

    // Execution phase: trap dispatch gate. Pending INT/TERM traps are processed before parsing.
    process_pending_traps(should_exit_shell);
    if (should_exit_shell) {
        return true;
    }

    if (input_line.empty()) {
        return true;
    }

    std::wstring lexical_error;
    if (!validate_shell_lexical_state(input_line, lexical_error)) {
        std::wcerr << lexical_error << L"\n";
        ksh_env.variables[L"?"] = L"2";
        return false;
    }

    auto is_double_parentheses = [](const std::wstring& text, std::wstring& inner) -> bool {
        const std::wstring trimmed = trim_copy(text);
        if (trimmed.size() < 4 || trimmed.substr(0, 2) != L"((" || trimmed.substr(trimmed.size() - 2) != L"))") {
            return false;
        }

        bool in_single_quotes = false;
        bool in_double_quotes = false;
        bool escaped = false;
        int depth = 0;

        for (size_t i = 0; i < trimmed.size(); ++i) {
            wchar_t ch = trimmed[i];
            if (escaped) {
                escaped = false;
                continue;
            }

            if (ch == L'\\' && !in_single_quotes) {
                escaped = true;
                continue;
            }

            if (ch == L'\'' && !in_double_quotes) {
                in_single_quotes = !in_single_quotes;
                continue;
            }

            if (ch == L'"' && !in_single_quotes) {
                in_double_quotes = !in_double_quotes;
                continue;
            }

            if (in_single_quotes || in_double_quotes) {
                continue;
            }

            if (ch == L'(') {
                depth++;
            } else if (ch == L')') {
                depth--;
                if (depth == 0 && i + 1 != trimmed.size()) {
                    return false;
                }
            }

            if (depth < 0) {
                return false;
            }
        }

        if (depth != 0) {
            return false;
        }

        inner = trim_copy(trimmed.substr(2, trimmed.size() - 4));
        return true;
    };

    auto is_enclosed_by_top_level_pair = [](const std::wstring& text, wchar_t open_ch, wchar_t close_ch, std::wstring& inner) -> bool {
        const std::wstring trimmed = trim_copy(text);
        if (trimmed.size() < 2 || trimmed.front() != open_ch || trimmed.back() != close_ch) {
            return false;
        }

        bool in_single_quotes = false;
        bool in_double_quotes = false;
        bool escaped = false;
        int depth = 0;

        for (size_t i = 0; i < trimmed.size(); ++i) {
            wchar_t ch = trimmed[i];
            if (escaped) {
                escaped = false;
                continue;
            }

            if (ch == L'\\' && !in_single_quotes) {
                escaped = true;
                continue;
            }

            if (ch == L'\'' && !in_double_quotes) {
                in_single_quotes = !in_single_quotes;
                continue;
            }

            if (ch == L'"' && !in_single_quotes) {
                in_double_quotes = !in_double_quotes;
                continue;
            }

            if (in_single_quotes || in_double_quotes) {
                continue;
            }

            if (ch == open_ch) {
                depth++;
            } else if (ch == close_ch) {
                depth--;
                if (depth == 0 && i + 1 != trimmed.size()) {
                    return false;
                }
            }

            if (depth < 0) {
                return false;
            }
        }

        if (depth != 0) {
            return false;
        }

        inner = trim_copy(trimmed.substr(1, trimmed.size() - 2));
        return true;
    };

    auto split_top_level_command_list = [](const std::wstring& text, std::vector<std::wstring>& segments, std::vector<std::wstring>& separators) {
        segments.clear();
        separators.clear();

        bool in_single_quotes = false;
        bool in_double_quotes = false;
        bool escaped = false;
        int paren_depth = 0;
        int brace_depth = 0;
        std::wstring current;

        for (size_t i = 0; i < text.size(); ++i) {
            wchar_t ch = text[i];

            if (escaped) {
                current.push_back(ch);
                escaped = false;
                continue;
            }

            if (ch == L'\\' && !in_single_quotes) {
                current.push_back(ch);
                escaped = true;
                continue;
            }

            if (ch == L'\'' && !in_double_quotes) {
                in_single_quotes = !in_single_quotes;
                current.push_back(ch);
                continue;
            }

            if (ch == L'"' && !in_single_quotes) {
                in_double_quotes = !in_double_quotes;
                current.push_back(ch);
                continue;
            }

            if (!in_single_quotes && !in_double_quotes) {
                if (ch == L'(') {
                    paren_depth++;
                    current.push_back(ch);
                    continue;
                }
                if (ch == L')' && paren_depth > 0) {
                    paren_depth--;
                    current.push_back(ch);
                    continue;
                }
                if (ch == L'{') {
                    brace_depth++;
                    current.push_back(ch);
                    continue;
                }
                if (ch == L'}' && brace_depth > 0) {
                    brace_depth--;
                    current.push_back(ch);
                    continue;
                }

                if (paren_depth == 0 && brace_depth == 0) {
                    if (ch == L'[' && (i + 1) < text.size() && text[i + 1] == L'[') {
                        current += L"[[";
                        i += 2;

                        bool cond_single_quotes = false;
                        bool cond_double_quotes = false;
                        bool cond_escaped = false;
                        for (; i < text.size(); ++i) {
                            wchar_t cond_ch = text[i];
                            current.push_back(cond_ch);

                            if (cond_escaped) {
                                cond_escaped = false;
                                continue;
                            }

                            if (cond_ch == L'\\' && !cond_single_quotes) {
                                cond_escaped = true;
                                continue;
                            }

                            if (cond_ch == L'\'' && !cond_double_quotes) {
                                cond_single_quotes = !cond_single_quotes;
                                continue;
                            }

                            if (cond_ch == L'"' && !cond_single_quotes) {
                                cond_double_quotes = !cond_double_quotes;
                                continue;
                            }

                            if (!cond_single_quotes && !cond_double_quotes && cond_ch == L']' &&
                                (i + 1) < text.size() && text[i + 1] == L']') {
                                current.push_back(L']');
                                i++;
                                break;
                            }
                        }

                        continue;
                    }

                    if (ch == L';') {
                        segments.push_back(trim_copy(current));
                        separators.push_back(L";");
                        current.clear();
                        continue;
                    }

                    if (ch == L'&' && (i + 1) < text.size() && text[i + 1] == L'&') {
                        segments.push_back(trim_copy(current));
                        separators.push_back(L"&&");
                        current.clear();
                        i++;
                        continue;
                    }

                    if (ch == L'|' && (i + 1) < text.size() && text[i + 1] == L'|') {
                        segments.push_back(trim_copy(current));
                        separators.push_back(L"||");
                        current.clear();
                        i++;
                        continue;
                    }
                }
            }

            current.push_back(ch);
        }

        segments.push_back(trim_copy(current));
    };

    // structural parsing. Handle { ... }, ( ... ), and top-level command lists first.
    // separators (;, &&, ||) are recognized only at top level, never inside quotes/[[...]].
    const std::wstring early_trimmed = trim_copy(input_line);
    std::wstring enclosed_inner;
    if (is_double_parentheses(early_trimmed, enclosed_inner)) {
        std::wstring expanded = expand_substitutions_left_to_right(enclosed_inner);
        if (g_expansion_error) {
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        double val = 0.0;
        bool parse_ok = true;
        try {
            ArithmeticParser parser(expanded);
            val = parser.parse();
        } catch (const std::exception& ex) {
            std::wstring wmsg = L"ksh: ((" + expanded + L")): " + std::wstring(ex.what(), ex.what() + strlen(ex.what()));
            std::wcerr << wmsg << L"\n";
            parse_ok = false;
        } catch (...) {
            std::wcerr << L"ksh: ((" << expanded << L")): arithmetic parse error\n";
            parse_ok = false;
        }

        if (!parse_ok) {
            ksh_env.variables[L"?"] = L"1";
            return true;
        }

        bool exit_success = (val != 0.0);
        ksh_env.variables[L"?"] = exit_success ? L"0" : L"1";
        return true;
    }
    if (is_enclosed_by_top_level_pair(early_trimmed, L'{', L'}', enclosed_inner)) {
        return execute_command_line(enclosed_inner, should_exit_shell);
    }
    if (is_enclosed_by_top_level_pair(early_trimmed, L'(', L')', enclosed_inner)) {
        const ULONGLONG subshell_start = GetTickCount64();
        bool stateless_fast_path = false;
        if (!has_unquoted_shell_metacharacters(enclosed_inner)) {
            std::vector<std::wstring> subshell_tokens = ksh_tokenize_preserve_quotes(enclosed_inner);
            stateless_fast_path = can_capture_builtin_command_substitution(subshell_tokens);
        }

        if (stateless_fast_path) {
            bool ok = execute_command_line(enclosed_inner, should_exit_shell);
            const ULONGLONG elapsed_ms = GetTickCount64() - subshell_start;
            record_parenthesized_subshell_path_perf(true, false, elapsed_ms);
            if (perf_trace) {
                std::wstring detail = L"path=fast count=" + std::to_wstring(g_subshell_path_perf_counters.parenthesized_fast_count) +
                    L" total_ms=" + std::to_wstring(g_subshell_path_perf_counters.parenthesized_fast_ms);
                log_performance_trace(L"subshell.parenthesized", elapsed_ms, detail);
            }
            return ok;
        }

        const bool require_state_snapshot = command_requires_environment_snapshot(enclosed_inner);
        ScopedSubshellDepthGuard subshell_depth_guard;
        std::optional<ShellStateSnapshot> saved_snapshot;
        ScopedSnapshotRestoreGuard snapshot_guard(saved_snapshot, L"1");

        if (require_state_snapshot) {
            saved_snapshot.emplace(capture_shell_state_snapshot());
        }

        bool ok = execute_command_line(enclosed_inner, should_exit_shell);
        const bool used_snapshot_restore = saved_snapshot.has_value();
        restore_shell_state_snapshot_if_present(saved_snapshot, L"1");
        snapshot_guard.dismiss();

        const ULONGLONG elapsed_ms = GetTickCount64() - subshell_start;
        record_parenthesized_subshell_path_perf(false, used_snapshot_restore, elapsed_ms);
        if (perf_trace) {
            std::wstring detail;
            if (used_snapshot_restore) {
                detail = L"path=snapshot count=" + std::to_wstring(g_subshell_path_perf_counters.parenthesized_snapshot_count) +
                    L" total_ms=" + std::to_wstring(g_subshell_path_perf_counters.parenthesized_snapshot_ms);
            } else {
                detail = L"path=direct_no_snapshot count=" + std::to_wstring(g_subshell_path_perf_counters.parenthesized_direct_count) +
                    L" total_ms=" + std::to_wstring(g_subshell_path_perf_counters.parenthesized_direct_ms);
            }
            log_performance_trace(L"subshell.parenthesized", elapsed_ms, detail);
        }

        return ok;
    }

    std::vector<std::wstring> list_segments;
    std::vector<std::wstring> list_separators;
    split_top_level_command_list(early_trimmed, list_segments, list_separators);
    if (!list_separators.empty()) {
        bool last_result = true;
        bool have_executed = false;

        for (size_t i = 0; i < list_segments.size(); ++i) {
            const std::wstring segment = trim_copy(list_segments[i]);
            if (segment.empty()) {
                continue;
            }

            bool should_run = true;
            if (have_executed && i > 0) {
                const std::wstring& sep = list_separators[i - 1];
                if (sep == L"&&") {
                    should_run = last_result;
                } else if (sep == L"||") {
                    should_run = !last_result;
                }
            }

            if (!should_run) {
                continue;
            }

            bool disable_err = (i < list_separators.size() && (list_separators[i] == L"&&" || list_separators[i] == L"||"));
            if (disable_err) {
                g_errexit_disabled_depth++;
            }
            bool invoke_ok = execute_command_line(segment, should_exit_shell);
            if (disable_err) {
                g_errexit_disabled_depth--;
            }
            (void)invoke_ok;
            std::map<std::wstring, std::wstring>::const_iterator status_it = ksh_env.variables.find(L"?");
            last_result = (status_it != ksh_env.variables.end() && status_it->second == L"0");
            have_executed = true;
            if (should_exit_shell || is_function_return_requested()) {
                return last_result;
            }
        }

        if (!have_executed) {
            ksh_env.variables[L"?"] = L"0";
            return true;
        }
        return true;
    }

    // Execution phase: history + alias preprocessing.
    // alias expansion applies only to the leading token and is recursion-capped.
    std::wstring resolved_input;
    if (!resolve_history_recall(input_line, resolved_input)) {
        ksh_env.variables[L"?"] = L"1";
        return false;
    }
    if (resolved_input != input_line) {
        std::wcout << resolved_input << L"\n";
    }
    append_history_entry_to_file(resolved_input);

    if (!bypass_aliases) {
        // Expand aliases on the leading command token with a small recursion cap.
        for (int alias_depth = 0; alias_depth < 16; ++alias_depth) {
            std::vector<std::wstring> alias_tokens = ksh_tokenize_preserve_quotes(resolved_input);
            if (alias_tokens.empty()) {
                break;
            }

            const std::wstring alias_name = alias_tokens[0];
            if (alias_name == L"alias" || alias_name == L"unalias") {
                break;
            }

            std::map<std::wstring, std::wstring>::const_iterator alias_it = g_aliases.find(alias_name);
            if (alias_it == g_aliases.end()) {
                break;
            }

            std::wstring remainder = join_tokens_with_spaces(alias_tokens, 1);
            resolved_input = alias_it->second;
            if (!remainder.empty()) {
                resolved_input += L" ";
                resolved_input += remainder;
            }
        }
    }

    // substitutions (parameter, command, arithmetic) on raw command text.
    // expansion happens before field splitting and pathname expansion.
    g_expansion_error = false;
    const ULONGLONG expand_start = perf_trace ? GetTickCount64() : 0;
    std::wstring expanded = expand_substitutions_left_to_right(resolved_input);
    if (perf_trace) {
        log_performance_trace(L"execute.expand_substitutions", GetTickCount64() - expand_start);
    }
    if (g_expansion_error) {
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    std::wstring pipeline_source;
    std::wstring coprocess_source;
    if (strip_trailing_unquoted_coprocess_marker(expanded, coprocess_source)) {
        std::vector<std::wstring> coprocess_tokens = ksh_tokenize_preserve_quotes(coprocess_source);
        std::vector<std::wstring> staged_tokens;
        for (const std::wstring& raw_token : coprocess_tokens) {
            std::vector<std::wstring> split_tokens = split_fields_by_ifs(raw_token);
            staged_tokens.insert(
                staged_tokens.end(),
                std::make_move_iterator(split_tokens.begin()),
                std::make_move_iterator(split_tokens.end()));
        }
        staged_tokens = expand_tilde_for_tokens(staged_tokens);
        staged_tokens = expand_globs_for_tokens(staged_tokens, staged_tokens);
        staged_tokens = remove_quotes_and_escapes_from_tokens(staged_tokens);
        if (staged_tokens.empty()) {
            std::wcerr << L"ksh: |& requires a command\n";
            ksh_env.variables[L"?"] = L"2";
            return false;
        }
        staged_tokens.insert(staged_tokens.begin(), L"coproc");
        return execute_builtin_coproc(staged_tokens);
    }

    bool pipeline_background = strip_trailing_unquoted_background_marker(expanded, pipeline_source);

    // pipeline routing.
    // pipeline splitting occurs only for top-level unquoted '|'.
    const ULONGLONG pipeline_split_start = perf_trace ? GetTickCount64() : 0;
    std::vector<std::wstring> pipeline_segments = split_pipeline_segments(pipeline_source);
    if (perf_trace) {
        log_performance_trace(L"execute.pipeline_split", GetTickCount64() - pipeline_split_start, L"segments=" + std::to_wstring(pipeline_segments.size()));
    }
    if (pipeline_segments.size() > 1) {
        DWORD pipeline_exit = 1;
        HANDLE background_handle = nullptr;
        DWORD background_pid = 0;
        std::vector<HANDLE> background_handles;
        std::vector<DWORD> background_pids;
        std::vector<std::wstring> pipeline_temp_files;
        const ULONGLONG pipeline_exec_start = perf_trace ? GetTickCount64() : 0;
        if (!execute_pipeline_segments(
                pipeline_segments,
                pipeline_background,
                pipeline_exit,
                background_handle,
                background_pid,
                &background_handles,
                &background_pids,
                &pipeline_temp_files)) {
            if (perf_trace) {
                log_performance_trace(L"execute.pipeline_run", GetTickCount64() - pipeline_exec_start, L"failed");
            }
            if (!g_last_pipeline_error_detail.empty()) {
                std::wcerr << g_last_pipeline_error_detail << L"\n";
            } else {
                std::wcerr << L"ksh: pipeline execution failed\n";
            }
            ksh_env.variables[L"?"] = std::to_wstring(pipeline_exit);
            return false;
        }
        if (perf_trace) {
            log_performance_trace(L"execute.pipeline_run", GetTickCount64() - pipeline_exec_start, L"exit=" + std::to_wstring(pipeline_exit));
        }

        if (pipeline_background) {
            BackgroundJob job;
            job.id = g_next_job_id++;
            job.pid = background_pid;
            job.process_handle = background_handle;
            job.pids = background_pids;
            job.process_handles = background_handles;
            job.command = trim_copy(pipeline_source);
            job.completed = false;
            job.exit_code = STILL_ACTIVE;
            job.completion_reported = false;
            job.temp_file_paths = std::move(pipeline_temp_files);
            g_background_jobs.push_back(job);

            std::wcout << L"[" << job.id << L"] " << job.pid << L"\n";
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        ksh_env.variables[L"?"] = std::to_wstring(pipeline_exit);
        return pipeline_exit == 0;
    }

    // token staging.
    // field splitting -> glob expansion -> quote/escape removal.
    // Quoted text is preserved through split/glob phases and stripped only at the final step.
    const ULONGLONG token_stage_start = perf_trace ? GetTickCount64() : 0;
    std::vector<std::wstring> lexed_tokens = ksh_tokenize_preserve_quotes(expanded);
    std::vector<std::wstring> tokens;
    tokens.reserve(lexed_tokens.size());
    for (const std::wstring& raw_token : lexed_tokens) {
        std::vector<std::wstring> split_tokens = split_fields_by_ifs(raw_token);
        if (split_tokens.empty()) {
            continue;
        }
        tokens.insert(tokens.end(), std::make_move_iterator(split_tokens.begin()), std::make_move_iterator(split_tokens.end()));
    }
    tokens = expand_tilde_for_tokens(tokens);
    tokens = expand_globs_for_tokens(tokens, tokens);
    if (perf_trace) {
        log_performance_trace(L"execute.token_stage", GetTickCount64() - token_stage_start, L"tokens=" + std::to_wstring(tokens.size()));
    }
    if (tokens.empty()) {
        return true;
    }

    bool run_in_background = false;
    if (!tokens.empty() && tokens.back() == L"&") {
        run_in_background = true;
        tokens.pop_back();
        if (tokens.empty()) {
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
    }

    RedirectionSpec redir;
    std::wstring redirection_error;
    // redirection parse after token staging.
    // redirection operators are parsed from staged tokens and removed from argv tokens.
    const ULONGLONG redirection_parse_start = perf_trace ? GetTickCount64() : 0;
    if (!parse_redirections(tokens, redir, redirection_error)) {
        if (perf_trace) {
            log_performance_trace(L"execute.redirection_parse", GetTickCount64() - redirection_parse_start, L"failed");
        }
        std::wcerr << redirection_error << L"\n";
        ksh_env.variables[L"?"] = L"1";
        return false;
    }
    redir.stdin_path = remove_quotes_and_escapes_from_token(redir.stdin_path);
    redir.stdout_path = remove_quotes_and_escapes_from_token(redir.stdout_path);
    redir.stderr_path = remove_quotes_and_escapes_from_token(redir.stderr_path);
    for (RedirectionSpec::CustomFdAction& action : redir.custom_fd_actions) {
        action.path = remove_quotes_and_escapes_from_token(action.path);
    }
    tokens = remove_quotes_and_escapes_from_tokens(tokens);
    if (perf_trace) {
        log_performance_trace(L"execute.redirection_parse", GetTickCount64() - redirection_parse_start);
    }

    if (!redir.custom_fd_actions.empty() && (tokens.empty() || tokens[0] != L"exec")) {
        std::wcerr << L"ksh: custom fd redirection (3-9) is currently supported only with exec\n";
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    if (tokens.empty()) {
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    struct StdinRedirector {
        HANDLE hOld;
        HANDLE hNew;
        bool active;
        StdinRedirector(const RedirectionSpec& r) : active(false), hOld(INVALID_HANDLE_VALUE), hNew(INVALID_HANDLE_VALUE) {
            if (r.has_stdin) {
                hOld = GetStdHandle(STD_INPUT_HANDLE);
                if (open_redirection_file(r.stdin_path, GENERIC_READ, OPEN_EXISTING, hNew)) {
                    SetStdHandle(STD_INPUT_HANDLE, hNew);
                    active = true;
                }
            }
        }
        ~StdinRedirector() {
            if (active) {
                SetStdHandle(STD_INPUT_HANDLE, hOld);
                CloseHandle(hNew);
            }
        }
        bool is_valid(const RedirectionSpec& r) const {
            return !r.has_stdin || active;
        }
    };

    std::wstring check_cmd = tokens[0];
    const std::vector<std::wstring>& builtins_list = builtin_commands();
    bool is_builtin_cmd = (std::find(builtins_list.begin(), builtins_list.end(), check_cmd) != builtins_list.end());
    bool is_function_cmd = (g_shell_functions.find(check_cmd) != g_shell_functions.end());

    std::unique_ptr<StdinRedirector> stdin_redir;
    if (is_builtin_cmd || is_function_cmd) {
        stdin_redir = std::make_unique<StdinRedirector>(redir);
        if (!stdin_redir->is_valid(redir)) {
            std::wcerr << L"ksh: redirection setup failed\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
    }

    bool builtin_output_started = false;
    auto write_builtin_output = [&](const std::wstring& text) -> bool {
        if (g_builtin_capture_output != nullptr && !redir.has_stdout && !redir.stdout_to_stderr) {
            g_builtin_capture_output->append(text);
            return true;
        }

        if (!redir.has_stdout && !redir.stdout_to_stderr) {
            if (g_pipeline_stdout != INVALID_HANDLE_VALUE) {
                std::string utf8;
                int len = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), NULL, 0, NULL, NULL);
                if (len > 0) {
                    utf8.resize(len);
                    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), &utf8[0], len, NULL, NULL);
                }
                DWORD written = 0;
                WriteFile(g_pipeline_stdout, utf8.data(), (DWORD)utf8.size(), &written, NULL);
                return true;
            }
            if (g_subshell_stdout != INVALID_HANDLE_VALUE) {
                std::string utf8;
                int len = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), NULL, 0, NULL, NULL);
                if (len > 0) {
                    utf8.resize(len);
                    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), &utf8[0], len, NULL, NULL);
                }
                DWORD written = 0;
                WriteFile(g_subshell_stdout, utf8.data(), (DWORD)utf8.size(), &written, NULL);
                return true;
            }
            if (!_isatty(_fileno(stdout))) {
                std::string utf8;
                int len = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), NULL, 0, NULL, NULL);
                if (len > 0) {
                    utf8.resize(len);
                    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), &utf8[0], len, NULL, NULL);
                }
                _write(_fileno(stdout), utf8.data(), (unsigned int)utf8.size());
                return true;
            }
            std::wcout << text;
            return true;
        }

        RedirectionSpec output_redir = redir;
        if (builtin_output_started) {
            output_redir.append_stdout = true;
        }

        if (!write_text_with_stdout_redirection(text, output_redir)) {
            return false;
        }

        builtin_output_started = true;
        return true;
    };

    std::wstring command_to_run = join_tokens_as_command_line(tokens);
    if (g_xtrace_enabled) {
        std::wcerr << L"+ " << command_to_run << L"\n";
        std::wcerr.flush();
    }

    std::wstring cmd = tokens[0];
    std::wstring external_command_to_run = command_to_run;
    ScriptInterpreterResolution interpreter_resolution = build_script_interpreter_command(tokens, external_command_to_run);
    if (interpreter_resolution == ScriptInterpreterResolution::Failed) {
        std::wcerr << L"ksh: failed to resolve script interpreter\n";
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    if (g_custom_types.find(cmd) != g_custom_types.end()) {
        for (size_t i = 1; i < tokens.size(); ++i) {
            std::wstring token = tokens[i];
            std::wstring var_name = token;
            std::wstring assign_part;
            size_t eq = token.find(L'=');
            if (eq != std::wstring::npos) {
                var_name = token.substr(0, eq);
                assign_part = token.substr(eq + 1);
            }
            
            instantiate_custom_type(cmd, var_name);
            
            if (!assign_part.empty()) {
                if (assign_part.front() == L'(' && assign_part.back() == L')') {
                    std::wstring inner = assign_part.substr(1, assign_part.size() - 2);
                    std::vector<std::wstring> sub_tokens = ksh_tokenize_preserve_quotes(inner);
                    for (const auto& sub_t : sub_tokens) {
                        size_t sub_eq = sub_t.find(L'=');
                        if (sub_eq != std::wstring::npos) {
                            std::wstring member = sub_t.substr(0, sub_eq);
                            std::wstring val = sub_t.substr(sub_eq + 1);
                            std::wstring full_name = var_name + L"." + member;
                            ksh_env.variables[full_name] = val;
                            SetEnvironmentVariableW(full_name.c_str(), val.c_str());
                        }
                    }
                }
            }
        }
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    if (cmd == L"exit") {
        int status_code = 0;
        if (tokens.size() > 2) {
            std::wcerr << L"ksh: exit: too many arguments\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        if (tokens.size() == 2 && !try_parse_int_strict(tokens[1], status_code)) {
            std::wcerr << L"ksh: exit: numeric argument required\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        should_exit_shell = true;
        ksh_env.variables[L"?"] = std::to_wstring(status_code);
        return true;
    }

    if (cmd == L"logout") {
        int status_code = 0;
        if (tokens.size() > 2) {
            std::wcerr << L"ksh: logout: too many arguments\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        if (tokens.size() == 2) {
            if (!try_parse_int_strict(tokens[1], status_code)) {
                std::wcerr << L"ksh: logout: numeric argument required\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
        }

        should_exit_shell = true;
        ksh_env.variables[L"?"] = std::to_wstring(status_code);
        return true;
    }

    if (cmd == L"true") {
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    if (cmd == L"false") {
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    if (cmd == L"dirs") {
        bool clear_stack = false;
        bool vertical = false;
        bool one_per_line = false;
        bool long_format = false;
        int print_index = -1;
        bool index_from_left = true;
        bool has_index = false;

        for (size_t i = 1; i < tokens.size(); ++i) {
            const std::wstring& arg = tokens[i];
            if (arg == L"-c") {
                clear_stack = true;
            } else if (arg == L"-v") {
                vertical = true;
            } else if (arg == L"-p") {
                one_per_line = true;
            } else if (arg == L"-l") {
                long_format = true;
            } else if (arg.size() > 1 && (arg[0] == L'+' || arg[0] == L'-')) {
                int parsed = 0;
                if (try_parse_int_strict(arg.substr(1), parsed)) {
                    has_index = true;
                    index_from_left = (arg[0] == L'+');
                    print_index = parsed;
                } else if (arg[0] == L'-') {
                    for (size_t j = 1; j < arg.size(); ++j) {
                        if (arg[j] == L'c') clear_stack = true;
                        else if (arg[j] == L'v') vertical = true;
                        else if (arg[j] == L'p') one_per_line = true;
                        else if (arg[j] == L'l') long_format = true;
                        else {
                            std::wcerr << L"ksh: dirs: invalid option: -" << arg[j] << L"\n";
                            ksh_env.variables[L"?"] = L"1";
                            return false;
                        }
                    }
                } else {
                    std::wcerr << L"ksh: dirs: invalid index: " << arg << L"\n";
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
            } else {
                std::wcerr << L"ksh: dirs: invalid argument: " << arg << L"\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
        }

        if (clear_stack) {
            g_directory_stack.clear();
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        std::wstring current_directory;
        if (!get_current_directory_path(current_directory)) {
            std::wcerr << L"ksh: dirs: unable to determine current directory\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        std::vector<std::wstring> all_dirs;
        all_dirs.push_back(current_directory);
        for (size_t i = g_directory_stack.size(); i > 0; --i) {
            all_dirs.push_back(g_directory_stack[i - 1]);
        }

        std::wstring home = get_environment_value(L"HOME");
        if (home.empty()) home = get_environment_value(L"USERPROFILE");
        auto format_entry = [&](const std::wstring& dir) -> std::wstring {
            if (!long_format && !home.empty() && starts_with_case_insensitive(dir, home)) {
                return L"~" + dir.substr(home.size());
            }
            return dir;
        };

        if (has_index) {
            size_t target_idx = 0;
            if (index_from_left) {
                target_idx = static_cast<size_t>(print_index);
            } else {
                if (static_cast<size_t>(print_index) >= all_dirs.size()) {
                    target_idx = all_dirs.size();
                } else {
                    target_idx = all_dirs.size() - 1 - static_cast<size_t>(print_index);
                }
            }
            if (target_idx >= all_dirs.size()) {
                std::wcerr << L"ksh: dirs: directory stack index out of range\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            write_builtin_output(format_entry(all_dirs[target_idx]) + L"\n");
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        if (vertical) {
            std::wstring out;
            for (size_t i = 0; i < all_dirs.size(); ++i) {
                out += std::to_wstring(i) + L"\t" + format_entry(all_dirs[i]) + L"\n";
            }
            write_builtin_output(out);
        } else if (one_per_line) {
            std::wstring out;
            for (size_t i = 0; i < all_dirs.size(); ++i) {
                out += format_entry(all_dirs[i]) + L"\n";
            }
            write_builtin_output(out);
        } else {
            std::wstring out;
            for (size_t i = 0; i < all_dirs.size(); ++i) {
                if (i > 0) out += L" ";
                out += format_entry(all_dirs[i]);
            }
            out += L"\n";
            write_builtin_output(out);
        }

        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    if (cmd == L"pushd") {
        if (tokens.size() > 2) {
            std::wcerr << L"ksh: pushd: too many arguments\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        std::wstring current_directory;
        if (!get_current_directory_path(current_directory)) {
            std::wcerr << L"ksh: pushd: unable to determine current directory\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        std::wstring target_directory;
        bool use_stack_entry = false;
        if (tokens.size() == 1) {
            if (g_directory_stack.empty()) {
                std::wcerr << L"ksh: pushd: directory stack empty\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            target_directory = g_directory_stack.back();
            use_stack_entry = true;
        } else {
            target_directory = normalize_cd_path(remove_quotes_and_escapes_from_token(tokens[1]));
        }

        if (!SetCurrentDirectoryW(target_directory.c_str())) {
            std::wcerr << L"ksh: pushd failed: " << target_directory << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        if (use_stack_entry) {
            g_directory_stack.pop_back();
        }
        g_directory_stack.push_back(current_directory);
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    if (cmd == L"popd") {
        if (tokens.size() != 1) {
            std::wcerr << L"ksh: popd: too many arguments\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        if (g_directory_stack.empty()) {
            std::wcerr << L"ksh: popd: directory stack empty\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        std::wstring target_directory = g_directory_stack.back();
        if (!SetCurrentDirectoryW(target_directory.c_str())) {
            std::wcerr << L"ksh: popd failed: " << target_directory << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        g_directory_stack.pop_back();
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    if (cmd == L"builtin") {
        if (tokens.size() == 1) {
            const std::vector<std::wstring>& builtins = builtin_commands();
            for (const std::wstring& builtin_name : builtins) {
                if (!write_builtin_output(builtin_name + L"\n")) {
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        size_t start = 1;
        if (tokens[1] == L"--") {
            start = 2;
        }

        if (tokens.size() <= start) {
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        const std::wstring builtin_name = tokens[start];
        std::wstring detail;
        if (resolve_command_kind(builtin_name, detail) != CommandResolutionKind::Builtin) {
            std::wcerr << L"ksh: builtin: " << builtin_name << L": not a shell builtin\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        std::vector<std::wstring> builtin_tokens(tokens.begin() + static_cast<std::ptrdiff_t>(start), tokens.end());
        std::wstring builtin_line = join_tokens_with_spaces(builtin_tokens);
        return execute_command_line(builtin_line, should_exit_shell, true, true);
    }

    if (cmd == L"read") {
        return execute_builtin_read(tokens);
    }

    if (cmd == L"help") {
        return execute_builtin_help(tokens, write_builtin_output);
    }

    if (cmd == L"version") {
        print_version_text(write_builtin_output);
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    if (cmd == L"clear") {
        if (tokens.size() != 1) {
            std::wcerr << L"ksh: clear: too many arguments\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        if (!clear_console_screen()) {
            std::wcerr << L"ksh: clear: unable to clear console\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    if (cmd == L"return") {
        if (!is_executing_function_scope()) {
            std::wcerr << L"ksh: return: not in a function\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        int status_code = 0;
        if (tokens.size() == 1) {
            std::map<std::wstring, std::wstring>::const_iterator status_it = ksh_env.variables.find(L"?");
            if (status_it != ksh_env.variables.end()) {
                if (!try_parse_int_strict(status_it->second, status_code)) {
                    status_code = 1;
                }
            }
        } else if (tokens.size() == 2) {
            if (!try_parse_int_strict(tokens[1], status_code)) {
                std::wcerr << L"ksh: return: numeric argument required\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
        } else {
            std::wcerr << L"ksh: return: too many arguments\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        request_function_return(status_code);
        ksh_env.variables[L"?"] = std::to_wstring(status_code);
        return true;
    }

    if (cmd == L"hash") {
        if (tokens.size() == 1) {
            if (g_command_hash_table.empty()) {
                ksh_env.variables[L"?"] = L"0";
                return true;
            }
            std::wstring out = L"hits\tcommand\n";
            for (const auto& [name, entry] : g_command_hash_table) {
                out += std::to_wstring(entry.hits) + L"\t" + entry.path + L"\n";
            }
            write_builtin_output(out);
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        bool verbose = false;
        bool refresh_only = false;
        bool delete_mode = false;
        bool print_path_only = false;
        size_t start = 1;

        while (start < tokens.size()) {
            if (tokens[start] == L"-r") {
                refresh_only = true;
                start++;
                continue;
            }
            if (tokens[start] == L"-v") {
                verbose = true;
                start++;
                continue;
            }
            if (tokens[start] == L"-d") {
                delete_mode = true;
                start++;
                continue;
            }
            if (tokens[start] == L"-t") {
                print_path_only = true;
                start++;
                continue;
            }
            if (tokens[start] == L"--") {
                start++;
                break;
            }
            break;
        }

        if (refresh_only) {
            g_command_hash_table.clear();
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        if (delete_mode) {
            bool ok = true;
            for (size_t i = start; i < tokens.size(); ++i) {
                if (g_command_hash_table.erase(tokens[i]) == 0) {
                    std::wcerr << L"ksh: hash: " << tokens[i] << L": not found\n";
                    ok = false;
                }
            }
            ksh_env.variables[L"?"] = ok ? L"0" : L"1";
            return ok;
        }

        if (print_path_only) {
            bool ok = true;
            for (size_t i = start; i < tokens.size(); ++i) {
                auto it = g_command_hash_table.find(tokens[i]);
                if (it != g_command_hash_table.end()) {
                    write_builtin_output(it->second.path + L"\n");
                } else {
                    std::wstring path;
                    if (resolve_external_command_path(tokens[i], path)) {
                        write_builtin_output(path + L"\n");
                    } else {
                        std::wcerr << L"ksh: hash: " << tokens[i] << L": not found\n";
                        ok = false;
                    }
                }
            }
            ksh_env.variables[L"?"] = ok ? L"0" : L"1";
            return ok;
        }

        if (tokens.size() <= start) {
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        bool ok = true;
        for (size_t i = start; i < tokens.size(); ++i) {
            std::wstring path;
            if (resolve_external_command_path(tokens[i], path)) {
                if (verbose) {
                    write_builtin_output(tokens[i] + L"=" + path + L"\n");
                }
            } else {
                std::wcerr << L"ksh: hash: " << tokens[i] << L": not found\n";
                ok = false;
            }
        }

        ksh_env.variables[L"?"] = ok ? L"0" : L"1";
        return ok;
    }

    if (cmd == L"source" || cmd == L".") {
        if (tokens.size() < 2) {
            std::wcerr << L"ksh: source requires a file path\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        std::vector<std::wstring> source_args;
        for (size_t i = 2; i < tokens.size(); ++i) {
            source_args.push_back(tokens[i]);
        }
        return execute_script_file(tokens[1], source_args, should_exit_shell);
    }

    if (cmd == L"set") {
        if (tokens.size() >= 2 && tokens[1] == L"-A") {
            if (tokens.size() < 3) {
                std::wcerr << L"ksh: set: usage: set -A name [value ...]\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            std::wstring array_name = resolve_variable_name(tokens[2]);
            if (!is_valid_shell_identifier(array_name) || is_internal_shell_state_variable(array_name)) {
                std::wcerr << L"ksh: set: invalid array name: " << tokens[2] << L"\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            if (get_flag_value(ksh_env.readonly_flags, array_name)) {
                std::wcerr << L"ksh: set: variable is read-only: " << array_name << L"\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            std::vector<std::wstring> values;
            for (size_t i = 3; i < tokens.size(); ++i) {
                values.push_back(tokens[i]);
            }

            set_flag_value(ksh_env.associative_flags, array_name, false);
            assign_array_parameter(array_name, values);
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        if (tokens.size() == 1) {
            for (std::map<std::wstring, std::wstring>::const_iterator it = ksh_env.variables.begin(); it != ksh_env.variables.end(); ++it) {
                if (is_internal_shell_state_variable(it->first)) {
                    continue;
                }
                if (!write_builtin_output(it->first + L"=" + it->second + L"\n")) {
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
            }
            for (std::map<std::wstring, std::map<std::wstring, std::wstring>>::const_iterator arr_it = ksh_env.arrays.begin(); arr_it != ksh_env.arrays.end(); ++arr_it) {
                std::wstring line = arr_it->first + L"=(";
                bool is_assoc = get_flag_value(ksh_env.associative_flags, arr_it->first);
                if (is_assoc) {
                    size_t idx = 0;
                    for (const auto& pair : arr_it->second) {
                        line += L"[" + pair.first + L"]=" + quote_command_argument(pair.second);
                        if (idx + 1 < arr_it->second.size()) {
                            line += L" ";
                        }
                        idx++;
                    }
                } else {
                    std::vector<std::wstring> values = get_array_values_vector(arr_it->first);
                    for (size_t i = 0; i < values.size(); ++i) {
                        line += quote_command_argument(values[i]);
                        if (i + 1 < values.size()) {
                            line += L" ";
                        }
                    }
                }
                line += L")\n";
                if (!write_builtin_output(line)) {
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        bool ok = true;
        bool end_of_options = false;
        std::vector<std::wstring> new_positionals;
        bool set_positionals = false;

        for (size_t i = 1; i < tokens.size(); ++i) {
            std::wstring arg = tokens[i];

            // "set --" ends option processing; everything after becomes $1 $2 ...
            if (!end_of_options && arg == L"--") {
                end_of_options = true;
                set_positionals = true;
                for (size_t j = i + 1; j < tokens.size(); ++j) {
                    new_positionals.push_back(tokens[j]);
                }
                break;
            }

            // "set -" alone: turn off xtrace and replace positionals with remaining
            if (!end_of_options && arg == L"-") {
                g_xtrace_enabled = false;
                set_positionals = true;
                for (size_t j = i + 1; j < tokens.size(); ++j) {
                    new_positionals.push_back(tokens[j]);
                }
                break;
            }

            if (!end_of_options && arg.size() > 1 && (arg[0] == L'-' || arg[0] == L'+')) {
                bool enable = (arg[0] == L'-');
                for (size_t j = 1; j < arg.size(); ++j) {
                    wchar_t flag = arg[j];
                    if (flag == L'x') {
                        g_xtrace_enabled = enable;
                    } else if (flag == L'e') {
                        g_errexit_enabled = enable;
                    } else if (flag == L'u') {
                        g_nounset_enabled = enable;
                    } else if (flag == L'o') {
                        std::wstring opt_name;
                        if (j + 1 < arg.size()) {
                            opt_name = arg.substr(j + 1);
                            j = arg.size();
                        } else if (i + 1 < tokens.size()) {
                            opt_name = tokens[++i];
                        } else {
                            std::wcerr << L"ksh: set: " << arg << L" requires option name\n";
                            ok = false;
                            break;
                        }

                        if (opt_name == L"xtrace") {
                            g_xtrace_enabled = enable;
                        } else if (opt_name == L"errexit") {
                            g_errexit_enabled = enable;
                        } else if (opt_name == L"nounset") {
                            g_nounset_enabled = enable;
                        } else if (opt_name == L"vi") {
                            g_vi_mode_enabled = enable;
                        } else if (opt_name == L"pipefail") {
                            g_pipefail_enabled = enable;
                        } else if (opt_name == L"powershellbypass") {
                            g_powershell_bypass_enabled = enable;
                        } else {
                            std::wcerr << L"ksh: set: unsupported option: " << opt_name << L"\n";
                            ok = false;
                        }
                    } else {
                        std::wcerr << L"ksh: set: unsupported flag: " << flag << L"\n";
                        ok = false;
                    }
                }
            } else {
                // Bare word — treat as start of positional parameter list
                set_positionals = true;
                new_positionals.push_back(arg);
                for (size_t j = i + 1; j < tokens.size(); ++j) {
                    new_positionals.push_back(tokens[j]);
                }
                break;
            }
        }

        // Apply new positional parameters if requested
        if (set_positionals) {
            if (!g_script_context_stack.empty()) {
                g_script_context_stack.back().args = new_positionals;
            } else {
                // At top level with no script context, push one
                ScriptContext ctx;
                ctx.script_name = L"ksh";
                ctx.args = new_positionals;
                g_script_context_stack.push_back(ctx);
            }
            // Sync $#, $*, $@, and individual $1..$N into the variable table
            ksh_env.variables[L"#"] = std::to_wstring(new_positionals.size());
            std::wstring star;
            for (size_t k = 0; k < new_positionals.size(); ++k) {
                if (k > 0) star += L" ";
                star += new_positionals[k];
            }
            ksh_env.variables[L"*"] = star;
            ksh_env.variables[L"@"] = star;
            for (size_t k = 0; k < new_positionals.size(); ++k) {
                ksh_env.variables[std::to_wstring(k + 1)] = new_positionals[k];
            }
        }

        ksh_env.variables[L"?"] = ok ? L"0" : L"1";
        return ok;
    }

    if (cmd == L"unset") {
        bool unset_functions = false;
        size_t start = 1;
        if (tokens.size() > 1 && tokens[1] == L"-f") {
            unset_functions = true;
            start = 2;
        }

        if (tokens.size() <= start) {
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        bool ok = true;
        for (size_t i = start; i < tokens.size(); ++i) {
            if (unset_functions) {
                g_shell_functions.erase(tokens[i]);
                continue;
            }

            std::wstring base_name;
            std::wstring index_text;
            bool has_index = false;
            if (!parse_array_reference_expression(tokens[i], base_name, index_text, has_index)) {
                std::wcerr << L"ksh: unset: invalid name: " << tokens[i] << L"\n";
                ok = false;
                continue;
            }

            if (has_index) {
                std::wstring resolved = resolve_variable_name(tokens[i]);
                if (!parse_array_reference_expression(resolved, base_name, index_text, has_index)) {
                    std::wcerr << L"ksh: unset: invalid name: " << tokens[i] << L"\n";
                    ok = false;
                    continue;
                }
            }

            if (is_internal_shell_state_variable(base_name)) {
                std::wcerr << L"ksh: unset: cannot unset internal shell state: " << base_name << L"\n";
                ok = false;
                continue;
            }

            if (get_flag_value(ksh_env.readonly_flags, base_name)) {
                std::wcerr << L"ksh: unset: " << base_name << L": is read only\n";
                ok = false;
                continue;
            }

            if (has_index) {
                bool is_assoc = get_flag_value(ksh_env.associative_flags, base_name);
                if (is_assoc) {
                    std::map<std::wstring, std::map<std::wstring, std::wstring>>::iterator arr_it = ksh_env.arrays.find(base_name);
                    if (arr_it != ksh_env.arrays.end()) {
                        arr_it->second.erase(index_text);
                    }
                } else {
                    size_t index = 0;
                    if (!parse_non_negative_index(index_text, index)) {
                        std::wcerr << L"ksh: unset: array index must be numeric: " << tokens[i] << L"\n";
                        ok = false;
                        continue;
                    }

                    std::map<std::wstring, std::map<std::wstring, std::wstring>>::iterator arr_it = ksh_env.arrays.find(base_name);
                    if (arr_it != ksh_env.arrays.end()) {
                        arr_it->second.erase(std::to_wstring(index));
                    }
                }
                continue;
            }

            ksh_env.variables.erase(base_name);
            ksh_env.arrays.erase(base_name);
            ksh_env.associative_flags.erase(base_name);
            ksh_env.nameref_flags.erase(base_name);
            ksh_env.exported.erase(base_name);
            ksh_env.readonly_flags.erase(base_name);
            ksh_env.integer_flags.erase(base_name);
            ksh_env.uppercase_flags.erase(base_name);
            ksh_env.lowercase_flags.erase(base_name);
            SetEnvironmentVariableW(base_name.c_str(), nullptr);
        }

        ksh_env.variables[L"?"] = ok ? L"0" : L"1";
        return ok;
    }

    if (cmd == L"let") {
        if (tokens.size() < 2) {
            std::wcerr << L"ksh: let: requires an expression\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        std::wstring expression = join_tokens_with_spaces(tokens, 1);
        std::wstring value = evaluate_arithmetic(expression);
        bool nonzero = false;
        try {
            nonzero = std::stod(value) != 0.0;
        } catch (...) {
            nonzero = false;
        }

        ksh_env.variables[L"?"] = nonzero ? L"0" : L"1";
        return true;
    }

    if (cmd == L"export") {
        if (tokens.size() == 1 || (tokens.size() == 2 && tokens[1] == L"-p")) {
            for (std::map<std::wstring, bool>::const_iterator it = ksh_env.exported.begin(); it != ksh_env.exported.end(); ++it) {
                if (!it->second) {
                    continue;
                }
                std::wstring value;
                std::map<std::wstring, std::wstring>::const_iterator scalar_it = ksh_env.variables.find(it->first);
                if (scalar_it != ksh_env.variables.end()) {
                    value = scalar_it->second;
                }
                if (!write_builtin_output(L"export " + it->first + L"=" + quote_command_argument(value) + L"\n")) {
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        bool ok = true;
        for (size_t i = 1; i < tokens.size(); ++i) {
            const std::wstring& token = tokens[i];
            size_t eq = token.find(L'=');
            std::wstring name = token;
            if (eq != std::wstring::npos) {
                name = token.substr(0, eq);
                std::wstring rhs = token.substr(eq + 1);
                std::wstring assignment_error;
                if (!assign_parameter_value(name, rhs, false, false, assignment_error)) {
                    std::wcerr << L"ksh: export: " << assignment_error << L"\n";
                    ok = false;
                    continue;
                }
            }

            if (!is_valid_shell_identifier(name)) {
                std::wcerr << L"ksh: export: invalid name: " << name << L"\n";
                ok = false;
                continue;
            }

            if (is_internal_shell_state_variable(name)) {
                std::wcerr << L"ksh: export: cannot export internal shell state: " << name << L"\n";
                ok = false;
                continue;
            }

            set_flag_value(ksh_env.exported, name, true);
            sync_exported_environment_variable(name);
        }

        ksh_env.variables[L"?"] = ok ? L"0" : L"1";
        return ok;
    }

    if (cmd == L"disown") {
        update_background_jobs(false);

        if (tokens.size() == 1) {
            BackgroundJob* default_job = find_default_background_job_for_resume_or_foreground();
            if (default_job == nullptr) {
                std::wcerr << L"ksh: disown: no current job\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            remove_background_job(default_job->id);
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        if (tokens[1] == L"-a") {
            cleanup_all_background_jobs();
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        bool ok = true;
        for (size_t i = 1; i < tokens.size(); ++i) {
            int job_id = 0;
            if (!resolve_job_reference(tokens[i], job_id)) {
                std::wcerr << L"ksh: disown: invalid job id\n";
                ok = false;
                continue;
            }

            if (find_background_job(job_id) == nullptr) {
                std::wcerr << L"ksh: disown: job not found: " << job_id << L"\n";
                ok = false;
                continue;
            }

            remove_background_job(job_id);
        }

        ksh_env.variables[L"?"] = ok ? L"0" : L"1";
        return ok;
    }

    if (cmd == L"readonly") {
        if (tokens.size() == 1 || (tokens.size() == 2 && tokens[1] == L"-p")) {
            for (std::map<std::wstring, bool>::const_iterator it = ksh_env.readonly_flags.begin(); it != ksh_env.readonly_flags.end(); ++it) {
                if (!it->second) {
                    continue;
                }

                std::wstring value;
                std::map<std::wstring, std::wstring>::const_iterator scalar_it = ksh_env.variables.find(it->first);
                if (scalar_it != ksh_env.variables.end()) {
                    value = scalar_it->second;
                }

                if (!write_builtin_output(L"readonly " + it->first + L"=" + quote_command_argument(value) + L"\n")) {
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        bool ok = true;
        for (size_t i = 1; i < tokens.size(); ++i) {
            const std::wstring& token = tokens[i];
            size_t eq = token.find(L'=');
            std::wstring name = token;
            if (eq != std::wstring::npos) {
                name = token.substr(0, eq);
                std::wstring rhs = token.substr(eq + 1);
                std::wstring assignment_error;
                if (!assign_parameter_value(name, rhs, false, false, assignment_error)) {
                    std::wcerr << L"ksh: readonly: " << assignment_error << L"\n";
                    ok = false;
                    continue;
                }
            }

            if (!is_valid_shell_identifier(name)) {
                std::wcerr << L"ksh: readonly: invalid name: " << name << L"\n";
                ok = false;
                continue;
            }

            if (is_internal_shell_state_variable(name)) {
                std::wcerr << L"ksh: readonly: cannot mark internal shell state readonly: " << name << L"\n";
                ok = false;
                continue;
            }

            set_flag_value(ksh_env.readonly_flags, name, true);
        }

        ksh_env.variables[L"?"] = ok ? L"0" : L"1";
        return ok;
    }

    if (cmd == L"alias") {
        if (tokens.size() == 1) {
            for (std::map<std::wstring, std::wstring>::const_iterator it = g_aliases.begin(); it != g_aliases.end(); ++it) {
                if (!write_builtin_output(L"alias " + it->first + L"=" + quote_for_single_quoted_shell_literal(it->second) + L"\n")) {
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        bool ok = true;
        for (size_t i = 1; i < tokens.size(); ++i) {
            const std::wstring& token = tokens[i];
            size_t eq = token.find(L'=');
            if (eq == std::wstring::npos) {
                std::map<std::wstring, std::wstring>::const_iterator it = g_aliases.find(token);
                if (it == g_aliases.end()) {
                    std::wcerr << L"ksh: alias: not found: " << token << L"\n";
                    ok = false;
                    continue;
                }
                if (!write_builtin_output(L"alias " + it->first + L"=" + quote_for_single_quoted_shell_literal(it->second) + L"\n")) {
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
                continue;
            }

            std::wstring name = token.substr(0, eq);
            std::wstring value = token.substr(eq + 1);
            if (!is_valid_shell_identifier(name)) {
                std::wcerr << L"ksh: alias: invalid name: " << name << L"\n";
                ok = false;
                continue;
            }
            // Enforce per-session alias table cap (new entries only).
            if (g_aliases.find(name) == g_aliases.end() && g_aliases.size() >= kMaxShellAliases) {
                std::wcerr << L"ksh: alias table limit reached, cannot add: " << name << L"\n";
                ok = false;
                continue;
            }
            g_aliases[name] = value;
        }

        ksh_env.variables[L"?"] = ok ? L"0" : L"1";
        return ok;
    }

    if (cmd == L"unalias") {
        if (tokens.size() < 2) {
            std::wcerr << L"ksh: unalias requires an alias name or -a\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        if (tokens[1] == L"-a") {
            g_aliases.clear();
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        bool ok = true;
        for (size_t i = 1; i < tokens.size(); ++i) {
            if (g_aliases.erase(tokens[i]) == 0) {
                std::wcerr << L"ksh: unalias: not found: " << tokens[i] << L"\n";
                ok = false;
            }
        }
        ksh_env.variables[L"?"] = ok ? L"0" : L"1";
        return ok;
    }

    if (cmd == L"type" || cmd == L"whence" || cmd == L"command") {
        bool as_type = (cmd == L"type");
        bool as_whence = (cmd == L"whence");

        if (cmd == L"command" && tokens.size() > 1 && (tokens[1] == L"-v" || tokens[1] == L"-V")) {
            bool verbose = (tokens[1] == L"-V");
            if (tokens.size() < 3) {
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            bool all_found = true;
            for (size_t i = 2; i < tokens.size(); ++i) {
                std::wstring out = command_kind_description(tokens[i], verbose, false);
                if (out.empty() || out == L"not found" || out.find(L"\tnot found") != std::wstring::npos) {
                    all_found = false;
                    continue;
                }
                if (!write_builtin_output(out + L"\n")) {
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
            }

            ksh_env.variables[L"?"] = all_found ? L"0" : L"1";
            return all_found;
        }

        if (as_type || as_whence) {
            bool verbose = as_type;
            bool path_only = false;
            bool no_functions = false;
            bool all_matches = false;
            size_t start_index = 1;

            while (start_index < tokens.size()) {
                const std::wstring& arg = tokens[start_index];
                if (arg == L"--") {
                    start_index++;
                    break;
                }
                if (arg.size() > 1 && arg[0] == L'-') {
                    bool valid_flag_token = true;
                    for (size_t c = 1; c < arg.size(); ++c) {
                        if (arg[c] == L'v') {
                            verbose = true;
                        } else if (arg[c] == L'p') {
                            path_only = true;
                        } else if (arg[c] == L'f') {
                            no_functions = true;
                        } else if (arg[c] == L'a') {
                            all_matches = true;
                        } else {
                            valid_flag_token = false;
                            break;
                        }
                    }
                    if (valid_flag_token) {
                        start_index++;
                        continue;
                    }
                }
                break;
            }

            if (start_index >= tokens.size()) {
                std::wcerr << L"ksh: " << cmd << L": requires one or more command names\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            (void)all_matches;
            bool all_found = true;
            for (size_t i = start_index; i < tokens.size(); ++i) {
                const std::wstring& target = tokens[i];
                std::wstring detail;
                CommandResolutionKind kind = resolve_command_kind(target, detail);

                if (path_only && (kind == CommandResolutionKind::Alias || kind == CommandResolutionKind::Function || kind == CommandResolutionKind::Builtin)) {
                    std::wstring ext_path;
                    if (resolve_external_command_path(target, ext_path)) {
                        kind = CommandResolutionKind::External;
                        detail = ext_path;
                    } else {
                        kind = CommandResolutionKind::Missing;
                    }
                }
                if (no_functions && kind == CommandResolutionKind::Function) {
                    std::wstring ext_path;
                    if (resolve_external_command_path(target, ext_path)) {
                        kind = CommandResolutionKind::External;
                        detail = ext_path;
                    } else {
                        kind = CommandResolutionKind::Missing;
                    }
                }

                if (kind == CommandResolutionKind::Missing) {
                    all_found = false;
                    std::wcerr << L"ksh: " << cmd << L": " << target << L": not found\n";
                    continue;
                }

                std::wstring line;
                if (verbose) {
                    if (kind == CommandResolutionKind::Alias) {
                        line = target + L" is an alias for " + detail;
                    } else if (kind == CommandResolutionKind::Function) {
                        line = target + L" is a function";
                    } else if (kind == CommandResolutionKind::Builtin) {
                        line = target + L" is a shell builtin";
                    } else if (kind == CommandResolutionKind::External) {
                        line = target + L" is " + detail;
                    }
                } else {
                    if (kind == CommandResolutionKind::Alias) {
                        line = detail;
                    } else if (kind == CommandResolutionKind::Function || kind == CommandResolutionKind::Builtin) {
                        line = target;
                    } else if (kind == CommandResolutionKind::External) {
                        line = detail;
                    }
                }

                if (!write_builtin_output(line + L"\n")) {
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
            }

            ksh_env.variables[L"?"] = all_found ? L"0" : L"1";
            return all_found;
        }

        if (cmd == L"command") {
            size_t start = 1;
            if (tokens.size() > 1 && tokens[1] == L"--") {
                start = 2;
            }

            if (tokens.size() <= start) {
                ksh_env.variables[L"?"] = L"0";
                return true;
            }

            std::vector<std::wstring> command_tokens(tokens.begin() + static_cast<std::ptrdiff_t>(start), tokens.end());
            const std::wstring target_name = command_tokens.front();

            std::map<std::wstring, ShellFunctionDefinition>::iterator fn_it = g_shell_functions.find(target_name);
            if (fn_it == g_shell_functions.end()) {
                std::wstring eval_line = join_tokens_with_spaces(command_tokens);
                return execute_command_line(eval_line, should_exit_shell);
            }

            ShellFunctionDefinition saved_function = fn_it->second;
            g_shell_functions.erase(fn_it);

            std::wstring eval_line = join_tokens_with_spaces(command_tokens);
            bool invoke_ok = execute_command_line(eval_line, should_exit_shell);

            g_shell_functions[target_name] = saved_function;
            return invoke_ok;
        }
    }

    if (cmd == L"eval") {
        if (tokens.size() < 2) {
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        ScopedEvalDepth eval_depth;
        if (!eval_depth.active()) {
            std::wcerr << L"ksh: eval: maximum recursion depth exceeded\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        std::wstring eval_line = join_tokens_with_spaces(tokens, 1);
        return execute_command_line(eval_line, should_exit_shell);
    }

    if (cmd == L"getopts") {
        if (tokens.size() < 3) {
            std::wcerr << L"ksh: getopts: usage: getopts optstring name [arg ...]\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        const std::wstring optstring = tokens[1];
        const std::wstring name = tokens[2];
        if (!is_valid_shell_identifier(name)) {
            std::wcerr << L"ksh: getopts: invalid variable name: " << name << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        std::vector<std::wstring> args;
        if (tokens.size() > 3) {
            args.assign(tokens.begin() + 3, tokens.end());
        } else {
            args = current_script_args();
        }

        int optind = 1;
        std::map<std::wstring, std::wstring>::const_iterator optind_it = ksh_env.variables.find(L"OPTIND");
        if (optind_it != ksh_env.variables.end()) {
            int parsed = 0;
            if (parse_positive_int(optind_it->second, parsed)) {
                optind = parsed;
            }
        }

        int cursor = 1;
        std::map<std::wstring, std::wstring>::const_iterator mirror_it = ksh_env.variables.find(kGetoptsOptindMirrorVar);
        if (mirror_it != ksh_env.variables.end() && mirror_it->second == std::to_wstring(optind)) {
            std::map<std::wstring, std::wstring>::const_iterator cursor_it = ksh_env.variables.find(kGetoptsCursorVar);
            if (cursor_it != ksh_env.variables.end()) {
                int parsed_cursor = 0;
                if (parse_positive_int(cursor_it->second, parsed_cursor)) {
                    cursor = parsed_cursor;
                }
            }
        }

        auto finish_no_more_options = [&]() {
            ksh_env.variables[name] = L"?";
            ksh_env.variables[L"OPTIND"] = std::to_wstring(optind);
            ksh_env.variables[kGetoptsCursorVar] = L"1";
            ksh_env.variables[kGetoptsOptindMirrorVar] = std::to_wstring(optind);
            ksh_env.variables[L"OPTARG"] = L"";
            ksh_env.variables[L"?"] = L"1";
            return true;
        };

        while (optind >= 1 && static_cast<size_t>(optind) <= args.size()) {
            const std::wstring& arg = args[static_cast<size_t>(optind - 1)];
            if (arg == L"--" && cursor <= 1) {
                optind++;
                return finish_no_more_options();
            }

            if (arg.size() < 2 || arg[0] != L'-') {
                return finish_no_more_options();
            }

            if (cursor >= static_cast<int>(arg.size())) {
                optind++;
                cursor = 1;
                continue;
            }

            wchar_t optchar = arg[static_cast<size_t>(cursor)];
            cursor++;

            size_t pos = optstring.find(optchar);
            bool silent = !optstring.empty() && optstring[0] == L':';
            if (pos == std::wstring::npos || optchar == L':') {
                ksh_env.variables[name] = L"?";
                ksh_env.variables[L"OPTARG"] = std::wstring(1, optchar);
                if (!silent) {
                    std::wcerr << L"ksh: getopts: illegal option -- " << optchar << L"\n";
                }
                if (cursor >= static_cast<int>(arg.size())) {
                    optind++;
                    cursor = 1;
                }
                ksh_env.variables[L"OPTIND"] = std::to_wstring(optind);
                ksh_env.variables[kGetoptsCursorVar] = std::to_wstring(cursor);
                ksh_env.variables[kGetoptsOptindMirrorVar] = std::to_wstring(optind);
                ksh_env.variables[L"?"] = L"0";
                return true;
            }

            bool requires_argument = (pos + 1 < optstring.size() && optstring[pos + 1] == L':');
            if (requires_argument) {
                std::wstring optarg;
                if (cursor < static_cast<int>(arg.size())) {
                    optarg = arg.substr(static_cast<size_t>(cursor));
                    optind++;
                    cursor = 1;
                } else if (static_cast<size_t>(optind) < args.size()) {
                    optarg = args[static_cast<size_t>(optind)];
                    optind += 2;
                    cursor = 1;
                } else {
                    if (silent) {
                        ksh_env.variables[name] = L":";
                        ksh_env.variables[L"OPTARG"] = std::wstring(1, optchar);
                    } else {
                        ksh_env.variables[name] = L"?";
                        ksh_env.variables[L"OPTARG"] = L"";
                        std::wcerr << L"ksh: getopts: option requires an argument -- " << optchar << L"\n";
                    }
                    ksh_env.variables[L"OPTIND"] = std::to_wstring(optind);
                    ksh_env.variables[kGetoptsCursorVar] = L"1";
                    ksh_env.variables[kGetoptsOptindMirrorVar] = std::to_wstring(optind);
                    ksh_env.variables[L"?"] = L"0";
                    return true;
                }

                ksh_env.variables[name] = std::wstring(1, optchar);
                ksh_env.variables[L"OPTARG"] = optarg;
            } else {
                ksh_env.variables[name] = std::wstring(1, optchar);
                ksh_env.variables[L"OPTARG"] = L"";
                if (cursor >= static_cast<int>(arg.size())) {
                    optind++;
                    cursor = 1;
                }
            }

            ksh_env.variables[L"OPTIND"] = std::to_wstring(optind);
            ksh_env.variables[kGetoptsCursorVar] = std::to_wstring(cursor);
            ksh_env.variables[kGetoptsOptindMirrorVar] = std::to_wstring(optind);
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        return finish_no_more_options();
    }

    if (is_ksh_script_path(cmd)) {
        std::vector<std::wstring> script_args;
        for (size_t i = 1; i < tokens.size(); ++i) {
            script_args.push_back(tokens[i]);
        }

        if (!run_in_background) {
            return execute_script_file(cmd, script_args, should_exit_shell);
        }

        std::wstring script_command;
        if (!build_self_script_command(cmd, script_args, script_command)) {
            std::wcerr << L"ksh: failed to resolve shell executable path\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        HANDLE process_handle = nullptr;
        DWORD pid = 0;
        if (!launch_process(script_command, process_handle, pid)) {
            std::wcerr << L"ksh: failed to start background script\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        BackgroundJob job;
        job.id = g_next_job_id++;
        job.pid = pid;
        job.process_handle = process_handle;
        job.pids.push_back(pid);
        job.process_handles.push_back(process_handle);
        job.command = script_command;
        job.completed = false;
        job.exit_code = STILL_ACTIVE;
        job.completion_reported = false;
        g_background_jobs.push_back(job);

        std::wcout << L"[" << job.id << L"] " << job.pid << L"\n";
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    if (cmd == L"history") {
        for (size_t i = 0; i < g_command_history.size(); ++i) {
            std::wstring line = std::to_wstring(i + 1) + L"  ";
            if (history_timestamps_enabled() && !g_command_history[i].timestamp.empty()) {
                line += L"[" + g_command_history[i].timestamp + L"] ";
            }
            line += g_command_history[i].command + L"\n";
            if (!write_builtin_output(line)) {
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
        }
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    if (cmd == L"jobs") {
        bool long_format = false;
        bool pid_only = false;
        bool end_of_options = false;
        std::vector<int> requested_job_ids;

        for (size_t i = 1; i < tokens.size(); ++i) {
            const std::wstring& token = tokens[i];
            if (!end_of_options && token == L"--") {
                end_of_options = true;
                continue;
            }

            if (!end_of_options && token.size() > 1 && token[0] == L'-') {
                for (size_t flag_index = 1; flag_index < token.size(); ++flag_index) {
                    if (token[flag_index] == L'l') {
                        long_format = true;
                    } else if (token[flag_index] == L'p') {
                        pid_only = true;
                    } else {
                        std::wcerr << L"ksh: jobs: invalid option: " << token << L"\n";
                        ksh_env.variables[L"?"] = L"1";
                        return false;
                    }
                }
                continue;
            }

            int job_id = 0;
            if (!resolve_job_reference(token, job_id)) {
                std::wcerr << L"ksh: jobs: invalid job id: " << token << L"\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            requested_job_ids.push_back(job_id);
        }

        update_background_jobs(false);
        for (const BackgroundJob& job : g_background_jobs) {
            if (!requested_job_ids.empty() &&
                std::find(requested_job_ids.begin(), requested_job_ids.end(), job.id) == requested_job_ids.end()) {
                continue;
            }

            std::wstring line;
            if (pid_only) {
                line = std::to_wstring(job.pid);
            } else {
                line = format_background_job_line(job);
                if (long_format && job.completed) {
                    line = L"[" + std::to_wstring(job.id) + L"] Done (pid=" +
                        std::to_wstring(job.pid) + L", exit=" + std::to_wstring(job.exit_code) + L") " + job.command;
                }
            }
            line += L"\n";
            if (!write_builtin_output(line)) {
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
        }
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    if (cmd == L"fg") {
        int job_id = 0;
        if (tokens.size() < 2) {
            BackgroundJob* default_job = find_default_background_job_for_resume_or_foreground();
            if (default_job == nullptr) {
                std::wcerr << L"ksh: fg: no current job\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            job_id = default_job->id;
        } else if (!resolve_job_reference(tokens[1], job_id)) {
            std::wcerr << L"ksh: invalid job id\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        update_background_jobs(false);
        BackgroundJob* job = find_background_job(job_id);
        if (job == nullptr) {
            std::wcerr << L"ksh: job not found: " << job_id << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        bool wait_ok = true;
        if (!job->completed) {
            std::wstring line = L"[" + std::to_wstring(job->id) + L"] foreground " + job->command + L"\n";
            if (!write_builtin_output(line)) {
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            DWORD code = 1;
            wait_ok = wait_for_background_job(*job, code);
        }

        std::wstring exit_code = std::to_wstring(job->exit_code);
        remove_background_job(job_id);
        ksh_env.variables[L"?"] = exit_code;
        return wait_ok;
    }

    if (cmd == L"bg") {
        int job_id = 0;
        if (tokens.size() < 2) {
            BackgroundJob* default_job = find_default_background_job_for_resume_or_foreground();
            if (default_job == nullptr) {
                std::wcerr << L"ksh: bg: no current job\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            job_id = default_job->id;
        } else if (!resolve_job_reference(tokens[1], job_id)) {
            std::wcerr << L"ksh: invalid job id\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        update_background_jobs(false);
        BackgroundJob* job = find_background_job(job_id);
        if (job == nullptr) {
            std::wcerr << L"ksh: job not found: " << job_id << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        if (job->completed) {
            std::wcerr << L"ksh: bg: job already completed: " << job_id << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        std::wstring line = L"[" + std::to_wstring(job->id) + L"] " + job->command + L"\n";
        if (!write_builtin_output(line)) {
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    if (cmd == L"kill") {
        if (tokens.size() < 2) {
            std::wcerr << L"ksh: kill: usage: kill [-l] [-s signal] [-p | -j] pid|%job...\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        
        if (tokens[1] == L"-l") {
            write_builtin_output(L"1) SIGHUP   2) SIGINT   3) SIGQUIT   8) SIGFPE   9) SIGKILL  10) SIGUSR1  11) SIGSEGV  12) SIGUSR2  14) SIGALRM  15) SIGTERM  17) SIGCHLD\n");
            ksh_env.variables[L"?"] = L"0";
            return true;
        }
        
        std::wstring signal_spec = L"TERM";

        enum class KillTargetMode {
            Auto,
            Process,
            Job
        };

        KillTargetMode target_mode = KillTargetMode::Auto;
        size_t target_start = 1;
        while (target_start < tokens.size()) {
            const std::wstring& option = tokens[target_start];
            if (option == L"-s") {
                if (target_start + 1 >= tokens.size()) {
                    std::wcerr << L"ksh: kill: -s requires an argument\n";
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
                signal_spec = tokens[target_start + 1];
                target_start += 2;
                continue;
            }

            if (option == L"-p") {
                target_mode = KillTargetMode::Process;
                target_start++;
                continue;
            }

            if (option == L"-j") {
                target_mode = KillTargetMode::Job;
                target_start++;
                continue;
            }

            if (option.size() > 1 && option[0] == L'-') {
                signal_spec = option.substr(1);
                target_start++;
                continue;
            }

            break;
        }
        
        if (target_start >= tokens.size()) {
            std::wcerr << L"ksh: kill: missing destination pid or job id\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        KillBuiltinSignal selected_signal = KillBuiltinSignal::Term;
        if (!parse_kill_builtin_signal(signal_spec, selected_signal)) {
            std::wcerr << L"ksh: kill: unsupported signal: " << signal_spec << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        
        bool all_ok = true;
        const DWORD self_pid = GetCurrentProcessId();
        ScopedDebugPrivilege scoped_debug_privilege;
        for (size_t i = target_start; i < tokens.size(); ++i) {
            const std::wstring& target = tokens[i];

            int job_id = 0;
            const bool explicit_job_target = !target.empty() && target[0] == L'%';
            const bool treat_as_job =
                explicit_job_target ||
                (target_mode == KillTargetMode::Job && resolve_job_reference(target, job_id));

            if (treat_as_job) {
                if (!resolve_job_reference(target, job_id)) {
                    std::wcerr << L"ksh: kill: invalid job target: " << target << L"\n";
                    all_ok = false;
                    continue;
                }
                BackgroundJob* job = find_background_job(job_id);
                if (job == nullptr) {
                    std::wcerr << L"ksh: kill: job not found: " << target << L"\n";
                    all_ok = false;
                    continue;
                }
                
                if (!job->completed) {
                    if (job->process_handles.empty() && job->process_handle != nullptr) {
                        job->process_handles.push_back(job->process_handle);
                    }
                    for (HANDLE handle : job->process_handles) {
                        if (handle != nullptr && handle != INVALID_HANDLE_VALUE) {
                            TerminateProcess(handle, 1);
                        }
                    }
                    DWORD ignored_code = 1;
                    wait_for_background_job(*job, ignored_code);
                }
                remove_background_job(job_id);
            } else {
                unsigned long parsed_pid = 0;
                if (!try_parse_unsigned_long_strict(target, parsed_pid)) {
                    std::wcerr << L"ksh: kill: invalid target: " << target << L"\n";
                    all_ok = false;
                    continue;
                }
                DWORD pid = static_cast<DWORD>(parsed_pid);
                std::wstring normalized_signal = signal_spec_to_trap_event(signal_spec);
                if (pid == self_pid && !normalized_signal.empty()) {
                    if (!queue_pending_trap_event(normalized_signal)) {
                        std::wcerr << L"ksh: kill: unsupported signal: " << signal_spec << L"\n";
                        all_ok = false;
                    }
                    continue;
                }

                bool success = false;
                switch (selected_signal) {
                case KillBuiltinSignal::Hup:
                case KillBuiltinSignal::Term:
                    success = graceful_kill_process_by_pid(pid);
                    break;
                case KillBuiltinSignal::Int:
                    success = send_console_event_to_pid(pid, CTRL_C_EVENT);
                    break;
                case KillBuiltinSignal::Quit:
                    success = send_console_event_to_pid(pid, CTRL_BREAK_EVENT);
                    break;
                case KillBuiltinSignal::Kill:
                    success = force_kill_process_by_pid(pid);
                    break;
                }

                if (!success) {
                    DWORD failure_error = GetLastError();
                    std::wcerr << L"ksh: kill: " << pid << L": ";
                    if (failure_error == ERROR_INVALID_PARAMETER || failure_error == ERROR_PROC_NOT_FOUND) {
                        std::wcerr << L"no such process\n";
                    } else if (failure_error == ERROR_ACCESS_DENIED) {
                        std::wcerr << L"operation not permitted\n";
                    } else {
                        std::wcerr << L"error " << failure_error << L"\n";
                    }
                    all_ok = false;
                }
            }
        }
        
        ksh_env.variables[L"?"] = all_ok ? L"0" : L"1";
        return all_ok;
    }

    if (cmd == L"find") {
        std::vector<std::wstring> paths;
        std::wstring name_pattern;
        bool iname = false;
        std::wstring type_filter;
        int maxdepth = -1;
        int mindepth = -1;
        bool check_empty = false;
        bool has_size = false;
        wchar_t size_op = L'=';
        uint64_t size_bytes = 0;
        
        size_t idx = 1;
        while (idx < tokens.size() && (tokens[idx].empty() || (tokens[idx][0] != L'-' && tokens[idx] != L"!"))) {
            paths.push_back(tokens[idx]);
            idx++;
        }
        
        if (paths.empty()) {
            paths.push_back(L".");
        }
        
        bool has_error = false;
        bool next_not = false;
        while (idx < tokens.size()) {
            const std::wstring& opt = tokens[idx];
            if (opt == L"!" || opt == L"-not") {
                next_not = !next_not;
                idx++;
                continue;
            }
            if (opt == L"-name" || opt == L"-iname") {
                if (idx + 1 >= tokens.size()) {
                    std::wcerr << L"ksh: find: " << opt << L" requires an argument\n";
                    has_error = true;
                    break;
                }
                name_pattern = tokens[idx + 1];
                iname = (opt == L"-iname");
                idx += 2;
            } else if (opt == L"-type") {
                if (idx + 1 >= tokens.size()) {
                    std::wcerr << L"ksh: find: -type requires an argument\n";
                    has_error = true;
                    break;
                }
                type_filter = tokens[idx + 1];
                if (type_filter != L"f" && type_filter != L"d") {
                    std::wcerr << L"ksh: find: invalid type: " << type_filter << L"\n";
                    has_error = true;
                    break;
                }
                idx += 2;
            } else if (opt == L"-maxdepth") {
                if (idx + 1 >= tokens.size()) {
                    std::wcerr << L"ksh: find: -maxdepth requires an argument\n";
                    has_error = true;
                    break;
                }
                if (!try_parse_int_strict(tokens[idx + 1], maxdepth) || maxdepth < 0) {
                    std::wcerr << L"ksh: find: invalid -maxdepth: " << tokens[idx + 1] << L"\n";
                    has_error = true;
                    break;
                }
                idx += 2;
            } else if (opt == L"-mindepth") {
                if (idx + 1 >= tokens.size()) {
                    std::wcerr << L"ksh: find: -mindepth requires an argument\n";
                    has_error = true;
                    break;
                }
                if (!try_parse_int_strict(tokens[idx + 1], mindepth) || mindepth < 0) {
                    std::wcerr << L"ksh: find: invalid -mindepth: " << tokens[idx + 1] << L"\n";
                    has_error = true;
                    break;
                }
                idx += 2;
            } else if (opt == L"-empty") {
                check_empty = true;
                idx++;
            } else if (opt == L"-size") {
                if (idx + 1 >= tokens.size()) {
                    std::wcerr << L"ksh: find: -size requires an argument\n";
                    has_error = true;
                    break;
                }
                std::wstring s_arg = tokens[idx + 1];
                if (s_arg.empty()) {
                    std::wcerr << L"ksh: find: invalid -size argument\n";
                    has_error = true;
                    break;
                }
                size_op = L'=';
                if (s_arg[0] == L'+' || s_arg[0] == L'-') {
                    size_op = s_arg[0];
                    s_arg = s_arg.substr(1);
                }
                uint64_t multiplier = 512; // default blocks (512-byte)
                if (!s_arg.empty()) {
                    wchar_t suffix = s_arg.back();
                    if (suffix == L'c') { multiplier = 1; s_arg.pop_back(); }
                    else if (suffix == L'k' || suffix == L'K') { multiplier = 1024; s_arg.pop_back(); }
                    else if (suffix == L'M') { multiplier = 1024 * 1024; s_arg.pop_back(); }
                    else if (suffix == L'G') { multiplier = 1024ULL * 1024 * 1024; s_arg.pop_back(); }
                    else if (suffix == L'b') { multiplier = 512; s_arg.pop_back(); }
                }
                int parsed_num = 0;
                if (!try_parse_int_strict(s_arg, parsed_num) || parsed_num < 0) {
                    std::wcerr << L"ksh: find: invalid -size: " << tokens[idx + 1] << L"\n";
                    has_error = true;
                    break;
                }
                size_bytes = static_cast<uint64_t>(parsed_num) * multiplier;
                has_size = true;
                idx += 2;
            } else if (opt == L"-print") {
                idx++;
            } else {
                std::wcerr << L"ksh: find: unknown option: " << opt << L"\n";
                has_error = true;
                break;
            }
            next_not = false;
        }
        
        if (has_error) {
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        
        std::vector<std::wstring> results;

        auto is_dir_empty = [](const std::wstring& dir_path) -> bool {
            std::wstring search = dir_path + L"\\*";
            WIN32_FIND_DATAW test_fd;
            HANDLE test_find = FindFirstFileW(search.c_str(), &test_fd);
            if (test_find == INVALID_HANDLE_VALUE) return true;
            bool empty = true;
            do {
                if (wcscmp(test_fd.cFileName, L".") != 0 && wcscmp(test_fd.cFileName, L"..") != 0) {
                    empty = false;
                    break;
                }
            } while (FindNextFileW(test_find, &test_fd));
            FindClose(test_find);
            return empty;
        };

        auto matches_entry = [&](const std::wstring& name, bool is_dir, uint64_t file_sz, int depth) -> bool {
            if (mindepth >= 0 && depth < mindepth) {
                return false;
            }
            if (maxdepth >= 0 && depth > maxdepth) {
                return false;
            }
            if (!name_pattern.empty()) {
                if (iname) {
                    if (!match_glob_pattern(to_lower_copy(name_pattern), to_lower_copy(name))) return false;
                } else {
                    if (!match_glob_pattern(name_pattern, name)) return false;
                }
            }
            if (!type_filter.empty()) {
                if (type_filter == L"f" && is_dir) return false;
                if (type_filter == L"d" && !is_dir) return false;
            }
            if (check_empty) {
                if (is_dir) {
                    if (!is_dir_empty(name)) return false;
                } else {
                    if (file_sz != 0) return false;
                }
            }
            if (has_size && !is_dir) {
                if (size_op == L'+' && !(file_sz > size_bytes)) return false;
                if (size_op == L'-' && !(file_sz < size_bytes)) return false;
                if (size_op == L'=' && !(file_sz == size_bytes)) return false;
            }
            return true;
        };
        
        std::function<void(const std::wstring&, int)> traverse;
        traverse = [&](const std::wstring& dir, int depth) {
            if (maxdepth >= 0 && depth >= maxdepth) {
                return;
            }
            std::wstring search_path = dir + L"\\*";
            WIN32_FIND_DATAW fd;
            HANDLE hFind = FindFirstFileW(search_path.c_str(), &fd);
            if (hFind == INVALID_HANDLE_VALUE) {
                return;
            }
            
            do {
                std::wstring name = fd.cFileName;
                if (name == L"." || name == L"..") {
                    continue;
                }
                
                std::wstring full_path = dir;
                if (full_path.back() != L'\\' && full_path.back() != L'/') {
                    full_path += L"/";
                }
                full_path += name;
                
                bool is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                uint64_t file_sz = (static_cast<uint64_t>(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow;
                
                if (matches_entry(name, is_dir, file_sz, depth + 1)) {
                    results.push_back(full_path);
                }
                
                if (is_dir) {
                    traverse(full_path, depth + 1);
                }
            } while (FindNextFileW(hFind, &fd));
            
            FindClose(hFind);
        };
        
        auto evaluate_path = [&](const std::wstring& path) {
            DWORD attrs = GetFileAttributesW(path.c_str());
            if (attrs == INVALID_FILE_ATTRIBUTES) {
                std::wcerr << L"ksh: find: " << path << L": No such file or directory\n";
                return;
            }
            
            bool is_dir = (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
            std::wstring name = path;
            size_t last_slash = path.find_last_of(L"\\/");
            if (last_slash != std::wstring::npos) {
                name = path.substr(last_slash + 1);
            }
            
            uint64_t file_sz = 0;
            if (!is_dir) {
                WIN32_FILE_ATTRIBUTE_DATA fad;
                if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad)) {
                    file_sz = (static_cast<uint64_t>(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
                }
            }

            if (matches_entry(name, is_dir, file_sz, 0)) {
                results.push_back(path);
            }
            
            if (is_dir) {
                traverse(path, 0);
            }
        };
        
        for (const auto& path : paths) {
            evaluate_path(path);
        }
        
        bool print_ok = true;
        for (const auto& r : results) {
            std::wstring unix_path = r;
            for (auto& ch : unix_path) {
                if (ch == L'\\') ch = L'/';
            }
            if (!write_builtin_output(unix_path + L"\n")) {
                print_ok = false;
                break;
            }
        }
        
        ksh_env.variables[L"?"] = print_ok ? L"0" : L"1";
        return print_ok;
    }

    if (cmd == L"wait") {
        if (tokens.size() == 2 && tokens[1] == L"-p") {
            if (!g_coprocess.active) {
                ksh_env.variables[L"?"] = L"127";
                return false;
            }
            if (g_coprocess.input_write != INVALID_HANDLE_VALUE) {
                CloseHandle(g_coprocess.input_write);
                g_coprocess.input_write = INVALID_HANDLE_VALUE;
            }
            WaitForSingleObject(g_coprocess.process, INFINITE);
            DWORD exit_code = 1;
            GetExitCodeProcess(g_coprocess.process, &exit_code);
            close_coprocess(false);
            ksh_env.variables[L"COPROC_ACTIVE"] = L"0";
            ksh_env.variables[L"?"] = std::to_wstring(exit_code);
            return exit_code == 0;
        }

        if (tokens.size() == 1) {
            update_background_jobs(false);
            std::vector<int> ids;
            for (const BackgroundJob& job : g_background_jobs) {
                ids.push_back(job.id);
            }

            DWORD last_status = 0;
            bool waited_any = false;
            for (int id : ids) {
                BackgroundJob* job = find_background_job(id);
                if (job != nullptr) {
                    DWORD code = 0;
                    if (!job->completed) {
                        wait_for_background_job(*job, code);
                    } else {
                        code = job->exit_code;
                    }
                    last_status = code;
                    waited_any = true;
                }
                remove_background_job(id);
            }

            ksh_env.variables[L"?"] = waited_any ? std::to_wstring(last_status) : L"0";
            return true;
        }

        DWORD last_status = 0;
        bool waited_any = false;
        bool ok = true;

        for (size_t i = 1; i < tokens.size(); ++i) {
            int job_id = 0;
            if (!resolve_job_reference(tokens[i], job_id)) {
                std::wcerr << L"ksh: invalid job id\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            BackgroundJob* job = find_background_job(job_id);
            if (job == nullptr) {
                std::wcerr << L"ksh: job not found: " << job_id << L"\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            DWORD code = 0;
            if (!job->completed) {
                if (!wait_for_background_job(*job, code)) {
                    ok = false;
                }
            } else {
                code = job->exit_code;
            }

            remove_background_job(job_id);
            last_status = code;
            waited_any = true;
        }

        ksh_env.variables[L"?"] = waited_any ? std::to_wstring(last_status) : L"0";
        return ok;
    }

    if (cmd == L"complete") {
        std::wstring prefix;
        for (size_t i = 1; i < tokens.size(); ++i) {
            if (!prefix.empty()) {
                prefix += L" ";
            }
            prefix += tokens[i];
        }
        std::vector<std::wstring> matches = collect_completion_candidates(prefix);
        for (const std::wstring& match : matches) {
            if (!write_builtin_output(match + L"\n")) {
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
        }
        ksh_env.variables[L"?"] = matches.empty() ? L"1" : L"0";
        return !matches.empty();
    }

    if (cmd == L"math") {
        if (tokens.size() < 2) {
            std::wcerr << L"ksh: math requires an expression\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        std::wstring expression;
        for (size_t i = 1; i < tokens.size(); ++i) {
            expression += tokens[i];
            if (i + 1 < tokens.size()) {
                expression += L" ";
            }
        }

        std::wstring value = evaluate_arithmetic(expression);
        if (!write_builtin_output(value + L"\n")) {
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    if (cmd == L"trap") {
        auto print_traps = [&](const std::vector<std::wstring>& event_filter) -> bool {
            if (event_filter.empty()) {
                for (std::map<std::wstring, std::wstring>::const_iterator it = g_trap_handlers.begin(); it != g_trap_handlers.end(); ++it) {
                    if (!write_builtin_output(L"trap -- " + quote_for_single_quoted_shell_literal(it->second) + L" " + it->first + L"\n")) {
                        return false;
                    }
                }
                return true;
            }

            for (const std::wstring& raw_event : event_filter) {
                const std::wstring event_name = normalize_trap_event_name(raw_event);
                if (event_name.empty()) {
                    std::wcerr << L"ksh: trap: unsupported event: " << raw_event << L"\n";
                    return false;
                }

                std::map<std::wstring, std::wstring>::const_iterator it = g_trap_handlers.find(event_name);
                if (it != g_trap_handlers.end()) {
                    if (!write_builtin_output(L"trap -- " + quote_for_single_quoted_shell_literal(it->second) + L" " + it->first + L"\n")) {
                        return false;
                    }
                }
            }

            return true;
        };

        if (tokens.size() == 1) {
            if (!print_traps(std::vector<std::wstring>())) {
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        if (tokens[1] == L"-p") {
            std::vector<std::wstring> filter;
            if (tokens.size() > 2) {
                filter.assign(tokens.begin() + 2, tokens.end());
            }
            if (!print_traps(filter)) {
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        bool clear_mode = false;
        std::wstring action = tokens[1];
        size_t event_start = 2;
        if (tokens[1] == L"-") {
            clear_mode = true;
            event_start = 2;
        }

        if (event_start >= tokens.size()) {
            std::wcerr << L"ksh: trap: usage: trap [command|-] EVENT [EVENT ...]\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        bool ok = true;
        for (size_t i = event_start; i < tokens.size(); ++i) {
            const std::wstring event_name = normalize_trap_event_name(tokens[i]);
            if (event_name.empty()) {
                std::wcerr << L"ksh: trap: unsupported event: " << tokens[i] << L"\n";
                ok = false;
                continue;
            }

            if (clear_mode) {
                g_trap_handlers.erase(event_name);
            } else {
                // Enforce per-session trap-handler table cap (new entries only).
                if (g_trap_handlers.find(event_name) == g_trap_handlers.end() &&
                    g_trap_handlers.size() >= kMaxTrapHandlers) {
                    std::wcerr << L"ksh: trap handler table limit reached\n";
                    ok = false;
                    continue;
                }
                g_trap_handlers[event_name] = action;
            }

            if (event_name == L"INT") {
                InterlockedExchange(&g_int_trap_active, clear_mode ? 0 : 1);
            } else if (event_name == L"BREAK") {
                InterlockedExchange(&g_break_trap_active, clear_mode ? 0 : 1);
            } else if (event_name == L"HUP") {
                InterlockedExchange(&g_hup_trap_active, clear_mode ? 0 : 1);
            } else if (event_name == L"ALRM") {
                InterlockedExchange(&g_alrm_trap_active, clear_mode ? 0 : 1);
            } else if (event_name == L"USR1") {
                InterlockedExchange(&g_usr1_trap_active, clear_mode ? 0 : 1);
            } else if (event_name == L"USR2") {
                InterlockedExchange(&g_usr2_trap_active, clear_mode ? 0 : 1);
            } else if (event_name == L"SEGV") {
                InterlockedExchange(&g_segv_trap_active, clear_mode ? 0 : 1);
            } else if (event_name == L"FPE") {
                InterlockedExchange(&g_fpe_trap_active, clear_mode ? 0 : 1);
            } else if (event_name == L"TERM") {
                InterlockedExchange(&g_term_trap_active, clear_mode ? 0 : 1);
            }
        }

        ksh_env.variables[L"?"] = ok ? L"0" : L"1";
        return ok;
    }

    // ksh93 Built-in: typeset (supports -a, -A, -i, -r, -x, -L, -R, -Z)
    if (cmd == L"typeset") {
        struct TypesetOptions {
            bool array_mode = false;
            bool associative_mode = false;
            bool nameref_mode = false;
            bool integer_mode = false;
            bool readonly_mode = false;
            bool export_mode = false;
            bool uppercase_mode = false;
            bool lowercase_mode = false;
            bool left_justify_mode = false;
            bool right_justify_mode = false;
            bool zero_pad_mode = false;
            bool clear_integer_mode = false;
            bool clear_export_mode = false;
            bool clear_uppercase_mode = false;
            bool clear_lowercase_mode = false;
            bool clear_left_justify_mode = false;
            bool clear_right_justify_mode = false;
            bool clear_zero_pad_mode = false;
            bool list_mode = false;
            bool type_mode = false;
            int justify_width = -1;
            bool justify_width_set = false;
        };

        TypesetOptions options;
        std::vector<std::wstring> operands;
        bool end_of_options = false;

        auto parse_option_token = [&](const std::wstring& token, bool enable_mode, size_t& option_index) -> bool {
            for (size_t j = 1; j < token.size(); ++j) {
                if (token[j] == L'a') {
                    if (!enable_mode) {
                        std::wcerr << L"ksh: typeset: option removal (+a) is not supported\n";
                        return false;
                    }
                    options.array_mode = true;
                } else if (token[j] == L'A') {
                    if (!enable_mode) {
                        std::wcerr << L"ksh: typeset: option removal (+A) is not supported\n";
                        return false;
                    }
                    options.associative_mode = true;
                } else if (token[j] == L'n') {
                    if (!enable_mode) {
                        std::wcerr << L"ksh: typeset: option removal (+n) is not supported\n";
                        return false;
                    }
                    options.nameref_mode = true;
                } else if (token[j] == L'i') {
                    if (enable_mode) {
                        options.integer_mode = true;
                        options.clear_integer_mode = false;
                    } else {
                        options.clear_integer_mode = true;
                        options.integer_mode = false;
                    }
                } else if (token[j] == L'r') {
                    if (!enable_mode) {
                        std::wcerr << L"ksh: typeset: option removal (+r) is not supported\n";
                        return false;
                    }
                    options.readonly_mode = true;
                } else if (token[j] == L'x') {
                    if (enable_mode) {
                        options.export_mode = true;
                        options.clear_export_mode = false;
                    } else {
                        options.clear_export_mode = true;
                        options.export_mode = false;
                    }
                } else if (token[j] == L'u') {
                    if (enable_mode) {
                        options.uppercase_mode = true;
                        options.lowercase_mode = false;
                        options.clear_uppercase_mode = false;
                    } else {
                        options.clear_uppercase_mode = true;
                        options.uppercase_mode = false;
                    }
                } else if (token[j] == L'l') {
                    if (enable_mode) {
                        options.lowercase_mode = true;
                        options.uppercase_mode = false;
                        options.clear_lowercase_mode = false;
                    } else {
                        options.clear_lowercase_mode = true;
                        options.lowercase_mode = false;
                    }
                } else if (token[j] == L'L' || token[j] == L'R' || token[j] == L'Z') {
                    if (enable_mode) {
                        if (options.left_justify_mode || options.right_justify_mode || options.zero_pad_mode) {
                            std::wcerr << L"ksh: typeset: justification attributes cannot be combined\n";
                            return false;
                        }

                        std::wstring width_text;
                        if (j + 1 < token.size()) {
                            width_text = token.substr(j + 1);
                            j = token.size();
                        } else if (option_index + 1 < tokens.size() && !tokens[option_index + 1].empty() && tokens[option_index + 1][0] != L'-' && tokens[option_index + 1][0] != L'+') {
                            width_text = tokens[++option_index];
                            j = token.size();
                        } else {
                            std::wcerr << L"ksh: typeset: justification width required for -" << token[j] << L"\n";
                            return false;
                        }

                        int width = 0;
                        if (!try_parse_int_strict(width_text, width) || width < 0) {
                            std::wcerr << L"ksh: typeset: invalid justification width: " << width_text << L"\n";
                            return false;
                        }

                        options.justify_width = width;
                        options.justify_width_set = true;
                        if (token[j] == L'L') {
                            options.left_justify_mode = true;
                            options.right_justify_mode = false;
                            options.zero_pad_mode = false;
                        } else if (token[j] == L'R') {
                            options.right_justify_mode = true;
                            options.left_justify_mode = false;
                            options.zero_pad_mode = false;
                        } else {
                            options.zero_pad_mode = true;
                            options.left_justify_mode = false;
                            options.right_justify_mode = false;
                        }
                    } else {
                        if (token[j] == L'L') {
                            options.clear_left_justify_mode = true;
                        } else if (token[j] == L'R') {
                            options.clear_right_justify_mode = true;
                        } else {
                            options.clear_zero_pad_mode = true;
                        }
                    }
                } else if (token[j] == L'p') {
                    if (!enable_mode) {
                        std::wcerr << L"ksh: typeset: option removal (+p) is not supported\n";
                        return false;
                    }
                    options.list_mode = true;
                } else if (token[j] == L'T') {
                    if (!enable_mode) {
                        std::wcerr << L"ksh: typeset: option removal (+T) is not supported\n";
                        return false;
                    }
                    options.type_mode = true;
                } else {
                    std::wcerr << L"ksh: typeset: unsupported option: -" << token[j] << L"\n";
                    return false;
                }
            }

            return true;
        };

        for (size_t i = 1; i < tokens.size(); ++i) {
            const std::wstring& token = tokens[i];
            if (!end_of_options && token == L"--") {
                end_of_options = true;
                continue;
            }

            if (!end_of_options && !token.empty() && token.size() > 1 && (token[0] == L'-' || token[0] == L'+')) {
                if (!parse_option_token(token, token[0] == L'-', i)) {
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
                continue;
            }

            operands.push_back(token);
        }

        if (options.nameref_mode && (options.array_mode || options.associative_mode || options.integer_mode ||
            options.uppercase_mode || options.lowercase_mode || options.clear_integer_mode ||
            options.clear_uppercase_mode || options.clear_lowercase_mode)) {
            std::wcerr << L"ksh: typeset: nameref cannot be combined with other attributes\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        if ((options.array_mode || options.associative_mode) && options.integer_mode) {
            std::wcerr << L"ksh: typeset: array and -i cannot be combined\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        if ((options.array_mode || options.associative_mode) &&
            (options.left_justify_mode || options.right_justify_mode || options.zero_pad_mode ||
             options.clear_left_justify_mode || options.clear_right_justify_mode || options.clear_zero_pad_mode)) {
            std::wcerr << L"ksh: typeset: justification attributes cannot be applied to arrays\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        if (options.justify_width_set && !(options.left_justify_mode || options.right_justify_mode || options.zero_pad_mode)) {
            std::wcerr << L"ksh: typeset: justification width requires -L, -R, or -Z\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        if (options.array_mode && options.associative_mode) {
            std::wcerr << L"ksh: typeset: -a and -A cannot be combined\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        if ((options.array_mode || options.associative_mode) && (options.uppercase_mode || options.lowercase_mode || options.clear_uppercase_mode || options.clear_lowercase_mode)) {
            std::wcerr << L"ksh: typeset: case attributes cannot be applied to arrays\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        if (options.type_mode) {
            std::wstring full_expr;
            for (size_t k = 0; k < operands.size(); ++k) {
                if (k > 0) full_expr += L" ";
                full_expr += operands[k];
            }
            
            size_t eq_pos = full_expr.find(L'=');
            if (eq_pos == std::wstring::npos) {
                std::wstring type_to_print = trim_copy(full_expr);
                auto t_it = g_custom_types.find(type_to_print);
                if (t_it != g_custom_types.end()) {
                    std::wstring out = L"typeset -T " + type_to_print + L"=(\n";
                    for (const auto& member : t_it->second.members) {
                        out += L"    ";
                        if (!member.type_name.empty()) {
                            out += member.type_name;
                        } else if (member.is_integer) {
                            out += L"integer";
                        } else {
                            out += L"typeset";
                        }
                        out += L" " + member.name;
                        if (!member.default_value.empty()) {
                            out += L"=" + member.default_value;
                        }
                        out += L"\n";
                    }
                    out += L")\n";
                    write_builtin_output(out);
                    ksh_env.variables[L"?"] = L"0";
                    return true;
                } else {
                    std::wcerr << L"ksh: typeset -T: type not found: " << type_to_print << L"\n";
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
            }
            
            std::wstring type_name = trim_copy(full_expr.substr(0, eq_pos));
            std::wstring body_part = trim_copy(full_expr.substr(eq_pos + 1));
            
            if (body_part.front() != L'(' || body_part.back() != L')') {
                std::wcerr << L"ksh: typeset -T: type definition must be enclosed in (...)\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            
            std::wstring body = body_part.substr(1, body_part.size() - 2);
            CustomTypeDefinition def;
            if (parse_custom_type_definition(type_name, body, def)) {
                g_custom_types[type_name] = def;
                ksh_env.variables[L"?"] = L"0";
                return true;
            } else {
                std::wcerr << L"ksh: typeset -T: failed to parse type definition\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
        }

        if (!operands.empty() && g_custom_types.find(operands[0]) != g_custom_types.end()) {
            std::wstring t_name = operands[0];
            for (size_t i = 1; i < operands.size(); ++i) {
                std::wstring token = operands[i];
                std::wstring var_name = token;
                std::wstring assign_part;
                size_t eq = token.find(L'=');
                if (eq != std::wstring::npos) {
                    var_name = token.substr(0, eq);
                    assign_part = token.substr(eq + 1);
                }
                
                instantiate_custom_type(t_name, var_name);
                
                if (!assign_part.empty()) {
                    if (assign_part.front() == L'(' && assign_part.back() == L')') {
                        std::wstring inner = assign_part.substr(1, assign_part.size() - 2);
                        std::vector<std::wstring> sub_tokens = ksh_tokenize_preserve_quotes(inner);
                        for (const auto& sub_t : sub_tokens) {
                            size_t sub_eq = sub_t.find(L'=');
                            if (sub_eq != std::wstring::npos) {
                                std::wstring member = sub_t.substr(0, sub_eq);
                                std::wstring val = sub_t.substr(sub_eq + 1);
                                std::wstring full_name = var_name + L"." + member;
                                ksh_env.variables[full_name] = val;
                                SetEnvironmentVariableW(full_name.c_str(), val.c_str());
                            }
                        }
                    }
                }
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        const bool local_scope = is_executing_function_scope();

        auto matches_list_filters = [&](const std::wstring& name, bool is_array) -> bool {
            if (options.nameref_mode && !get_flag_value(ksh_env.nameref_flags, name)) {
                return false;
            }
            if (options.array_mode && (!is_array || get_flag_value(ksh_env.associative_flags, name))) {
                return false;
            }
            if (options.associative_mode && (!is_array || !get_flag_value(ksh_env.associative_flags, name))) {
                return false;
            }
            if (options.integer_mode && !get_flag_value(ksh_env.integer_flags, name)) {
                return false;
            }
            if (options.readonly_mode && !get_flag_value(ksh_env.readonly_flags, name)) {
                return false;
            }
            if (options.export_mode && !get_flag_value(ksh_env.exported, name)) {
                return false;
            }
            if (options.uppercase_mode && !get_flag_value(ksh_env.uppercase_flags, name)) {
                return false;
            }
            if (options.lowercase_mode && !get_flag_value(ksh_env.lowercase_flags, name)) {
                return false;
            }
            std::map<std::wstring, wchar_t>::const_iterator justify_mode_it = ksh_env.justify_modes.find(name);
            wchar_t justify_mode = (justify_mode_it != ksh_env.justify_modes.end()) ? justify_mode_it->second : 0;
            if (options.left_justify_mode && justify_mode != L'L') {
                return false;
            }
            if (options.right_justify_mode && justify_mode != L'R') {
                return false;
            }
            if (options.zero_pad_mode && justify_mode != L'Z') {
                return false;
            }
            return true;
        };

        auto write_typeset_entry = [&](const std::wstring& name, bool require_existing) -> bool {
            const bool has_scalar = ksh_env.variables.find(name) != ksh_env.variables.end();
            const bool has_array = ksh_env.arrays.find(name) != ksh_env.arrays.end();
            const bool is_nameref = get_flag_value(ksh_env.nameref_flags, name);
            const bool has_any_state = has_scalar || has_array || is_nameref ||
                get_flag_value(ksh_env.exported, name) ||
                get_flag_value(ksh_env.readonly_flags, name) ||
                get_flag_value(ksh_env.integer_flags, name) ||
                get_flag_value(ksh_env.uppercase_flags, name) ||
                get_flag_value(ksh_env.lowercase_flags, name);

            if (!has_any_state) {
                if (require_existing) {
                    std::wcerr << L"ksh: typeset: not found: " << name << L"\n";
                }
                return !require_existing;
            }

            const bool is_array = has_array;
            if (!matches_list_filters(name, is_array)) {
                return true;
            }

            std::wstring line = L"typeset";
            if (is_nameref) {
                line += L" -n";
            }
            if (is_array) {
                if (get_flag_value(ksh_env.associative_flags, name)) {
                    line += L" -A";
                } else {
                    line += L" -a";
                }
            }
            if (get_flag_value(ksh_env.integer_flags, name)) {
                line += L" -i";
            }
            if (get_flag_value(ksh_env.readonly_flags, name)) {
                line += L" -r";
            }
            if (get_flag_value(ksh_env.exported, name)) {
                line += L" -x";
            }
            if (get_flag_value(ksh_env.uppercase_flags, name)) {
                line += L" -u";
            }
            if (get_flag_value(ksh_env.lowercase_flags, name)) {
                line += L" -l";
            }

            std::map<std::wstring, wchar_t>::const_iterator justify_mode_it = ksh_env.justify_modes.find(name);
            if (justify_mode_it != ksh_env.justify_modes.end()) {
                if (justify_mode_it->second == L'L') {
                    line += L" -L";
                } else if (justify_mode_it->second == L'R') {
                    line += L" -R";
                } else if (justify_mode_it->second == L'Z') {
                    line += L" -Z";
                }

                std::map<std::wstring, int>::const_iterator justify_width_it = ksh_env.justify_widths.find(name);
                if (justify_width_it != ksh_env.justify_widths.end() && justify_width_it->second >= 0) {
                    line += L" ";
                    line += std::to_wstring(justify_width_it->second);
                }
            }

            line += L" ";
            line += name;

            if (has_array) {
                line += L"=(";
                const std::map<std::wstring, std::wstring>& m = ksh_env.arrays[name];
                bool is_assoc = get_flag_value(ksh_env.associative_flags, name);
                if (is_assoc) {
                    size_t idx = 0;
                    for (const auto& pair : m) {
                        line += L"[" + pair.first + L"]=" + quote_command_argument(pair.second);
                        if (idx + 1 < m.size()) {
                            line += L" ";
                        }
                        idx++;
                    }
                } else {
                    std::vector<std::wstring> values = get_array_values_vector(name);
                    for (size_t index = 0; index < values.size(); ++index) {
                        line += quote_command_argument(values[index]);
                        if (index + 1 < values.size()) {
                            line += L" ";
                        }
                    }
                }
                line += L")";
            } else if (has_scalar) {
                line += L"=";
                line += quote_command_argument(ksh_env.variables[name]);
            }

            return write_builtin_output(line + L"\n");
        };

        const bool has_mutating_flags = options.array_mode || options.associative_mode || options.integer_mode || options.readonly_mode ||
            options.export_mode || options.uppercase_mode || options.lowercase_mode ||
            options.left_justify_mode || options.right_justify_mode || options.zero_pad_mode ||
            options.clear_integer_mode || options.clear_export_mode || options.clear_uppercase_mode ||
            options.clear_lowercase_mode || options.clear_left_justify_mode || options.clear_right_justify_mode ||
            options.clear_zero_pad_mode;

        if (options.list_mode || operands.empty()) {
            if (!operands.empty()) {
                bool ok = true;
                for (const std::wstring& name : operands) {
                    if (!is_valid_shell_identifier(name)) {
                        std::wcerr << L"ksh: typeset: invalid name: " << name << L"\n";
                        ok = false;
                        continue;
                    }
                    if (!write_typeset_entry(name, true)) {
                        ok = false;
                    }
                }
                ksh_env.variables[L"?"] = ok ? L"0" : L"1";
                return ok;
            }

            if (!options.list_mode && !has_mutating_flags) {
                // Fall through to print all variables.
            } else if (!options.list_mode && has_mutating_flags) {
                // ksh93-style option-only invocations print matching declarations.
            }

            bool ok = true;
            for (std::map<std::wstring, std::wstring>::const_iterator it = ksh_env.variables.begin(); it != ksh_env.variables.end(); ++it) {
                if (is_internal_shell_state_variable(it->first)) {
                    continue;
                }
                if (!write_typeset_entry(it->first, false)) {
                    ok = false;
                    break;
                }
            }
            if (ok) {
                ksh_env.variables[L"?"] = L"0";
                return true;
            }
        }

        for (const std::wstring& token : operands) {
            size_t assignment_eq = token.find(L'=');
            if (assignment_eq != std::wstring::npos) {
                std::wstring lhs = token.substr(0, assignment_eq);
                std::wstring rhs = token.substr(assignment_eq + 1);

                std::wstring base_name;
                std::wstring index_text;
                bool has_index = false;
                if (!parse_array_reference_expression(lhs, base_name, index_text, has_index)) {
                    std::wcerr << L"ksh: typeset: invalid name: " << lhs << L"\n";
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }

                if (options.readonly_mode && (options.clear_integer_mode || options.clear_export_mode || options.clear_uppercase_mode || options.clear_lowercase_mode)) {
                    // no-op, combined set/clear remains meaningful for non-readonly flags
                }

                if ((options.array_mode || options.associative_mode || options.nameref_mode) && has_index) {
                    std::wcerr << L"ksh: typeset: indexed assignment is not allowed with -a/-A/-n: " << lhs << L"\n";
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }

                if (local_scope) {
                    snapshot_local_variable_if_needed(base_name);
                }

                if (options.clear_integer_mode) {
                    set_flag_value(ksh_env.integer_flags, base_name, false);
                }
                if (options.clear_export_mode) {
                    set_flag_value(ksh_env.exported, base_name, false);
                    SetEnvironmentVariableW(base_name.c_str(), nullptr);
                }
                if (options.clear_uppercase_mode) {
                    set_flag_value(ksh_env.uppercase_flags, base_name, false);
                }
                if (options.clear_lowercase_mode) {
                    set_flag_value(ksh_env.lowercase_flags, base_name, false);
                }

                if (options.clear_left_justify_mode || options.clear_right_justify_mode || options.clear_zero_pad_mode) {
                    ksh_env.justify_modes.erase(base_name);
                    ksh_env.justify_widths.erase(base_name);
                }

                if (options.array_mode || options.associative_mode) {
                    set_flag_value(ksh_env.integer_flags, base_name, false);
                }
                if (options.integer_mode) {
                    set_flag_value(ksh_env.integer_flags, base_name, true);
                }
                if (options.uppercase_mode) {
                    set_flag_value(ksh_env.uppercase_flags, base_name, true);
                    set_flag_value(ksh_env.lowercase_flags, base_name, false);
                }
                if (options.lowercase_mode) {
                    set_flag_value(ksh_env.lowercase_flags, base_name, true);
                    set_flag_value(ksh_env.uppercase_flags, base_name, false);
                }

                if (options.left_justify_mode || options.right_justify_mode || options.zero_pad_mode) {
                    wchar_t justify_mode = options.left_justify_mode ? L'L' : (options.right_justify_mode ? L'R' : L'Z');
                    ksh_env.justify_modes[base_name] = justify_mode;
                    ksh_env.justify_widths[base_name] = options.justify_width;
                }

                if (options.associative_mode) {
                    set_flag_value(ksh_env.associative_flags, base_name, true);
                } else if (options.array_mode) {
                    set_flag_value(ksh_env.associative_flags, base_name, false);
                }

                std::wstring assignment_error;
                if (!assign_parameter_value(lhs, rhs, options.array_mode || options.associative_mode, local_scope, assignment_error, options.nameref_mode)) {
                    std::wcerr << L"ksh: typeset: " << assignment_error << L"\n";
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }

                if (options.export_mode) {
                    set_flag_value(ksh_env.exported, base_name, true);
                    sync_exported_environment_variable(base_name);
                }

                if (options.readonly_mode) {
                    set_flag_value(ksh_env.readonly_flags, base_name, true);
                }

                continue;
            }

            if (!is_valid_shell_identifier(token)) {
                std::wcerr << L"ksh: typeset: invalid name: " << token << L"\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            if (options.clear_integer_mode) {
                set_flag_value(ksh_env.integer_flags, token, false);
            }
            if (options.clear_export_mode) {
                set_flag_value(ksh_env.exported, token, false);
                SetEnvironmentVariableW(token.c_str(), nullptr);
            }
            if (options.clear_uppercase_mode) {
                set_flag_value(ksh_env.uppercase_flags, token, false);
            }
            if (options.clear_lowercase_mode) {
                set_flag_value(ksh_env.lowercase_flags, token, false);
            }

            if (options.clear_left_justify_mode || options.clear_right_justify_mode || options.clear_zero_pad_mode) {
                ksh_env.justify_modes.erase(token);
                ksh_env.justify_widths.erase(token);
            }

            if (options.nameref_mode) {
                if (local_scope) {
                    snapshot_local_variable_if_needed(token);
                }
                if (get_flag_value(ksh_env.readonly_flags, token)) {
                    std::wcerr << L"ksh: typeset: variable is read-only: " << token << L"\n";
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
                set_flag_value(ksh_env.nameref_flags, token, true);
                ksh_env.variables.erase(token);
                ksh_env.arrays.erase(token);
                ksh_env.associative_flags.erase(token);
                set_flag_value(ksh_env.integer_flags, token, false);
                set_flag_value(ksh_env.uppercase_flags, token, false);
                set_flag_value(ksh_env.lowercase_flags, token, false);
            } else if (options.array_mode || options.associative_mode) {
                if (local_scope) {
                    snapshot_local_variable_if_needed(token);
                }
                if (get_flag_value(ksh_env.readonly_flags, token)) {
                    std::wcerr << L"ksh: typeset: variable is read-only: " << token << L"\n";
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
                ksh_env.arrays.try_emplace(token);
                if (options.associative_mode) {
                    set_flag_value(ksh_env.associative_flags, token, true);
                } else {
                    set_flag_value(ksh_env.associative_flags, token, false);
                }
                ksh_env.variables.erase(token);
                SetEnvironmentVariableW(token.c_str(), nullptr);
                set_flag_value(ksh_env.integer_flags, token, false);
                set_flag_value(ksh_env.uppercase_flags, token, false);
                set_flag_value(ksh_env.lowercase_flags, token, false);
            } else if (ksh_env.variables.find(token) == ksh_env.variables.end() && ksh_env.arrays.find(token) == ksh_env.arrays.end()) {
                if (local_scope) {
                    snapshot_local_variable_if_needed(token);
                }
                if (options.integer_mode) {
                    assign_scalar_parameter(token, L"0");
                } else {
                    assign_scalar_parameter(token, L"");
                }
            } else if (local_scope) {
                snapshot_local_variable_if_needed(token);
            }

            if (options.integer_mode) {
                if (ksh_env.arrays.find(token) != ksh_env.arrays.end()) {
                    std::wcerr << L"ksh: typeset: cannot apply -i to array: " << token << L"\n";
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }

                std::map<std::wstring, std::wstring>::const_iterator scalar_it = ksh_env.variables.find(token);
                if (scalar_it != ksh_env.variables.end() && !scalar_it->second.empty()) {
                    assign_scalar_parameter(token, evaluate_arithmetic(scalar_it->second));
                }
                set_flag_value(ksh_env.integer_flags, token, true);
            }

            if (options.left_justify_mode || options.right_justify_mode || options.zero_pad_mode) {
                if (ksh_env.arrays.find(token) != ksh_env.arrays.end()) {
                    std::wcerr << L"ksh: typeset: cannot apply justification attributes to array: " << token << L"\n";
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
                wchar_t justify_mode = options.left_justify_mode ? L'L' : (options.right_justify_mode ? L'R' : L'Z');
                ksh_env.justify_modes[token] = justify_mode;
                ksh_env.justify_widths[token] = options.justify_width;
                std::map<std::wstring, std::wstring>::const_iterator scalar_it = ksh_env.variables.find(token);
                if (scalar_it != ksh_env.variables.end()) {
                    assign_scalar_parameter(token, scalar_it->second);
                }
            }

            if (options.uppercase_mode) {
                if (ksh_env.arrays.find(token) != ksh_env.arrays.end()) {
                    std::wcerr << L"ksh: typeset: cannot apply -u to array: " << token << L"\n";
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
                set_flag_value(ksh_env.uppercase_flags, token, true);
                set_flag_value(ksh_env.lowercase_flags, token, false);
                std::map<std::wstring, std::wstring>::const_iterator scalar_it = ksh_env.variables.find(token);
                if (scalar_it != ksh_env.variables.end()) {
                    assign_scalar_parameter(token, scalar_it->second);
                }
            }

            if (options.lowercase_mode) {
                if (ksh_env.arrays.find(token) != ksh_env.arrays.end()) {
                    std::wcerr << L"ksh: typeset: cannot apply -l to array: " << token << L"\n";
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
                set_flag_value(ksh_env.lowercase_flags, token, true);
                set_flag_value(ksh_env.uppercase_flags, token, false);
                std::map<std::wstring, std::wstring>::const_iterator scalar_it = ksh_env.variables.find(token);
                if (scalar_it != ksh_env.variables.end()) {
                    assign_scalar_parameter(token, scalar_it->second);
                }
            }

            if (options.export_mode) {
                set_flag_value(ksh_env.exported, token, true);
                sync_exported_environment_variable(token);
            }

            if (options.readonly_mode) {
                set_flag_value(ksh_env.readonly_flags, token, true);
            }
        }

        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    // ksh93 Built-in: [[ conditional test ]]
    if (cmd == L"[[") {
        std::wstring cond_block;
        std::wstring parse_error;
        if (!extract_ksh_conditional_block(expanded, cond_block, parse_error)) {
            std::wcerr << L"ksh: [[ ... ]]: " << parse_error << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        bool test_result = false;
        std::wstring eval_error;
        if (!evaluate_ksh_conditional(cond_block, test_result, eval_error)) {
            std::wcerr << L"ksh: [[ ... ]]: " << eval_error << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        ksh_env.variables[L"?"] = test_result ? L"0" : L"1";
        return true;
    }

    if (cmd == L"enum") {
        if (tokens.size() < 2) {
            std::wcerr << L"ksh: enum: usage: enum Name=(value ...)\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        std::wstring declaration;
        for (size_t index = 1; index < tokens.size(); ++index) {
            if (index > 1) declaration += L" ";
            declaration += tokens[index];
        }
        const size_t equals = declaration.find(L'=');
        if (equals == std::wstring::npos) {
            std::wcerr << L"ksh: enum: expected Name=(value ...)\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        const std::wstring type_name = trim_copy(declaration.substr(0, equals));
        std::wstring values = trim_copy(declaration.substr(equals + 1));
        if (type_name.empty() || values.size() < 2 || values.front() != L'(' || values.back() != L')') {
            std::wcerr << L"ksh: enum: expected Name=(value ...)\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        CustomTypeDefinition definition;
        definition.name = type_name;
        const std::vector<std::wstring> names = ksh_tokenize_preserve_quotes(values.substr(1, values.size() - 2));
        if (names.empty()) {
            std::wcerr << L"ksh: enum: enumeration must contain at least one value\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        const auto is_identifier = [](const std::wstring& name) {
            if (name.empty() || !(std::iswalpha(name.front()) || name.front() == L'_')) return false;
            return std::all_of(name.begin() + 1, name.end(), [](wchar_t character) {
                return std::iswalnum(character) || character == L'_';
            });
        };
        for (size_t index = 0; index < names.size(); ++index) {
            if (!is_identifier(names[index])) {
                std::wcerr << L"ksh: enum: invalid value: " << names[index] << L"\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            CustomTypeMember member;
            member.name = names[index];
            member.is_integer = true;
            member.default_value = std::to_wstring(index);
            definition.members.push_back(std::move(member));
        }
        g_custom_types[type_name] = std::move(definition);
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    if (cmd == L"coproc") {
        return execute_builtin_coproc(tokens);
    }

    if (cmd == L"sleep") {
        if (tokens.size() != 2) {
            std::wcerr << L"ksh: sleep: usage: sleep seconds\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        try {
            size_t consumed = 0;
            const double seconds = std::stod(tokens[1], &consumed);
            if (consumed != tokens[1].size() || !std::isfinite(seconds) || seconds < 0.0 || seconds > 4294967.295) {
                throw std::invalid_argument("seconds");
            }
            const ULONGLONG duration_ms = static_cast<ULONGLONG>(std::llround(seconds * 1000.0));
            const ULONGLONG deadline = GetTickCount64() + duration_ms;
            while (GetTickCount64() < deadline) {
                const bool interrupted =
                    InterlockedCompareExchange(&g_pending_int_trap, 0, 0) != 0 ||
                    InterlockedCompareExchange(&g_pending_break_trap, 0, 0) != 0 ||
                    InterlockedCompareExchange(&g_pending_hup_trap, 0, 0) != 0 ||
                    InterlockedCompareExchange(&g_pending_term_trap, 0, 0) != 0;
                if (interrupted) {
                    bool interrupted_should_exit = false;
                    process_pending_traps(interrupted_should_exit);
                    return false;
                }
                const ULONGLONG remaining_ms = deadline - GetTickCount64();
                Sleep(static_cast<DWORD>(remaining_ms < 20 ? remaining_ms : 20));
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        } catch (...) {
            std::wcerr << L"ksh: sleep: invalid time interval: " << tokens[1] << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
    }

    // ksh93 Built-in: test / [ ]
    if (cmd == L"test" || cmd == L"[") {
        std::vector<std::wstring> condition_tokens;
        if (cmd == L"[") {
            if (tokens.size() < 2 || tokens.back() != L"]") {
                std::wcerr << L"ksh: [: missing closing ]\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            condition_tokens.assign(tokens.begin() + 1, tokens.end() - 1);
        } else {
            condition_tokens.assign(tokens.begin() + 1, tokens.end());
        }

        if (condition_tokens.empty()) {
            ksh_env.variables[L"?"] = L"1";
            return true;
        }

        bool test_result = false;
        std::wstring eval_error;
        if (!evaluate_test_condition_expression(condition_tokens, test_result, eval_error)) {
            std::wcerr << L"ksh: " << (cmd == L"[" ? L"[: " : L"test: ") << eval_error << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        ksh_env.variables[L"?"] = test_result ? L"0" : L"1";
        return true;
    }

    std::map<std::wstring, ShellFunctionDefinition>::const_iterator function_it = g_shell_functions.find(cmd);
    if (!bypass_function_lookup && function_it != g_shell_functions.end()) {
        std::vector<std::wstring> function_args;
        for (size_t i = 1; i < tokens.size(); ++i) {
            function_args.push_back(tokens[i]);
        }
        return execute_defined_function(cmd, function_args, should_exit_shell);
    }

    // Variable assignment standard ksh: var=value
    size_t eq = cmd.find(L'=');
    if (eq != std::wstring::npos && eq > 0) {
        std::wstring k = cmd.substr(0, eq);
        std::wstring v = cmd.substr(eq + 1);
        std::wstring assignment_error;
        if (!assign_parameter_value(k, v, false, false, assignment_error)) {
            std::wcerr << L"ksh: " << assignment_error << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    // Builtin group: process control and shell state.
    // Built-in: exec
    if (cmd == L"exec") {
        if (tokens.size() == 1) {
            if (!redir.custom_fd_actions.empty()) {
                std::wstring custom_fd_error;
                if (!apply_persistent_custom_fd_actions(redir, custom_fd_error)) {
                    std::wcerr << custom_fd_error << L"\n";
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
            }

            HANDLE std_in = nullptr;
            HANDLE std_out = nullptr;
            HANDLE std_err = nullptr;
            bool close_in = false;
            bool close_out = false;
            bool close_err = false;
            
            if (setup_redirection_handles(&redir, std_in, std_out, std_err, close_in, close_out, close_err)) {
                if (redir.has_stdin) {
                    SetStdHandle(STD_INPUT_HANDLE, std_in);
                }
                if (redir.has_stdout) {
                    SetStdHandle(STD_OUTPUT_HANDLE, std_out);
                }
                if (redir.has_stderr) {
                    SetStdHandle(STD_ERROR_HANDLE, std_err);
                }
                ksh_env.variables[L"?"] = L"0";
                return true;
            } else {
                std::wcerr << L"ksh: exec: redirection failed\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
        } else {
            if (!redir.custom_fd_actions.empty()) {
                std::wcerr << L"ksh: exec: custom fd redirection (3-9) with command replacement is not supported\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            std::vector<std::wstring> exec_tokens(tokens.begin() + 1, tokens.end());
            std::wstring cmd_line = join_tokens_as_command_line(exec_tokens);
            
            STARTUPINFOW si;
            PROCESS_INFORMATION pi;
            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            ZeroMemory(&pi, sizeof(pi));
            
            HANDLE std_in = nullptr;
            HANDLE std_out = nullptr;
            HANDLE std_err = nullptr;
            bool close_in = false;
            bool close_out = false;
            bool close_err = false;
            if (!setup_redirection_handles(&redir, std_in, std_out, std_err, close_in, close_out, close_err)) {
                std::wcerr << L"ksh: exec: redirection failed\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdInput = std_in;
            si.hStdOutput = std_out;
            si.hStdError = std_err;
            std::vector<HANDLE> inherited_handles = { std_in, std_out, std_err };

            if (create_process_with_handle_list(cmd_line, si, inherited_handles, 0, pi)) {
                WaitForSingleObject(pi.hProcess, INFINITE);
                DWORD exit_code = 0;
                GetExitCodeProcess(pi.hProcess, &exit_code);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                close_redirection_handles(std_in, std_out, std_err, close_in, close_out, close_err);
                exit(static_cast<int>(exit_code));
            } else {
                close_redirection_handles(std_in, std_out, std_err, close_in, close_out, close_err);
                std::wcerr << L"ksh: exec: failed to execute " << exec_tokens[0] << L"\n";
                ksh_env.variables[L"?"] = L"127";
                return false;
            }
        }
    }

    // Builtin group: permissions and terminal state.
    // Built-in: umask
    if (cmd == L"umask") {
        bool symbolic = false;
        std::wstring mask_arg;
        for (size_t i = 1; i < tokens.size(); ++i) {
            if (tokens[i] == L"-S") {
                symbolic = true;
            } else {
                mask_arg = tokens[i];
            }
        }
        
        if (mask_arg.empty()) {
            if (symbolic) {
                int u = (g_umask >> 6) & 7;
                int g = (g_umask >> 3) & 7;
                int o = g_umask & 7;
                auto to_sym = [](int bits) -> std::wstring {
                    std::wstring res;
                    if (!(bits & 4)) res += L"r";
                    if (!(bits & 2)) res += L"w";
                    if (!(bits & 1)) res += L"x";
                    return res;
                };
                std::wstring symbolic_out = L"u=" + to_sym(u) + L",g=" + to_sym(g) + L",o=" + to_sym(o) + L"\n";
                if (!write_builtin_output(symbolic_out)) {
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
            } else {
                wchar_t buf[32];
                swprintf(buf, 32, L"%04o\n", g_umask);
                if (!write_builtin_output(buf)) {
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        } else {
            wchar_t* end_ptr = nullptr;
            errno = 0;
            long new_mask = std::wcstol(mask_arg.c_str(), &end_ptr, 8);
            if (errno != 0 || end_ptr == mask_arg.c_str() || (end_ptr != nullptr && *end_ptr != L'\0') || new_mask < 0 || new_mask > 0777) {
                std::wcerr << L"ksh: umask: invalid mask: " << mask_arg << L"\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            g_umask = static_cast<int>(new_mask);
            ksh_env.variables[L"?"] = L"0";
            return true;
        }
    }

    // Builtin group: terminal and timing utilities.
    // Built-in: stty
    if (cmd == L"stty") {
        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
        DWORD mode = 0;
        bool has_mode = GetConsoleMode(hIn, &mode) != FALSE;
        
        if (tokens.size() == 1) {
            if (has_mode) {
                std::wstring settings;
                settings += (mode & ENABLE_ECHO_INPUT) ? L"echo " : L"-echo ";
                settings += (mode & ENABLE_LINE_INPUT) ? L"icanon " : L"-icanon ";
                settings += (mode & ENABLE_PROCESSED_INPUT) ? L"isig " : L"-isig ";
                
                CONSOLE_SCREEN_BUFFER_INFO csbi;
                if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
                    int rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
                    int cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
                    settings += L"speed 38400 baud; rows " + std::to_wstring(rows) + L"; columns " + std::to_wstring(cols) + L";";
                }
                if (!write_builtin_output(settings + L"\n")) {
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
                ksh_env.variables[L"?"] = L"0";
                return true;
            }
            std::wcerr << L"ksh: stty: standard input is not a console\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        
        bool ok = true;
        for (size_t i = 1; i < tokens.size(); ++i) {
            std::wstring arg = tokens[i];
            if (arg == L"size") {
                CONSOLE_SCREEN_BUFFER_INFO csbi;
                if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
                    int rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
                    int cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
                    std::wstring sz = std::to_wstring(rows) + L" " + std::to_wstring(cols) + L"\n";
                    if (!write_builtin_output(sz)) {
                        ok = false;
                    }
                } else {
                    std::wcerr << L"ksh: stty: failed to get console size\n";
                    ok = false;
                }
            } else if (has_mode) {
                if (arg == L"echo") {
                    SetConsoleMode(hIn, mode | ENABLE_ECHO_INPUT);
                    mode |= ENABLE_ECHO_INPUT;
                } else if (arg == L"-echo") {
                    SetConsoleMode(hIn, mode & ~ENABLE_ECHO_INPUT);
                    mode &= ~ENABLE_ECHO_INPUT;
                } else if (arg == L"icanon") {
                    SetConsoleMode(hIn, mode | ENABLE_LINE_INPUT);
                    mode |= ENABLE_LINE_INPUT;
                } else if (arg == L"-icanon") {
                    SetConsoleMode(hIn, mode & ~ENABLE_LINE_INPUT);
                    mode &= ~ENABLE_LINE_INPUT;
                } else if (arg == L"isig") {
                    SetConsoleMode(hIn, mode | ENABLE_PROCESSED_INPUT);
                    mode |= ENABLE_PROCESSED_INPUT;
                } else if (arg == L"-isig") {
                    SetConsoleMode(hIn, mode & ~ENABLE_PROCESSED_INPUT);
                    mode &= ~ENABLE_PROCESSED_INPUT;
                } else if (arg == L"raw") {
                    SetConsoleMode(hIn, mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT));
                    mode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
                } else if (arg == L"cooked" || arg == L"-raw") {
                    SetConsoleMode(hIn, mode | (ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT));
                    mode |= (ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
                } else {
                    std::wcerr << L"ksh: stty: unknown mode: " << arg << L"\n";
                    ok = false;
                }
            } else {
                std::wcerr << L"ksh: stty: standard input is not a console\n";
                ok = false;
            }
        }
        ksh_env.variables[L"?"] = ok ? L"0" : L"1";
        return ok;
    }

    // Built-in: times
    if (cmd == L"times") {
        FILETIME creationTime, exitTime, kernelTime, userTime;
        if (GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime)) {
            ULARGE_INTEGER uUser, uKernel;
            uUser.LowPart = userTime.dwLowDateTime;
            uUser.HighPart = userTime.dwHighDateTime;
            uKernel.LowPart = kernelTime.dwLowDateTime;
            uKernel.HighPart = kernelTime.dwHighDateTime;
            
            double sh_user = uUser.QuadPart / 10000000.0;
            double sh_kernel = uKernel.QuadPart / 10000000.0;
            double ch_user = g_child_user_time / 10000000.0;
            double ch_kernel = g_child_kernel_time / 10000000.0;
            
            wchar_t buf1[256], buf2[256];
            swprintf(buf1, 256, L"%dm%.2fs %dm%.2fs\n", static_cast<int>(sh_user / 60), sh_user - (static_cast<int>(sh_user / 60)) * 60, static_cast<int>(sh_kernel / 60), sh_kernel - (static_cast<int>(sh_kernel / 60)) * 60);
            swprintf(buf2, 256, L"%dm%.2fs %dm%.2fs\n", static_cast<int>(ch_user / 60), ch_user - (static_cast<int>(ch_user / 60)) * 60, static_cast<int>(ch_kernel / 60), ch_kernel - (static_cast<int>(ch_kernel / 60)) * 60);
            std::wstring out = std::wstring(buf1) + std::wstring(buf2);
            if (!write_builtin_output(out)) {
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        }
        std::wcerr << L"ksh: times failed\n";
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    // Builtin group: history and editing helpers.
    // Built-in: fc
    if (cmd == L"fc") {
        if (g_command_history.empty()) {
            load_command_history_from_file();
        }
        bool list_mode = false;
        bool suppress_numbers = false;
        bool reverse_order = false;
        std::wstring editor;
        std::wstring env_ed = get_environment_value(L"FCEDIT");
        if (env_ed.empty()) env_ed = get_environment_value(L"VISUAL");
        if (env_ed.empty()) env_ed = get_environment_value(L"EDITOR");
        if (!env_ed.empty()) editor = env_ed;
        else editor = L"notepad";

        std::wstring substitute_old;
        std::wstring substitute_new;
        bool has_substitution = false;
        std::vector<std::wstring> positional_args;
        
        for (size_t i = 1; i < tokens.size(); ++i) {
            std::wstring arg = tokens[i];
            if (arg == L"-s") {
                editor = L"-";
            } else if (!arg.empty() && arg[0] == L'-' && arg.size() > 1 && arg[1] != L'-') {
                for (size_t k = 1; k < arg.size(); ++k) {
                    wchar_t c = arg[k];
                    if (c == L'l') list_mode = true;
                    else if (c == L'n') suppress_numbers = true;
                    else if (c == L'r') reverse_order = true;
                    else if (c == L's') {
                        editor = L"-";
                    } else if (c == L'e' && i + 1 < tokens.size()) {
                        editor = tokens[++i];
                        break;
                    }
                }
            } else {
                size_t equal_pos = arg.find(L'=');
                if (editor == L"-" && equal_pos != std::wstring::npos && !has_substitution) {
                    substitute_old = arg.substr(0, equal_pos);
                    substitute_new = arg.substr(equal_pos + 1);
                    has_substitution = true;
                } else {
                    positional_args.push_back(arg);
                }
            }
        }
        
        int history_count = static_cast<int>(g_command_history.size());
        int first_idx = -1;
        int last_idx = -1;
        
        if (list_mode) {
            first_idx = (history_count > 16) ? history_count - 16 : 0;
            last_idx = (history_count > 0) ? history_count - 1 : 0;
        } else {
            first_idx = (history_count > 0) ? history_count - 1 : 0;
            last_idx = first_idx;
        }
        
        auto parse_positional = [&](const std::wstring& arg, int& idx) -> bool {
            int num = 0;
            if (!try_parse_int_strict(arg, num)) {
                for (int k = history_count - 1; k >= 0; --k) {
                    if (g_command_history[k].command.rfind(arg, 0) == 0) {
                        idx = k;
                        return true;
                    }
                }
                return false;
            }

            if (num < 0) {
                idx = history_count + num;
            } else {
                idx = num - 1;
            }
            if (idx < 0) idx = 0;
            if (idx >= history_count) idx = history_count - 1;
            return true;
        };
        
        if (positional_args.size() >= 1) {
            if (!parse_positional(positional_args[0], first_idx)) {
                std::wcerr << L"ksh: fc: history specification not found: " << positional_args[0] << L"\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            if (positional_args.size() >= 2) {
                if (!parse_positional(positional_args[1], last_idx)) {
                    std::wcerr << L"ksh: fc: history specification not found: " << positional_args[1] << L"\n";
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
            } else {
                last_idx = list_mode ? (history_count - 1) : first_idx;
            }
        }
        
        if (list_mode) {
            int start = first_idx;
            int end = last_idx;
            int step = (start <= end) ? 1 : -1;
            if (reverse_order) {
                std::swap(start, end);
                step = -step;
            }
            
            bool print_ok = true;
            for (int k = start; ; k += step) {
                if (k < 0 || k >= history_count) break;
                
                std::wstring line;
                if (!suppress_numbers) {
                    line += std::to_wstring(k + 1) + L"\t";
                }
                line += g_command_history[k].command + L"\n";
                if (!write_builtin_output(line)) {
                    print_ok = false;
                    break;
                }
                if (k == end) break;
            }
            ksh_env.variables[L"?"] = print_ok ? L"0" : L"1";
            return print_ok;
        } else {
            bool re_execute = (editor == L"-");
            wchar_t temp_path[MAX_PATH];
            wchar_t temp_file[MAX_PATH];
            if (GetTempPathW(MAX_PATH, temp_path) == 0 || GetTempFileNameW(temp_path, L"ksh_fc", 0, temp_file) == 0) {
                std::wcerr << L"ksh: fc: failed to create temporary file\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            
            {
                std::wofstream out(temp_file);
                if (!out) {
                    std::wcerr << L"ksh: fc: failed to open temporary file\n";
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
                int start = first_idx;
                int end = last_idx;
                int step = (start <= end) ? 1 : -1;
                if (reverse_order) {
                    std::swap(start, end);
                    step = -step;
                }
                for (int k = start; ; k += step) {
                    if (k >= 0 && k < history_count) {
                        std::wstring cmd_text = g_command_history[k].command;
                        if (has_substitution && !substitute_old.empty()) {
                            size_t sub_pos = cmd_text.find(substitute_old);
                            if (sub_pos != std::wstring::npos) {
                                cmd_text.replace(sub_pos, substitute_old.size(), substitute_new);
                            }
                        }
                        out << cmd_text << L"\n";
                    }
                    if (k == end) break;
                }
            }
            
            if (re_execute) {
                std::wifstream in(temp_file);
                std::wstring line;
                bool ok = true;
                while (std::getline(in, line)) {
                    std::wstring trimmed = trim_copy(line);
                    if (!trimmed.empty()) {
                        write_builtin_output(trimmed + L"\n");
                        bool should_exit = false;
                        ok = execute_command_line_impl(trimmed, should_exit);
                        if (should_exit) exit(0);
                    }
                }
                DeleteFileW(temp_file);
                ksh_env.variables[L"?"] = ok ? L"0" : L"1";
                return ok;
            } else {
                std::wstring cmd_line = quote_command_argument(editor) + L" " + quote_command_argument(temp_file);
                STARTUPINFOW si;
                PROCESS_INFORMATION pi;
                ZeroMemory(&si, sizeof(si));
                si.cb = sizeof(si);
                ZeroMemory(&pi, sizeof(pi));
                
                si.dwFlags = STARTF_USESTDHANDLES;
                si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
                si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
                si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
                std::vector<HANDLE> inherited_handles = { si.hStdInput, si.hStdOutput, si.hStdError };

                if (create_process_with_handle_list(cmd_line, si, inherited_handles, 0, pi)) {
                    WaitForSingleObject(pi.hProcess, INFINITE);
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                    
                    std::wifstream in(temp_file);
                    std::wstring line;
                    bool ok = true;
                    while (std::getline(in, line)) {
                        std::wstring trimmed = trim_copy(line);
                        if (!trimmed.empty()) {
                            write_builtin_output(trimmed + L"\n");
                            bool should_exit = false;
                            ok = execute_command_line_impl(trimmed, should_exit);
                            if (should_exit) exit(0);
                        }
                    }
                    DeleteFileW(temp_file);
                    ksh_env.variables[L"?"] = ok ? L"0" : L"1";
                    return ok;
                } else {
                    std::wcerr << L"ksh: fc: failed to launch editor: " << editor << L"\n";
                    DeleteFileW(temp_file);
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
            }
        }
    }

    // Builtin group: system metadata and path helpers.
    // Built-in: getconf
    if (cmd == L"getconf") {
        return execute_builtin_getconf(tokens, write_builtin_output);
    }

    // Built-in: pathchk
    if (cmd == L"pathchk") {
        return execute_builtin_pathchk(tokens);
    }

    // Builtin group: positional and parameter utilities.
    // Built-in: :
    if (cmd == L":") {
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    // Built-in: shift
    if (cmd == L"shift") {
        size_t n = 1;
        if (tokens.size() > 1) {
            int parsed = 0;
            if (!try_parse_positive_int_strict(tokens[1], parsed)) {
                std::wcerr << L"ksh: shift: invalid shift count: " << tokens[1] << L"\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            n = static_cast<size_t>(parsed);
        }
        
        if (g_script_context_stack.empty()) {
            ksh_env.variables[L"?"] = L"0";
            return true;
        }
        
        std::vector<std::wstring>& args = g_script_context_stack.back().args;
        if (n > args.size()) {
            std::wcerr << L"ksh: shift: shift count out of range\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        
        size_t old_size = args.size();
        args.erase(args.begin(), args.begin() + n);
        
        // Sync $#, $*, $@, and individual $1..$N
        ksh_env.variables[L"#"] = std::to_wstring(args.size());
        std::wstring star;
        for (size_t k = 0; k < args.size(); ++k) {
            if (k > 0) star += L" ";
            star += args[k];
        }
        ksh_env.variables[L"*"] = star;
        ksh_env.variables[L"@"] = star;
        for (size_t k = 0; k < args.size(); ++k) {
            ksh_env.variables[std::to_wstring(k + 1)] = args[k];
        }
        for (size_t k = args.size(); k < old_size; ++k) {
            ksh_env.variables.erase(std::to_wstring(k + 1));
        }

        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    // Builtin group: directory and prompt-oriented utilities.
    // Built-in: cd
    if (cmd == L"cd") {
        wchar_t prev_cwd[MAX_PATH];
        std::wstring old_cwd_str;
        if (GetCurrentDirectoryW(MAX_PATH, prev_cwd) > 0) {
            old_cwd_str = prev_cwd;
        }

        std::wstring target;
        bool print_after = false;

        if (tokens.size() <= 1) {
            std::wstring home = get_environment_value(L"HOME");
            if (home.empty()) {
                home = get_environment_value(L"USERPROFILE");
            }
            if (home.empty()) {
                home = L"C:\\";
            }
            target = home;
        } else if (tokens[1] == L"-") {
            std::wstring oldpwd = get_environment_value(L"OLDPWD");
            if (oldpwd.empty()) {
                std::wcerr << L"ksh: cd: OLDPWD not set\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            target = oldpwd;
            print_after = true;
        } else {
            target = tokens[1];
        }

        std::wstring p = normalize_cd_path(target);
        if (SetCurrentDirectoryW(p.c_str())) {
            wchar_t new_cwd[MAX_PATH];
            if (GetCurrentDirectoryW(MAX_PATH, new_cwd) > 0) {
                if (!old_cwd_str.empty()) {
                    ksh_env.variables[L"OLDPWD"] = old_cwd_str;
                    SetEnvironmentVariableW(L"OLDPWD", old_cwd_str.c_str());
                }
                std::wstring new_cwd_str = new_cwd;
                ksh_env.variables[L"PWD"] = new_cwd_str;
                SetEnvironmentVariableW(L"PWD", new_cwd_str.c_str());
                if (print_after) {
                    write_builtin_output(new_cwd_str + L"\n");
                }
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        }
        std::wcerr << L"ksh: cd failed: " << p << L"\n";
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    // Built-in: pwd
    if (cmd == L"pwd") {
        wchar_t currentDir[MAX_PATH];
        if (GetCurrentDirectoryW(MAX_PATH, currentDir) > 0) {
            std::wstring dir_str = currentDir;
            dir_str += L"\n";
            if (!write_builtin_output(dir_str)) {
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        }
        std::wcerr << L"ksh: pwd failed\n";
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    // Builtin group: formatted output.
    // Built-in: printf
    if (cmd == L"printf") {
        std::wstring output_str;
        std::wstring error_msg;
        if (!evaluate_builtin_printf(tokens, output_str, error_msg)) {
            std::wcerr << error_msg << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        if (!write_builtin_output(output_str)) {
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    // Built-in: print / echo
    if (cmd == L"print" || cmd == L"echo") {
        bool omit_newline = false;
        bool raw_mode = false;
        bool use_coprocess_output = false;
        size_t start_idx = 1;

        if (cmd == L"print") {
            while (start_idx < tokens.size()) {
                const std::wstring& arg = tokens[start_idx];
                if (arg.size() > 1 && arg[0] == L'-') {
                    bool recognized = false;
                    bool opt_n = false;
                    bool opt_r = false;
                    for (size_t j = 1; j < arg.size(); ++j) {
                        if (arg[j] == L'n') {
                            opt_n = true;
                            recognized = true;
                        } else if (arg[j] == L'r') {
                            opt_r = true;
                            recognized = true;
                        } else if (arg[j] == L'p' && g_coprocess.active) {
                            use_coprocess_output = true;
                            recognized = true;
                        } else {
                            recognized = false;
                            break;
                        }
                    }
                    if (recognized) {
                        if (opt_n) omit_newline = true;
                        if (opt_r) raw_mode = true;
                        start_idx++;
                    } else {
                        break;
                    }
                } else {
                    break;
                }
            }

            if (start_idx < tokens.size() && tokens[start_idx].size() > 1 &&
                tokens[start_idx][0] == L'-' && tokens[start_idx].find(L'p') != std::wstring::npos &&
                !g_coprocess.active) {
                std::wcerr << L"ksh: print -p: no active co-process\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
        } else if (cmd == L"echo") {
            if (tokens.size() > 1 && tokens[1] == L"-n") {
                omit_newline = true;
                start_idx = 2;
            }
        }

        std::wstring line;
        for (size_t i = start_idx; i < tokens.size(); ++i) {
            line += tokens[i];
            if (i + 1 < tokens.size()) {
                line += L" ";
            }
        }

        if (cmd == L"print" && !raw_mode) {
            std::wstring expanded_line;
            for (size_t i = 0; i < line.size(); ++i) {
                if (line[i] == L'\\' && i + 1 < line.size()) {
                    wchar_t next = line[i + 1];
                    if (next == L'n') { expanded_line += L'\n'; i++; }
                    else if (next == L't') { expanded_line += L'\t'; i++; }
                    else if (next == L'v') { expanded_line += L'\v'; i++; }
                    else if (next == L'b') { expanded_line += L'\b'; i++; }
                    else if (next == L'r') { expanded_line += L'\r'; i++; }
                    else if (next == L'f') { expanded_line += L'\f'; i++; }
                    else if (next == L'a') { expanded_line += L'\a'; i++; }
                    else if (next == L'\\') { expanded_line += L'\\'; i++; }
                    else { expanded_line += line[i]; }
                } else {
                    expanded_line += line[i];
                }
            }
            line = expanded_line;
        }

        if (!omit_newline) {
            line += L"\n";
        }

        if (use_coprocess_output) {
            if (line.empty()) {
                ksh_env.variables[L"?"] = L"0";
                return true;
            }

            const int utf8_length = WideCharToMultiByte(
                CP_UTF8,
                0,
                line.c_str(),
                static_cast<int>(line.size()),
                nullptr,
                0,
                nullptr,
                nullptr);
            if (utf8_length <= 0) {
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            std::string utf8(static_cast<size_t>(utf8_length), '\0');
            WideCharToMultiByte(
                CP_UTF8,
                0,
                line.c_str(),
                static_cast<int>(line.size()),
                &utf8[0],
                utf8_length,
                nullptr,
                nullptr);
            DWORD written = 0;
            if (!WriteFile(g_coprocess.input_write, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr) ||
                written != utf8.size()) {
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
        } else if (!write_builtin_output(line)) {
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    // Execute external processes or system fallback
    if (!run_in_background) {
        const ULONGLONG launch_start = perf_trace ? GetTickCount64() : 0;
        DWORD exit_code = 1;
        bool launched = execute_native_or_fallback(external_command_to_run, &redir, &exit_code);
        ksh_env.variables[L"?"] = std::to_wstring(exit_code);
        if (perf_trace) {
            log_performance_trace(L"execute.command_launch", GetTickCount64() - launch_start, L"background=0 exit=" + std::to_wstring(exit_code));
            log_performance_trace(L"execute.total", GetTickCount64() - command_start);
        }
        return launched;
    }

    const ULONGLONG launch_start = perf_trace ? GetTickCount64() : 0;
    HANDLE process_handle = nullptr;
    DWORD pid = 0;
    if (!launch_process(external_command_to_run, process_handle, pid, &redir)) {
        if (perf_trace) {
            log_performance_trace(L"execute.command_launch", GetTickCount64() - launch_start, L"background=1 failed");
        }
        std::wcerr << L"ksh: failed to start background command\n";
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    BackgroundJob job;
    job.id = g_next_job_id++;
    job.pid = pid;
    job.process_handle = process_handle;
    job.pids.push_back(pid);
    job.process_handles.push_back(process_handle);
    job.command = command_to_run;
    job.completed = false;
    job.exit_code = STILL_ACTIVE;
    job.completion_reported = false;
    g_background_jobs.push_back(job);

    std::wcout << L"[" << job.id << L"] " << job.pid << L"\n";
    ksh_env.variables[L"?"] = L"0";
    if (perf_trace) {
        log_performance_trace(L"execute.command_launch", GetTickCount64() - launch_start, L"background=1 pid=" + std::to_wstring(pid));
        log_performance_trace(L"execute.total", GetTickCount64() - command_start);
    }
    return true;
}

// SECTION 13: Built-in Self-Tests and Verification
int run_internal_self_tests() {
    int passed = 0;
    int failed = 0;

    auto assert_true = [&](bool condition, const std::wstring& test_name) {
        if (condition) {
            std::wcout << L"PASS: " << test_name << L"\n";
            passed++;
        } else {
            std::wcerr << L"FAIL: " << test_name << L"\n";
            failed++;
        }
    };

    auto assert_eq = [&](const std::wstring& actual, const std::wstring& expected, const std::wstring& test_name) {
        if (actual == expected) {
            std::wcout << L"PASS: " << test_name << L"\n";
            passed++;
        } else {
            std::wcerr << L"FAIL: " << test_name << L" (Expected: " << expected << L", Actual: " << actual << L")\n";
            failed++;
        }
    };

    std::wcout << L"--- Running Internal Self-Tests ---\n";

    // 1. Quoting Cleanup & Escapes Removal
    assert_eq(remove_quotes_and_escapes_from_token(L"\"hello\""), L"hello", L"remove_quotes \"hello\"");
    assert_eq(remove_quotes_and_escapes_from_token(L"'world'"), L"world", L"remove_quotes 'world'");
    assert_eq(remove_quotes_and_escapes_from_token(L"\"hello \\\"nested\\\"\""), L"hello \"nested\"", L"remove_quotes escaped double-quotes");

    // 2. Array Reference Parsing with Bracket Nesting
    {
        std::wstring base_name, index_text;
        bool has_index = false;
        
        assert_true(parse_array_reference_expression(L"arr[1]", base_name, index_text, has_index), L"parse_array_reference simple bracket");
        assert_eq(base_name, L"arr", L"base_name arr[1]");
        assert_eq(index_text, L"1", L"index_text arr[1]");
        
        assert_true(parse_array_reference_expression(L"arr[sub[1]]", base_name, index_text, has_index), L"parse_array_reference nested bracket");
        assert_eq(base_name, L"arr", L"base_name arr[sub[1]]");
        assert_eq(index_text, L"sub[1]", L"index_text arr[sub[1]]");

        assert_true(!parse_array_reference_expression(L"arr[1] + arr[2]", base_name, index_text, has_index), L"parse_array_reference multiple brackets (binary expr)");
    }

    // 3. Arithmetic Parser & Evaluation
    assert_eq(evaluate_arithmetic(L"1 + 1"), L"2", L"evaluate_arithmetic simple addition");
    assert_eq(evaluate_arithmetic(L"2 * 3.5"), L"7", L"evaluate_arithmetic whole number float conversion");
    assert_eq(evaluate_arithmetic(L"5 / 2"), L"2", L"evaluate_arithmetic integer division truncation");
    assert_eq(evaluate_arithmetic(L"5.0 / 2"), L"2.5", L"evaluate_arithmetic float division returning decimal");
    assert_eq(evaluate_arithmetic(L"2 + 3 * 4"), L"14", L"arithmetic multiplication precedence");
    assert_eq(evaluate_arithmetic(L"2 ** 3"), L"8", L"arithmetic exponentiation operator");
    assert_eq(evaluate_arithmetic(L"5 ^ 3"), L"6", L"arithmetic bitwise xor operator");
    assert_eq(evaluate_arithmetic(L"1 << 3"), L"8", L"arithmetic shift operator");
    assert_eq(evaluate_arithmetic(L"1 < 2 && 3 > 2"), L"1", L"arithmetic logical comparison");
    assert_eq(evaluate_arithmetic(L"1 || 0"), L"1", L"arithmetic logical or parses rhs");
    assert_eq(evaluate_arithmetic(L"0 && 1"), L"0", L"arithmetic logical and parses rhs");
    assert_eq(evaluate_arithmetic(L"0 ? 7 : 9"), L"9", L"arithmetic ternary operator");

    // Test integer casting for whole number indices in arithmetic evaluations
    {
        double idx_val_whole = 123.0;
        std::wstring idx_str_whole;
        if (idx_val_whole == std::floor(idx_val_whole)) {
            idx_str_whole = std::to_wstring(static_cast<long long>(idx_val_whole));
        }
        assert_eq(idx_str_whole, L"123", L"whole number index cast directly to long long");
    }

    // 4. Set positional parameter assignment and shift tests
    {
        ScriptContext ctx;
        ctx.script_name = L"ksh";
        ctx.args = { L"foo", L"bar", L"baz" };
        g_script_context_stack.push_back(ctx);
        assert_eq(std::to_wstring(current_script_args().size()), L"3", L"initial positional args count");
        assert_eq(current_script_args()[0], L"foo", L"initial positional arg 1");
        assert_eq(current_script_args()[1], L"bar", L"initial positional arg 2");
        assert_eq(current_script_args()[2], L"baz", L"initial positional arg 3");
        g_script_context_stack.pop_back();
    }

    // 5. Command hash table tests
    {
        g_command_hash_table.clear();
        assert_true(g_command_hash_table.empty(), L"hash table starts empty after clear");
        g_command_hash_table[L"testcmd"] = { L"C:\\test\\testcmd.exe", 5 };
        assert_eq(g_command_hash_table[L"testcmd"].path, L"C:\\test\\testcmd.exe", L"hash table stores path");
        assert_eq(std::to_wstring(g_command_hash_table[L"testcmd"].hits), L"5", L"hash table stores hits");
        g_command_hash_table.clear();
    }

    // 6. Signal & Trap Atomic Dispatch Verification
    {
        InterlockedExchange(&g_int_trap_active, 1);
        InterlockedExchange(&g_pending_int_trap, 0);
        BOOL handled = ksh_console_ctrl_handler(CTRL_C_EVENT);
        assert_true(handled == TRUE, L"ksh_console_ctrl_handler handles CTRL_C when active");
        assert_true(InterlockedCompareExchange(&g_pending_int_trap, 0, 0) == 1, L"g_pending_int_trap set by ctrl handler");
        InterlockedExchange(&g_pending_int_trap, 0);
        InterlockedExchange(&g_int_trap_active, 0);
    }

    // 7. Arithmetic Parser Boundary & Shift Guard Verification
    {
        assert_eq(evaluate_arithmetic(L"1 << 64"), L"0", L"evaluate_arithmetic shift overflow to zero");
        assert_eq(evaluate_arithmetic(L"1 << -1"), L"0", L"evaluate_arithmetic negative shift left to zero");
        assert_eq(evaluate_arithmetic(L"1 >> -1"), L"0", L"evaluate_arithmetic negative shift right to zero");
        assert_eq(evaluate_arithmetic(L"16 >> 2"), L"4", L"evaluate_arithmetic normal shift right");
    }

    // 8. DoS and Nesting Depth Limits Verification
    {
        // Test arithmetic nesting depth exceeding kMaxArithmeticParseDepth (200)
        std::wstring deep_arith;
        for (int d = 0; d < 250; ++d) deep_arith += L"(";
        deep_arith += L"1";
        for (int d = 0; d < 250; ++d) deep_arith += L")";
        assert_eq(evaluate_arithmetic(deep_arith), L"0", L"evaluate_arithmetic safely handles excessive nesting");

        // Test command substitution depth guard
        std::wstring deep_subst = L"echo hi";
        for (int s = 0; s < 70; ++s) {
            deep_subst = L"$(echo " + deep_subst + L")";
        }
        g_expansion_error = false;
        std::wstring sub_res = evaluate_command_substitutions(deep_subst);
        assert_true(g_expansion_error, L"evaluate_command_substitutions flags error on excessive nesting");
        g_expansion_error = false;
    }

    // 9. Fast Function Header Lexer Verification
    {
        std::wstring fn_name;
        bool has_brace = false;
        assert_true(parse_function_header(L"function my_func() {", fn_name, has_brace) && fn_name == L"my_func" && has_brace, L"parse_function_header keyword form with parens and brace");
        assert_true(parse_function_header(L"function my_func {", fn_name, has_brace) && fn_name == L"my_func" && has_brace, L"parse_function_header keyword form with brace");
        assert_true(parse_function_header(L"my_func() {", fn_name, has_brace) && fn_name == L"my_func" && has_brace, L"parse_function_header standard ksh form with brace");
        assert_true(!parse_function_header(L"not_a_func arg1 arg2", fn_name, has_brace), L"parse_function_header rejects non-function line");
    }

    // 10. Process Substitution Pipe Generator Verification
    {
        std::wstring pipe1 = make_process_substitution_pipe_name(1);
        std::wstring pipe2 = make_process_substitution_pipe_name(2);
        assert_true(pipe1.rfind(L"\\\\.\\pipe\\ksh_ps_", 0) == 0, L"make_process_substitution_pipe_name prefix matches");
        assert_true(pipe1 != pipe2, L"make_process_substitution_pipe_name generates distinct pipe paths");
    }

    // 11. Subshell State Isolation Verification (Functions, Aliases, Variables)
    {
        bool should_exit = false;
        // Test variable mutation in subshell
        ksh_env.variables[L"TEST_SUB_VAR"] = L"parent_val";
        execute_command_line(L"(TEST_SUB_VAR=sub_val; export TEST_SUB_VAR)", should_exit);
        assert_eq(ksh_env.variables[L"TEST_SUB_VAR"], L"parent_val", L"subshell variable isolation");

        // Test alias definition in subshell
        execute_command_line(L"(alias isolated_sub_alias=echo)", should_exit);
        assert_true(g_aliases.find(L"isolated_sub_alias") == g_aliases.end(), L"subshell alias definition isolation");

        // Test function definition isolation via ShellStateSnapshot
        {
            ShellStateSnapshot snap = capture_shell_state_snapshot();
            g_shell_functions[L"test_inner_fn"] = ShellFunctionDefinition{ { L"echo test" } };
            std::optional<ShellStateSnapshot> snap_opt = snap;
            restore_shell_state_snapshot_if_present(snap_opt, L"0");
            assert_true(g_shell_functions.find(L"test_inner_fn") == g_shell_functions.end(), L"subshell function snapshot restore isolation");
        }
    }

    // 12. Custom FD Redirection Token Parsing Verification
    {
        RedirectionSpec redir;
        std::wstring err;
        bool handled = false;
        std::vector<std::wstring> tokens = { L"printf", L"%d", L"42" };
        size_t idx = 2;
        // Non-redirection numeric token '42' must not be captured as a custom FD redirection
        assert_true(try_parse_custom_fd_redirection_token(L"42", tokens, idx, redir, err, handled) && !handled, L"numeric token 42 is not custom fd redir");
        assert_true(redir.custom_fd_actions.empty(), L"no custom fd actions for numeric token 42");

        // Valid custom fd redirection token '3>file'
        tokens = { L"echo", L"hi", L"3>output.txt" };
        idx = 2;
        assert_true(try_parse_custom_fd_redirection_token(L"3>output.txt", tokens, idx, redir, err, handled) && handled, L"token 3>output.txt parsed as custom fd redir");
        assert_eq(std::to_wstring(redir.custom_fd_actions.size()), L"1", L"custom fd action recorded");
    }

    // 13. Tokenizer Tab Separator Verification
    {
        std::vector<std::wstring> tokens1 = ksh_tokenize(L"cmd\targ1 \t arg2");
        assert_eq(std::to_wstring(tokens1.size()), L"3", L"ksh_tokenize splits on tabs");
        if (tokens1.size() == 3) {
            assert_eq(tokens1[0], L"cmd", L"tab token 0");
            assert_eq(tokens1[1], L"arg1", L"tab token 1");
            assert_eq(tokens1[2], L"arg2", L"tab token 2");
        }

        std::vector<std::wstring> tokens2 = ksh_tokenize_preserve_quotes(L"echo\t\"hello world\"\t'foo\tbar'");
        assert_eq(std::to_wstring(tokens2.size()), L"3", L"ksh_tokenize_preserve_quotes splits on tabs outside quotes");
        if (tokens2.size() == 3) {
            assert_eq(tokens2[1], L"\"hello world\"", L"preserve quotes with tab separation");
            assert_eq(tokens2[2], L"'foo\tbar'", L"tab preserved inside quotes");
        }
    }

    // 14. Process Substitution Quote Context Verification
    {
        std::wstring line1 = L"echo \"<(foo)\"";
        parse_and_replace_process_substitutions(line1);
        assert_eq(line1, L"echo \"<(foo)\"", L"process substitution ignored inside double quotes");

        std::wstring line2 = L"echo '<(foo)'";
        parse_and_replace_process_substitutions(line2);
        assert_eq(line2, L"echo '<(foo)'", L"process substitution ignored inside single quotes");
    }

    // 15. Extended Globbing in Parameter Expansion Verification
    {
        ksh_env.variables[L"TEST_EXTGLOB"] = L"aaabbbccc";
        std::wstring exp1 = expand_substitutions_left_to_right(L"${TEST_EXTGLOB#+(a)}");
        assert_eq(exp1, L"aabbbccc", L"parameter expansion with +(a) shortest match");

        std::wstring exp2 = expand_substitutions_left_to_right(L"${TEST_EXTGLOB##+(a)}");
        assert_eq(exp2, L"bbbccc", L"parameter expansion with +(a) longest match");

        std::wstring exp3 = expand_substitutions_left_to_right(L"${TEST_EXTGLOB%+(c)}");
        assert_eq(exp3, L"aaabbbcc", L"parameter expansion with +(c) shortest suffix match");

        std::wstring exp4 = expand_substitutions_left_to_right(L"${TEST_EXTGLOB%%+(c)}");
        assert_eq(exp4, L"aaabbb", L"parameter expansion with +(c) longest suffix match");
    }

    // 16. PowerShell Bypass Shell Option & cd - OLDPWD Tracking Verification
    {
        assert_true(!is_powershell_bypass_enabled(), L"powershell bypass disabled by default");
        g_powershell_bypass_enabled = true;
        assert_true(is_powershell_bypass_enabled(), L"powershell bypass enabled via shell option");
        g_powershell_bypass_enabled = false;

        // cd - and PWD tracking
        wchar_t curr[MAX_PATH];
        if (GetCurrentDirectoryW(MAX_PATH, curr) > 0) {
            std::wstring orig_cwd = curr;
            bool should_exit = false;
            std::wstring capture_buf;
            g_builtin_capture_output = &capture_buf;
            execute_command_line(L"cd ..", should_exit);
            assert_eq(ksh_env.variables[L"OLDPWD"], orig_cwd, L"cd sets OLDPWD");
            execute_command_line(L"cd -", should_exit);
            g_builtin_capture_output = nullptr;
            assert_eq(ksh_env.variables[L"PWD"], orig_cwd, L"cd - restores PWD");
        }
    }

    // 17. Performance Benchmarks
    std::wcout << L"\n--- Running Performance Benchmarks ---\n";
    {
        // Benchmark 1: Arithmetic parser throughput
        const int kArithIterations = 50000;
        auto t0 = std::chrono::high_resolution_clock::now();
        double sum = 0;
        for (int i = 0; i < kArithIterations; ++i) {
            std::wstring res = evaluate_arithmetic(L"2 * (3 + 4) - 10 / 2");
            sum += (res == L"9") ? 1.0 : 0.0;
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double ops_per_sec = (kArithIterations / (ms / 1000.0));
        std::wcout << L"PERF: Arithmetic Parsing: " << kArithIterations << L" iterations in "
                   << ms << L" ms (" << static_cast<long long>(ops_per_sec) << L" ops/sec)\n";
        assert_true(sum == static_cast<double>(kArithIterations), L"arithmetic benchmark consistency");
        assert_true(ms < 3000.0, L"arithmetic parsing benchmark threshold (<3000ms for 50k)");
    }

    {
        // Benchmark 2: Tokenizer throughput
        const int kTokenIterations = 20000;
        auto t0 = std::chrono::high_resolution_clock::now();
        size_t token_count = 0;
        for (int i = 0; i < kTokenIterations; ++i) {
            std::vector<std::wstring> tokens = ksh_tokenize_preserve_quotes(L"echo \"arg with spaces\" 'single quoted' $VAR normal_arg");
            token_count += tokens.size();
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double ops_per_sec = (kTokenIterations / (ms / 1000.0));
        std::wcout << L"PERF: Tokenizer: " << kTokenIterations << L" iterations in "
                   << ms << L" ms (" << static_cast<long long>(ops_per_sec) << L" ops/sec)\n";
        assert_true(token_count == kTokenIterations * 5, L"tokenizer benchmark consistency");
        assert_true(ms < 3000.0, L"tokenizer benchmark threshold (<3000ms for 20k)");
    }

    {
        // Benchmark 3: Variable lookup & assignment throughput
        const int kVarIterations = 20000;
        auto t0 = std::chrono::high_resolution_clock::now();
        std::wstring err;
        for (int i = 0; i < kVarIterations; ++i) {
            assign_parameter_value(L"BENCH_VAR", L"bench_val_123", false, false, err, false);
            bool is_set = false;
            std::wstring val = get_variable_value_with_hooks(L"BENCH_VAR", is_set);
            (void)val;
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double ops_per_sec = (kVarIterations / (ms / 1000.0));
        std::wcout << L"PERF: Variable Assign & Lookup: " << kVarIterations << L" iterations in "
                   << ms << L" ms (" << static_cast<long long>(ops_per_sec) << L" ops/sec)\n";
        assert_true(ms < 3000.0, L"variable benchmark threshold (<3000ms for 20k)");
    }

    {
        // Benchmark 4: Function header scanner throughput
        const int kHeaderIterations = 100000;
        auto t0 = std::chrono::high_resolution_clock::now();
        size_t match_count = 0;
        std::wstring fn_name;
        bool has_brace = false;
        for (int i = 0; i < kHeaderIterations; ++i) {
            if (parse_function_header(L"function compute_metrics() {", fn_name, has_brace)) {
                match_count++;
            }
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double ops_per_sec = (kHeaderIterations / (ms / 1000.0));
        std::wcout << L"PERF: Fast Function Header Lexer: " << kHeaderIterations << L" iterations in "
                   << ms << L" ms (" << static_cast<long long>(ops_per_sec) << L" ops/sec)\n";
        assert_true(match_count == kHeaderIterations, L"function header benchmark consistency");
        assert_true(ms < 1000.0, L"function header lexer benchmark threshold (<1000ms for 100k)");
    }

    {
        // Benchmark 5: In-Process Scoped Subshell throughput
        const int kSubshellIterations = 1000;
        bool should_exit = false;
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < kSubshellIterations; ++i) {
            execute_command_line(L"(x=1; y=$((x + 1)))", should_exit);
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        double ops_per_sec = (kSubshellIterations / (ms / 1000.0));
        std::wcout << L"PERF: In-Process Scoped Subshell: " << kSubshellIterations << L" iterations in "
                   << ms << L" ms (" << static_cast<long long>(ops_per_sec) << L" ops/sec)\n";
        assert_true(ms < 5000.0, L"subshell benchmark threshold (<5000ms for 1k)");
    }

    std::wcout << L"\n--- Self-Test Summary: " << passed << L" passed, " << failed << L" failed ---\n";
    return (failed == 0) ? 0 : 1;
}

int wmain(int argc, wchar_t* argv[]) {
    enable_ansi_support();
    ensure_registry_namespaces_registered();
    initialize_default_shell_aliases();
    if (argc > 1 && std::wstring(argv[1]) == L"--self-test") {
        return run_internal_self_tests();
    }
    if (_isatty(_fileno(stdout))) {
        _setmode(_fileno(stdout), _O_U16TEXT);
    } else {
        _setmode(_fileno(stdout), _O_BINARY);
    }
    if (_isatty(_fileno(stdin))) {
        _setmode(_fileno(stdin), _O_U16TEXT);
    } else {
        _setmode(_fileno(stdin), _O_BINARY);
    }
    if (_isatty(_fileno(stderr))) {
        _setmode(_fileno(stderr), _O_U16TEXT);
    } else {
        _setmode(_fileno(stderr), _O_BINARY);
    }
    enable_red_error_output();

    bool should_exit_shell = false;
    StartupOptions startup_options;
    int first_positional_index = argc;
    bool show_help = false;
    bool show_version = false;

    SetSearchPathMode(BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE | BASE_SEARCH_PATH_PERMANENT);
    SetConsoleCtrlHandler(ksh_console_ctrl_handler, TRUE);

    auto finalize_shell_exit = [&](int code) {
        InterlockedExchange(&g_in_interactive_loop, 0);
        InterlockedExchange(&g_foreground_process_active, 0);
        run_exit_trap_once(should_exit_shell);
        close_coprocess(true);
        cleanup_all_background_jobs();
        for (size_t fd = 0; fd < g_custom_fd_table.size(); ++fd) {
            if (is_valid_handle_value(g_custom_fd_table[fd])) {
                CloseHandle(g_custom_fd_table[fd]);
                g_custom_fd_table[fd] = INVALID_HANDLE_VALUE;
            }
        }
        SetConsoleCtrlHandler(ksh_console_ctrl_handler, FALSE);
        return code;
    };

    if (!parse_startup_arguments(argc, argv, startup_options, first_positional_index, show_help, show_version)) {
        return finalize_shell_exit(1);
    }

    if (show_help) {
        print_help_text();
        return finalize_shell_exit(0);
    }

    if (show_version && first_positional_index == argc) {
        std::wcout << L"\n"; 
        std::wcout << L"  CrossShellKSH 3.1.16-2026 Copyright (C) 2026, Roberto J Dohnert\n";
        std::wcout << L"         All Rights Reserved. License: BSD-3-Clause        \n";
        std::wcout << L"\n";
        std::wcout << L"Microsoft Windows is a registered trademark of Microsoft Corporation.\n";
        std::wcout << L"KornShell is a registered trademark of AT&T Research released under the\n";
        std::wcout << L"Eclipse Public License.\n";
        std::wcout << L"\n";
        std::wcout << L"This program is licensed under the BSD-3 Clause License.\n";
        std::wcout << L"\n";
        std::wcout << L"OS Release: Microsoft " << get_windows_release_text() << L"\n";
        std::wcout << L"\n";
        return finalize_shell_exit(0);
    }

    if (startup_options.script_test_set) {
        std::vector<std::wstring> script_args;
        for (int index = first_positional_index; index < argc; ++index) {
            script_args.push_back(argv[index]);
        }
        const bool ok = execute_script_file(startup_options.script_test_path, script_args, should_exit_shell);
        int exit_code = ok ? 0 : 1;
        std::map<std::wstring, std::wstring>::const_iterator status_it = ksh_env.variables.find(L"?");
        if (status_it != ksh_env.variables.end() && !try_parse_int_strict(status_it->second, exit_code)) {
            exit_code = ok ? 0 : 1;
        }
        if (ok && exit_code == 0) {
            std::wcout << L"ksh: script test PASS: " << startup_options.script_test_path << L"\n";
        } else {
            if (exit_code == 0) exit_code = 1;
            std::wcerr << L"ksh: script test FAIL (exit " << exit_code << L"): " << startup_options.script_test_path << L"\n";
        }
        return finalize_shell_exit(exit_code);
    }

    wchar_t initial_cwd[MAX_PATH];
    if (GetCurrentDirectoryW(MAX_PATH, initial_cwd) > 0) {
        ksh_env.variables[L"PWD"] = initial_cwd;
        SetEnvironmentVariableW(L"PWD", initial_cwd);
    }
    if (ksh_env.variables.find(L"HOME") == ksh_env.variables.end()) {
        std::wstring userprofile = get_system_env_var(L"USERPROFILE");
        if (!userprofile.empty()) {
            ksh_env.variables[L"HOME"] = userprofile;
            SetEnvironmentVariableW(L"HOME", userprofile.c_str());
        }
    }

    load_startup_profile(startup_options, should_exit_shell);
    if (should_exit_shell) {
        std::map<std::wstring, std::wstring>::const_iterator status_it = ksh_env.variables.find(L"?");
        if (status_it != ksh_env.variables.end()) {
            int parsed_status = 0;
            if (try_parse_int_strict(status_it->second, parsed_status)) {
                return finalize_shell_exit(parsed_status);
            }
            return finalize_shell_exit(1);
        }
        return finalize_shell_exit(0);
    }

    if (startup_options.command_string_set) {
        ScriptContext context;
        context.script_name = L"ksh";
        if (first_positional_index < argc) {
            context.script_name = argv[first_positional_index];
            for (int i = first_positional_index + 1; i < argc; ++i) {
                context.args.push_back(argv[i]);
            }
        }
        g_script_context_stack.push_back(context);

        bool ok = execute_command_line(startup_options.command_string, should_exit_shell);

        g_script_context_stack.pop_back();

        int exit_code = 0;
        std::map<std::wstring, std::wstring>::const_iterator status_it = ksh_env.variables.find(L"?");
        if (status_it != ksh_env.variables.end()) {
            if (!try_parse_int_strict(status_it->second, exit_code)) {
                exit_code = ok ? 0 : 1;
            }
        } else {
            exit_code = ok ? 0 : 1;
        }
        return finalize_shell_exit(exit_code);
    }

    if (first_positional_index < argc) {
        std::vector<std::wstring> script_args;
        for (int i = first_positional_index + 1; i < argc; ++i) {
            script_args.push_back(argv[i]);
        }
        bool ok = execute_script_file(argv[first_positional_index], script_args, should_exit_shell);
        if (!ok) {
            return finalize_shell_exit(1);
        }

        std::map<std::wstring, std::wstring>::const_iterator status_it = ksh_env.variables.find(L"?");
        if (status_it != ksh_env.variables.end()) {
            int parsed_status = 0;
            if (try_parse_int_strict(status_it->second, parsed_status)) {
                return finalize_shell_exit(parsed_status);
            }
            return finalize_shell_exit(1);
        }

        return finalize_shell_exit(0);
    }

    // Match real ksh startup behavior: begin interactive mode without a banner.

    g_is_interactive_session = true;
    load_command_history_from_file();

    std::wstring input_line;

    InterlockedExchange(&g_in_interactive_loop, 1);
    while (true) {
        update_background_jobs(true);
        process_pending_traps(should_exit_shell);
        if (should_exit_shell) {
            break;
        }

        std::wstring prompt = get_interactive_prompt();

        if (!read_interactive_line(prompt, input_line)) {
            break;
        }

        std::wstring trimmed = trim_copy(input_line);
        if (trimmed.empty()) {
            continue;
        }

        execute_command_line(trimmed, should_exit_shell);
        if (should_exit_shell) {
            break;
        }
    }
    InterlockedExchange(&g_in_interactive_loop, 0);

    return finalize_shell_exit(0);
}

bool file_exists_regular(const std::wstring& path) {
    DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool ensure_default_home_kshrc_exists(const std::wstring& profile_path) {
    if (profile_path.empty()) {
        return false;
    }

    DWORD attributes = GetFileAttributesW(profile_path.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES) {
        return (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    std::ofstream profile_file(profile_path.c_str(), std::ios::binary);
    if (!profile_file.is_open()) {
        std::wcerr << L"ksh: warning: could not create startup profile: " << profile_path << L"\n";
        return false;
    }

    const char* default_profile =
        "# ksh startup profile (.kshrc)\n"
        "# This file is loaded automatically when ksh starts.\n"
        "# Add aliases, variables, and shell functions here.\n"
        "\n"
        "# Prompt customization tokens for PS1:\n"
        "#   %u user, %d domain, %w cwd, %m host, %# role-char, %% literal-percent\n"
        "# Example:\n"
        "# PS1='%u@%d %w %# '\n"
        "# alias ll='dir'\n";
    profile_file.write(default_profile, static_cast<std::streamsize>(std::strlen(default_profile)));
    if (!profile_file.good()) {
        std::wcerr << L"ksh: warning: failed writing startup profile: " << profile_path << L"\n";
        return false;
    }

    return true;
}

std::wstring get_default_startup_profile_path() {
    wchar_t userprofile_path[MAX_PATH];
    DWORD userprofile_len = GetEnvironmentVariableW(L"USERPROFILE", userprofile_path, MAX_PATH);
    if (userprofile_len > 0 && userprofile_len < MAX_PATH) {
        std::wstring candidate = std::wstring(userprofile_path) + L"\\.kshrc";
        if (file_exists_regular(candidate) || ensure_default_home_kshrc_exists(candidate)) {
            return candidate;
        }
    }

    // Legacy fallback for existing installs that use the AppData profile path.
    wchar_t appdata_path[MAX_PATH];
    DWORD appdata_len = GetEnvironmentVariableW(L"APPDATA", appdata_path, MAX_PATH);
    if (appdata_len > 0 && appdata_len < MAX_PATH) {
        std::wstring candidate = std::wstring(appdata_path) + L"\\ksh_profile.ksh";
        if (file_exists_regular(candidate)) {
            return candidate;
        }
    }

    return L"";
}

bool load_startup_profile(const StartupOptions& options, bool& should_exit_shell) {
    if (options.disable_profile) {
        return true;
    }

    std::wstring profile_path;
    if (options.profile_override_set) {
        profile_path = trim_copy(options.profile_override_path);
        if (profile_path.empty()) {
            return true;
        }
    } else {
        profile_path = get_default_startup_profile_path();
        if (profile_path.empty()) {
            return true;
        }
    }

    if (!file_exists_regular(profile_path)) {
        if (options.profile_override_set) {
            std::wcerr << L"ksh: cannot open startup profile: " << profile_path << L"\n";
        }
        return true;
    }

    std::vector<std::wstring> empty_args;
    if (!execute_script_file(profile_path, empty_args, should_exit_shell)) {
        std::wcerr << L"ksh: startup profile reported an error: " << profile_path << L"\n";
    }

    return true;
}

// Startup section: command-line option parsing and shell bootstrap.
bool parse_startup_arguments(int argc, wchar_t* argv[], StartupOptions& options, int& first_positional_index, bool& show_help, bool& show_version) {
    first_positional_index = argc;

    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];

        if (arg == L"--") {
            first_positional_index = i + 1;
            return true;
        }

        if (arg == L"-c") {
            if (i + 1 >= argc) {
                std::wcerr << L"ksh: -c requires a command string\n";
                return false;
            }
            options.command_string_set = true;
            options.command_string = argv[++i];
            first_positional_index = i + 1;
            return true;
        }

        if (arg == L"--script-test") {
            if (i + 1 >= argc) {
                std::wcerr << L"ksh: --script-test requires a script path\n";
                return false;
            }
            options.script_test_set = true;
            options.script_test_path = argv[++i];
            options.disable_profile = true;
            first_positional_index = i + 1;
            return true;
        }

        if (arg == L"-h" || arg == L"--help") {
            show_help = true;
            continue;
        }

        if (arg == L"--version") {
            show_version = true;
            continue;
        }

        if (arg == L"--no-profile") {
            options.disable_profile = true;
            continue;
        }

        if (arg == L"--profile") {
            if (i + 1 >= argc) {
                std::wcerr << L"ksh: --profile requires a file path\n";
                return false;
            }
            options.profile_override_set = true;
            options.profile_override_path = argv[++i];
            continue;
        }

        if (arg.rfind(L"--profile=", 0) == 0) {
            options.profile_override_set = true;
            options.profile_override_path = arg.substr(10);
            continue;
        }

        first_positional_index = i;
        return true;
    }

    return true;
}
