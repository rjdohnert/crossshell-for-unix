#include "zsh.hpp"
#include "engine.hpp"
#include "scripting.hpp"
#include "terminal.hpp"
#include "builtins.hpp"
#include "jobs.hpp"
#include "selftest.hpp"

int main(int argc, char* argv[]) {
    enable_ansi_support();

    // Convert Win32 wide command line arguments to UTF-8 so Unicode and Nerd Font arguments are preserved
    int wide_argc = 0;
    LPWSTR* wide_argv = CommandLineToArgvW(GetCommandLineW(), &wide_argc);
    vector<string> utf8_args;
    vector<char*> utf8_argv_ptrs;
    if (wide_argv) {
        for (int i = 0; i < wide_argc; ++i) {
            int len = WideCharToMultiByte(CP_UTF8, 0, wide_argv[i], -1, nullptr, 0, nullptr, nullptr);
            if (len > 0) {
                string s(len - 1, '\0');
                WideCharToMultiByte(CP_UTF8, 0, wide_argv[i], -1, &s[0], len, nullptr, nullptr);
                utf8_args.push_back(s);
            } else {
                utf8_args.push_back("");
            }
        }
        LocalFree(wide_argv);
        argc = wide_argc;
        for (auto& s : utf8_args) utf8_argv_ptrs.push_back(&s[0]);
        argv = utf8_argv_ptrs.data();
    }

    if (argc >= 4 && string(argv[1]) == "--multios-tee") {
        vector<unique_ptr<ofstream>> outputs;
        for (int i = 2; i + 1 < argc; i += 2) {
            ios::openmode mode = ios::binary | ios::out | (string(argv[i]) == "append" ? ios::app : ios::trunc);
            auto output = make_unique<ofstream>(normalize_path_to_win(argv[i + 1]), mode);
            if (!*output) return 1;
            outputs.push_back(std::move(output));
        }
        char buffer[64 * 1024];
        while (cin) {
            cin.read(buffer, sizeof(buffer));
            streamsize count = cin.gcount();
            for (auto& output : outputs) output->write(buffer, count);
        }
        for (auto& output : outputs) if (!*output) return 1;
        return 0;
    }
    if (argc >= 4 && string(argv[1]) == "--pipeline-stage") {
        string state_path = argv[2];
        if (!load_pipeline_shell_state(state_path)) {
            cerr << "zsh: unable to load pipeline shell state\n";
            return 1;
        }
        error_code remove_error;
        fs::remove(normalize_path_to_win(state_path), remove_error);
        vector<string> stage_args(argv + 3, argv + argc);
        int status = dispatch_command(stage_args);
        cout.flush();
        cerr.flush();
        return status;
    }
    if (argc > 1) {
        string arg1 = argv[1];
        if (arg1 == "--self-test" || arg1 == "--regression-test" || arg1 == "--test") {
            return run_internal_self_tests();
        }
    }
    bool load_rcs = true;
    string command;
    string script_path;
    int operand_index = 1;

    while (operand_index < argc) {
        string arg = argv[operand_index];
        if (arg == "-f" || arg == "--no-rcs") {
            load_rcs = false;
            ++operand_index;
        } else if (arg == "-l" || arg == "--login") {
            g_login_shell = true;
            ++operand_index;
        } else if (arg == "-c") {
            if (operand_index + 1 >= argc) {
                cerr << "zsh: -c requires a command\n";
                return 2;
            }
            command = argv[operand_index + 1];
            operand_index += 2;
            break;
        } else if (arg == "--") {
            ++operand_index;
            if (operand_index < argc) script_path = argv[operand_index++];
            break;
        } else if (!arg.empty() && arg[0] == '-') {
            break;
        } else {
            script_path = arg;
            ++operand_index;
            break;
        }
    }

    if (!command.empty()) {
        if (operand_index < argc) g_env.vars["0"] = argv[operand_index++];
        for (; operand_index < argc; ++operand_index) g_env.positional_args.push_back(argv[operand_index]);
    } else if (!script_path.empty()) {
        g_env.vars["0"] = script_path;
        for (; operand_index < argc; ++operand_index) g_env.positional_args.push_back(argv[operand_index]);
    }

    enable_ansi_support();
    init_signal_handlers();
    g_env.load_history();

    // Set ls/clear aliases only when the native commands are absent from PATH.
    if (find_executable_in_path("ls").empty())    g_env.aliases["ls"]    = "dir";
    if (find_executable_in_path("clear").empty()) g_env.aliases["clear"] = "cls";

    error_code _rc_ec;
    if (load_rcs && !fs::exists(g_env.zshrc_path, _rc_ec)) {
        ofstream rc(g_env.zshrc_path);
        if (rc.is_open()) {
            rc << "# =============================================================================\n"
               << "# CrossShellZSH configuration file (~/.zshrc)\n"
               << "# Generated automatically on first run. Edit to customise your environment.\n"
               << "# =============================================================================\n"
               << "\n"
               << "# -----------------------------------------------------------------------------\n"
               << "# Prompt Color Configuration\n"
               << "# -----------------------------------------------------------------------------\n"
               << "# Specify colors by name using %F{color_name} and reset with %f.\n"
               << "# Available colors: cyan, green, yellow, red, blue, magenta, white, orange, pink,\n"
               << "#                   br_cyan, br_green, br_yellow, br_red, br_blue, br_magenta, br_white\n"
               << "#\n"
               << "# Clean Multi-Color Theme (Cyan user@host, Green directory, Yellow % / #):\n"
               << "# PROMPT=\"%F{cyan}[%n@%m]%f %F{green}%~%f %F{yellow}%#%f \"\n"
               << "\n"
               << "# Additional prompt theme examples (uncomment to activate):\n"
               << "# PROMPT=\"%B%F{br_cyan}%~%f%b %F{br_green}❯%f \"\n"
               << "# PROMPT=\"%F{magenta}%n%f@%F{yellow}%m%f %F{blue}%1~%f %F{br_green}%#%f \"\n"
               << "\n"
               << "# -----------------------------------------------------------------------------\n"
               << "# History settings\n"
               << "# -----------------------------------------------------------------------------\n"
               << "HISTSIZE=10000\n"
               << "SAVEHIST=10000\n"
               << "\n"
               << "# -----------------------------------------------------------------------------\n"
               << "# Common aliases\n"
               << "# -----------------------------------------------------------------------------\n"
               << "alias ll='ls -la'\n"
               << "alias grep='grep --color=auto'\n"
               << "alias gs='git status'\n"
               << "alias gd='git diff'\n"
               << "\n"
               << "# -----------------------------------------------------------------------------\n"
               << "# Environment\n"
               << "# -----------------------------------------------------------------------------\n"
               << "export EDITOR=notepad\n"
               << "export PAGER=more\n";
            rc.close();
            cerr << COLOR_BR_GREEN << "zsh: created " << g_env.zshrc_path << COLOR_RESET << "\n";
        }
    } else if (load_rcs) {
        execute_script(g_env.zshrc_path);
    }

    if (argc > 1) {
        string arg1 = argv[1];
        if (arg1 == "--version" || arg1 == "-v") {
            cout << ZSH_VERSION_STRING << "\n"
                 << "Copyright (c) 2026 Roberto J Dohnert\n"
                 << "Licensed under the BSD 3-Clause License.\n";
            return 0;
        }
        if (arg1 == "--help" || arg1 == "-h") { print_comprehensive_help(); return 0; }
        if (!command.empty()) { int rc = parse_and_execute(command); fire_exit_trap(); return rc; }
        if (!script_path.empty()) { int rc = execute_script(script_path); fire_exit_trap(); return rc; }
        if (operand_index < argc || (!arg1.empty() && arg1[0] == '-')) {
            cerr << "zsh: unsupported option: " << argv[operand_index] << "\n";
            return 2;
        }
    }

    cout << COLOR_BR_CYAN << format_now_header_line() << COLOR_RESET << "\n\n";

    string pending_edit_line;
    while (true) {
        string line;
        if (!pending_edit_line.empty()) {
            string edit_buf = pending_edit_line;
            pending_edit_line.clear();
            line = read_line_interactive(edit_buf);
        } else {
            cout << render_prompt();
            line = read_line_interactive();
        }
        if (!line.empty()) {
            string action_line = line;
            if (!apply_interactive_typo_correction(line, action_line, pending_edit_line)) {
                continue;
            }
            g_env.add_history(action_line);
            parse_and_execute(action_line);
        }
    }
    return 0;
}
