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

#ifndef CROSSSHELL_KSH_JOBS_H
#define CROSSSHELL_KSH_JOBS_H

#include "ksh_types.h"

void register_child_process_times(HANDLE hProcess);
bool create_process_with_handle_list(
    const std::wstring& command_line,
    const STARTUPINFOW& startup,
    const std::vector<HANDLE>& handles_to_inherit,
    DWORD creation_flags,
    PROCESS_INFORMATION& process_info);

bool launch_process(const std::wstring& command, HANDLE& process_handle, DWORD& pid, const RedirectionSpec* redir = nullptr);
bool execute_native_or_fallback(const std::wstring& command, const RedirectionSpec* redir = nullptr, DWORD* out_exit_code = nullptr);

void update_background_jobs(bool report_completion);
void cleanup_all_background_jobs();
BackgroundJob* find_background_job(int job_id);
BackgroundJob* find_default_background_job_for_resume_or_foreground();
bool resolve_job_reference(const std::wstring& raw, int& job_id);
std::wstring format_background_job_line(const BackgroundJob& job);
void close_background_job_handles(BackgroundJob& job);
bool wait_for_background_job(BackgroundJob& job, DWORD& exit_code);
void remove_background_job(int job_id);

std::vector<std::wstring> split_pipeline_segments(const std::wstring& input);
DWORD WINAPI run_inprocess_pipeline_stage(LPVOID raw_context);
bool launch_pipeline_stage_process(
    const std::wstring& segment,
    HANDLE segment_stdin,
    HANDLE segment_stdout,
    HANDLE segment_stderr,
    PROCESS_INFORMATION& pi,
    std::wstring& temp_file_path_out,
    bool allow_inprocess);
bool execute_pipeline_segments(
    const std::vector<std::wstring>& segments,
    bool run_in_background,
    DWORD& last_exit_code,
    HANDLE& background_handle,
    DWORD& background_pid,
    std::vector<HANDLE>* background_handles = nullptr,
    std::vector<DWORD>* background_pids = nullptr,
    std::vector<std::wstring>* temp_files_out = nullptr);

bool strip_trailing_unquoted_background_marker(const std::wstring& input, std::wstring& stripped_output);
bool strip_trailing_unquoted_coprocess_marker(const std::wstring& input, std::wstring& stripped_output);
std::wstring format_win32_error_message(DWORD error_code);

bool execute_builtin_coproc(const std::vector<std::wstring>& tokens);
bool close_coprocess(bool terminate_process);

bool build_cmd_shell_command_line(const std::wstring& raw_command, std::wstring& cmd_line);

bool is_windows_internal_command(const std::wstring& cmd);
bool get_system_directory_path(std::wstring& system_directory);
bool resolve_cmd_exe_path(std::wstring& cmd_path);
bool resolve_powershell_exe_path(std::wstring& powershell_path);
bool resolve_bash_exe_path(std::wstring& bash_path);
bool is_absolute_windows_path(const std::wstring& path);
std::wstring normalize_cd_path(const std::wstring& input_path);
bool is_powershell_bypass_enabled();
ScriptInterpreterResolution build_script_interpreter_command(const std::vector<std::wstring>& tokens, std::wstring& command_line);
bool is_ksh_script_path(const std::wstring& path);
bool build_self_script_command(const std::wstring& script_path, const std::vector<std::wstring>& script_args, std::wstring& command_line);

#endif // CROSSSHELL_KSH_JOBS_H
