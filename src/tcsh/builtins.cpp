#include "engine.hpp"
#include "builtins.hpp"

const std::vector<TcshHelpEntry>& tcsh_help_entries() {
    static const std::vector<TcshHelpEntry> help_entries = {
        {":", "Null command; always succeeds."},
        {".", "Read and execute commands from a script file in the current shell."},
        {"@", "Evaluate a simple arithmetic assignment or expression."},
        {"alias", "Create or list command aliases."},
        {"bg", "Resume a stopped/background job."},
        {"builtin", "Force execution of a shell builtin."},
        {"builtins", "List the builtin command set."},
        {"cd", "Change the current directory. / and \\ are both accepted."},
        {"chdir", "Change the current directory. / and \\ are both accepted."},
        {"command", "Run a command without alias expansion."},
        {"complete", "Create or list completion rules."},
        {"continue", "Continue the innermost loop in a script."},
        {"dirs", "Print the directory stack."},
        {"disown", "Remove jobs from the job table without killing them."},
        {"echo", "Print arguments to standard output."},
        {"echotc", "Echo terminal capabilities or ANSI control codes."},
        {"export", "Export variables to the process environment."},
        {"fg", "Bring a background job to the foreground."},
        {"hash", "Show or refresh command lookup information."},
        {"history", "Print the command history buffer."},
        {"jobs", "List tracked background jobs."},
        {"kill", "Terminate a process by PID or job reference."},
        {"let", "Evaluate arithmetic and return success when non-zero."},
        {"ls-F", "List directory contents with file type indicators (/ for dir, * for exe, @ for link)."},
        {"onintr", "Control shell interrupt handling (- to ignore, label to jump)."},
        {"print", "Print arguments to standard output."},
        {"printenv", "Print shell variables and environment values."},
        {"pwd", "Print the current working directory."},
        {"pushd", "Push the current directory and change to another one. / and \\ are both accepted."},
        {"read", "Read a line from standard input into variables."},
        {"rehash", "Enable or refresh command lookup caching."},
        {"repeat", "Run a command repeatedly."},
        {"return", "Stop execution of the current sourced script."},
        {"select", "Prompt for a menu choice and store the selection."},
        {"set", "Assign or list shell variables."},
        {"setenv", "Set an environment variable."},
        {"shift", "Shift positional arguments left."},
        {"source", "Read and execute commands from a script file."},
        {"test", "Evaluate a conditional expression and set status."},
        {"trap", "Install, list, or clear trap handlers."},
        {"typeset", "Declare or assign shell variables."},
        {"unalias", "Remove an alias."},
        {"uncomplete", "Remove a completion rule."},
        {"unset", "Remove a shell variable."},
        {"unsetenv", "Remove an environment variable."},
        {"whence", "Show how a command name resolves."},
        {"where", "Locate a command on disk."},
        {"which", "Locate a command on disk."}
    };
    return help_entries;
}

const std::vector<std::string>& tcsh_builtin_names() {
    static const std::vector<std::string> names = []() {
        std::vector<std::string> out;
        const std::vector<TcshHelpEntry>& help_entries = tcsh_help_entries();
        out.reserve(help_entries.size() + 64);

        for (const auto& entry : help_entries) {
            out.push_back(entry.name);
        }

        static const std::vector<const char*> supplemental_names = {
            "alloc", "bindkey", "break", "breaksw", "case", "clear", "cls", "default",
            "echotc", "else", "end", "endif", "endsw", "eval", "exec", "filetest",
            "foreach", "glob", "goto", "hashstat", "hup", "if", "limit", "log", "login",
            "ls-F", "migrate", "newgrp", "nice", "nohup", "notify", "onintr", "popd",
            "sched", "settc", "setty", "stop", "suspend", "switch", "telltc", "termname",
            "time", "umask", "unhash", "universe", "unlimit", "ver", "wait", "while"
        };

        for (const char* name : supplemental_names) {
            if (std::find(out.begin(), out.end(), name) == out.end()) {
                out.push_back(name);
            }
        }

        return out;
    }();

    return names;
}

bool TcshEngine::isBuiltinCommand(const std::string& command) const {
    return std::find(builtins.begin(), builtins.end(), command) != builtins.end();
}

std::string TcshEngine::expandLeadingAlias(const std::string& line, const std::vector<std::string>& origArgs) const {
    std::vector<std::string> tokens = tokenize(line, ' ');
    if (tokens.empty()) return line;

    auto it = aliases.find(tokens[0]);
    if (it == aliases.end()) return line;

    std::string aliasBody = it->second;

    if (aliasBody.find("\\!") != std::string::npos) {
        std::string result = aliasBody;
        std::string allArgs = "";
        for (size_t i = 1; i < origArgs.size(); ++i) {
            if (i > 1) allArgs += " ";
            allArgs += origArgs[i];
        }
        
        size_t pos;
        while ((pos = result.find("\\!*")) != std::string::npos) result.replace(pos, 3, allArgs);
        while ((pos = result.find("\\!^")) != std::string::npos) result.replace(pos, 3, origArgs.size() > 1 ? origArgs[1] : "");
        while ((pos = result.find("\\!$")) != std::string::npos) result.replace(pos, 3, origArgs.size() > 1 ? origArgs.back() : "");
        
        for (size_t i = 1; i < origArgs.size(); ++i) {
            std::string tag = "\\!:" + std::to_string(i);
            while ((pos = result.find(tag)) != std::string::npos) result.replace(pos, tag.length(), origArgs[i]);
        }
        return result;
    }

    std::string expanded = aliasBody;
    for (size_t i = 1; i < tokens.size(); ++i) {
        expanded += " " + tokens[i];
    }
    return expanded;
}

