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

// -----------------------------------------------------------------------------
// Builtin Help System Catalog & Data
// -----------------------------------------------------------------------------

struct BuiltinCommandHelp {
    const wchar_t* name;
    const wchar_t* description;
};

struct BuiltinCommandGroup {
    const wchar_t* title;
    std::vector<const wchar_t*> names;
};

const std::vector<BuiltinCommandHelp>& builtin_command_help_entries() {
    static const std::vector<BuiltinCommandHelp> entries = {
        {L"exit", L"Exit the shell."},
        {L"logout", L"Exit the shell session."},
        {L"source", L"Run commands from a file in the current shell."},
        {L".", L"Alias for source."},
        {L"builtin", L"Invoke a shell builtin directly."},
        {L"set", L"Set shell options/variables or print current variables (supports set -A name values...)."},
        {L"unset", L"Unset variables or functions."},
        {L"export", L"Mark variables for export to child processes."},
        {L"readonly", L"Mark variables as read-only."},
        {L"alias", L"Define or show command aliases."},
        {L"unalias", L"Remove command aliases."},
        {L"command", L"Run command lookup or execute without function lookup."},
        {L"whence", L"Report how command names are resolved."},
        {L"type", L"Describe command type (builtin/function/alias/external)."},
        {L"hash", L"Show or refresh command resolution results."},
        {L"eval", L"Evaluate and execute constructed command text."},
        {L"getopts", L"Parse positional options into shell variables."},
        {L"typeset", L"Declare or inspect shell variables, including -L/-R/-Z justification."},
        {L"enum", L"Define an integer enumeration: enum Name=(value ...)."},
        {L"[[", L"Evaluate a ksh-style conditional expression."},
        {L"test", L"Evaluate a conditional expression."},
        {L"[", L"Alias form of test requiring closing ]."},
        {L"cd", L"Change the current working directory. Accepts Unix-style /path and Windows-style C:\\path."},
        {L"pwd", L"Print the current working directory."},
        {L"dirs", L"Print the current directory stack."},
        {L"pushd", L"Push the current directory onto the stack and change to another directory."},
        {L"popd", L"Pop the directory stack and change to the saved directory."},
        {L"print", L"Write arguments to standard output."},
        {L"echo", L"Write arguments to standard output."},
        {L"printf", L"Format and write arguments to standard output, including %q shell escaping and %T local time output."},
        {L"true", L"Return success status 0."},
        {L"false", L"Return failure status 1."},
        {L"let", L"Evaluate arithmetic expressions and return success when the final value is non-zero."},
        {L"return", L"Return from the current function."},
        {L"clear", L"Clear the terminal screen."},
        {L"history", L"Show the current command history."},
        {L"complete", L"Show command completion candidates."},
        {L"math", L"Evaluate an arithmetic expression."},
        {L"trap", L"Install, clear, or show trap handlers for EXIT/HUP/INT/BREAK/TERM/CHLD and related signal names."},
        {L"jobs", L"List background jobs (jobs [-l|-p] [job...])."},
        {L"fg", L"Bring a background job to the foreground."},
        {L"bg", L"Continue a background job."},
        {L"disown", L"Remove background jobs from the job table without terminating them."},
        {L"kill", L"Send a signal to a process ID or shell job (%job, -p for pid, -j for job)."},
        {L"wait", L"Wait for one or more jobs to finish."},
        {L"coproc", L"Run a command as a bidirectional co-process; use read -p and print -p for I/O."},
        {L"find", L"Search for files in a directory hierarchy."},
        {L"read", L"Read a line from standard input and assign fields to variables or -A arrays."},
        {L"exec", L"Replace the shell process with the specified command, or redirect shell handles."},
        {L"umask", L"Set or print the file mode creation mask."},
        {L"fc", L"List, edit, and re-execute historical shell commands."},
        {L"stty", L"Set or print terminal line configuration settings."},
        {L"times", L"Print accumulated user and system CPU times for shell and children."},
        {L"sleep", L"Pause for a decimal number of seconds; pending console signals interrupt the wait."},
        {L"shift", L"Shift positional parameters left by N places (default 1)."},
        {L":", L"Null command (returns success)."},
        {L"getconf", L"Query system configuration variables (getconf system_var [pathname] or getconf -a)."},
        {L"pathchk", L"Check that pathnames are valid and portable (pathchk [-p] [-P] pathname...)."},
        {L"help", L"Display help information about built-in commands and user-defined functions."},
        {L"version", L"Display shell version information and OS details."}
    };
    return entries;
}

const BuiltinCommandHelp* find_builtin_command_help(std::wstring_view name) {
    const std::vector<BuiltinCommandHelp>& entries = builtin_command_help_entries();
    for (const BuiltinCommandHelp& entry : entries) {
        if (name == entry.name) {
            return &entry;
        }
    }
    return nullptr;
}

const std::vector<BuiltinCommandGroup>& builtin_command_help_groups() {
    static const std::vector<BuiltinCommandGroup> groups = {
        { L"Shell control", { L"exit", L"logout", L"builtin", L"command", L"whence", L"type", L"eval", L"source", L".", L"exec", L"stty" } },
        { L"Directory navigation", { L"cd", L"pwd", L"dirs", L"pushd", L"popd" } },
        { L"Variables and expansion", { L"set", L"unset", L"export", L"readonly", L"typeset", L"enum", L"getopts", L"alias", L"unalias", L"hash", L"let", L"return", L"[[", L"test", L"[" } },
        { L"Jobs and signals", { L"jobs", L"fg", L"bg", L"disown", L"kill", L"wait", L"trap", L"coproc" } },
        { L"Files and system", { L"find", L"pathchk", L"getconf", L"umask", L"times", L"sleep", L"shift", L":" } },
        { L"Interactive and diagnostics", { L"print", L"echo", L"printf", L"true", L"false", L"history", L"complete", L"fc", L"clear", L"math", L"read" } }
    };
    return groups;
}

const std::array<const wchar_t*, 13> kHelpControlFlowKeywordLines = {
    L"\nControl flow keywords:\n",
    L"  break     Exit the nearest enclosing loop.\n",
    L"  continue  Skip to the next loop iteration.\n",
    L"  if / elif / else / fi  Conditional command blocks.\n",
    L"  for / do / done        Iterate over words or positional parameters.\n",
    L"  while / do / done     Repeat while a command succeeds.\n",
    L"  until / do / done     Repeat until a command succeeds.\n",
    L"  select / do / done    Present a numbered selection menu.\n",
    L"  case / in / esac      Match a word against shell patterns.\n",
    L"\nExamples:\n",
    L"  if test -n \"$x\"; then echo set; else echo unset; fi\n",
    L"  for x in 1 2 3; do echo \"$x\"; done\n",
    L"  case \"$x\" in yes) echo true ;; *) echo false ;; esac\n"
};

const std::array<const wchar_t*, 25> kHelpShellSyntaxLines = {
    L"\nShell syntax and execution:\n",
    L"  command1 | command2       Pipe standard output into the next command.\n",
    L"  command &                 Run a command or pipeline in the background.\n",
    L"  <(command) / >(command)   Windows named-pipe process substitution.\n",
    L"  < file, > file, >> file   Redirect standard input or output.\n",
    L"  2> file, 2>> file         Redirect standard error.\n",
    L"  2>&1, 1>&2               Duplicate standard output/error streams.\n",
    L"  command1; command2        Execute top-level commands in sequence.\n",
    L"  command |                 Continue a pipeline on the next script line.\n",
    L"  backslash at line end     Continue a logical command on the next line.\n",
    L"  # comment                Begins a comment outside quotes.\n",
    L"\nExpansion:\n",
    L"  $name, ${name}            Expand shell variables.\n",
    L"  ${array[index]}           Expand indexed or associative array elements.\n",
    L"  $(command)                Capture command substitution output.\n",
    L"  $((expression))           Evaluate arithmetic expansion.\n",
    L"  ${HKLM.Key.Path.Value}    Read a Windows Registry value.\n",
    L"  ${HKLM.Key/With.Dots.Value} Use / for Registry paths containing dots.\n",
    L"\nRegistry namespaces:\n",
    L"  HKLM and HKCU expose dynamic Registry properties.\n",
    L"  HKLM.Key.Path.Value=TEXT writes existing REG_SZ/REG_EXPAND_SZ values.\n",
    L"  Existing REG_DWORD and REG_QWORD values accept numeric assignments.\n",
    L"  Unsupported Registry types are rejected instead of converted silently.\n",
    L"\nStatus and errors:\n",
    L"  $? holds the most recent command status; 0 means success.\n"
};

const std::array<const wchar_t*, 12> kHelpRuntimeBehaviorLines = {
    L"\nScripts and runtime behavior:\n",
    L"  ksh script.sh [args...]    Run a .sh or .ksh script with positional parameters.\n",
    L"  ksh -c \"command\" [name [args...]] Execute a command string.\n",
    L"  ksh --script-test FILE     Run FILE without a startup profile and report PASS/FAIL.\n",
    L"  source FILE or . FILE      Run a file in the current shell state.\n",
    L"  Functions use name() { ... } or function name { ... } syntax.\n",
    L"  Pipeline builtin/function stages use isolated shell state.\n",
    L"  Exported variables are inherited by external child processes.\n",
    L"  Startup profile: ~/.kshrc, or the path supplied by --profile.\n",
    L"  --no-profile disables startup profile loading.\n",
    L"  Script file limit is 64 MiB; variable/function/alias tables have bounded capacities.\n",
    L"  Process-substitution connection timeout: ksh_PROC_SUB_CONNECT_TIMEOUT_MS (1000..600000 ms).\n"
};

