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

#include "ksh_internal.h"

// Runtime state definitions
std::map<std::wstring, CustomTypeDefinition> g_main_custom_types;
KshEnvironment g_main_env;
std::vector<ScriptContext> g_main_script_context_stack;
std::map<std::wstring, ShellFunctionDefinition> g_main_shell_functions;
std::vector<FunctionScopeContext> g_main_function_scope_stack;
std::set<HANDLE> g_pipeline_thread_handles;
std::mutex g_pipeline_threads_mutex;
std::map<std::wstring, std::wstring> g_main_aliases;

thread_local std::vector<HistoryEntry> g_command_history;
thread_local std::vector<BackgroundJob> g_background_jobs;
thread_local CoProcessState g_coprocess;
thread_local std::vector<std::wstring> g_directory_stack;
thread_local std::map<std::wstring, std::wstring> g_trap_handlers;
thread_local std::map<std::wstring, CommandHashEntry> g_command_hash_table;

thread_local KshEnvironment* g_current_env = &g_main_env;
thread_local std::map<std::wstring, CustomTypeDefinition>* g_current_custom_types = &g_main_custom_types;
thread_local std::vector<ScriptContext>* g_current_script_context_stack = &g_main_script_context_stack;
thread_local std::map<std::wstring, ShellFunctionDefinition>* g_current_shell_functions = &g_main_shell_functions;
thread_local std::vector<FunctionScopeContext>* g_current_function_scope_stack = &g_main_function_scope_stack;
thread_local std::map<std::wstring, std::wstring>* g_current_aliases = &g_main_aliases;

thread_local bool g_inprocess_pipeline_stage = false;
thread_local int g_subshell_nesting_depth = 0;

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

thread_local bool g_running_trap_handler = false;
thread_local bool g_xtrace_enabled = false;
thread_local bool g_errexit_enabled = false;
thread_local bool g_nounset_enabled = false;
thread_local bool g_vi_mode_enabled = false;
thread_local bool g_pipefail_enabled = false;
thread_local int g_errexit_disabled_depth = 0;
thread_local bool g_exit_trap_executed = false;
thread_local int g_umask = 022;

thread_local SubshellPathPerfCounters g_subshell_path_perf_counters;
thread_local std::set<std::wstring> g_active_get_hooks;
thread_local std::set<std::wstring> g_active_set_hooks;
thread_local std::set<std::wstring> g_active_init_hooks;
thread_local HANDLE g_subshell_stdout = INVALID_HANDLE_VALUE;
thread_local HANDLE g_pipeline_stdin = INVALID_HANDLE_VALUE;
thread_local HANDLE g_pipeline_stdout = INVALID_HANDLE_VALUE;
thread_local HANDLE g_pipeline_stderr = INVALID_HANDLE_VALUE;
thread_local std::wstring* g_builtin_capture_output = nullptr;
thread_local unsigned long long g_child_user_time = 0;
thread_local unsigned long long g_child_kernel_time = 0;

#undef SetEnvironmentVariableW
#undef GetEnvironmentVariableW

BOOL set_shell_environment_variable(LPCWSTR name, LPCWSTR value) {
    if (g_inprocess_pipeline_stage || g_subshell_nesting_depth > 0) {
        return TRUE;
    }
    return ::SetEnvironmentVariableW(name, value);
}

DWORD get_shell_environment_variable(LPCWSTR name, LPWSTR buffer, DWORD size) {
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

bool get_flag_value(const std::map<std::wstring, bool>& flags, const std::wstring& name) {
    std::map<std::wstring, bool>::const_iterator it = flags.find(name);
    return it != flags.end() && it->second;
}

void set_flag_value(std::map<std::wstring, bool>& flags, const std::wstring& name, bool value) {
    flags[name] = value;
}

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

bool is_perf_trace_enabled() {
    auto it = ksh_env.variables.find(L"KSH_PERF_TRACE");
    if (it != ksh_env.variables.end() && it->second == L"1") {
        return true;
    }
    wchar_t buf[8] = {};
    DWORD n = GetEnvironmentVariableW(L"KSH_PERF_TRACE", buf, _countof(buf));
    return (n > 0 && std::wstring(buf, n) == L"1");
}

void log_performance_trace(const std::wstring& phase, ULONGLONG duration_ms, const std::wstring& details) {
    if (!is_perf_trace_enabled()) {
        return;
    }
    if (details.empty()) {
        std::wcerr << L"[perf] " << phase << L": " << duration_ms << L" ms\n";
    } else {
        std::wcerr << L"[perf] " << phase << L": " << duration_ms << L" ms (" << details << L")\n";
    }
}

std::wstring get_system_env_var(const std::wstring& name) {
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

std::wstring get_status_value(const std::wstring& default_value) {
    std::map<std::wstring, std::wstring>::const_iterator status_it = ksh_env.variables.find(L"?");
    if (status_it != ksh_env.variables.end()) {
        return status_it->second;
    }
    return default_value;
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
        return is_truthy_setting_value(std::wstring(buffer, ret));
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

bool pattern_match_case_sensitive() {
    return !get_setting_bool(L"ksh_PATTERN_CASE_INSENSITIVE", false);
}

ShellStateSnapshot capture_shell_state_snapshot() {
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

void restore_shell_state_snapshot_if_present(
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

ScopedSnapshotRestoreGuard::ScopedSnapshotRestoreGuard(
    std::optional<ShellStateSnapshot>& snapshot,
    const wchar_t* default_status)
    : snapshot_ref_(snapshot),
      status_default_(default_status != nullptr ? default_status : L"0") {}

ScopedSnapshotRestoreGuard::~ScopedSnapshotRestoreGuard() {
    if (!dismissed_) {
        restore_shell_state_snapshot_if_present(snapshot_ref_, status_default_);
    }
}

void ScopedSnapshotRestoreGuard::dismiss() {
    dismissed_ = true;
}