bool TcshEngine::tryParseNonNegativeInt(const std::string& text, int& value, const std::string& commandName, const std::string& argumentName) const {
    if (text.empty()) {
        print_error_message(commandName + ": missing " + argumentName + "\n");
        return false;
    }

    char* end = nullptr;
    errno = 0;
    long parsed = std::strtol(text.c_str(), &end, 10);
    if (errno == ERANGE || end == text.c_str() || *end != '\0' || parsed < 0 || parsed > INT_MAX) {
        print_error_message(commandName + ": invalid " + argumentName + ": " + text + "\n");
        return false;
    }

    value = static_cast<int>(parsed);
    return true;
}

bool TcshEngine::shouldFallbackToCmd(const std::string& command) const {
    static const std::vector<std::string> cmdBuiltins = {
        "assoc", "break", "call", "cd", "chdir", "cls", "color", "copy", "date", "del",
        "dir", "echo", "endlocal", "erase", "exit", "for", "ftype", "goto", "if", "md",
        "mkdir", "mklink", "move", "path", "pause", "popd", "prompt", "pushd", "rd", "rem",
        "ren", "rename", "rmdir", "set", "setlocal", "shift", "start", "time", "title", "type",
        "ver", "verify", "vol"
    };

    std::string lowered = command;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    return std::find(cmdBuiltins.begin(), cmdBuiltins.end(), lowered) != cmdBuiltins.end();
}


void TcshEngine::displayVersion() {
    std::cout << "\nCrossShellTCSH 6.24.00\n";
    std::cout << "CrossShellTCSH tcsh-compatible shell\n";
    std::cout << "Copyright (C) 2026, Roberto J. Dohnert.\n";
    std::cout << "Licensed under the BSD-3-Clause license.\n\n";
    std::cout << "System: " << wstring_to_string(get_windows_release_text()) << "\n\n";
}

void TcshEngine::displayHelp() {
    const std::vector<TcshHelpEntry>& helpEntries = tcsh_help_entries();

    std::cout << "\nCrossShellTCSH                General Help\n";
    std::cout << "---------------------------------------------\n";
    for (const auto& entry : helpEntries) {
        std::cout << std::left << std::setw(12) << entry.name << " - " << entry.description << "\n";
    }
    std::cout << "\n---------------------------------------------\n";
    std::cout << "Job Control        : jobs, fg, bg, stop, kill, wait, disown, &\n";
    std::cout << "Directory Navigation: cd, chdir, pushd, popd, dirs (accept / or \\)\n";
    std::cout << "Variables          : set, unset, export, typeset, printenv, read\n";
    std::cout << "Execution Control   : command, builtin, whence, hash, test, let, return, select, trap\n";
    std::cout << "Text and Output     : echo, print, repeat, time\n";
    std::cout << "Prompt Format      : %n user, %d domain, %m host, %M fullhost, %~ cwd, %/ fullcwd, %# role, %? status\n";
    std::cout << "Prompt How-To       : put 'set prompt = \"...\"' in ~/.tcshrc (or ~/.cshrc)\n";
    std::cout << "                     Default style: set prompt = \"% \"\n";
    std::cout << "                     Remove identity: set prompt = \"%~ %# \"\n";
    std::cout << "                     Prompt char only: set prompt = \"%# \"\n";
    std::cout << "                     Include host: set prompt = \"%n@%m %~ %# \"\n";
    std::cout << "---------------------------------------------\n\n";
}


