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

#ifndef CROSSSHELL_KSH_TYPES_H
#define CROSSSHELL_KSH_TYPES_H

#include "ksh_common.h"

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

enum class CommandResolutionKind {
    None,
    Alias,
    Function,
    Builtin,
    External,
    Missing
};

enum class ScriptInterpreterResolution {
    NotScript,
    Resolved,
    Failed
};

struct CommandHashEntry {
    std::wstring path;
    int hits = 0;
};

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

struct CompletionCandidate {
    std::wstring value;
    int rank = 0;
};

struct CompletionQuery {
    std::wstring normalized_prefix;
    bool quoted = false;
    wchar_t quote_char = L'\0';
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

#endif // CROSSSHELL_KSH_TYPES_H