const std::array<const wchar_t*, 3> kHelpExternalUtilitiesLines = {
    L"\nExternal utilities:\n",
    L"  suspend   Suspend or resume processes by PID.\n",
    L"  ulimit    Report resource usage or run a child under Windows job limits.\n"
};

const std::array<const wchar_t*, 4> kHelpPathHandlingLines = {
    L"\nPath handling:\n",
    L"  cd accepts both Windows paths and Unix-style rooted paths.\n",
    L"  Example: cd /src/project\n",
    L"  Example: cd C:\\src\\project\n"
};

const std::array<const wchar_t*, 4> kHelpPromptCustomizationLines = {
    L"\nPrompt customization (.kshrc):\n",
    L"  If PS1 is not set, default prompt is '$ ' for users and '# ' for admin.\n",
    L"  Set PS1 in ~/.kshrc to customize order/content.\n",
    L"  Supported prompt tokens: %u user, %d domain, %w cwd, %m host, %# role-char, %% literal-percent.\n"
};

const std::array<const wchar_t*, 2> kHelpPromptCustomizationShortLines = {
    L"  Example: PS1='%u@%d %w %# '\n",
    L"  Example: PS1='%w %# '\n"
};

const std::array<const wchar_t*, 4> kHelpViModeLines = {
    L"\nLine editing mode:\n",
    L"  set -o vi   Enable vi-style command mode in interactive input.\n",
    L"  set +o vi   Return to default insert editing mode.\n",
    L"  vi keys: Esc enter command mode, i/a/A/I switch to insert, 0/^/_ line-start, h/l move, w/W/b/B/e/E/ge/gE jump, | (or N|) column jump, f/F/t/T find, ; repeat, , reverse-repeat, j/k history, x/D delete. Prefix motions with N (e.g., 3w, 4h, 2k).\n"
};

const std::array<const wchar_t*, 7> kHelpCdCommandLines = {
    L"cd\n",
    L"  Change the current working directory.\n",
    L"\nPath handling:\n",
    L"  Accepts Unix-style rooted paths like /src/project.\n",
    L"  Accepts traditional Windows paths like C:\\src\\project.\n",
    L"\nExamples:\n",
    L"  cd /src/project\n"
};

const std::array<const wchar_t*, 1> kHelpCdCommandLinesTail = {
    L"  cd C:\\src\\project\n"
};

const std::array<const wchar_t*, 16> kHelpKillCommandLines = {
    L"kill\n",
    L"  Send a signal to a process ID or shell job.\n",
    L"\nUsage:\n",
    L"  kill [-l] [-s signal] [-p | -j] pid|%job ...\n",
    L"\nTarget selection:\n",
    L"  Bare numeric targets are treated as process IDs.\n",
    L"  Targets beginning with % are treated as shell jobs.\n",
    L"  -p forces process-ID interpretation.\n",
    L"  -j forces shell-job interpretation.\n",
    L"\nSignals:\n",
    L"  TERM/HUP attempt graceful close first, then force termination.\n",
    L"  INT sends CTRL+C, QUIT sends CTRL+BREAK, KILL force-terminates.\n",
    L"\nExamples:\n",
    L"  kill 11576\n",
    L"  kill -p -9 11576\n",
    L"  kill %1\n"
};

const std::array<const wchar_t*, 1> kHelpKillCommandLinesTail = {
    L"  kill -j %1\n"
};

const std::array<const wchar_t*, 9> kHelpPromptCommandLines = {
    L"prompt / PS1\n",
    L"  Customize interactive prompt text from ~/.kshrc using PS1.\n",
    L"\nBehavior:\n",
    L"  If PS1 is unset, default prompt remains '$ ' for users and '# ' for admin.\n",
    L"\nSupported tokens:\n",
    L"  %u user, %d domain, %w cwd, %m host, %# role-char, %% literal-percent\n",
    L"\nExamples:\n",
    L"  PS1='%u@%d %w %# '\n",
    L"  PS1='%w %# '\n"
};

const std::array<const wchar_t*, 1> kHelpPromptCommandLinesTail = {
    L"  PS1='%# '\n"
};

const std::array<const wchar_t*, 2> kHelpLoopKeywordLines = {
    L"  Loop-control keyword recognized inside while, until, for, and select blocks.\n",
    L"  Use an optional positive integer to target outer loops.\n"
};

template <size_t N>
static bool write_help_lines(const std::function<bool(const std::wstring&)>& write_output, const std::array<const wchar_t*, N>& lines) {
    for (const wchar_t* line : lines) {
        if (!write_output(line)) {
            return false;
        }
    }
    return true;
}

static bool write_grouped_builtin_help(const std::function<bool(const std::wstring&)>& write_output, bool leading_blank_line_per_group) {
    const std::vector<BuiltinCommandGroup>& groups = builtin_command_help_groups();
    bool first_group = true;
    for (const BuiltinCommandGroup& group : groups) {
        if (leading_blank_line_per_group || !first_group) {
            if (!write_output(L"\n")) {
                return false;
            }
        }
        first_group = false;

        if (!write_output(std::wstring(group.title) + L":\n")) {
            return false;
        }

        for (const wchar_t* name : group.names) {
            if (const BuiltinCommandHelp* entry = find_builtin_command_help(name)) {
                if (!write_output(std::wstring(L"  ") + entry->name + L" - " + entry->description + L"\n")) {
                    return false;
                }
            }
        }
    }
    if (!write_output(L"\n")) {
        return false;
    }
    return true;
}

const std::vector<std::wstring>& builtin_commands() {
    static std::vector<std::wstring> commands;
    if (commands.empty()) {
        const std::vector<BuiltinCommandHelp>& entries = builtin_command_help_entries();
        commands.reserve(entries.size());
        for (const BuiltinCommandHelp& entry : entries) {
            commands.push_back(entry.name);
        }
    }
    return commands;
}

static void write_help_topic_index(const std::function<bool(const std::wstring&)>& write_output) {
    write_output(L"CrossShellKSH help\n\n");
    write_output(L"Use one of these forms:\n");
    write_output(L"  help                         Show this topic index and command groups.\n");
    write_output(L"  help COMMAND                 Show help for a builtin or shell function.\n");
    write_output(L"  help TOPIC                   Show a help section.\n");
    write_output(L"  help all                     Show the complete reference.\n\n");
    write_output(L"Topics:\n");
    write_output(L"  commands, builtins           Builtin commands grouped by purpose.\n");
    write_output(L"  control, flow                if, for, while, until, case, and select.\n");
    write_output(L"  syntax, expansion            Pipelines, redirection, substitution, and status.\n");
    write_output(L"  scripts, runtime             Scripts, profiles, functions, and limits.\n");
    write_output(L"  utilities, external          External helpers such as suspend and ulimit.\n");
    write_output(L"  paths, cd                    Windows and Unix-style path handling.\n");
    write_output(L"  prompt, ps1                  Prompt tokens and .kshrc customization.\n");
    write_output(L"  vi, editing                  Vi-style interactive line editing.\n");
    write_output(L"  help, overview               Explain help navigation.\n\n");
    write_output(L"Examples:\n");
    write_output(L"  help cd                      Help for the cd builtin.\n");
    write_output(L"  help syntax                  Shell syntax and expansion.\n");
    write_output(L"  help prompt                  Prompt customization.\n");
    write_output(L"  ksh --help                   Complete startup reference.\n");
}

bool write_help_topic(const std::wstring& raw_topic, const std::function<bool(const std::wstring&)>& write_output) {
    const std::wstring topic = to_lower_copy(raw_topic);
    if (topic == L"help" || topic == L"overview" || topic == L"index" || topic == L"topics") {
        write_help_topic_index(write_output);
        return true;
    }
    if (topic == L"commands" || topic == L"builtins" || topic == L"builtin") {
        write_output(L"Builtin commands:\n\n");
        return write_grouped_builtin_help(write_output, false);
    }
    if (topic == L"control" || topic == L"flow" || topic == L"keywords") {
        return write_help_lines(write_output, kHelpControlFlowKeywordLines);
    }
    if (topic == L"syntax" || topic == L"expansion" || topic == L"redirection" || topic == L"shell") {
        return write_help_lines(write_output, kHelpShellSyntaxLines);
    }
    if (topic == L"scripts" || topic == L"runtime" || topic == L"startup") {
        return write_help_lines(write_output, kHelpRuntimeBehaviorLines);
    }
    if (topic == L"utilities" || topic == L"external") {
        return write_help_lines(write_output, kHelpExternalUtilitiesLines);
    }
    if (topic == L"paths" || topic == L"path" || topic == L"cd") {
        return write_help_lines(write_output, kHelpPathHandlingLines);
    }
    if (topic == L"prompt" || topic == L"ps1") {
        return write_help_lines(write_output, kHelpPromptCustomizationLines) &&
            write_help_lines(write_output, kHelpPromptCustomizationShortLines);
    }
    if (topic == L"vi" || topic == L"editing" || topic == L"line-editing") {
        return write_help_lines(write_output, kHelpViModeLines);
    }
    return false;
}

