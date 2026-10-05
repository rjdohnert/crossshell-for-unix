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

#include "../include/ksh_internal.h"

void register_child_process_times(HANDLE hProcess) {
    if (hProcess == nullptr || hProcess == INVALID_HANDLE_VALUE) return;
    FILETIME creationTime, exitTime, kernelTime, userTime;
    if (GetProcessTimes(hProcess, &creationTime, &exitTime, &kernelTime, &userTime)) {
        ULARGE_INTEGER uUser, uKernel;
        uUser.LowPart = userTime.dwLowDateTime;
        uUser.HighPart = userTime.dwHighDateTime;
        uKernel.LowPart = kernelTime.dwLowDateTime;
        uKernel.HighPart = kernelTime.dwHighDateTime;
        g_child_user_time += uUser.QuadPart;
        g_child_kernel_time += uKernel.QuadPart;
    }
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
        std::wstring trimmed = trim_copy(segment);
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
    std::vector<std::wstring>* temp_files_out) {
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
                std::wstring trimmed_stage = trim_copy(segment);
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

bool execute_native_or_fallback(const std::wstring& full_command, const RedirectionSpec* redir, DWORD* exit_code_out) {
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

bool launch_process(const std::wstring& full_command, HANDLE& process_handle, DWORD& pid, const RedirectionSpec* redir) {
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

bool is_windows_internal_command(const std::wstring& cmd) {
    static const std::vector<std::wstring> internals = {
        L"dir", L"copy", L"move", L"del", L"erase", L"type", L"ren", L"rename",
        L"md", L"mkdir", L"rd", L"rmdir", L"cls", L"color", L"date", L"time",
        L"ver", L"vol", L"path", L"prompt", L"assoc", L"ftype", L"mklink"
    };
    std::wstring lower_cmd = to_lower_copy(cmd);
    return std::find(internals.begin(), internals.end(), lower_cmd) != internals.end();
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

bool build_cmd_shell_command_line(const std::wstring& command, std::wstring& command_line) {
    std::wstring cmd_path;
    if (!resolve_cmd_exe_path(cmd_path)) {
        return false;
    }

    command_line = quote_command_argument(cmd_path) + L" /d /s /c \"" + command + L"\"";
    return true;
}
