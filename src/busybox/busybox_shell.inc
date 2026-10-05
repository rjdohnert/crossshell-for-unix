// ============================================================================
// ABSTRACT SYNTAX TREE (AST) & SECURE WIN32 EXECUTION ENGINE
// ============================================================================

class ASTNode {
public:
    std::string redirect_in;
    std::string redirect_out;
    bool append_out = false;
    bool is_background = false;

    virtual ~ASTNode() = default;
    virtual int execute_impl(ShellContext& ctx, HANDLE hIn, HANDLE hOut, size_t depth) = 0;

    int execute(ShellContext& ctx, HANDLE hIn, HANDLE hOut, size_t depth = 0) {
        if (depth > MAX_AST_RECURSION_DEPTH) {
            std::cerr << "busybox: fatal: AST recursion depth limit exceeded\n";
            return 1;
        }

        HANDLE effective_in = hIn ? hIn : GetStdHandle(STD_INPUT_HANDLE);
        HANDLE effective_out = hOut ? hOut : GetStdHandle(STD_OUTPUT_HANDLE);
        ScopedHandle hFileIn, hFileOut;

        if (!redirect_in.empty()) {
            std::string exp_in = argstring_to_string(expand_arg_variables(redirect_in, ctx));
            HANDLE h = CreateFileW(utf8_to_utf16(normalize_path(exp_in)).c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (h == INVALID_HANDLE_VALUE) { std::cerr << "busybox: cannot open " << exp_in << "\n"; return 1; }
            hFileIn = ScopedHandle(h); effective_in = hFileIn.get();
        }
        if (!redirect_out.empty()) {
            std::string exp_out = argstring_to_string(expand_arg_variables(redirect_out, ctx));
            DWORD creation = append_out ? OPEN_ALWAYS : CREATE_ALWAYS;
            HANDLE h = CreateFileW(utf8_to_utf16(normalize_path(exp_out)).c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, creation, FILE_ATTRIBUTE_NORMAL, NULL);
            if (h == INVALID_HANDLE_VALUE) { std::cerr << "busybox: cannot open " << exp_out << "\n"; return 1; }
            hFileOut = ScopedHandle(h);
            if (append_out) SetFilePointer(hFileOut.get(), 0, NULL, FILE_END);
            effective_out = hFileOut.get();
        }

        return execute_impl(ctx, effective_in, effective_out, depth);
    }
};

class CommandNode : public ASTNode {
public:
    std::vector<std::string> args;

