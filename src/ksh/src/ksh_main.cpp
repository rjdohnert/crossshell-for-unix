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
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ksh_internal.h"

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