void write_full_help_text(const std::function<bool(const std::wstring&)>& write_output) {
    auto write = [&](const std::wstring& text) -> bool {
        return write_output(text);
    };

    write(L"Usage:\n");
    write(L"  ksh [options] [script [args...]]\n");
    write(L"  ksh -c COMMAND [name [args...]]\n");
    write(L"  ksh --script-test SCRIPT [args...]\n");
    write(L"  ksh -- [script [args...]]\n");
    write(L"\nOptions:\n");
    write(L"  -h, --help       Show this help text and list built-in commands.\n");
    write(L"  --version        Show version information.\n");
    write(L"  --no-profile     Skip loading the startup profile.\n");
    write(L"  --profile FILE   Load the specified startup profile.\n");
    write(L"  -c COMMAND       Execute COMMAND, then exit with its status.\n");
    write(L"  --script-test SCRIPT  Run SCRIPT without a startup profile and report PASS or FAIL.\n");
    write(L"  --               End options; treat remaining arguments as script and arguments.\n");

    write(L"\nBuilt-in commands:\n");
    if (!write_grouped_builtin_help(write, true)) return;
    if (!write_help_lines(write, kHelpControlFlowKeywordLines)) return;
    if (!write_help_lines(write, kHelpShellSyntaxLines)) return;
    if (!write_help_lines(write, kHelpRuntimeBehaviorLines)) return;
    if (!write_help_lines(write, kHelpExternalUtilitiesLines)) return;
    if (!write_help_lines(write, kHelpPathHandlingLines)) return;
    if (!write_help_lines(write, kHelpPromptCustomizationLines)) return;
    if (!write_help_lines(write, kHelpPromptCustomizationShortLines)) return;
    write_help_lines(write, kHelpViModeLines);
}

void print_help_text() {
    auto write_stdout = [](const std::wstring& text) -> bool {
        std::wcout << text;
        return true;
    };
    write_full_help_text(write_stdout);
}