    int execute_impl(ShellContext& ctx, HANDLE hIn, HANDLE hOut, size_t depth) override {
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
        bool is_applet = (applets.count(cmd) > 0);
        bool needs_process_isolation = (hIn != GetStdHandle(STD_INPUT_HANDLE) || hOut != GetStdHandle(STD_OUTPUT_HANDLE) || !redirect_in.empty() || !redirect_out.empty() || is_background);

        if (is_applet && !needs_process_isolation) {
            try { return applets[cmd](expanded_args, ctx); }
            catch (const std::exception& e) { std::cerr << "busybox: applet exception: " << e.what() << "\n"; return 1; }
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

        std::set<HANDLE> raw_handles;
        if (hIn && hIn != INVALID_HANDLE_VALUE) raw_handles.insert(hIn);
        if (hOut && hOut != INVALID_HANDLE_VALUE) raw_handles.insert(hOut);
        HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
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

                std::lock_guard<std::mutex> lock(ctx.ctx_mutex);
                int job_id = static_cast<int>(ctx.jobs.size() + 1);

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
};

class PipelineNode : public ASTNode {
public:
    std::vector<std::shared_ptr<ASTNode>> stages;

    int execute_impl(ShellContext& ctx, HANDLE hIn, HANDLE hOut, size_t depth) override {
        if (stages.empty()) return 0;
        if (stages.size() == 1) return stages[0]->execute(ctx, hIn, hOut, depth + 1);

        size_t n = stages.size();
        std::vector<ScopedHandle> hReads(n - 1), hWrites(n - 1);
        SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

        for (size_t i = 0; i < n - 1; ++i) {
            HANDLE r, w;
            if (!CreatePipe(&r, &w, &sa, 0)) return 1;
            hReads[i] = ScopedHandle(r);
            hWrites[i] = ScopedHandle(w);
        }

        std::vector<int> exit_codes(n, 0);
        std::vector<std::thread> stage_threads;

        for (size_t i = 0; i < n; ++i) {
            HANDLE stage_in = (i == 0) ? hIn : hReads[i - 1].get();
            HANDLE stage_out = (i == n - 1) ? hOut : hWrites[i].get();

            stage_threads.emplace_back([this, i, &ctx, stage_in, stage_out, depth, &exit_codes, &hReads, &hWrites, n]() {
                ShellContext stage_ctx(false);
                {
                    std::lock_guard<std::mutex> lock(ctx.ctx_mutex);
                    stage_ctx.variables = ctx.variables;
                    stage_ctx.last_return_code = ctx.last_return_code;
                    stage_ctx.timeout_ms = ctx.timeout_ms;
                    stage_ctx.audit_logging_enabled = ctx.audit_logging_enabled;
                    stage_ctx.pipeline_stage = true;
                }
                try {
                    exit_codes[i] = stages[i]->execute(stage_ctx, stage_in, stage_out, depth + 1);
                } catch (const ShellExitSignal& signal) {
                    exit_codes[i] = signal.code();
                }
                restore_nested_environment(ctx, stage_ctx);
                
                if (i < n - 1) hWrites[i].close();
                if (i > 0) hReads[i - 1].close();
            });
        }

        for (auto& th : stage_threads) {
            if (th.joinable()) th.join();
        }

        return exit_codes.back();
    }
};

class ChainNode : public ASTNode {
public:
    std::shared_ptr<ASTNode> left;
    std::shared_ptr<ASTNode> right;
    LexTokenType op;

    int execute_impl(ShellContext& ctx, HANDLE hIn, HANDLE hOut, size_t depth) override {
        int res = left->execute(ctx, hIn, hOut, depth + 1);
        ctx.last_return_code = res;
        if (this->op == LexTokenType::And && res != 0) return res;
        if (this->op == LexTokenType::Or && res == 0) return res;
        if (right) return right->execute(ctx, hIn, hOut, depth + 1);
        return res;
    }
};

class IfNode : public ASTNode {
public:
    std::shared_ptr<ASTNode> cond;
    std::shared_ptr<ASTNode> then_body;
    std::shared_ptr<ASTNode> else_body;

    int execute_impl(ShellContext& ctx, HANDLE hIn, HANDLE hOut, size_t depth) override {
        int res = cond->execute(ctx, hIn, hOut, depth + 1);
        if (res == 0) return then_body ? then_body->execute(ctx, hIn, hOut, depth + 1) : 0;
        else return else_body ? else_body->execute(ctx, hIn, hOut, depth + 1) : 0;
    }
};

class WhileNode : public ASTNode {
public:
    std::shared_ptr<ASTNode> cond;
    std::shared_ptr<ASTNode> body;

    int execute_impl(ShellContext& ctx, HANDLE hIn, HANDLE hOut, size_t depth) override {
        int last_code = 0;
        while (cond && cond->execute(ctx, hIn, hOut, depth + 1) == 0) {
            if (body) last_code = body->execute(ctx, hIn, hOut, depth + 1);
        }
        return last_code;
    }
};

class ForNode : public ASTNode {
public:
    std::string var_name;
    std::vector<std::string> raw_values;
    std::shared_ptr<ASTNode> body;

    int execute_impl(ShellContext& ctx, HANDLE hIn, HANDLE hOut, size_t depth) override {
        if (!body) return 0;

        std::vector<ArgString> expanded_objs;
        for (const auto& val : raw_values) {
            expanded_objs.push_back(expand_arg_variables(val, ctx));
        }
        std::vector<std::string> expanded_values = expand_globs(expanded_objs);

        int last_code = 0;
        if (expanded_values.empty()) {
            ctx.set_var(var_name, "");
            return body->execute(ctx, hIn, hOut, depth + 1);
        }

        for (const auto& value : expanded_values) {
            ctx.set_var(var_name, value);
            last_code = body->execute(ctx, hIn, hOut, depth + 1);
        }
        return last_code;
    }
};

class SubshellNode : public ASTNode {
public:
    std::shared_ptr<ASTNode> child;

    int execute_impl(ShellContext& ctx, HANDLE hIn, HANDLE hOut, size_t depth) override {
        if (!child) return 0;
        ShellContext sub_ctx(false);
        {
            std::lock_guard<std::mutex> lock(ctx.ctx_mutex);
            sub_ctx.variables = ctx.variables;
            sub_ctx.last_return_code = ctx.last_return_code;
            sub_ctx.timeout_ms = ctx.timeout_ms;
            sub_ctx.audit_logging_enabled = ctx.audit_logging_enabled;
        }
        ScopedWorkingDirectory cwd_guard;
        int res = 0;
        try {
            res = child->execute(sub_ctx, hIn, hOut, depth + 1);
        } catch (const ShellExitSignal& signal) {
            res = signal.code();
        } catch (...) {
            restore_nested_environment(ctx, sub_ctx);
            throw;
        }
        restore_nested_environment(ctx, sub_ctx);
        ctx.last_return_code = res;
        return res;
    }
};

class Parser {
    std::vector<Token> tokens_;
    size_t pos_ = 0;

    Token peek() const { return pos_ < tokens_.size() ? tokens_[pos_] : Token{LexTokenType::Newline, ""}; }
    Token get() { return pos_ < tokens_.size() ? tokens_[pos_++] : Token{LexTokenType::Newline, ""}; }

public:
    explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

    bool is_block_stop_word(const std::string& word) const {
        static const std::set<std::string> stop_words = {"then", "else", "elif", "fi", "do", "done", "in", "esac"};
        return !word.empty() && stop_words.count(to_lower(word)) > 0;
    }

    bool should_stop(const std::initializer_list<std::string>& stop_words) const {
        if (peek().type != LexTokenType::Word) return false;
        std::string word = to_lower(peek().value);
        for (const auto& stop : stop_words) {
            if (word == stop) return true;
        }
        return false;
    }

    std::shared_ptr<ASTNode> parse_command(std::initializer_list<std::string> stop_words = {}) {
        std::shared_ptr<ASTNode> node;

        if (peek().type == LexTokenType::Word && should_stop(stop_words)) {
            return nullptr;
        }

        if (peek().type == LexTokenType::LParen) {
            get();
            auto sub_node = std::make_shared<SubshellNode>();
            sub_node->child = parse_chain();
            if (peek().type == LexTokenType::RParen) {
                get();
            }
            node = sub_node;
        } else if (peek().type == LexTokenType::Word && peek().value == "if") {
            get();
            auto if_node = std::make_shared<IfNode>();
            if_node->cond = parse_chain({"then", "else", "elif", "fi"});
            if (peek().type == LexTokenType::Word && to_lower(peek().value) == "then") get();
            if_node->then_body = parse_chain({"else", "elif", "fi"});
            if (peek().type == LexTokenType::Word && to_lower(peek().value) == "else") {
                get(); if_node->else_body = parse_chain({"fi"});
            }
            if (peek().type == LexTokenType::Word && to_lower(peek().value) == "fi") get();
            node = if_node;
        } else if (peek().type == LexTokenType::Word && peek().value == "while") {
            get();
            auto while_node = std::make_shared<WhileNode>();
            while_node->cond = parse_chain({"do", "done"});
            if (peek().type == LexTokenType::Word && to_lower(peek().value) == "do") get();
            while_node->body = parse_chain({"done"});
            if (peek().type == LexTokenType::Word && to_lower(peek().value) == "done") get();
            node = while_node;
        } else if (peek().type == LexTokenType::Word && peek().value == "for") {
            get();
            auto for_node = std::make_shared<ForNode>();
            if (peek().type == LexTokenType::Word) {
                for_node->var_name = get().value;
            }
            if (peek().type == LexTokenType::Word && to_lower(peek().value) == "in") {
                get();
                while (pos_ < tokens_.size()) {
                    Token t = peek();
                    if (t.type == LexTokenType::Word && !is_block_stop_word(t.value)) {
                        for_node->raw_values.push_back(get().value);
                    } else {
                        break;
                    }
                }
            }
            if (peek().type == LexTokenType::Word && to_lower(peek().value) == "do") get();
            for_node->body = parse_chain({"done"});
            if (peek().type == LexTokenType::Word && to_lower(peek().value) == "done") get();
            node = for_node;
        } else {
            auto cmd = std::make_shared<CommandNode>();
            while (pos_ < tokens_.size()) {
                Token t = peek();
                if (t.type == LexTokenType::Word) { cmd->args.push_back(get().value); }
                else if (t.type == LexTokenType::RedirectOut) {
                    get(); if (peek().type == LexTokenType::Word) cmd->redirect_out = get().value; cmd->append_out = false;
                }
                else if (t.type == LexTokenType::RedirectAppend) {
                    get(); if (peek().type == LexTokenType::Word) cmd->redirect_out = get().value; cmd->append_out = true;
                }
                else if (t.type == LexTokenType::RedirectIn) {
                    get(); if (peek().type == LexTokenType::Word) cmd->redirect_in = get().value;
                }
                else break;
            }
            node = cmd;
        }

        while (pos_ < tokens_.size()) {
            Token t = peek();
            if (t.type == LexTokenType::RedirectOut) {
                get(); if (peek().type == LexTokenType::Word) node->redirect_out = get().value; node->append_out = false;
            } else if (t.type == LexTokenType::RedirectAppend) {
                get(); if (peek().type == LexTokenType::Word) node->redirect_out = get().value; node->append_out = true;
            } else if (t.type == LexTokenType::RedirectIn) {
                get(); if (peek().type == LexTokenType::Word) node->redirect_in = get().value;
            } else if (t.type == LexTokenType::Ampersand) {
                get(); node->is_background = true;
            } else break;
        }

        return node;
    }

    std::shared_ptr<ASTNode> parse_pipeline(std::initializer_list<std::string> stop_words = {}) {
        auto pipe_node = std::make_shared<PipelineNode>();
        auto first = parse_command(stop_words);
        if (!first) return nullptr;
        pipe_node->stages.push_back(first);
        while (peek().type == LexTokenType::Pipe) {
            get();
            auto next = parse_command(stop_words);
            if (!next) break;
            pipe_node->stages.push_back(next);
        }
        return pipe_node;
    }

    std::shared_ptr<ASTNode> parse_chain(std::initializer_list<std::string> stop_words = {}) {
        auto left = parse_pipeline(stop_words);
        if (!left) return nullptr;
        while (peek().type == LexTokenType::And || peek().type == LexTokenType::Or || 
               peek().type == LexTokenType::Semi || peek().type == LexTokenType::Newline) {
            Token op = get();
            if (op.type == LexTokenType::Newline || op.type == LexTokenType::Semi) {
                if (pos_ >= tokens_.size()) break;
                if (peek().type == LexTokenType::Newline || peek().type == LexTokenType::Semi) continue;
                auto right = parse_pipeline(stop_words);
                if (!right) break;
                auto chain = std::make_shared<ChainNode>();
                chain->left = left; chain->right = right; chain->op = op.type;
                left = chain;
            } else {
                auto right = parse_pipeline(stop_words);
                if (!right) break;
                auto chain = std::make_shared<ChainNode>();
                chain->left = left; chain->right = right; chain->op = op.type;
                left = chain;
            }
        }
        return left;
    }
};

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
