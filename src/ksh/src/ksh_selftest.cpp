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

    {
        assert_true(is_ksh_script_path(L"script.sh"), L"native .sh script recognition");
        assert_true(is_ksh_script_path(L"script.SH"), L"case-insensitive .sh script recognition");
        assert_true(is_ksh_script_path(L"script.ksh"), L"native .ksh script recognition");
        assert_true(!is_ksh_script_path(L"script.ps1"), L"PowerShell scripts retain their interpreter");
        const std::vector<std::wstring> args = { L"argument with spaces", L"quoted\"argument" };
        std::wstring expected_command;
        assert_true(build_self_script_command(L"folder with spaces\\script.sh", args, expected_command),
            L"build native .sh interpreter command");
        std::wstring actual_command;
        assert_true(build_script_interpreter_command(
            { L"folder with spaces\\script.sh", args[0], args[1] }, actual_command) == ScriptInterpreterResolution::Resolved,
            L"resolve .sh without an external Bash interpreter");
        assert_eq(actual_command, expected_command, L"native .sh interpreter preserves argument quoting");
    }

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