bool execute_builtin_help(const std::vector<std::wstring>& tokens, const std::function<bool(const std::wstring&)>& write_output) {
    if (tokens.size() > 2) {
        std::wcerr << L"ksh: help: too many arguments. Usage: help [command]\n";
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    if (tokens.size() == 2) {
        std::wstring target = tokens[1];

        if (target == L"cd") {
            if (!write_help_lines(write_output, kHelpCdCommandLines) || !write_help_lines(write_output, kHelpCdCommandLinesTail)) {
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        if (target == L"kill") {
            if (!write_help_lines(write_output, kHelpKillCommandLines) || !write_help_lines(write_output, kHelpKillCommandLinesTail)) {
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        if (target == L"prompt" || target == L"PS1") {
            if (!write_help_lines(write_output, kHelpPromptCommandLines) || !write_help_lines(write_output, kHelpPromptCommandLinesTail)) {
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        if (target == L"break" || target == L"continue") {
            if (!write_output(target + L"\n") || !write_help_lines(write_output, kHelpLoopKeywordLines)) {
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        const std::wstring lowered_target = to_lower_copy(target);
        if (lowered_target == L"all" || lowered_target == L"full" || lowered_target == L"reference") {
            write_full_help_text(write_output);
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        if (write_help_topic(target, write_output)) {
            ksh_env.variables[L"?"] = L"0";
            return true;
        }
        
        if (const BuiltinCommandHelp* entry = find_builtin_command_help(target)) {
            write_output(std::wstring(entry->name) + L" - " + entry->description + L"\n");
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        std::map<std::wstring, ShellFunctionDefinition>::const_iterator fn_it = g_shell_functions.find(target);
        if (fn_it != g_shell_functions.end()) {
            std::wstring body = L"function " + target + L"\n{\n";
            for (const auto& line : fn_it->second.body_lines) {
                body += L"    " + line + L"\n";
            }
            body += L"}\n";
            write_output(body);
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        std::wcerr << L"ksh: help: no help found for: " << target << L"\n";
        ksh_env.variables[L"?"] = L"1";
        return false;
    }
    write_output(L"\n");
    write_help_topic_index(write_output);
    write_output(L"\nCrossShellKSH Commands:\n");
    if (!write_grouped_builtin_help(write_output, false)) {
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    write_output(L"\nPrompt customization:\n");
    write_output(L"  Set PS1 in ~/.kshrc to customize prompt order/content.\n");
    write_output(L"  Tokens: %u user, %d domain, %w cwd, %m host, %# role-char, %% literal-percent\n");
    if (!write_help_lines(write_output, kHelpPromptCustomizationShortLines)) {
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    write_output(L"\n");
    
    ksh_env.variables[L"?"] = L"0";
    return true;
}

// -----------------------------------------------------------------------------
// Command Resolution & Names
// -----------------------------------------------------------------------------

bool resolve_external_command_path(const std::wstring& command_name, std::wstring& resolved_path) {
    resolved_path.clear();
    if (command_name.empty()) {
        return false;
    }

    if (command_name.find_first_of(L"/\\") != std::wstring::npos) {
        DWORD attrs = GetFileAttributesW(command_name.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            resolved_path = command_name;
            return true;
        }
        return false;
    }

    auto hash_it = g_command_hash_table.find(command_name);
    if (hash_it != g_command_hash_table.end()) {
        DWORD attrs = GetFileAttributesW(hash_it->second.path.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            hash_it->second.hits++;
            resolved_path = hash_it->second.path;
            return true;
        } else {
            g_command_hash_table.erase(hash_it);
        }
    }

    wchar_t buffer[MAX_PATH];
    DWORD length = SearchPathW(nullptr, command_name.c_str(), nullptr, MAX_PATH, buffer, nullptr);
    if (length > 0 && length < MAX_PATH) {
        resolved_path = buffer;
        g_command_hash_table[command_name] = { resolved_path, 1 };
        return true;
    }

    std::wstring pathext = get_system_env_var(L"PATHEXT");
    if (pathext.empty()) {
        pathext = L".COM;.EXE;.BAT;.CMD";
    }

    std::vector<std::wstring> extensions;
    size_t ext_start = 0;
    while (ext_start < pathext.size()) {
        size_t semi = pathext.find(L';', ext_start);
        std::wstring ext = (semi != std::wstring::npos) ? pathext.substr(ext_start, semi - ext_start) : pathext.substr(ext_start);
        ext = trim_copy(ext);
        if (!ext.empty()) {
            if (ext[0] != L'.') {
                ext = L"." + ext;
            }
            extensions.push_back(to_lower_copy(ext));
        }
        if (semi == std::wstring::npos) break;
        ext_start = semi + 1;
    }
    if (extensions.empty()) {
        extensions = { L".com", L".exe", L".bat", L".cmd" };
    }

    for (const auto& ext : extensions) {
        length = SearchPathW(nullptr, command_name.c_str(), ext.c_str(), MAX_PATH, buffer, nullptr);
        if (length > 0 && length < MAX_PATH) {
            resolved_path = buffer;
            g_command_hash_table[command_name] = { resolved_path, 1 };
            return true;
        }
    }

    return false;
}

CommandResolutionKind resolve_command_kind(const std::wstring& name, std::wstring& detail) {
    detail.clear();
    if (name.empty()) {
        return CommandResolutionKind::Missing;
    }

    std::map<std::wstring, std::wstring>::const_iterator alias_it = g_aliases.find(name);
    if (alias_it != g_aliases.end()) {
        detail = alias_it->second;
        return CommandResolutionKind::Alias;
    }

    if (g_shell_functions.find(name) != g_shell_functions.end()) {
        return CommandResolutionKind::Function;
    }

    const std::vector<std::wstring>& builtins = builtin_commands();
    if (std::find(builtins.begin(), builtins.end(), name) != builtins.end()) {
        return CommandResolutionKind::Builtin;
    }

    std::wstring external_path;
    if (resolve_external_command_path(name, external_path)) {
        detail = external_path;
        return CommandResolutionKind::External;
    }

    return CommandResolutionKind::Missing;
}

std::wstring command_kind_description(const std::wstring& name, bool verbose, bool ksh_style) {
    std::wstring detail;
    CommandResolutionKind kind = resolve_command_kind(name, detail);

    if (ksh_style) {
        if (kind == CommandResolutionKind::Alias) {
            return name + L" is an alias for " + detail;
        }
        if (kind == CommandResolutionKind::Function) {
            return name + L" is a function";
        }
        if (kind == CommandResolutionKind::Builtin) {
            return name + L" is a shell builtin";
        }
        if (kind == CommandResolutionKind::External) {
            return name + L" is " + detail;
        }
        return name + L" not found";
    }

    if (verbose) {
        if (kind == CommandResolutionKind::Alias) {
            return name + L"\talias\t" + detail;
        }
        if (kind == CommandResolutionKind::Function) {
            return name + L"\tfunction";
        }
        if (kind == CommandResolutionKind::Builtin) {
            return name + L"\tbuiltin";
        }
        if (kind == CommandResolutionKind::External) {
            return name + L"\texternal\t" + detail;
        }
        return name + L"\tnot found";
    }

    if (kind == CommandResolutionKind::Alias) {
        return L"alias";
    }
    if (kind == CommandResolutionKind::Function) {
        return L"function";
    }
    if (kind == CommandResolutionKind::Builtin) {
        return L"builtin";
    }
    if (kind == CommandResolutionKind::External) {
        return detail;
    }
    return L"";
}

std::wstring get_command_name(const std::wstring& full_command) {
    size_t start = full_command.find_first_not_of(L" \t\r\n");
    if (start == std::wstring::npos) return L"";
    size_t end = full_command.find_first_of(L" \t\r\n", start);
    if (end == std::wstring::npos) return full_command.substr(start);
    return full_command.substr(start, end - start);
}

bool is_ksh_builtin_command(const std::wstring& cmd) {
    const std::vector<std::wstring>& list = builtin_commands();
    return std::find(list.begin(), list.end(), cmd) != list.end();
}

bool is_valid_builtin_name(const std::wstring& name) {
    return is_ksh_builtin_command(name);
}

bool is_alias_defined(const std::wstring& name) {
    return g_aliases.find(name) != g_aliases.end();
}

bool initialize_default_shell_aliases() {
    if (g_main_aliases.empty()) {
        g_main_aliases[L"integer"] = L"typeset -i";
        g_main_aliases[L"float"] = L"typeset -E";
        g_main_aliases[L"nameref"] = L"typeset -n";
        g_main_aliases[L"functions"] = L"typeset -f";
        g_main_aliases[L"autoload"] = L"typeset -fu";
        g_main_aliases[L"compound"] = L"typeset -C";
        g_main_aliases[L"history"] = L"fc -l";
        g_main_aliases[L"r"] = L"fc -s";
        g_main_aliases[L"type"] = L"whence -v";
    }
    return true;
}

// -----------------------------------------------------------------------------
// Conditional Testing: test, [, [[
// -----------------------------------------------------------------------------

bool extract_ksh_conditional_block(const std::wstring& expanded_input, std::wstring& condition, std::wstring& error_message) {
    condition.clear();
    error_message.clear();

    size_t start = expanded_input.find(L"[[");
    if (start == std::wstring::npos) {
        error_message = L"missing opening [[";
        return false;
    }

    bool in_single_quotes = false;
    bool in_double_quotes = false;
    bool escaped = false;
    size_t i = start + 2;
    size_t closing = std::wstring::npos;

    while (i < expanded_input.size()) {
        wchar_t ch = expanded_input[i];

        if (escaped) {
            escaped = false;
            i++;
            continue;
        }

        if (ch == L'\\' && !in_single_quotes) {
            escaped = true;
            i++;
            continue;
        }

        if (ch == L'\'' && !in_double_quotes) {
            in_single_quotes = !in_single_quotes;
            i++;
            continue;
        }

        if (ch == L'"' && !in_single_quotes) {
            in_double_quotes = !in_double_quotes;
            i++;
            continue;
        }

        if (!in_single_quotes && !in_double_quotes && ch == L']' && (i + 1) < expanded_input.size() && expanded_input[i + 1] == L']') {
            closing = i;
            break;
        }

        i++;
    }

    if (closing == std::wstring::npos) {
        error_message = L"missing closing ]]";
        return false;
    }

    condition = trim_copy(expanded_input.substr(start + 2, closing - (start + 2)));

    std::wstring trailing = trim_copy(expanded_input.substr(closing + 2));
    if (!trailing.empty()) {
        error_message = L"unexpected tokens after ]]";
        return false;
    }

    return true;
}

bool tokenize_ksh_conditional(const std::wstring& condition, std::vector<std::wstring>& tokens, std::wstring& error_message) {
    tokens.clear();
    error_message.clear();

    std::wstring current;
    bool in_single_quotes = false;
    bool in_double_quotes = false;
    bool escaped = false;
    bool in_token = false;

    auto flush_current = [&]() {
        if (in_token) {
            tokens.push_back(current);
            current.clear();
            in_token = false;
        }
    };

    for (size_t i = 0; i < condition.size(); ) {
        wchar_t ch = condition[i];

        if (escaped) {
            current += ch;
            in_token = true;
            escaped = false;
            i++;
            continue;
        }

        if (ch == L'\\' && !in_single_quotes) {
            escaped = true;
            i++;
            continue;
        }

        if (ch == L'$' && i + 1 < condition.length() && condition[i + 1] == L'\'' && !in_double_quotes && !in_single_quotes) {
            current += expand_ansi_c_quoting(condition, i);
            in_token = true;
            continue;
        }

        if (ch == L'\'' && !in_double_quotes) {
            in_single_quotes = !in_single_quotes;
            in_token = true;
            i++;
            continue;
        }

        if (ch == L'"' && !in_single_quotes) {
            in_double_quotes = !in_double_quotes;
            in_token = true;
            i++;
            continue;
        }

        if (!in_single_quotes && !in_double_quotes && std::iswspace(ch)) {
            flush_current();
            i++;
            continue;
        }

        current += ch;
        in_token = true;
        i++;
    }

    if (escaped) {
        error_message = L"unfinished escape sequence";
        return false;
    }
    if (in_single_quotes || in_double_quotes) {
        error_message = L"unterminated quoted string";
        return false;
    }

    flush_current();
    return true;
}

static bool evaluate_file_unary_operator(const std::wstring& op, const std::wstring& path, bool& result) {
    result = false;

    DWORD attrs = GetFileAttributesW(path.c_str());
    const bool exists = attrs != INVALID_FILE_ATTRIBUTES;

    if (op == L"-e" || op == L"-a") {
        result = exists;
        return true;
    }

    if (op == L"-f") {
        result = exists && ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0);
        return true;
    }

    if (op == L"-d") {
        result = exists && ((attrs & FILE_ATTRIBUTE_DIRECTORY) != 0);
        return true;
    }

    if (op == L"-h" || op == L"-L") {
        result = exists && ((attrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0);
        return true;
    }

    if (op == L"-s") {
        if (!exists || ((attrs & FILE_ATTRIBUTE_DIRECTORY) != 0)) {
            result = false;
            return true;
        }

        WIN32_FILE_ATTRIBUTE_DATA file_data;
        if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &file_data)) {
            result = false;
            return true;
        }

        ULONGLONG size = (static_cast<ULONGLONG>(file_data.nFileSizeHigh) << 32) | file_data.nFileSizeLow;
        result = size > 0;
        return true;
    }

    if (op == L"-r") {
        result = _waccess(path.c_str(), 4) == 0;
        return true;
    }

    if (op == L"-w") {
        result = _waccess(path.c_str(), 2) == 0;
        return true;
    }

    if (op == L"-x") {
        result = _waccess(path.c_str(), 0) == 0;
        return true;
    }

    return false;
}

static bool evaluate_file_comparison_operator(const std::wstring& left, const std::wstring& op, const std::wstring& right, bool& result, std::wstring& error_message) {
    error_message.clear();

    if (op == L"-ef") {
        wchar_t left_full[MAX_PATH];
        wchar_t right_full[MAX_PATH];
        DWORD left_len = GetFullPathNameW(left.c_str(), MAX_PATH, left_full, nullptr);
        DWORD right_len = GetFullPathNameW(right.c_str(), MAX_PATH, right_full, nullptr);
        if (left_len == 0 || right_len == 0) {
            result = false;
            return true;
        }
        result = _wcsicmp(left_full, right_full) == 0;
        return true;
    }

    if (op == L"-nt" || op == L"-ot") {
        WIN32_FILE_ATTRIBUTE_DATA left_data;
        WIN32_FILE_ATTRIBUTE_DATA right_data;
        if (!GetFileAttributesExW(left.c_str(), GetFileExInfoStandard, &left_data) ||
            !GetFileAttributesExW(right.c_str(), GetFileExInfoStandard, &right_data)) {
            result = false;
            return true;
        }

        LONG cmp = CompareFileTime(&left_data.ftLastWriteTime, &right_data.ftLastWriteTime);
        result = (op == L"-nt") ? (cmp > 0) : (cmp < 0);
        return true;
    }

    return false;
}

static bool evaluate_ksh_simple_condition(const std::vector<std::wstring>& tokens, size_t begin, size_t end, bool& result, std::wstring& error_message) {
    error_message.clear();
    result = false;

    if (begin >= end) {
        error_message = L"missing conditional expression";
        return false;
    }

    size_t negate_count = 0;
    while (begin < end && tokens[begin] == L"!") {
        negate_count++;
        begin++;
    }

    if (begin >= end) {
        error_message = L"missing conditional expression after !";
        return false;
    }

    size_t count = end - begin;

    if (count == 1) {
        result = !tokens[begin].empty();
    } else if (count == 2) {
        const std::wstring op = tokens[begin];
        const std::wstring operand = tokens[begin + 1];
        if (op == L"-n") {
            result = !operand.empty();
        } else if (op == L"-z") {
            result = operand.empty();
        } else if (evaluate_file_unary_operator(op, operand, result)) {
            // File test operator evaluated.
        } else {
            error_message = L"unsupported unary operator: " + op;
            return false;
        }
    } else if (count == 3) {
        const std::wstring left = tokens[begin];
        const std::wstring op = tokens[begin + 1];
        const std::wstring right = tokens[begin + 2];

        if (op == L"==" || op == L"=") {
            result = match_glob_pattern(right, left);
        } else if (op == L"!=") {
            result = !match_glob_pattern(right, left);
        } else if (op == L"=~") {
            if (!is_regex_enabled()) {
                error_message = L"regex matching is disabled";
                return false;
            }
            if (right.size() > kMaxRegexPatternLength) {
                error_message = L"regex pattern exceeds limit";
                return false;
            }
            if (left.size() > kMaxRegexInputLength) {
                error_message = L"regex input exceeds limit";
                return false;
            }
            try {
                std::wregex rx(right);
                std::wsmatch match;
                result = std::regex_search(left, match, rx);
                if (result) {
                    ksh_env.arrays[L".sh.match"].clear();
                    for (size_t group_idx = 0; group_idx < match.size(); ++group_idx) {
                        std::wstring idx_str = std::to_wstring(group_idx);
                        ksh_env.arrays[L".sh.match"][idx_str] = match[group_idx].str();
                    }
                } else {
                    ksh_env.arrays[L".sh.match"].clear();
                }
            } catch (...) {
                ksh_env.arrays[L".sh.match"].clear();
                error_message = L"invalid regex pattern";
                return false;
            }
        } else if (op == L"<") {
            result = left < right;
        } else if (op == L">") {
            result = left > right;
        } else if (op == L"-eq" || op == L"-ne" || op == L"-gt" || op == L"-ge" || op == L"-lt" || op == L"-le") {
            double left_num = 0.0;
            double right_num = 0.0;
            if (!try_parse_strict_double(left, left_num) || !try_parse_strict_double(right, right_num)) {
                error_message = L"numeric comparison requires numeric operands";
                return false;
            }

            if (op == L"-eq") result = (left_num == right_num);
            if (op == L"-ne") result = (left_num != right_num);
            if (op == L"-gt") result = (left_num > right_num);
            if (op == L"-ge") result = (left_num >= right_num);
            if (op == L"-lt") result = (left_num < right_num);
            if (op == L"-le") result = (left_num <= right_num);
        } else {
            bool file_result = false;
            if (evaluate_file_comparison_operator(left, op, right, file_result, error_message)) {
                result = file_result;
            } else {
                error_message = L"unsupported binary operator: " + op;
                return false;
            }
        }
    } else {
        error_message = L"unable to parse conditional expression";
        return false;
    }

    if ((negate_count % 2) == 1) {
        result = !result;
    }
    return true;
}

static bool is_test_open_paren(const std::wstring& token) {
    return token == L"(" || token == L"\\(";
}

static bool is_test_close_paren(const std::wstring& token) {
    return token == L")" || token == L"\\)";
}

static bool is_supported_unary_condition_operator(const std::wstring& op) {
    return op == L"-n" || op == L"-z" || op == L"-e" || op == L"-a" || op == L"-f" || op == L"-d" ||
           op == L"-h" || op == L"-L" || op == L"-s" || op == L"-r" || op == L"-w" || op == L"-x";
}

static bool is_supported_binary_condition_operator(const std::wstring& op) {
    return op == L"==" || op == L"=" || op == L"!=" || op == L"=~" || op == L"<" || op == L">" ||
           op == L"-eq" || op == L"-ne" || op == L"-gt" || op == L"-ge" || op == L"-lt" || op == L"-le" ||
           op == L"-ef" || op == L"-nt" || op == L"-ot";
}

static bool evaluate_test_condition_or(const std::vector<std::wstring>& tokens, size_t& index, bool& result, std::wstring& error_message);

static bool evaluate_test_condition_primary(const std::vector<std::wstring>& tokens, size_t& index, bool& result, std::wstring& error_message) {
    if (index >= tokens.size()) {
        error_message = L"missing conditional expression";
        return false;
    }

    if (is_test_open_paren(tokens[index])) {
        index++;
        if (!evaluate_test_condition_or(tokens, index, result, error_message)) {
            return false;
        }
        if (index >= tokens.size() || !is_test_close_paren(tokens[index])) {
            error_message = L"missing closing parenthesis in test expression";
            return false;
        }
        index++;
        return true;
    }

    if (is_test_close_paren(tokens[index])) {
        error_message = L"unexpected closing parenthesis in test expression";
        return false;
    }

    size_t start = index;
    size_t consume = 1;

    if ((index + 1) < tokens.size() && is_supported_unary_condition_operator(tokens[index])) {
        consume = 2;
    } else if ((index + 2) < tokens.size() && is_supported_binary_condition_operator(tokens[index + 1])) {
        consume = 3;
    }

    if ((start + consume) > tokens.size()) {
        error_message = L"missing operand in conditional expression";
        return false;
    }

    if (!evaluate_ksh_simple_condition(tokens, start, start + consume, result, error_message)) {
        return false;
    }

    index += consume;
    return true;
}

static bool evaluate_test_condition_not(const std::vector<std::wstring>& tokens, size_t& index, bool& result, std::wstring& error_message) {
    if (index < tokens.size() && tokens[index] == L"!") {
        index++;
        if (!evaluate_test_condition_not(tokens, index, result, error_message)) {
            return false;
        }
        result = !result;
        return true;
    }

    return evaluate_test_condition_primary(tokens, index, result, error_message);
}

static bool evaluate_test_condition_and(const std::vector<std::wstring>& tokens, size_t& index, bool& result, std::wstring& error_message) {
    if (!evaluate_test_condition_not(tokens, index, result, error_message)) {
        return false;
    }

    while (index < tokens.size() && tokens[index] == L"-a") {
        index++;
        bool rhs = false;
        if (!evaluate_test_condition_not(tokens, index, rhs, error_message)) {
            if (error_message.empty()) {
                error_message = L"missing expression after -a";
            }
            return false;
        }
        result = result && rhs;
    }

    return true;
}

static bool evaluate_test_condition_or(const std::vector<std::wstring>& tokens, size_t& index, bool& result, std::wstring& error_message) {
    if (!evaluate_test_condition_and(tokens, index, result, error_message)) {
        return false;
    }

    while (index < tokens.size() && tokens[index] == L"-o") {
        index++;
        bool rhs = false;
        if (!evaluate_test_condition_and(tokens, index, rhs, error_message)) {
            if (error_message.empty()) {
                error_message = L"missing expression after -o";
            }
            return false;
        }
        result = result || rhs;
    }

    return true;
}

bool evaluate_test_condition_expression(const std::vector<std::wstring>& tokens, bool& result, std::wstring& error_message) {
    result = false;
    error_message.clear();

    if (tokens.empty()) {
        error_message = L"missing conditional expression";
        return false;
    }

    size_t index = 0;
    if (!evaluate_test_condition_or(tokens, index, result, error_message)) {
        return false;
    }

    if (index != tokens.size()) {
        error_message = L"unexpected token in test expression: " + tokens[index];
        return false;
    }

    return true;
}

bool evaluate_ksh_conditional(const std::wstring& condition_str, bool& value, std::wstring& error_message) {
    value = false;
    error_message.clear();

    std::vector<std::wstring> tokens;
    if (!tokenize_ksh_conditional(condition_str, tokens, error_message)) {
        return false;
    }

    if (tokens.empty()) {
        error_message = L"empty conditional expression";
        return false;
    }

    bool overall_value = false;
    bool have_or_term = false;
    size_t i = 0;

    while (i < tokens.size()) {
        bool and_value = true;
        bool have_and_value = false;

        while (i < tokens.size() && tokens[i] != L"||") {
            size_t segment_start = i;
            while (i < tokens.size() && tokens[i] != L"&&" && tokens[i] != L"||") {
                i++;
            }

            bool segment_value = false;
            if (!evaluate_ksh_simple_condition(tokens, segment_start, i, segment_value, error_message)) {
                return false;
            }

            if (!have_and_value) {
                and_value = segment_value;
                have_and_value = true;
            } else {
                and_value = and_value && segment_value;
            }

            if (i < tokens.size() && tokens[i] == L"&&") {
                i++;
                if (i >= tokens.size()) {
                    error_message = L"dangling && in conditional expression";
                    return false;
                }
            }
        }

        if (!have_and_value) {
            error_message = L"missing expression between logical operators";
            return false;
        }

        if (!have_or_term) {
            overall_value = and_value;
            have_or_term = true;
        } else {
            overall_value = overall_value || and_value;
        }

        if (i < tokens.size() && tokens[i] == L"||") {
            i++;
            if (i >= tokens.size()) {
                error_message = L"dangling || in conditional expression";
                return false;
            }
        }
    }

    value = overall_value;
    return true;
}

// -----------------------------------------------------------------------------
// Formatted Output & System Builtin Execution
// -----------------------------------------------------------------------------

bool format_printf_time_argument(const std::wstring& arg_value, std::wstring& formatted_time) {
    __time64_t timestamp = _time64(nullptr);

    if (!arg_value.empty() && arg_value != L"-1") {
        try {
            timestamp = static_cast<__time64_t>(std::stoll(arg_value, nullptr, 0));
        } catch (...) {
            return false;
        }
    }

    struct tm local_tm;
    if (_localtime64_s(&local_tm, &timestamp) != 0) {
        return false;
    }

    wchar_t time_buffer[128];
    size_t written = wcsftime(time_buffer, _countof(time_buffer), L"%Y-%m-%d %H:%M:%S", &local_tm);
    if (written == 0) {
        return false;
    }

    formatted_time.assign(time_buffer, written);
    return true;
}

bool evaluate_builtin_printf(const std::vector<std::wstring>& tokens, std::wstring& output_str, std::wstring& error_msg) {
    if (tokens.size() < 2) {
        error_msg = L"ksh: printf: usage: printf [-v varname] format [arguments ...]";
        return false;
    }
    
    bool has_var = false;
    std::wstring varname;
    size_t format_idx = 1;
    
    if (tokens[1] == L"-v") {
        if (tokens.size() < 4) {
            error_msg = L"ksh: printf: usage: printf [-v varname] format [arguments ...]";
            return false;
        }
        has_var = true;
        varname = tokens[2];
        format_idx = 3;
    }
    
    std::wstring format = tokens[format_idx];
    size_t arg_idx = format_idx + 1;
    output_str.clear();
    
    bool first_pass = true;
    while (first_pass || arg_idx < tokens.size()) {
        first_pass = false;
        
        size_t last_consumed_arg_idx = arg_idx;
        
        for (size_t i = 0; i < format.size(); ) {
            if (format[i] == L'\\') {
                if (i + 1 < format.size()) {
                    wchar_t next = format[i + 1];
                    if (next == L'n') { output_str += L'\n'; i += 2; }
                    else if (next == L't') { output_str += L'\t'; i += 2; }
                    else if (next == L'r') { output_str += L'\r'; i += 2; }
                    else if (next == L'a') { output_str += L'\a'; i += 2; }
                    else if (next == L'b') { output_str += L'\b'; i += 2; }
                    else if (next == L'f') { output_str += L'\f'; i += 2; }
                    else if (next == L'v') { output_str += L'\v'; i += 2; }
                    else if (next == L'\\') { output_str += L'\\'; i += 2; }
                    else if (next == L'c') {
                        if (has_var) {
                            std::wstring assign_err;
                            if (!assign_parameter_value(varname, output_str, false, false, assign_err)) {
                                error_msg = L"ksh: printf: variable assignment failed: " + assign_err;
                                return false;
                            }
                            output_str.clear();
                        }
                        return true;
                    }
                    else if (next == L'0') {
                        size_t oct_end = i + 2;
                        while (oct_end < format.size() && oct_end < i + 5 && format[oct_end] >= L'0' && format[oct_end] <= L'7') {
                            oct_end++;
                        }
                        if (oct_end > i + 2) {
                            long val = std::wcstol(format.substr(i + 2, oct_end - (i + 2)).c_str(), nullptr, 8);
                            output_str += static_cast<wchar_t>(val);
                            i = oct_end;
                        } else {
                            output_str += L'\0';
                            i += 2;
                        }
                    } else {
                        output_str += next;
                        i += 2;
                    }
                } else {
                    output_str += L'\\';
                    i++;
                }
                continue;
            }
            
            if (format[i] == L'%') {
                if (i + 1 < format.size() && format[i + 1] == L'%') {
                    output_str += L'%';
                    i += 2;
                    continue;
                }
                
                size_t spec_start = i;
                i++;
                
                std::wstring flags;
                while (i < format.size() && (format[i] == L'-' || format[i] == L'+' || format[i] == L' ' || format[i] == L'#' || format[i] == L'0')) {
                    flags += format[i];
                    i++;
                }
                
                std::wstring width_str;
                if (i < format.size() && format[i] == L'*') {
                    if (arg_idx < tokens.size()) {
                        width_str = tokens[arg_idx++];
                    }
                    i++;
                } else {
                    while (i < format.size() && std::iswdigit(format[i])) {
                        width_str += format[i];
                        i++;
                    }
                }
                
                std::wstring prec_str;
                bool has_prec = false;
                if (i < format.size() && format[i] == L'.') {
                    has_prec = true;
                    i++;
                    if (i < format.size() && format[i] == L'*') {
                        if (arg_idx < tokens.size()) {
                            prec_str = tokens[arg_idx++];
                        }
                        i++;
                    } else {
                        while (i < format.size() && std::iswdigit(format[i])) {
                            prec_str += format[i];
                            i++;
                        }
                    }
                }
                
                if (i >= format.size()) {
                    output_str += format.substr(spec_start);
                    break;
                }
                
                wchar_t type_char = format[i];
                i++;
                
                std::wstring cur_arg = L"";
                if (arg_idx < tokens.size()) {
                    cur_arg = tokens[arg_idx++];
                }
                
                std::wstring fmt_spec = L"%" + flags;
                if (!width_str.empty()) fmt_spec += width_str;
                if (has_prec) fmt_spec += L"." + prec_str;
                
                wchar_t buf[1024];
                buf[0] = 0;
                
                if (type_char == L's') {
                    fmt_spec += L"s";
                    _snwprintf_s(buf, _countof(buf), _TRUNCATE, fmt_spec.c_str(), cur_arg.c_str());
                    output_str += buf;
                } else if (type_char == L'q') {
                    std::wstring escaped = escape_for_double_quotes(cur_arg);
                    output_str += escaped;
                } else if (type_char == L'T') {
                    std::wstring formatted_time;
                    if (format_printf_time_argument(cur_arg, formatted_time)) {
                        output_str += formatted_time;
                    } else {
                        output_str += cur_arg;
                    }
                } else if (type_char == L'c') {
                    wchar_t ch_val = cur_arg.empty() ? L'\0' : cur_arg[0];
                    output_str += ch_val;
                } else if (type_char == L'd' || type_char == L'i') {
                    fmt_spec += L"lld";
                    long long val = 0;
                    if (!cur_arg.empty()) {
                        try { val = std::stoll(cur_arg, nullptr, 0); } catch(...) { val = 0; }
                    }
                    _snwprintf_s(buf, _countof(buf), _TRUNCATE, fmt_spec.c_str(), val);
                    output_str += buf;
                } else if (type_char == L'u' || type_char == L'o' || type_char == L'x' || type_char == L'X') {
                    fmt_spec += L"ll";
                    fmt_spec += type_char;
                    unsigned long long val = 0;
                    if (!cur_arg.empty()) {
                        try { val = std::stoull(cur_arg, nullptr, 0); } catch(...) { val = 0; }
                    }
                    _snwprintf_s(buf, _countof(buf), _TRUNCATE, fmt_spec.c_str(), val);
                    output_str += buf;
                } else if (type_char == L'f' || type_char == L'e' || type_char == L'E' || type_char == L'g' || type_char == L'G') {
                    fmt_spec += type_char;
                    double val = 0.0;
                    if (!cur_arg.empty()) {
                        try { val = std::stod(cur_arg); } catch(...) { val = 0.0; }
                    }
                    _snwprintf_s(buf, _countof(buf), _TRUNCATE, fmt_spec.c_str(), val);
                    output_str += buf;
                } else {
                    output_str += format.substr(spec_start, i - spec_start);
                }
                continue;
            }
            
            output_str += format[i];
            i++;
        }
        
        if (arg_idx == last_consumed_arg_idx) {
            break;
        }
    }
    if (has_var) {
        std::wstring assign_err;
        if (!assign_parameter_value(varname, output_str, false, false, assign_err)) {
            error_msg = L"ksh: printf: variable assignment failed: " + assign_err;
            return false;
        }
        output_str.clear();
    }
    return true;
}

std::wstring decode_raw_bytes(const std::string& bytes) {
    if (bytes.empty()) return L"";

    bool is_utf16 = false;
    if (bytes.size() >= 2) {
        if (static_cast<unsigned char>(bytes[0]) == 0xFF && static_cast<unsigned char>(bytes[1]) == 0xFE) {
            is_utf16 = true;
        } else if (bytes.size() % 2 == 0) {
            size_t null_count = 0;
            for (size_t i = 1; i < bytes.size(); i += 2) {
                if (bytes[i] == '\0') null_count++;
            }
            if (null_count == bytes.size() / 2) {
                is_utf16 = true;
            }
        }
    }

    if (is_utf16) {
        std::wstring wstr;
        size_t start = 0;
        if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF && static_cast<unsigned char>(bytes[1]) == 0xFE) {
            start = 2;
        }
        wstr.resize((bytes.size() - start) / 2);
        if (!wstr.empty()) {
            memcpy(&wstr[0], bytes.data() + start, wstr.size() * 2);
        }
        return wstr;
    }

    int wlen = MultiByteToWideChar(CP_UTF8, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
    if (wlen > 0) {
        std::wstring wstr(wlen, 0);
        MultiByteToWideChar(CP_UTF8, 0, bytes.data(), static_cast<int>(bytes.size()), &wstr[0], wlen);
        return wstr;
    }

    int alen = MultiByteToWideChar(CP_ACP, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
    if (alen > 0) {
        std::wstring wstr(alen, 0);
        MultiByteToWideChar(CP_ACP, 0, bytes.data(), static_cast<int>(bytes.size()), &wstr[0], alen);
        return wstr;
    }

    std::wstring wstr;
    for (char c : bytes) {
        wstr.push_back(static_cast<wchar_t>(c));
    }
    return wstr;
}

bool execute_builtin_read(const std::vector<std::wstring>& tokens) {
    std::wstring prompt = L"";
    bool silent = false;
    bool raw = false;
    bool array_mode = false;
    bool use_coprocess_input = false;
    int max_chars = -1;
    double timeout_sec = -1.0;
    wchar_t delim = L'\n';
    std::vector<std::wstring> var_names;

    for (size_t i = 1; i < tokens.size(); ++i) {
        std::wstring arg = tokens[i];
        if (arg.size() > 1 && arg[0] == L'-') {
            for (size_t j = 1; j < arg.size(); ++j) {
                wchar_t flag = arg[j];
                bool consumed_option_argument = false;
                if (flag == L'r') {
                    raw = true;
                } else if (flag == L's') {
                    silent = true;
                } else if (flag == L'A') {
                    array_mode = true;
                } else if (flag == L'p') {
                    const bool has_attached_prompt =
                        j + 1 < arg.size() &&
                        arg[j + 1] != L'r' && arg[j + 1] != L's' && arg[j + 1] != L'A' &&
                        arg[j + 1] != L'p' && arg[j + 1] != L'n' && arg[j + 1] != L't' &&
                        arg[j + 1] != L'd';
                    if (!has_attached_prompt) {
                        if (!g_coprocess.active) {
                            std::wcerr << L"ksh: read -p: no active co-process\n";
                            ksh_env.variables[L"?"] = L"1";
                            return false;
                        }
                        use_coprocess_input = true;
                    } else if (j + 1 < arg.size()) {
                        prompt = arg.substr(j + 1);
                        consumed_option_argument = true;
                    } else if (i + 1 < tokens.size()) {
                        prompt = tokens[++i];
                        consumed_option_argument = true;
                    } else {
                        std::wcerr << L"ksh: read: -p requires an argument\n";
                        ksh_env.variables[L"?"] = L"1";
                        return false;
                    }
                } else if (flag == L'n') {
                    std::wstring n_val;
                    if (j + 1 < arg.size()) {
                        n_val = arg.substr(j + 1);
                        consumed_option_argument = true;
                    } else if (i + 1 < tokens.size()) {
                        n_val = tokens[++i];
                        consumed_option_argument = true;
                    } else {
                        std::wcerr << L"ksh: read: -n requires an argument\n";
                        ksh_env.variables[L"?"] = L"1";
                        return false;
                    }
                    if (!try_parse_int_strict(n_val, max_chars)) {
                        std::wcerr << L"ksh: read: invalid nchars\n";
                        ksh_env.variables[L"?"] = L"1";
                        return false;
                    }
                } else if (flag == L't') {
                    std::wstring t_val;
                    if (j + 1 < arg.size()) {
                        t_val = arg.substr(j + 1);
                        consumed_option_argument = true;
                    } else if (i + 1 < tokens.size()) {
                        t_val = tokens[++i];
                        consumed_option_argument = true;
                    } else {
                        std::wcerr << L"ksh: read: -t requires an argument\n";
                        ksh_env.variables[L"?"] = L"1";
                        return false;
                    }
                    try {
                        timeout_sec = std::stod(t_val);
                    } catch (...) {
                        std::wcerr << L"ksh: read: invalid timeout\n";
                        ksh_env.variables[L"?"] = L"1";
                        return false;
                    }
                } else if (flag == L'd') {
                    std::wstring d_val;
                    if (j + 1 < arg.size()) {
                        d_val = arg.substr(j + 1);
                        consumed_option_argument = true;
                    } else if (i + 1 < tokens.size()) {
                        d_val = tokens[++i];
                        consumed_option_argument = true;
                    } else {
                        std::wcerr << L"ksh: read: -d requires an argument\n";
                        ksh_env.variables[L"?"] = L"1";
                        return false;
                    }
                    if (!d_val.empty()) {
                        delim = d_val[0];
                    }
                } else {
                    std::wcerr << L"ksh: read: invalid option: -" << flag << L"\n";
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
                if (consumed_option_argument) {
                    break;
                }
            }
        } else {
            var_names.push_back(arg);
        }
    }

    if (var_names.empty()) {
        if (array_mode) {
            std::wcerr << L"ksh: read: -A requires an array name\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        var_names.push_back(L"REPLY");
    }

    if (array_mode && var_names.size() != 1) {
        std::wcerr << L"ksh: read: -A accepts exactly one array name\n";
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    for (const auto& name : var_names) {
        if (!is_valid_shell_identifier(name)) {
            std::wcerr << L"ksh: read: invalid variable name: " << name << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
    }

    if (!prompt.empty() && !use_coprocess_input) {
        std::wcerr << prompt;
        std::wcerr.flush();
    }

    std::wstring result;
    int read_status = 0;
    HANDLE hIn = use_coprocess_input
        ? g_coprocess.output_read
        : ((g_pipeline_stdin != INVALID_HANDLE_VALUE) ? g_pipeline_stdin : GetStdHandle(STD_INPUT_HANDLE));
    DWORD mode;
    bool is_console = !use_coprocess_input && GetConsoleMode(hIn, &mode) != FALSE;

    auto start_time = std::chrono::steady_clock::now();
    bool escape_next = false;

    if (is_console) {
        DWORD origMode = 0;
        bool mode_set = false;
        if (GetConsoleMode(hIn, &origMode)) {
            DWORD newMode = origMode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
            if (SetConsoleMode(hIn, newMode)) {
                mode_set = true;
            }
        }

        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);

        while (true) {
            if (max_chars > 0 && static_cast<int>(result.length()) >= max_chars) {
                break;
            }

            DWORD wait_ms = INFINITE;
            if (timeout_sec >= 0.0) {
                auto now = std::chrono::steady_clock::now();
                double elapsed = std::chrono::duration<double>(now - start_time).count();
                double remaining = timeout_sec - elapsed;
                if (remaining <= 0.0) {
                    read_status = 142;
                    break;
                }
                wait_ms = static_cast<DWORD>(remaining * 1000.0);
            }

            DWORD wait_res = WaitForSingleObject(hIn, wait_ms);
            if (wait_res == WAIT_TIMEOUT) {
                read_status = 142;
                break;
            }
            if (wait_res != WAIT_OBJECT_0) {
                read_status = 1;
                break;
            }

            INPUT_RECORD ir;
            DWORD records_read = 0;
            if (!ReadConsoleInputW(hIn, &ir, 1, &records_read) || records_read == 0) {
                read_status = 1;
                break;
            }

            if (ir.EventType != KEY_EVENT || !ir.Event.KeyEvent.bKeyDown) {
                continue;
            }

            wchar_t ch = ir.Event.KeyEvent.uChar.UnicodeChar;
            WORD vk = ir.Event.KeyEvent.wVirtualKeyCode;

            if (ch == 0) continue;
            if (ch == 3) {
                read_status = 130;
                break;
            }
            if (ch == 26) {
                break;
            }

            if (vk == VK_BACK || ch == L'\b') {
                if (!result.empty()) {
                    result.pop_back();
                    if (!silent) {
                        DWORD written;
                        WriteConsoleW(hOut, L"\b \b", 3, &written, NULL);
                    }
                }
                continue;
            }

            if (escape_next && (ch == L'\r' || ch == L'\n')) {
                escape_next = false;
                if (!silent) {
                    DWORD written;
                    WriteConsoleW(hOut, L"\r\n> ", 4, &written, NULL);
                }
                continue;
            }

            bool is_delim = false;
            if (delim == L'\n' && (ch == L'\r' || ch == L'\n')) {
                is_delim = true;
            } else if (ch == delim) {
                is_delim = true;
            }

            if (is_delim) {
                if (!silent) {
                    DWORD written;
                    WriteConsoleW(hOut, L"\r\n", 2, &written, NULL);
                }
                break;
            }

            if (!raw && !escape_next && ch == L'\\') {
                escape_next = true;
                continue;
            }

            escape_next = false;
            result.push_back(ch);

            if (!silent) {
                DWORD written;
                WriteConsoleW(hOut, &ch, 1, &written, NULL);
            }
        }

        if (mode_set) {
            SetConsoleMode(hIn, origMode);
        }
    } else {
        std::string raw_bytes;
        while (true) {
            if (max_chars > 0 && static_cast<int>(raw_bytes.length()) >= max_chars) {
                break;
            }

            if (timeout_sec >= 0.0) {
                auto now = std::chrono::steady_clock::now();
                double elapsed = std::chrono::duration<double>(now - start_time).count();
                if (elapsed >= timeout_sec) {
                    read_status = 142;
                    break;
                }

                DWORD avail = 0;
                if (PeekNamedPipe(hIn, NULL, 0, NULL, &avail, NULL) && avail == 0) {
                    Sleep(10);
                    continue;
                }
            }

            char c_char;
            DWORD bytesRead = 0;
            if (!ReadFile(hIn, &c_char, 1, &bytesRead, NULL) || bytesRead == 0) {
                if (use_coprocess_input) {
                    if (!raw_bytes.empty()) {
                        break;
                    }

                    DWORD exit_code = 1;
                    if (g_coprocess.process != INVALID_HANDLE_VALUE) {
                        GetExitCodeProcess(g_coprocess.process, &exit_code);
                    }
                    close_coprocess(false);
                    ksh_env.variables[L"COPROC_ACTIVE"] = L"0";
                    ksh_env.variables[L"?"] = std::to_wstring(exit_code == STILL_ACTIVE ? 1 : exit_code);
                    return false;
                }
                read_status = 1;
                break;
            }

            raw_bytes.push_back(c_char);

            bool is_delim = false;
            if (delim == L'\n') {
                if (c_char == '\n') {
                    is_delim = true;
                }
            } else {
                if (static_cast<wchar_t>(c_char) == delim) {
                    is_delim = true;
                }
            }

            if (is_delim) {
                if (raw_bytes.size() >= 2 && raw_bytes[raw_bytes.size() - 2] == '\n' && raw_bytes.back() == '\0') {
                    // Handled
                } else if (delim == L'\n') {
                    char next_c;
                    DWORD peekRead = 0;
                    if (PeekNamedPipe(hIn, &next_c, 1, &peekRead, NULL, NULL) && peekRead > 0 && next_c == '\0') {
                        ReadFile(hIn, &next_c, 1, &peekRead, NULL);
                        raw_bytes.push_back(next_c);
                    }
                }
                break;
            }
        }
        result = decode_raw_bytes(raw_bytes);
    }

    if (read_status != 0 && read_status != 130) {
        ksh_env.variables[L"?"] = std::to_wstring(read_status);
        return false;
    }
    if (read_status == 130) {
        ksh_env.variables[L"?"] = L"130";
        return false;
    }

    std::wstring ifs = effective_ifs_value();
    auto is_ifs = [&](wchar_t ch) {
        return ifs.find(ch) != std::wstring::npos;
    };

    std::vector<std::wstring> words;
    std::wstring current_word;
    size_t char_idx = 0;

    while (char_idx < result.length() && is_ifs(result[char_idx])) {
        char_idx++;
    }

    while (char_idx < result.length()) {
        if (words.size() + 1 == var_names.size()) {
            std::wstring remainder = result.substr(char_idx);
            while (!remainder.empty() && is_ifs(remainder.back())) {
                remainder.pop_back();
            }
            words.push_back(remainder);
            break;
        }

        if (is_ifs(result[char_idx])) {
            words.push_back(current_word);
            current_word.clear();
            while (char_idx < result.length() && is_ifs(result[char_idx])) {
                char_idx++;
            }
        } else {
            current_word.push_back(result[char_idx]);
            char_idx++;
        }
    }
    if (words.size() < var_names.size() && !current_word.empty()) {
        words.push_back(current_word);
    }

    while (words.size() < var_names.size()) {
        words.push_back(L"");
    }

    if (array_mode) {
        const std::wstring& array_name = var_names[0];
        std::wstring resolved_array_name = resolve_variable_name(array_name);
        if (is_executing_function_scope()) {
            snapshot_local_variable_if_needed(resolved_array_name);
        }
        if (get_flag_value(ksh_env.readonly_flags, resolved_array_name)) {
            std::wcerr << L"ksh: read: variable is read-only: " << resolved_array_name << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        assign_array_parameter(resolved_array_name, words);
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    bool assign_ok = true;
    for (size_t i = 0; i < var_names.size(); ++i) {
        std::wstring err;
        if (!assign_parameter_value(var_names[i], words[i], false, is_executing_function_scope(), err)) {
            std::wcerr << L"ksh: read: " << err << L"\n";
            assign_ok = false;
        }
    }

    ksh_env.variables[L"?"] = assign_ok ? L"0" : L"1";
    return assign_ok;
}

bool execute_builtin_getconf(const std::vector<std::wstring>& tokens, const std::function<bool(const std::wstring&)>& write_output) {
    if (tokens.size() < 2) {
        std::wcerr << L"getconf: usage: getconf [-v specification] system_var [pathname] or getconf -a\n";
        ksh_env.variables[L"?"] = L"1";
        return true;
    }

    static const std::unordered_map<std::wstring, std::wstring> kStaticGetconfValues = {
        {L"CHAR_BIT", L"8"},
        {L"INT_MAX", L"2147483647"},
        {L"LONG_MAX", L"2147483647"},
        {L"ULONG_MAX", L"4294967295"},
        {L"PAGE_SIZE", L"4096"},
        {L"ARG_MAX", L"32767"},
        {L"PATH_MAX", L"260"}
    };

    auto get_runtime_getconf_value = [](const std::wstring& name, std::wstring& value_out) -> bool {
        if (name == L"PATH") {
            wchar_t windir[MAX_PATH];
            DWORD windir_len = GetEnvironmentVariableW(L"windir", windir, MAX_PATH);
            if (windir_len > 0 && windir_len < MAX_PATH) {
                value_out = std::wstring(windir) + L"\\System32;" + std::wstring(windir);
            } else {
                value_out = L"C:\\Windows\\System32;C:\\Windows";
            }
            return true;
        }

        if (name == L"TMPDIR") {
            wchar_t tmp[MAX_PATH];
            DWORD tmp_len = GetTempPathW(MAX_PATH, tmp);
            if (tmp_len > 0 && tmp_len < MAX_PATH) {
                value_out = tmp;
            } else {
                value_out = L"C:\\Temp";
            }
            return true;
        }

        std::unordered_map<std::wstring, std::wstring>::const_iterator it = kStaticGetconfValues.find(name);
        if (it != kStaticGetconfValues.end()) {
            value_out = it->second;
            return true;
        }

        return false;
    };

    static const std::vector<std::wstring> kGetconfListOrder = {
        L"CHAR_BIT",
        L"INT_MAX",
        L"LONG_MAX",
        L"ULONG_MAX",
        L"PAGE_SIZE",
        L"ARG_MAX",
        L"PATH_MAX",
        L"PATH",
        L"TMPDIR"
    };

    if (tokens[1] == L"-a") {
        for (const std::wstring& key : kGetconfListOrder) {
            std::wstring value;
            if (!get_runtime_getconf_value(key, value)) {
                continue;
            }
            if (!write_output(key + L" = " + value + L"\n")) {
                ksh_env.variables[L"?"] = L"1";
                return true;
            }
        }
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    std::wstring var = tokens[1];
    std::wstring value;
    if (get_runtime_getconf_value(var, value)) {
        if (!write_output(value + L"\n")) {
            ksh_env.variables[L"?"] = L"1";
            return true;
        }
        ksh_env.variables[L"?"] = L"0";
        return true;
    } else {
        std::wcerr << L"getconf: unknown variable: " << var << L"\n";
        ksh_env.variables[L"?"] = L"1";
        return true;
    }
}

bool execute_builtin_pathchk(const std::vector<std::wstring>& tokens) {
    if (tokens.size() < 2) {
        std::wcerr << L"pathchk: usage: pathchk [-p] [-P] pathname...\n";
        ksh_env.variables[L"?"] = L"1";
        return true;
    }

    bool posix_portable = false;
    bool check_empty_components = false;
    size_t start_idx = 1;

    while (start_idx < tokens.size() && tokens[start_idx].size() > 1 && tokens[start_idx][0] == L'-') {
        std::wstring arg = tokens[start_idx];
        for (size_t j = 1; j < arg.size(); ++j) {
            if (arg[j] == L'p') posix_portable = true;
            else if (arg[j] == L'P') check_empty_components = true;
            else {
                std::wcerr << L"pathchk: invalid option: -" << arg[j] << L"\n";
                ksh_env.variables[L"?"] = L"1";
                return true;
            }
        }
        start_idx++;
    }

    if (start_idx >= tokens.size()) {
        std::wcerr << L"pathchk: missing pathname\n";
        ksh_env.variables[L"?"] = L"1";
        return true;
    }

    bool all_ok = true;
    for (size_t i = start_idx; i < tokens.size(); ++i) {
        std::wstring path = tokens[i];

        if (path.empty()) {
            std::wcerr << L"pathchk: empty pathname is invalid\n";
            all_ok = false;
            continue;
        }

        std::vector<std::wstring> components;
        std::wstring current;
        for (wchar_t ch : path) {
            if (ch == L'\\' || ch == L'/') {
                if (!current.empty()) {
                    components.push_back(current);
                    current.clear();
                } else if (check_empty_components) {
                    std::wcerr << L"pathchk: '" << path << L"': empty component is not portable\n";
                    all_ok = false;
                }
            } else {
                current.push_back(ch);
            }
        }
        if (!current.empty()) {
            components.push_back(current);
        }

        if (posix_portable) {
            if (path.size() > 256) {
                std::wcerr << L"pathchk: '" << path << L"': pathname length exceeds POSIX limit (256)\n";
                all_ok = false;
            }
            for (const auto& comp : components) {
                if (comp.size() > 14) {
                    std::wcerr << L"pathchk: '" << path << L"': component '" << comp << L"' length exceeds POSIX limit (14)\n";
                    all_ok = false;
                }
                for (wchar_t ch : comp) {
                    bool ok_char = (ch >= L'A' && ch <= L'Z') || (ch >= L'a' && ch <= L'z') ||
                                   (ch >= L'0' && ch <= L'9') || ch == L'.' || ch == L'_' || ch == L'-';
                    if (!ok_char) {
                        std::wcerr << L"pathchk: '" << path << L"': character '" << ch << L"' in '" << comp << L"' is not portable\n";
                        all_ok = false;
                    }
                }
            }
        } else {
            if (path.size() > 260) {
                std::wcerr << L"pathchk: '" << path << L"': pathname length exceeds Windows limit (260)\n";
                all_ok = false;
            }
            for (const auto& comp : components) {
                if (comp.size() > 255) {
                    std::wcerr << L"pathchk: '" << path << L"': component '" << comp << L"' length exceeds Windows limit (255)\n";
                    all_ok = false;
                }
                for (size_t char_idx = 0; char_idx < comp.size(); ++char_idx) {
                    wchar_t ch = comp[char_idx];
                    if (ch == L':' && char_idx == 1 && comp.size() == 2 && i == start_idx) {
                        continue;
                    }
                    if (ch == L'<' || ch == L'>' || ch == L':' || ch == L'"' || ch == L'|' || ch == L'?' || ch == L'*') {
                        std::wcerr << L"pathchk: '" << path << L"': component '" << comp << L"' contains invalid character '" << ch << L"'\n";
                        all_ok = false;
                    }
                }
            }
        }
    }

    ksh_env.variables[L"?"] = all_ok ? L"0" : L"1";
    return true;
}
