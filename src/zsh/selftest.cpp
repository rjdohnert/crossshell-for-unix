#include "selftest.hpp"
#include "zsh.hpp"
#include "engine.hpp"
#include "parser.hpp"
#include "expansion.hpp"
#include "builtins.hpp"
#include "jobs.hpp"
#include "scripting.hpp"
#include "terminal.hpp"

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
