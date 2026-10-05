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

#ifndef CROSSSHELL_KSH_STATE_H
#define CROSSSHELL_KSH_STATE_H

#include "ksh_types.h"

// Runtime state globals
extern std::map<std::wstring, CustomTypeDefinition> g_main_custom_types;
extern KshEnvironment g_main_env;
extern std::vector<ScriptContext> g_main_script_context_stack;
extern std::map<std::wstring, ShellFunctionDefinition> g_main_shell_functions;
extern std::vector<FunctionScopeContext> g_main_function_scope_stack;
extern std::set<HANDLE> g_pipeline_thread_handles;
extern std::mutex g_pipeline_threads_mutex;
extern std::map<std::wstring, std::wstring> g_main_aliases;

extern thread_local std::vector<HistoryEntry> g_command_history;
extern thread_local std::vector<BackgroundJob> g_background_jobs;
extern thread_local CoProcessState g_coprocess;
extern thread_local std::vector<std::wstring> g_directory_stack;
extern thread_local std::map<std::wstring, std::wstring> g_trap_handlers;
extern thread_local std::map<std::wstring, CommandHashEntry> g_command_hash_table;

extern thread_local KshEnvironment* g_current_env;
extern thread_local std::map<std::wstring, CustomTypeDefinition>* g_current_custom_types;
extern thread_local std::vector<ScriptContext>* g_current_script_context_stack;
extern thread_local std::map<std::wstring, ShellFunctionDefinition>* g_current_shell_functions;
extern thread_local std::vector<FunctionScopeContext>* g_current_function_scope_stack;
extern thread_local std::map<std::wstring, std::wstring>* g_current_aliases;

#define ksh_env (*g_current_env)
#define g_custom_types (*g_current_custom_types)
#define g_script_context_stack (*g_current_script_context_stack)
#define g_shell_functions (*g_current_shell_functions)
#define g_function_scope_stack (*g_current_function_scope_stack)
#define g_aliases (*g_current_aliases)

extern thread_local bool g_inprocess_pipeline_stage;
extern thread_local int g_subshell_nesting_depth;

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

extern thread_local std::array<HANDLE, kCustomFdTableSize> g_custom_fd_table;
extern thread_local int g_next_job_id;
extern thread_local std::wstring g_history_file_path;
extern thread_local std::wstring g_last_pipeline_error_detail;
extern thread_local bool g_is_interactive_session;
extern thread_local bool g_powershell_bypass_enabled;
extern thread_local bool g_expansion_error;
extern thread_local int g_expansion_nesting_depth;
extern thread_local int g_eval_nesting_depth;
extern volatile LONG g_in_interactive_loop;
extern volatile LONG g_foreground_process_active;
extern volatile LONG g_pending_int_trap;
extern volatile LONG g_pending_break_trap;
extern volatile LONG g_pending_hup_trap;
extern volatile LONG g_pending_chld_trap;
extern volatile LONG g_pending_alrm_trap;
extern volatile LONG g_pending_usr1_trap;
extern volatile LONG g_pending_usr2_trap;
extern volatile LONG g_pending_segv_trap;
extern volatile LONG g_pending_fpe_trap;
extern volatile LONG g_pending_term_trap;
extern volatile LONG g_int_trap_active;
extern volatile LONG g_break_trap_active;
extern volatile LONG g_hup_trap_active;
extern volatile LONG g_alrm_trap_active;
extern volatile LONG g_usr1_trap_active;
extern volatile LONG g_usr2_trap_active;
extern volatile LONG g_segv_trap_active;
extern volatile LONG g_fpe_trap_active;
extern volatile LONG g_term_trap_active;

extern thread_local bool g_running_trap_handler;
extern thread_local bool g_xtrace_enabled;
extern thread_local bool g_errexit_enabled;
extern thread_local bool g_nounset_enabled;
extern thread_local bool g_vi_mode_enabled;
extern thread_local bool g_pipefail_enabled;
extern thread_local int g_errexit_disabled_depth;
extern thread_local bool g_exit_trap_executed;
extern thread_local int g_umask;

extern thread_local SubshellPathPerfCounters g_subshell_path_perf_counters;
extern thread_local std::set<std::wstring> g_active_get_hooks;
extern thread_local std::set<std::wstring> g_active_set_hooks;
extern thread_local std::set<std::wstring> g_active_init_hooks;
extern thread_local HANDLE g_subshell_stdout;
extern thread_local HANDLE g_pipeline_stdin;
extern thread_local HANDLE g_pipeline_stdout;
extern thread_local HANDLE g_pipeline_stderr;
extern thread_local std::wstring* g_builtin_capture_output;
extern thread_local unsigned long long g_child_user_time;
extern thread_local unsigned long long g_child_kernel_time;

struct ScopedTrapExecutionFlag {
    explicit ScopedTrapExecutionFlag(bool& flag) : flag_(flag) {
        flag_ = true;
    }
    ~ScopedTrapExecutionFlag() {
        flag_ = false;
    }
private:
    bool& flag_;
};

BOOL set_shell_environment_variable(LPCWSTR name, LPCWSTR value);
DWORD get_shell_environment_variable(LPCWSTR name, LPWSTR buffer, DWORD size);

#ifndef KSH_AMALGAMATION_INLINE_SHIMS
#define SetEnvironmentVariableW set_shell_environment_variable
#define GetEnvironmentVariableW get_shell_environment_variable
#endif

bool get_flag_value(const std::map<std::wstring, bool>& flags, const std::wstring& name);
void set_flag_value(std::map<std::wstring, bool>& flags, const std::wstring& name, bool value);
void record_command_substitution_path_perf(bool used_fast_capture, bool used_snapshot_restore, ULONGLONG elapsed_ms);
void record_parenthesized_subshell_path_perf(bool used_fast_path, bool used_snapshot_restore, ULONGLONG elapsed_ms);
void log_performance_trace(const std::wstring& phase, ULONGLONG duration_ms, const std::wstring& details = L"");
bool is_perf_trace_enabled();
std::wstring get_system_env_var(const std::wstring& name);
std::wstring get_environment_value(const std::wstring& name);

struct ShellStateSnapshot {
    KshEnvironment environment;
    std::map<std::wstring, ShellFunctionDefinition> shell_functions;
    std::map<std::wstring, std::wstring> aliases;
    std::map<std::wstring, CustomTypeDefinition> custom_types;
    std::array<HANDLE, kCustomFdTableSize> custom_fd_table;
    std::map<std::wstring, std::wstring> trap_handlers;
    std::wstring saved_dir;
};

ShellStateSnapshot capture_shell_state_snapshot();
void restore_shell_state_snapshot_if_present(std::optional<ShellStateSnapshot>& snapshot_opt, const std::wstring& status_default_value);

class ScopedSnapshotRestoreGuard {
public:
    ScopedSnapshotRestoreGuard(
        std::optional<ShellStateSnapshot>& snapshot,
        const wchar_t* default_status = L"0");
    ~ScopedSnapshotRestoreGuard();
    void dismiss();

private:
    std::optional<ShellStateSnapshot>& snapshot_ref_;
    std::wstring status_default_;
    bool dismissed_ = false;
};

bool is_hardened_mode_enabled();
bool is_regex_enabled();
bool is_performance_telemetry_enabled();
bool pattern_match_case_sensitive();
bool is_truthy_setting_value(const std::wstring& value);
bool get_setting_bool(const std::wstring& name, bool default_value);

#endif // CROSSSHELL_KSH_STATE_H
