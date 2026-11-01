// Shell AST execution engine and command-substitution capture.

#include "busybox.h"
#include "busybox_shell.h"
#include "busybox_applets.h"

int CommandNode::execute_impl(ShellContext& ctx, HANDLE hIn, HANDLE hOut, size_t depth) {
        if (args.empty()) return 0;

        size_t raw_command_index = 0;
        while (raw_command_index < args.size()) {
            size_t eq = args[raw_command_index].find('=');
            std::string name = eq == std::string::npos ? "" : args[raw_command_index].substr(0, eq);
            if (eq == std::string::npos || !is_valid_assignment_name(name)) {
                break;
            }
            std::string value = argstring_to_string(
                expand_arg_variables(args[raw_command_index].substr(eq + 1), ctx));
            ctx.set_var(name, value);
            raw_command_index++;
        }
        if (raw_command_index == args.size()) return 0;

        std::vector<ArgString> expanded_arg_objs;
        for (size_t i = raw_command_index; i < args.size(); ++i) {
            expanded_arg_objs.push_back(expand_arg_variables(args[i], ctx));
        }
        std::vector<std::string> expanded_args = expand_globs(expanded_arg_objs);

        if (expanded_args.empty() || expanded_args[0].empty()) return 0;

        if (expanded_args[0] == "cd") {
            if (expanded_args.size() > 1) {
                std::error_code ec;
                fs::path target(utf8_to_utf16(normalize_path(expanded_args[1])));
                if (ctx.pipeline_stage) {
                    if (!fs::is_directory(target, ec)) {
                        std::cerr << "cd: " << expanded_args[1] << ": No such directory\n";
                        return 1;
                    }
                    return 0;
                }
                fs::current_path(target, ec);
                if (ec) { std::cerr << "cd: " << expanded_args[1] << ": No such directory\n"; return 1; }
            }
            return 0;
        }
        if (expanded_args[0] == "exit") {
            int exit_code = 0;
            if (expanded_args.size() > 1) {
                try {
                    exit_code = std::stoi(expanded_args[1]);
                } catch (...) {
                    exit_code = 2;
                }
            }
            throw ShellExitSignal(exit_code);
        }

        std::string cmd = to_lower(expanded_args[0]);

        // User-defined shell function call: re-parse the stored body and execute
        // it with positional parameters bound to the call arguments.
        std::string bodySrc;
        bool isFunction = false;
        {
            std::lock_guard<std::mutex> lock(ctx.ctx_mutex);
            auto fnIt = ctx.functions.find(cmd);
            if (fnIt != ctx.functions.end()) {
                bodySrc = fnIt->second;
                isFunction = true;
            }
        }
        if (isFunction) {
            // Save current positional params and bind the call arguments.
            std::map<std::string, std::string> savedParams;
            {
                std::lock_guard<std::mutex> lock(ctx.ctx_mutex);
                for (int pi = 0; pi <= 9; ++pi) {
                    std::string pn = std::to_string(pi);
                    auto it = ctx.variables.find(pn);
                    if (it != ctx.variables.end()) savedParams[pn] = it->second;
                }
            }
            std::string savedCount = ctx.get_var("#");

            // Bind call arguments to $1..$N (expanded_args[0] is the function name).
            for (size_t ai = 1; ai < expanded_args.size(); ++ai) {
                ctx.set_var(std::to_string(ai), expanded_args[ai]);
            }
            ctx.set_var("#", std::to_string(expanded_args.size() - 1));

            // Push a new local scope for `local` declarations in this call.
            {
                std::lock_guard<std::mutex> lock(ctx.ctx_mutex);
                ctx.local_scopes.push_back({});
            }

            int res = 0;
            try {
                Parser fnParser(tokenize(bodySrc));
                auto fnAst = fnParser.parse_chain();
                res = fnAst ? fnAst->execute(ctx, hIn, hOut, depth + 1) : 0;
            } catch (const ShellExitSignal& signal) {
                res = signal.code();
            }

            // Pop the local scope, restoring the saved values of `local` vars.
            // Collect the saved map under the lock, then restore without holding
            // it (set_var locks the mutex itself).
            std::map<std::string, std::string> toRestore;
            {
                std::lock_guard<std::mutex> lock(ctx.ctx_mutex);
                if (!ctx.local_scopes.empty()) {
                    toRestore = ctx.local_scopes.back();
                    ctx.local_scopes.pop_back();
                }
            }
            for (const auto& kv : toRestore) {
                ctx.set_var(kv.first, kv.second);
            }

            // Restore positional params.
            for (int pi = 0; pi <= 9; ++pi) {
                std::string pn = std::to_string(pi);
                if (savedParams.count(pn)) ctx.set_var(pn, savedParams[pn]);
                else ctx.unset_var(pn);
            }
            ctx.set_var("#", savedCount);
            return res;
        }

        bool is_applet = (applets.count(cmd) > 0);
        bool needs_process_isolation = (hIn != GetStdHandle(STD_INPUT_HANDLE) || hOut != GetStdHandle(STD_OUTPUT_HANDLE) || !redirect_in.empty() || !redirect_out.empty() || is_background);

        if (is_applet && !needs_process_isolation && redirect_err.empty() && !err_to_out) {
            try { return applets[cmd](expanded_args, ctx); }
            catch (const std::exception& e) { std::cerr << "busybox: applet exception: " << e.what() << "\n"; return 1; }
        }

        // In-process applet with a stderr redirect: capture std::cerr and write
        // it to the redirect target (file for 2>, stdout for 2>&1).
        if (is_applet && !needs_process_isolation && (!redirect_err.empty() || err_to_out)) {
            std::ostringstream errCapture;
            std::streambuf* oldErr = std::cerr.rdbuf(errCapture.rdbuf());
            int status = 0;
            try { status = applets[cmd](expanded_args, ctx); }
            catch (const std::exception& e) { errCapture << "busybox: applet exception: " << e.what() << "\n"; status = 1; }
            std::cerr.rdbuf(oldErr);

            std::string captured = errCapture.str();
            if (!captured.empty()) {
                if (err_to_out) {
                    std::cout << captured;
                } else {
                    std::string exp_err = argstring_to_string(expand_arg_variables(redirect_err, ctx));
                    std::ofstream errFile(utf8_to_utf16(normalize_path(exp_err)), std::ios::binary | std::ios::trunc);
                    if (errFile) errFile << captured;
                }
            }
            return status;
        }

        std::string resolved_exe;
        std::string cmdline;
        bool is_batch_file = false;

        if (is_applet) {
            resolved_exe = get_self_exe_path();
            cmdline = escape_win32_arg(resolved_exe);
            for (const auto& arg : expanded_args) cmdline += " " + escape_win32_arg(arg);
        } else {
            resolved_exe = resolve_executable_path(expanded_args[0]);
            if (!resolved_exe.empty()) {
                std::string ext = to_lower(fs::path(resolved_exe).extension().string());
                if (ext == ".bat" || ext == ".cmd") is_batch_file = true;
            }

            if (is_batch_file) {
                cmdline = "cmd.exe /c " + escape_win32_arg(resolved_exe);
                for (size_t i = 1; i < expanded_args.size(); ++i) {
                    cmdline += " " + escape_cmd_metachars(escape_win32_arg(expanded_args[i]));
                }
            } else {
                if (!resolved_exe.empty()) {
                    cmdline = escape_win32_arg(resolved_exe);
                } else {
                    cmdline = escape_win32_arg(expanded_args[0]);
                }
                for (size_t i = 1; i < expanded_args.size(); ++i) {
                    cmdline += " " + escape_win32_arg(expanded_args[i]);
                }
            }
        }

        std::wstring resolved_exe_w = utf8_to_utf16(resolved_exe);
        std::wstring cmdline_w = utf8_to_utf16(cmdline);

        ctx.audit_log("EXEC", (resolved_exe.empty() ? expanded_args[0] : resolved_exe));

        // Resolve stderr redirection for this command. 2>&1 duplicates stderr
        // onto the (already-resolved) stdout handle; 2>file opens a file.
        ScopedHandle hFileErr;
        HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
        if (err_to_out) {
            hErr = hOut;
        } else if (!redirect_err.empty()) {
            std::string exp_err = argstring_to_string(expand_arg_variables(redirect_err, ctx));
            HANDLE h = CreateFileW(utf8_to_utf16(normalize_path(exp_err)).c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (h == INVALID_HANDLE_VALUE) { std::cerr << "busybox: cannot open " << exp_err << "\n"; return 1; }
            hFileErr = ScopedHandle(h);
            hErr = hFileErr.get();
        }

        std::set<HANDLE> raw_handles;
        if (hIn && hIn != INVALID_HANDLE_VALUE) raw_handles.insert(hIn);
        if (hOut && hOut != INVALID_HANDLE_VALUE) raw_handles.insert(hOut);
        if (hErr && hErr != INVALID_HANDLE_VALUE) raw_handles.insert(hErr);

        std::vector<HANDLE> valid_handles;
        for (HANDLE h : raw_handles) {
            SetHandleInformation(h, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
            valid_handles.push_back(h);
        }

        SIZE_T cbAttributeListSize = 0;
        InitializeProcThreadAttributeList(NULL, 1, 0, &cbAttributeListSize);
        std::vector<BYTE> attributeListBuffer(cbAttributeListSize);
        PPROC_THREAD_ATTRIBUTE_LIST pAttributeList = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(attributeListBuffer.data());
        InitializeProcThreadAttributeList(pAttributeList, 1, 0, &cbAttributeListSize);

        if (!valid_handles.empty()) {
            UpdateProcThreadAttribute(pAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, 
                                      valid_handles.data(), valid_handles.size() * sizeof(HANDLE), NULL, NULL);
        }

        STARTUPINFOEXW siex = { 0 };
        siex.StartupInfo.cb = sizeof(STARTUPINFOEXW);
        siex.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        siex.StartupInfo.hStdInput = hIn;
        siex.StartupInfo.hStdOutput = hOut;
        siex.StartupInfo.hStdError = hErr;
        siex.lpAttributeList = pAttributeList;

        PROCESS_INFORMATION pi = { 0 };
        std::vector<wchar_t> cmd_buf(cmdline_w.begin(), cmdline_w.end()); cmd_buf.push_back(L'\0');

        LPCWSTR lpAppName = (!resolved_exe_w.empty() && !is_batch_file) ? resolved_exe_w.c_str() : NULL;

        BOOL created = CreateProcessW(lpAppName, cmd_buf.data(), NULL, NULL, TRUE, 
                                      EXTENDED_STARTUPINFO_PRESENT | CREATE_SUSPENDED, NULL, NULL, 
                                      &siex.StartupInfo, &pi);

        DeleteProcThreadAttributeList(pAttributeList);

        if (created) {
            ScopedHandle hProc(pi.hProcess), hThread(pi.hThread);
            if (g_hJobObject) AssignProcessToJobObject(g_hJobObject, hProc.get());

            if (is_background) {
                HANDLE hThreadRelease = hThread.release();
                HANDLE hProcRelease = hProc.release();

                // Reap finished jobs first so the table does not leak handles
                // or accumulate stale entries across many background launches.
                ctx.reap_jobs();

                std::lock_guard<std::mutex> lock(ctx.ctx_mutex);
                int job_id = ctx.jobs.empty() ? 1 : (ctx.jobs.back().id + 1);

                std::string full_cmd;
                for (const auto& arg : expanded_args) {
                    if (!full_cmd.empty()) full_cmd += " ";
                    full_cmd += arg;
                }

                ctx.jobs.push_back({job_id, pi.dwProcessId, hProcRelease, full_cmd, true});
                ResumeThread(hThreadRelease);
                CloseHandle(hThreadRelease);

                std::cout << "[" << job_id << "] " << pi.dwProcessId << "\n";
                return 0;
            }

            register_active_process(hProc.get());
            ResumeThread(hThread.get());

            DWORD wait_res = WaitForSingleObject(hProc.get(), ctx.timeout_ms);
            unregister_active_process(hProc.get());

            if (wait_res == WAIT_TIMEOUT) {
                TerminateProcess(hProc.get(), 124);
                std::cerr << "busybox: execution timed out after " << ctx.timeout_ms << "ms\n";
                return 124;
            }
            DWORD dwCode = 0; GetExitCodeProcess(hProc.get(), &dwCode);
            return static_cast<int>(dwCode);
        }

        std::cerr << "busybox: " << expanded_args[0] << ": command not found or execution failed\n";
        return 127;
}

std::string capture_cmd_output(const std::string& cmd, ShellContext& ctx) {
    HANDLE hReadRaw, hWriteRaw;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    if (!CreatePipe(&hReadRaw, &hWriteRaw, &sa, 0)) return "";
    SetHandleInformation(hReadRaw, HANDLE_FLAG_INHERIT, 0);

    std::string output;
    std::thread reader([hReadRaw, &output]() {
        char buffer[2048]; DWORD bytesRead = 0;
        while (ReadFile(hReadRaw, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            if (output.size() < MAX_COMMAND_SUBSTITUTION_BYTES) {
                size_t remaining = MAX_COMMAND_SUBSTITUTION_BYTES - output.size();
                output.append(buffer, (std::min)(remaining, static_cast<size_t>(bytesRead)));
            }
        }
        CloseHandle(hReadRaw);
    });

    ShellContext sub_ctx(false);
    {
        std::lock_guard<std::mutex> lock(ctx.ctx_mutex);
        sub_ctx.variables = ctx.variables;
        sub_ctx.last_return_code = ctx.last_return_code;
        sub_ctx.timeout_ms = ctx.timeout_ms;
        sub_ctx.audit_logging_enabled = ctx.audit_logging_enabled;
    }

    ScopedWorkingDirectory cwd_guard;
    Parser parser(tokenize(cmd));
    auto ast = parser.parse_chain();
    int command_status = 0;
    if (ast) {
        try {
            command_status = ast->execute(sub_ctx, NULL, hWriteRaw);
        } catch (const ShellExitSignal& signal) {
            command_status = signal.code();
        }
    }

    restore_nested_environment(ctx, sub_ctx);
    
    CloseHandle(hWriteRaw);
    if (reader.joinable()) reader.join();

    while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) output.pop_back();
    ctx.last_return_code = command_status;
    return output;
}

// ============================================================================