bool TcshEngine::executeBuiltin(const std::vector<std::string>& args) {
    if (args.empty()) return false;
    const std::string cmd = args[0];

    auto okStatus = [&]() {
        setVariableList("status", { "0" });
        return true;
    };

    auto failStatus = [&]() {
        setVariableList("status", { "1" });
        return true;
    };

    auto joinArgs = [&](size_t startIndex) {
        std::string result;
        for (size_t i = startIndex; i < args.size(); ++i) {
            if (i > startIndex) result += " ";
            result += stripOuterQuotes(args[i]);
        }
        return result;
    };

    if (cmd == "exit" || cmd == "logout") { running = false; return okStatus(); }
    if (cmd == ":") { return okStatus(); }
    if (cmd == "help" || cmd == "builtins") { displayHelp(); return okStatus(); }
    if (cmd == "--version" || cmd == "version" || cmd == "-v" || cmd == "ver") { displayVersion(); return okStatus(); }
    if (cmd == "." || cmd == "source") {
        if (args.size() < 2) {
            print_error_message(cmd + ": Missing script file\n");
            return failStatus();
        }
        std::vector<std::string> scriptArgs;
        for (size_t i = 2; i < args.size(); ++i) scriptArgs.push_back(stripOuterQuotes(args[i]));
        if (!runScript(stripOuterQuotes(args[1]), scriptArgs)) {
            return failStatus();
        }
        return okStatus();
    }
    if (cmd == "pwd") {
        wchar_t currentDirW[MAX_PATH] = { 0 };
        GetCurrentDirectoryW(MAX_PATH, currentDirW);
        std::cout << wstring_to_string(currentDirW) << "\n";
        return okStatus();
    }
    if (cmd == "history") {
        for (size_t i = 0; i < history.size(); ++i) std::cout << (i + 1) << "  " << history[i] << "\n";
        return okStatus();
    }
    if (cmd == "alias") {
        if (args.size() == 1) {
            for (const auto& entry : aliases) std::cout << entry.first << "\t" << entry.second << "\n";
        } else if (args.size() >= 3) {
            aliases[stripOuterQuotes(args[1])] = stripOuterQuotes(args[2]);
        }
        return okStatus();
    }
    if (cmd == "unalias") {
        if (args.size() < 2) {
            print_error_message("unalias: Missing alias name\n");
            return failStatus();
        }
        aliases.erase(stripOuterQuotes(args[1]));
        return okStatus();
    }
    if (cmd == "complete") {
        if (args.size() == 1) {
            for (const auto& entry : completions) std::cout << entry.first << "\t" << entry.second << "\n";
        } else if (args.size() >= 3) {
            completions[stripOuterQuotes(args[1])] = stripOuterQuotes(args[2]);
        }
        return okStatus();
    }
    if (cmd == "uncomplete") {
        if (args.size() < 2) {
            print_error_message("uncomplete: Missing completion name\n");
            return failStatus();
        }
        completions.erase(stripOuterQuotes(args[1]));
        return okStatus();
    }
    if (cmd == "print") {
        for (size_t i = 1; i < args.size(); ++i) std::cout << stripOuterQuotes(args[i]) << (i + 1 < args.size() ? " " : "");
        std::cout << "\n";
        return okStatus();
    }
    if (cmd == "command") {
        const std::string remainder = joinArgs(1);
        if (remainder.empty()) {
            print_error_message("command: missing command name\n");
            return failStatus();
        }

        ParsedPipeline pipeline;
        if (!parseCommandLine(remainder, pipeline) || pipeline.commands.empty()) return failStatus();
        executePipeline(pipeline);
        return okStatus();
    }
    if (cmd == "builtin") {
        if (args.size() < 2) {
            print_error_message("builtin: missing builtin name\n");
            return failStatus();
        }
        std::vector<std::string> builtinArgs(args.begin() + 1, args.end());
        if (!executeBuiltin(builtinArgs)) {
            print_error_message("builtin: not a builtin: " + stripOuterQuotes(args[1]) + "\n");
            return failStatus();
        }
        return okStatus();
    }
    if (cmd == "whence" || cmd == "hash") {
        if (args.size() < 2) {
            std::cout << (cmd == "hash" ? "hashing: " : "") << (hashEnabled ? "enabled" : "disabled") << "\n";
            return okStatus();
        }

        for (size_t i = 1; i < args.size(); ++i) {
            const std::string name = stripOuterQuotes(args[i]);
            auto aliasIt = aliases.find(name);
            if (aliasIt != aliases.end()) {
                std::cout << name << " is an alias for " << aliasIt->second << "\n";
                continue;
            }

            if (isBuiltinCommand(name)) {
                std::cout << name << " is a shell builtin\n";
                continue;
            }

            bool foundOnDisk = false;
            std::string resolved = resolveExecutable(name, foundOnDisk);
            if (foundOnDisk) {
                std::cout << resolved << "\n";
            } else {
                std::cout << name << " not found\n";
            }
        }
        return okStatus();
    }
    if (cmd == "test") {
        const std::string expr = joinArgs(1);
        if (expr.empty()) return failStatus();
        return evalCondition(expr) ? okStatus() : failStatus();
    }
    if (cmd == "let") {
        const std::string expr = joinArgs(1);
        if (expr.empty()) return failStatus();

        std::vector<std::string> tokens = tokenize(expr, ' ');
        long long result = 0;
        auto resolveValue = [&](const std::string& token) -> long long {
            std::string clean = stripOuterQuotes(token);
            auto it = variables.find(clean);
            if (it != variables.end() && !it->second.empty()) {
                return std::atoll(it->second[0].c_str());
            }
            return std::atoll(clean.c_str());
        };

        if (tokens.size() >= 3 && tokens[1] == "=") {
            result = resolveValue(tokens[2]);
            if (tokens.size() >= 5) {
                const std::string op = tokens[3];
                const long long rhs = resolveValue(tokens[4]);
                if (op == "+") result += rhs;
                else if (op == "-") result -= rhs;
                else if (op == "*") result *= rhs;
                else if (op == "/") result = (rhs != 0) ? (result / rhs) : 0;
                else if (op == "%") result = (rhs != 0) ? (result % rhs) : 0;
            }
            setVariableList(tokens[0], { std::to_string(result) });
        } else if (!tokens.empty()) {
            result = resolveValue(tokens[0]);
        }

        return (result != 0) ? okStatus() : failStatus();
    }
    if (cmd == "return") {
        if (!executingScript) {
            print_error_message("return: not in a sourced script\n");
            return failStatus();
        }
        scriptDirective = ScriptDirective::Return;
        return okStatus();
    }
    if (cmd == "select") {
        if (args.size() < 3) {
            print_error_message("select: usage: select name item...\n");
            return failStatus();
        }

        const std::string varName = stripOuterQuotes(args[1]);
        std::vector<std::string> items;
        for (size_t i = 2; i < args.size(); ++i) {
            if (args[i] == "in") continue;
            items.push_back(stripOuterQuotes(args[i]));
        }
        if (items.empty()) return failStatus();

        for (size_t i = 0; i < items.size(); ++i) {
            std::cout << (i + 1) << ") " << items[i] << "\n";
        }
        std::cout << "? ";
        std::string choiceLine;
        if (!std::getline(std::cin, choiceLine)) return failStatus();
        int choice = std::atoi(choiceLine.c_str());
        if (choice < 1 || static_cast<size_t>(choice) > items.size()) return failStatus();

        setVariableList(varName, { items[static_cast<size_t>(choice) - 1] });
        return okStatus();
    }
    if (cmd == "trap") {
        if (args.size() == 1) {
            for (const auto& entry : trapHandlers) {
                std::cout << entry.second << "\t" << entry.first << "\n";
            }
            return okStatus();
        }

        auto normalizeTrap = [](std::string value) {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
                return static_cast<char>(std::toupper(ch));
            });
            if (value.rfind("SIG", 0) == 0) value.erase(0, 3);
            if (value == "QUIT") value = "BREAK";
            return value;
        };

        const bool clearMode = (args[1] == "-");
        std::string action = clearMode ? "" : stripOuterQuotes(args[1]);
        size_t signalStart = clearMode ? 2 : 2;
        if (signalStart >= args.size()) return failStatus();

        for (size_t i = signalStart; i < args.size(); ++i) {
            std::string sig = normalizeTrap(stripOuterQuotes(args[i]));
            if (sig.empty()) continue;
            if (clearMode) {
                trapHandlers.erase(sig);
            } else {
                trapHandlers[sig] = action;
            }
        }
        return okStatus();
    }
    if (cmd == "disown") {
        auto closeJobHandles = [](Job& job) {
            if (job.hProcess && job.hProcess != INVALID_HANDLE_VALUE) {
                CloseHandle(job.hProcess);
                job.hProcess = NULL;
            }
            if (job.hJob && job.hJob != INVALID_HANDLE_VALUE) {
                CloseHandle(job.hJob);
                job.hJob = NULL;
            }
        };

        if (args.size() == 1) {
            if (!jobList.empty()) {
                jobList.erase(std::remove_if(jobList.begin(), jobList.end(), [&](Job& job) {
                    if (!job.isRunning) {
                        closeJobHandles(job);
                        return true;
                    }
                    return false;
                }), jobList.end());
            }
            return okStatus();
        }

        for (size_t i = 1; i < args.size(); ++i) {
            std::string spec = stripOuterQuotes(args[i]);
            if (!spec.empty() && spec[0] == '%') spec.erase(spec.begin());
            int jobId = 0;
            if (!tryParseNonNegativeInt(spec, jobId, "disown", "job id")) return failStatus();
            jobList.erase(std::remove_if(jobList.begin(), jobList.end(), [&](Job& job) {
                if (static_cast<int>(job.id) == jobId) {
                    closeJobHandles(job);
                    return true;
                }
                return false;
            }), jobList.end());
        }
        return okStatus();
    }
    if (cmd == "printenv") {
        if (args.size() == 1) {
            for (const auto& entry : variables) {
                std::cout << entry.first << "=" << getVariableString(entry.first) << "\n";
            }
        } else {
            for (size_t i = 1; i < args.size(); ++i) {
                const std::string name = stripOuterQuotes(args[i]);
                const std::string value = getVariableString(name);
                if (!value.empty()) {
                    std::cout << value << "\n";
                } else {
                    std::wstring wName = string_to_wstring(name);
                    DWORD dwRet = GetEnvironmentVariableW(wName.c_str(), nullptr, 0);
                    if (dwRet > 0) {
                        std::vector<wchar_t> envBuf(dwRet);
                        if (GetEnvironmentVariableW(wName.c_str(), envBuf.data(), dwRet) > 0) {
                            std::cout << wstring_to_string(envBuf.data()) << "\n";
                        }
                    }
                }
            }
        }
        return okStatus();
    }
    if (cmd == "read") {
        std::string line;
        if (!std::getline(std::cin, line)) return failStatus();

        std::vector<std::string> values = tokenize(line, ' ');
        if (args.size() < 2) {
            setVariableList("REPLY", { line });
            return okStatus();
        }

        size_t valueIndex = 0;
        for (size_t i = 1; i < args.size(); ++i) {
            const std::string name = stripOuterQuotes(args[i]);
            if (i + 1 == args.size()) {
                std::string remainder;
                for (; valueIndex < values.size(); ++valueIndex) {
                    if (!remainder.empty()) remainder += " ";
                    remainder += values[valueIndex];
                }
                setVariableList(name, { remainder });
                break;
            }

            if (valueIndex < values.size()) {
                setVariableList(name, { values[valueIndex++] });
            } else {
                setVariableList(name, { "" });
            }
        }
        return okStatus();
    }
    if (cmd == "export") {
        if (args.size() == 1) {
            for (const auto& entry : variables) {
                SetEnvironmentVariableW(string_to_wstring(entry.first).c_str(), string_to_wstring(getVariableString(entry.first)).c_str());
            }
            return okStatus();
        }

        for (size_t i = 1; i < args.size(); ++i) {
            std::string spec = stripOuterQuotes(args[i]);
            size_t eq = spec.find('=');
            if (eq != std::string::npos) {
                std::string name = spec.substr(0, eq);
                std::string value = spec.substr(eq + 1);
                setVariableList(name, { value });
                SetEnvironmentVariableW(string_to_wstring(name).c_str(), string_to_wstring(value).c_str());
            } else {
                SetEnvironmentVariableW(string_to_wstring(spec).c_str(), string_to_wstring(getVariableString(spec)).c_str());
            }
        }
        return okStatus();
    }
    if (cmd == "typeset") {
        if (args.size() == 1) {
            for (const auto& p : variables) std::cout << p.first << "\t" << getVariableString(p.first) << "\n";
            return okStatus();
        }
        std::vector<std::string> setArgs = args;
        setArgs[0] = "set";
        return executeBuiltin(setArgs);
    }
    if (cmd == "dirs") {
        wchar_t cwdW[MAX_PATH] = { 0 };
        std::string currentDir = GetCurrentDirectoryW(MAX_PATH, cwdW) ? wstring_to_string(cwdW) : homeDir;
        std::cout << currentDir;
        for (const auto& entry : dirStack) std::cout << " " << entry;
        std::cout << "\n";
        return okStatus();
    }
    if (cmd == "pushd") {
        wchar_t cwdW[MAX_PATH] = { 0 };
        if (!GetCurrentDirectoryW(MAX_PATH, cwdW)) return failStatus();

        std::string currentDir = wstring_to_string(cwdW);
        std::string targetDir;
        if (args.size() >= 2) {
            targetDir = normalizePathSeparators(stripOuterQuotes(args[1]));
        } else if (!dirStack.empty()) {
            targetDir = normalizePathSeparators(dirStack.back());
            dirStack.pop_back();
        } else {
            print_error_message("pushd: No directory specified\n");
            return failStatus();
        }

        if (SetCurrentDirectoryW(string_to_wstring(targetDir).c_str())) {
            dirStack.push_back(currentDir);
            return okStatus();
        }

        print_error_message("pushd: No such file or directory: " + targetDir + "\n");
        return failStatus();
    }
    if (cmd == "popd") {
        if (dirStack.empty()) {
            print_error_message("popd: Directory stack empty\n");
            return failStatus();
        }

        std::string targetDir = normalizePathSeparators(dirStack.back());
        dirStack.pop_back();
        if (!SetCurrentDirectoryW(string_to_wstring(targetDir).c_str())) {
            print_error_message("popd: Failed to change directory\n");
            return failStatus();
        }

        return okStatus();
    }
    if (cmd == "jobs") { return listJobs(args) ? okStatus() : failStatus(); }
    if (cmd == "fg") {
        DWORD jobId = 0;
        if (args.size() >= 2) {
            std::string jobSpec = stripOuterQuotes(args[1]);
            if (!jobSpec.empty() && jobSpec[0] == '%') jobSpec.erase(jobSpec.begin());
            int parsed = 0;
            if (!tryParseNonNegativeInt(jobSpec, parsed, "fg", "job id") || parsed <= 0) return failStatus();
            jobId = static_cast<DWORD>(parsed);
        } else if (!jobList.empty()) {
            jobId = jobList.back().id;
        }
        if (jobId == 0) {
            print_error_message("fg: no current job\n");
            return failStatus();
        }
        bringJobToForeground(jobId);
        return okStatus();
    }
    if (cmd == "bg") {
        DWORD jobId = 0;
        if (args.size() >= 2) {
            std::string jobSpec = stripOuterQuotes(args[1]);
            if (!jobSpec.empty() && jobSpec[0] == '%') jobSpec.erase(jobSpec.begin());
            int parsed = 0;
            if (!tryParseNonNegativeInt(jobSpec, parsed, "bg", "job id") || parsed <= 0) return failStatus();
            jobId = static_cast<DWORD>(parsed);
        } else if (!jobList.empty()) {
            jobId = jobList.back().id;
        }
        if (jobId == 0) {
            print_error_message("bg: no current job\n");
            return failStatus();
        }
        sendJobToBackground(jobId);
        return okStatus();
    }
    if (cmd == "wait") {
        if (args.size() == 1) {
            for (auto& job : jobList) {
                if (job.isRunning && job.hProcess) WaitForSingleObject(job.hProcess, INFINITE);
            }
            updateJobs();
            return okStatus();
        }

        std::string jobSpec = stripOuterQuotes(args[1]);
        if (!jobSpec.empty() && jobSpec[0] == '%') jobSpec.erase(jobSpec.begin());
        int parsed = 0;
        if (!tryParseNonNegativeInt(jobSpec, parsed, "wait", "job id") || parsed <= 0) return failStatus();

        for (auto& job : jobList) {
            if (static_cast<int>(job.id) == parsed && job.isRunning && job.hProcess) {
                WaitForSingleObject(job.hProcess, INFINITE);
                updateJobs();
                return okStatus();
            }
        }

        print_error_message("wait: No such job\n");
        return failStatus();
    }
    if (cmd == "break") { scriptDirective = ScriptDirective::Break; return okStatus(); }
    if (cmd == "continue") { scriptDirective = ScriptDirective::Continue; return okStatus(); }
    if (cmd == "eval") {
        const std::string expr = joinArgs(1);
        if (!expr.empty()) executeCommandLine(expr);
        return okStatus();
    }
    if (cmd == "exec") {
        const std::string expr = joinArgs(1);
        if (!expr.empty()) executeCommandLine(expr);
        running = false;
        return okStatus();
    }
    if (cmd == "repeat") {
        if (args.size() < 3) {
            print_error_message("repeat: Usage: repeat count command\n");
            return failStatus();
        }

        int count = 0;
        if (!tryParseNonNegativeInt(stripOuterQuotes(args[1]), count, "repeat", "count")) return failStatus();
        const std::string expr = joinArgs(2);
        for (int i = 0; i < count; ++i) executeCommandLine(expr);
        return okStatus();
    }
    if (cmd == "which" || cmd == "where") {
        if (args.size() < 2) {
            print_error_message(cmd + ": Missing command name\n");
            return failStatus();
        }

        for (size_t i = 1; i < args.size(); ++i) {
            const std::string name = stripOuterQuotes(args[i]);
            bool foundOnDisk = false;
            const std::string resolved = resolveExecutable(name, foundOnDisk);
            if (foundOnDisk) {
                std::cout << resolved << "\n";
            } else if (isBuiltinCommand(name)) {
                std::cout << name << " is a shell builtin\n";
            } else {
                print_error_message(cmd + ": " + name + ": not found\n");
            }
        }
        return okStatus();
    }
    if (cmd == "rehash") { hashEnabled = true; return okStatus(); }
    if (cmd == "unhash") { hashEnabled = false; return okStatus(); }
    if (cmd == "hashstat") {
        std::cout << "hashing: " << (hashEnabled ? "enabled" : "disabled") << "\n";
        return okStatus();
    }
    if (cmd == "time") {
        if (args.size() == 1) {
            auto now = std::chrono::system_clock::now();
            std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
            std::cout << std::ctime(&nowTime);
            return okStatus();
        }

        const auto started = std::chrono::steady_clock::now();
        executeCommandLine(joinArgs(1));
        const auto finished = std::chrono::steady_clock::now();
        const std::chrono::duration<double> elapsed = finished - started;
        std::cout << "real\t" << elapsed.count() << "s\n";
        return okStatus();
    }
    if (cmd == "umask") {
        if (args.size() >= 2) {
            setVariableList("umask", { stripOuterQuotes(args[1]) });
        } else {
            std::cout << getVariableString("umask") << "\n";
        }
        return okStatus();
    }
    if (cmd == "notify") { notifyJobs = true; return okStatus(); }
    if (cmd == "hup") {
        for (auto& job : jobList) {
            if (job.isRunning && job.hProcess) {
                TerminateProcess(job.hProcess, 1);
                job.isRunning = false;
                if (job.hProcess && job.hProcess != INVALID_HANDLE_VALUE) {
                    CloseHandle(job.hProcess);
                    job.hProcess = NULL;
                }
                if (job.hJob && job.hJob != INVALID_HANDLE_VALUE) {
                    CloseHandle(job.hJob);
                    job.hJob = NULL;
                }
            }
        }
        updateJobs();
        return okStatus();
    }
    if (cmd == "clear" || cmd == "cls") {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (hOut == INVALID_HANDLE_VALUE || hOut == NULL || !GetConsoleScreenBufferInfo(hOut, &csbi)) return failStatus();
        COORD home = { 0, 0 };
        DWORD cellCount = static_cast<DWORD>(csbi.dwSize.X) * static_cast<DWORD>(csbi.dwSize.Y);
        DWORD written = 0;
        bool ok = FillConsoleOutputCharacterW(hOut, L' ', cellCount, home, &written) != FALSE;
        ok = ok && (FillConsoleOutputAttribute(hOut, csbi.wAttributes, cellCount, home, &written) != FALSE);
        ok = ok && (SetConsoleCursorPosition(hOut, home) != FALSE);
        setVariableList("status", { ok ? "0" : "1" });
        return true;
    }
    if (cmd == "kill") {
        if (args.size() < 2) {
            print_error_message("kill: Missing PID or Job ID\n");
            return failStatus();
        }

        const std::string target = stripOuterQuotes(args[1]);
        if (!target.empty() && target[0] == '%') {
            int jobId = 0;
            if (!tryParseNonNegativeInt(target.substr(1), jobId, "kill", "job id") || jobId <= 0) return failStatus();

            auto it = std::find_if(jobList.begin(), jobList.end(), [jobId](const Job& job) {
                return static_cast<int>(job.id) == jobId;
            });
            if (it == jobList.end() || !it->isRunning || !it->hProcess) {
                print_error_message("kill: No such job\n");
                return failStatus();
            }

            bool ok = TerminateProcess(it->hProcess, 1) != FALSE;
            if (it->hJob && it->hJob != INVALID_HANDLE_VALUE) TerminateJobObject(it->hJob, 1);
            it->isRunning = false;
            if (it->hProcess && it->hProcess != INVALID_HANDLE_VALUE) { CloseHandle(it->hProcess); it->hProcess = NULL; }
            if (it->hJob && it->hJob != INVALID_HANDLE_VALUE) { CloseHandle(it->hJob); it->hJob = NULL; }
            if (!ok) print_error_message("kill: Failed to terminate job\n");
            setVariableList("status", { ok ? "0" : "1" });
            return true;
        }

        int pid = 0;
        if (!tryParseNonNegativeInt(target, pid, "kill", "pid") || pid <= 0) return failStatus();
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
        if (!hProcess) {
            print_error_message("kill: No such process or access denied\n");
            return failStatus();
        }
        bool ok = TerminateProcess(hProcess, 1) != FALSE;
        CloseHandle(hProcess);
        if (!ok) print_error_message("kill: Failed to terminate process\n");
        setVariableList("status", { ok ? "0" : "1" });
        return true;
    }
    if (cmd == "cd" || cmd == "chdir") {
        std::string target = (args.size() > 1) ? stripOuterQuotes(args[1]) : homeDir;
        if (target == "-") {
            target = getVariableString("owd");
            if (target.empty()) target = homeDir;
        }
        target = normalizePathSeparators(target);

        wchar_t cwdW[MAX_PATH] = { 0 };
        if (GetCurrentDirectoryW(MAX_PATH, cwdW)) {
            setVariableList("owd", { wstring_to_string(cwdW) });
        }

        if (!SetCurrentDirectoryW(string_to_wstring(target).c_str())) {
            print_error_message("cd: No such file or directory: " + target + "\n");
            return failStatus();
        }

        if (GetCurrentDirectoryW(MAX_PATH, cwdW)) {
            setVariableList("cwd", { wstring_to_string(cwdW) });
        }
        return okStatus();
    }
    if (cmd == "set") {
        if (args.size() == 1) {
            for (const auto& p : variables) std::cout << p.first << "\t" << getVariableString(p.first) << "\n";
            return okStatus();
        }
        if (args.size() >= 4 && args[2] == "=" && args[3] == "(") {
            std::vector<std::string> list;
            for (size_t i = 4; i < args.size(); ++i) {
                if (args[i] == ")") break;
                list.push_back(stripOuterQuotes(args[i]));
            }
            setVariableList(stripOuterQuotes(args[1]), list);
            return okStatus();
        }
        if (args.size() >= 3 && args[2] == "=") {
            std::string varSpec = stripOuterQuotes(args[1]);
            size_t bracketOpen = varSpec.find('[');
            size_t bracketClose = varSpec.find(']');
            if (bracketOpen != std::string::npos && bracketClose != std::string::npos && bracketClose > bracketOpen) {
                std::string varName = varSpec.substr(0, bracketOpen);
                int idx = std::atoi(varSpec.substr(bracketOpen + 1, bracketClose - bracketOpen - 1).c_str());
                if (variables.count(varName) && idx >= 1 && static_cast<size_t>(idx) <= variables[varName].size()) {
                    variables[varName][idx - 1] = stripOuterQuotes(args[3]);
                }
            } else {
                setVariableList(varSpec, { stripOuterQuotes(args[3]) });
            }
            return okStatus();
        }
        return failStatus();
    }
    if (cmd == "unset") {
        if (args.size() > 1) variables.erase(stripOuterQuotes(args[1]));
        return okStatus();
    }
    if (cmd == "setenv") {
        if (args.size() < 3) return failStatus();
        SetEnvironmentVariableW(string_to_wstring(stripOuterQuotes(args[1])).c_str(), string_to_wstring(stripOuterQuotes(args[2])).c_str());
        return okStatus();
    }
    if (cmd == "unsetenv") {
        if (args.size() < 2) return failStatus();
        SetEnvironmentVariableW(string_to_wstring(stripOuterQuotes(args[1])).c_str(), NULL);
        return okStatus();
    }
    if (cmd == "echo") {
        for (size_t i = 1; i < args.size(); ++i) std::cout << stripOuterQuotes(args[i]) << (i + 1 < args.size() ? " " : "");
        std::cout << "\n";
        return okStatus();
    }
    if (cmd == "@") {
        if (args.size() > 1) {
            std::string expr;
            for (size_t i = 1; i < args.size(); ++i) expr += args[i] + " ";
            evaluateArithmetic(expr);
        }
        return okStatus();
    }
    if (cmd == "shift") {
        shiftPositionalArguments(1);
        return okStatus();
    }

    if (cmd == "alloc") {
        std::cout << "variables=" << variables.size()
                  << " aliases=" << aliases.size()
                  << " completions=" << completions.size()
                  << " jobs=" << jobList.size()
                  << " history=" << history.size() << "\n";
        return okStatus();
    }
    if (cmd == "bindkey") {
        if (args.size() == 1) {
            for (const auto& entry : keyBindings) {
                std::cout << entry.first << "\t" << entry.second << "\n";
            }
        } else if (args.size() >= 3) {
            keyBindings[stripOuterQuotes(args[1])] = joinArgs(2);
        }
        return okStatus();
    }
    if (cmd == "breaksw") { scriptDirective = ScriptDirective::Break; return okStatus(); }
    if (cmd == "case" || cmd == "default" || cmd == "else" || cmd == "end" || cmd == "endif" || cmd == "endsw" || cmd == "foreach" || cmd == "switch" || cmd == "while") { return okStatus(); }
    if (cmd == "filetest") {
        const std::string expr = joinArgs(1);
        return expr.empty() ? failStatus() : (evalCondition(expr) ? okStatus() : failStatus());
    }
    if (cmd == "glob") {
        if (args.size() > 1) {
            std::vector<std::string> globs(args.begin() + 1, args.end());
            std::vector<std::string> expanded = expandGlobs(globs);
            for (const auto& item : expanded) std::cout << item << "\n";
        }
        return okStatus();
    }
    if (cmd == "goto") {
        if (args.size() < 2) return failStatus();
        print_error_message("goto: handled inside script execution context\n");
        return okStatus();
    }
    if (cmd == "if") {
        const std::string expr = joinArgs(1);
        return expr.empty() ? failStatus() : (evalCondition(expr) ? okStatus() : failStatus());
    }
    if (cmd == "limit") {
        if (args.size() == 1) {
            std::cout << getVariableString("limits") << "\n";
        } else {
            setVariableList("limits", { joinArgs(1) });
        }
        return okStatus();
    }
    if (cmd == "log") {
        if (args.size() == 1) {
            std::ifstream logIn(logFile);
            std::string line;
            while (std::getline(logIn, line)) std::cout << line << "\n";
        } else {
            std::ofstream logOut(logFile, std::ios::app);
            logOut << joinArgs(1) << "\n";
        }
        return okStatus();
    }
    if (cmd == "login") {
        wchar_t userNameW[256] = { 0 };
        DWORD len = ARRAYSIZE(userNameW);
        if (GetUserNameW(userNameW, &len)) {
            std::string userName = wstring_to_string(userNameW);
            setVariableList("login", { userName });
            std::cout << userName << "\n";
        }
        return okStatus();
    }
    if (cmd == "migrate") {
        setVariableList("migrate", { joinArgs(1) });
        std::cout << "migrate: " << joinArgs(1) << "\n";
        return okStatus();
    }
    if (cmd == "newgrp") {
        const std::string groupName = (args.size() > 1) ? stripOuterQuotes(args[1]) : getVariableString("login");
        setVariableList("group", { groupName });
        std::cout << groupName << "\n";
        return okStatus();
    }
    if (cmd == "nice") {
        if (args.size() < 2) return failStatus();
        std::string expr = joinArgs(1);
        if (!expr.empty() && expr[0] == '-' && args.size() > 2) {
            size_t firstSpace = expr.find(' ');
            if (firstSpace != std::string::npos) expr = expr.substr(firstSpace + 1);
        }
        executeCommandLine(expr);
        return okStatus();
    }
    if (cmd == "nohup") {
        if (args.size() < 2) return failStatus();
        executeCommandLine(joinArgs(1) + " &");
        return okStatus();
    }
    if (cmd == "sched") {
        if (args.size() == 1) {
            for (const auto& task : scheduledTasks) {
                std::cout << task.timeStr << "\t" << task.command << "\n";
            }
        } else if (args.size() >= 3) {
            ScheduledTask task;
            task.timeStr = stripOuterQuotes(args[1]);
            task.command = joinArgs(2);
            if (task.timeStr == "now" || task.timeStr == "NOW") {
                executeCommandLine(task.command);
            } else {
                scheduledTasks.push_back(task);
            }
        }
        return okStatus();
    }
    if (cmd == "stop" || cmd == "suspend") {
        if (args.size() < 2) {
            print_error_message(cmd + ": Missing job id\n");
            return failStatus();
        }
        std::string target = stripOuterQuotes(args[1]);
        if (!target.empty() && target[0] == '%') target.erase(target.begin());
        int jobId = 0;
        if (!tryParseNonNegativeInt(target, jobId, cmd, "job id") || jobId <= 0) return failStatus();
        auto it = std::find_if(jobList.begin(), jobList.end(), [jobId](const Job& job) { return static_cast<int>(job.id) == jobId; });
        if (it == jobList.end()) {
            print_error_message(cmd + ": No such job\n");
            return failStatus();
        }
        it->isRunning = false;
        std::cout << "[" << it->id << "] stopped\n";
        return okStatus();
    }
    if (cmd == "settc" || cmd == "setty" || cmd == "termname" || cmd == "telltc" || cmd == "universe") {
        if (cmd == "termname") {
            if (args.size() == 1) {
                std::cout << getVariableString("termname") << "\n";
            } else {
                setVariableList("termname", { joinArgs(1) });
                std::cout << joinArgs(1) << "\n";
            }
        } else if (cmd == "telltc") {
            std::cout << getVariableString("settc") << "\n" << getVariableString("setty") << "\n";
        } else {
            setVariableList(cmd, { joinArgs(1) });
            if (!joinArgs(1).empty()) std::cout << joinArgs(1) << "\n";
        }
        return okStatus();
    }
    if (cmd == "unlimit") {
        setVariableList("limits", {});
        return okStatus();
    }
    if (cmd == "echotc") {
        if (args.size() < 2) {
            print_error_message("echotc: Too few arguments.\n");
            return failStatus();
        }
        for (size_t i = 1; i < args.size(); ++i) {
            std::string cap = stripOuterQuotes(args[i]);
            std::transform(cap.begin(), cap.end(), cap.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (cap == "home" || cap == "ho") {
                std::cout << "\x1b[H";
            } else if (cap == "clear" || cap == "cl") {
                std::cout << "\x1b[2J\x1b[H";
            } else if (cap == "cd") {
                std::cout << "\x1b[J";
            } else if (cap == "ce") {
                std::cout << "\x1b[K";
            } else if (cap == "bold" || cap == "md") {
                std::cout << "\x1b[1m";
            } else if (cap == "standout" || cap == "so") {
                std::cout << "\x1b[7m";
            } else if (cap == "underline" || cap == "us") {
                std::cout << "\x1b[4m";
            } else if (cap == "normal" || cap == "me" || cap == "se" || cap == "ue") {
                std::cout << "\x1b[0m";
            } else if (cap == "bell" || cap == "bl") {
                std::cout << "\a";
            } else if (cap == "up" || cap == "up1") {
                std::cout << "\x1b[A";
            } else if (cap == "down" || cap == "do1") {
                std::cout << "\x1b[B";
            } else if (cap == "left" || cap == "le") {
                std::cout << "\x1b[D";
            } else if (cap == "right" || cap == "nd") {
                std::cout << "\x1b[C";
            } else if (cap == "cols" || cap == "co" || cap == "lines" || cap == "li") {
                CONSOLE_SCREEN_BUFFER_INFO csbi;
                HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
                if (hOut != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(hOut, &csbi)) {
                    if (cap == "cols" || cap == "co") {
                        std::cout << csbi.dwSize.X << "\n";
                    } else {
                        std::cout << (csbi.srWindow.Bottom - csbi.srWindow.Top + 1) << "\n";
                    }
                }
            } else {
                print_error_message("echotc: Unknown terminal capability `" + cap + "'.\n");
                return failStatus();
            }
        }
        std::cout.flush();
        return okStatus();
    }
    if (cmd == "ls-F" || cmd == "ls_F") {
        std::vector<std::string> targets;
        for (size_t i = 1; i < args.size(); ++i) {
            std::string t = stripOuterQuotes(args[i]);
            if (!t.empty() && t[0] != '-') targets.push_back(t);
        }
        if (targets.empty()) targets.push_back(".");

        auto formatEntry = [](const std::string& name, DWORD attr) -> std::string {
            if (attr == INVALID_FILE_ATTRIBUTES) return name;
            if (attr & FILE_ATTRIBUTE_REPARSE_POINT) return name + "@";
            if (attr & FILE_ATTRIBUTE_DIRECTORY) return name + "/";
            std::string lower = name;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (lower.size() >= 4) {
                std::string ext = lower.substr(lower.size() - 4);
                if (ext == ".exe" || ext == ".bat" || ext == ".cmd" || ext == ".com" || ext == ".ps1") {
                    return name + "*";
                }
            }
            return name;
        };

        for (size_t t = 0; t < targets.size(); ++t) {
            std::string target = normalizePathSeparators(targets[t]);
            std::wstring wTarget = string_to_wstring(target);
            DWORD attr = GetFileAttributesW(wTarget.c_str());

            if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
                std::cout << formatEntry(target, attr) << "\n";
                continue;
            }

            std::string searchPattern = target;
            if (!searchPattern.empty() && searchPattern.back() != '\\' && searchPattern.back() != '/') {
                searchPattern += "\\*";
            } else {
                searchPattern += "*";
            }

            WIN32_FIND_DATAW fd;
            HANDLE hFind = FindFirstFileW(string_to_wstring(searchPattern).c_str(), &fd);
            if (hFind == INVALID_HANDLE_VALUE) {
                print_error_message("ls-F: " + target + ": No such file or directory\n");
                return failStatus();
            }

            std::vector<std::string> entries;
            do {
                std::wstring wName = fd.cFileName;
                if (wName == L"." || wName == L"..") continue;
                std::string name = wstring_to_string(wName);
                entries.push_back(formatEntry(name, fd.dwFileAttributes));
            } while (FindNextFileW(hFind, &fd));
            FindClose(hFind);

            std::sort(entries.begin(), entries.end());
            for (size_t i = 0; i < entries.size(); ++i) {
                std::cout << entries[i] << (i + 1 < entries.size() ? "  " : "\n");
            }
        }
        return okStatus();
    }
    if (cmd == "onintr") {
        if (args.size() == 1) {
            trapHandlers.erase("INT");
            trapHandlers.erase("BREAK");
            return okStatus();
        }
        std::string action = stripOuterQuotes(args[1]);
        if (action == "-") {
            trapHandlers["INT"] = ":";
            trapHandlers["BREAK"] = ":";
        } else {
            std::string handler = (action.rfind("goto ", 0) == 0) ? action : ("goto " + action);
            trapHandlers["INT"] = handler;
            trapHandlers["BREAK"] = handler;
        }
        return okStatus();
    }

    return false;
}

