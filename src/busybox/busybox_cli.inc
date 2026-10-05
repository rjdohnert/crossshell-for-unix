int run_self_test() {
    int failures = 0;
    auto check = [&](const std::string& name, bool passed) {
        std::cout << "SELF-TEST " << (passed ? "PASS" : "FAIL") << ": " << name << "\n";
        if (!passed) failures++;
    };

    std::error_code ec;
    fs::path original_directory = fs::current_path(ec);
    fs::path test_root = fs::temp_directory_path(ec) /
        ("busybox_self_test_" + std::to_string(GetCurrentProcessId()));
    fs::remove_all(test_root, ec);
    fs::create_directories(test_root / "nested", ec);
    if (ec) {
        std::cerr << "SELF-TEST ERROR: cannot create temporary test directory\n";
        return 1;
    }

    ShellContext ctx(false);
    const std::string test_variable = "BUSYBOX_SELF_TEST_VAR";
    DWORD old_variable_length = GetEnvironmentVariableW(utf8_to_utf16(test_variable).c_str(), nullptr, 0);
    std::wstring old_variable;
    bool had_old_variable = old_variable_length > 0;
    if (had_old_variable) {
        old_variable.resize(old_variable_length - 1);
        GetEnvironmentVariableW(utf8_to_utf16(test_variable).c_str(), old_variable.data(), old_variable_length);
    }
    SetEnvironmentVariableW(utf8_to_utf16(test_variable).c_str(), nullptr);

    check("applet dispatch and sequence output",
        capture_cmd_output("seq 1 3", ctx) == "1\n2\n3");
    check("assignment prefix executes the command",
        capture_cmd_output("TEST_VALUE=visible echo $TEST_VALUE", ctx) == "visible");

    Parser assignment_parser(tokenize("TEST_VALUE=stateful"));
    auto assignment_ast = assignment_parser.parse_chain();
    if (assignment_ast) assignment_ast->execute(ctx, NULL, NULL);
    check("shell assignment updates shell state", ctx.get_var("TEST_VALUE") == "stateful");

    fs::path saved_directory = fs::current_path(ec);
    Parser cwd_parser(tokenize("(cd \"" + path_to_utf8(test_root / "nested") + "\")"));
    auto cwd_ast = cwd_parser.parse_chain();
    if (cwd_ast) cwd_ast->execute(ctx, NULL, NULL);
    check("subshell restores current directory", fs::current_path(ec) == saved_directory);

    Parser pipeline_cwd_parser(tokenize("(cd \"" + path_to_utf8(test_root / "nested") + "\") | pwd"));
    auto pipeline_cwd_ast = pipeline_cwd_parser.parse_chain();
    if (pipeline_cwd_ast) pipeline_cwd_ast->execute(ctx, NULL, NULL);
    check("pipeline directory changes do not leak", fs::current_path(ec) == saved_directory);

    Parser environment_parser(tokenize("(BUSYBOX_SELF_TEST_VAR=inner)"));
    auto environment_ast = environment_parser.parse_chain();
    if (environment_ast) environment_ast->execute(ctx, NULL, NULL);
    DWORD leaked_length = GetEnvironmentVariableW(utf8_to_utf16(test_variable).c_str(), nullptr, 0);
    check("subshell restores process environment", leaked_length == 0);
    check("command substitution isolates shell variables",
        capture_cmd_output("(BUSYBOX_SELF_TEST_VAR=inner); echo $BUSYBOX_SELF_TEST_VAR", ctx).empty());

    Parser exit_parser(tokenize("exit 7"));
    auto exit_ast = exit_parser.parse_chain();
    int exit_status = 0;
    try {
        if (exit_ast) exit_ast->execute(ctx, NULL, NULL);
    } catch (const ShellExitSignal& signal) {
        exit_status = signal.code();
    }
    check("nested exit returns a shell status", exit_status == 7);

    fs::path script_path = test_root / "self_test.sh";
    fs::path script_output = test_root / "script_output.txt";
    {
        std::ofstream script(script_path, std::ios::binary);
        script << "echo $1 > \"" << path_to_utf8(script_output) << "\"\n";
    }
    int script_status = cmd_sh({"sh", path_to_utf8(script_path), "script-argument"}, ctx);
    std::ifstream script_result(script_output, std::ios::binary);
    std::string script_value((std::istreambuf_iterator<char>(script_result)), std::istreambuf_iterator<char>());
    while (!script_value.empty() && (script_value.back() == '\r' || script_value.back() == '\n')) script_value.pop_back();
    check("script execution and positional arguments", script_status == 0 && script_value == "script-argument");

    fs::path oversized_script = test_root / "oversized.sh";
    {
        std::ofstream script(oversized_script, std::ios::binary);
        script << std::string(MAX_SCRIPT_BYTES + 1, 'x');
    }
    check("oversized scripts are rejected", cmd_sh({"sh", path_to_utf8(oversized_script)}, ctx) == 126);

    std::wstring saved_userprofile;
    DWORD userprofile_length = GetEnvironmentVariableW(L"USERPROFILE", nullptr, 0);
    bool had_userprofile = userprofile_length > 0;
    if (had_userprofile) {
        saved_userprofile.resize(userprofile_length - 1);
        GetEnvironmentVariableW(L"USERPROFILE", saved_userprofile.data(), userprofile_length);
    }
    fs::path history_root = test_root / "history_home";
    fs::create_directories(history_root, ec);
    {
        std::ofstream history(history_root / ".busybox_history", std::ios::binary);
        for (size_t i = 0; i < MAX_HISTORY_ENTRIES + 100; ++i) history << "history-" << i << "\n";
    }
    SetEnvironmentVariableW(L"USERPROFILE", history_root.wstring().c_str());
    ShellContext history_ctx(true);
    check("history loading is bounded", history_ctx.history.size() == MAX_HISTORY_ENTRIES);
    if (had_userprofile) SetEnvironmentVariableW(L"USERPROFILE", saved_userprofile.c_str());
    else SetEnvironmentVariableW(L"USERPROFILE", nullptr);

    ctx.timeout_ms = 50;
    Parser timeout_parser(tokenize("sleep 2 &; fg"));
    auto timeout_ast = timeout_parser.parse_chain();
    int timeout_status = timeout_ast ? timeout_ast->execute(ctx, NULL, NULL) : 1;
    check("foreground jobs honor shell timeout", timeout_status == 124);

    for (auto& job : ctx.jobs) {
        if (job.hProcess != NULL) {
            TerminateProcess(job.hProcess, 124);
            CloseHandle(job.hProcess);
            job.hProcess = NULL;
        }
    }

    SetEnvironmentVariableW(
        utf8_to_utf16(test_variable).c_str(),
        had_old_variable ? old_variable.c_str() : nullptr);
    if (original_directory != fs::current_path(ec)) {
        fs::current_path(original_directory, ec);
    }
    fs::remove_all(test_root, ec);

    std::cout << "SELF-TEST RESULT: " << (failures == 0 ? "PASS" : "FAIL")
              << " (" << failures << " failure(s))\n";
    return failures == 0 ? 0 : 1;
}

int main(int argc, char* argv[]) {
    init_vt100_and_binary_mode();
    init_job_object();
    SetConsoleCtrlHandler(console_signal_handler, TRUE);

    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i) args.push_back(argv[i]);

    if (args.size() > 1 && to_lower(args[1]) == "--self-test") {
        return run_self_test();
    }

    ShellContext ctx;

    try {
        if (!args.empty()) {
            std::string prog_name = fs::path(utf8_to_utf16(args[0])).stem().string();
            std::transform(prog_name.begin(), prog_name.end(), prog_name.begin(), ::tolower);
            if (applets.count(prog_name)) {
                return applets[prog_name](args, ctx);
            }
        }

        if (args.size() > 1) {
            std::string first_arg = to_lower(args[1]);
            if (first_arg == "--") {
                if (args.size() > 2) {
                    std::string next = to_lower(args[2]);
                    if (applets.count(next)) {
                        std::vector<std::string> applet_args(args.begin() + 2, args.end());
                        return applets[next](applet_args, ctx);
                    }
                }
                return cmd_sh(args, ctx);
            }
            if (first_arg == "--help" || first_arg == "-h" || first_arg == "help") {
                std::vector<std::string> help_args = (args.size() > 2) ? std::vector<std::string>{"help", args[2]} : std::vector<std::string>{"help"};
                return cmd_help(help_args, ctx);
            }
            if (first_arg == "--version" || first_arg == "-v") {
                std::cout << "busybox\n";
                return 0;
            }
            if (applets.count(first_arg)) {
                std::vector<std::string> applet_args(args.begin() + 1, args.end());
                return applets[first_arg](applet_args, ctx);
            }
        }

        return cmd_sh(args, ctx);
    } catch (const std::exception& e) {
        std::cerr << "busybox: fatal error: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "busybox: fatal error: unknown exception\n";
        return 1;
    }
}
