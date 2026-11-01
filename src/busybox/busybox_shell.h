#pragma once

// Shell AST, parser, and execution engine for the busybox multi-call binary.

#include "busybox.h"
#include "busybox_runtime.h"

// ============================================================================
// ABSTRACT SYNTAX TREE (AST) & SECURE WIN32 EXECUTION ENGINE
// ============================================================================

class ASTNode {
public:
    std::string redirect_in;
    std::string redirect_out;
    std::string redirect_err;
    std::string heredoc_body;   // here-document content (<<)
    std::string herestring;     // here-string content (<<<)
    bool append_out = false;
    bool err_to_out = false;
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
        // Here-document / here-string: feed the inline content as stdin via a pipe.
        ScopedHandle hPipeInRead, hPipeInWrite;
        std::string inlineInput;
        if (!heredoc_body.empty()) inlineInput = heredoc_body;
        else if (!herestring.empty()) inlineInput = herestring + "\n";
        if (!inlineInput.empty()) {
            SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
            HANDLE r, w;
            if (CreatePipe(&r, &w, &sa, 0)) {
                DWORD written = 0;
                WriteFile(w, inlineInput.data(), static_cast<DWORD>(inlineInput.size()), &written, NULL);
                CloseHandle(w); // signal EOF to the reader
                hPipeInRead = ScopedHandle(r);
                effective_in = hPipeInRead.get();
            }
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

    int execute_impl(ShellContext& ctx, HANDLE hIn, HANDLE hOut, size_t depth) override;
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
                // Pipeline stages run concurrently and must NOT merge their
                // shell variables back into the shared parent context (that
                // races). Each stage's process-environment changes are reverted
                // to the parent's values, restoring the pre-pipeline process env.
                restore_process_environment(ctx, stage_ctx);

                if (i < n - 1) hWrites[i].close();
                if (i > 0) hReads[i - 1].close();
            });
        }

        for (auto& th : stage_threads) {
            if (th.joinable()) th.join();
        }

        // pipefail: return the rightmost non-zero stage status; otherwise the
        // last stage's status (POSIX default).
        if (ctx.pipefail) {
            for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
                if (exit_codes[i] != 0) return exit_codes[i];
            }
            return 0;
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

class CaseNode : public ASTNode {
public:
    std::string subject;                          // the word being matched
    std::vector<std::pair<std::vector<std::string>, std::shared_ptr<ASTNode>>> clauses; // (patterns, body)

    int execute_impl(ShellContext& ctx, HANDLE hIn, HANDLE hOut, size_t depth) override {
        // Expand the subject once, then match against each clause's patterns.
        std::string value = argstring_to_string(expand_arg_variables(subject, ctx));
        for (const auto& clause : clauses) {
            for (const auto& pat : clause.first) {
                std::string expandedPat = argstring_to_string(expand_arg_variables(pat, ctx));
                if (wildcard_match(value, expandedPat)) {
                    return clause.second ? clause.second->execute(ctx, hIn, hOut, depth + 1) : 0;
                }
            }
        }
        return 0; // no clause matched
    }
};

// Function definition: name() { body }. Stores the body source in the context's
// function table; calling the name later re-parses and executes it.
class FunctionDefNode : public ASTNode {
public:
    std::string name;
    std::string body_source;

    int execute_impl(ShellContext& ctx, HANDLE, HANDLE, size_t) override {
        std::lock_guard<std::mutex> lock(ctx.ctx_mutex);
        ctx.functions[to_lower(name)] = body_source;
        return 0;
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
            // Revert the subshell's process-env changes to the parent's values;
            // do not merge its shell variables back (subshells are isolated).
            restore_process_environment(ctx, sub_ctx);
            throw;
        }
        // Subshell isolation: revert the subshell's process-env changes to the
        // parent's values, but do NOT copy its shell variables back.
        restore_process_environment(ctx, sub_ctx);
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

    // Collect here-document body: read raw tokens until a line whose trimmed
    // content equals `delim`. Reconstructs the body text with newlines.
    std::string collect_heredoc(const std::string& delim) {
        std::string body;
        std::string currentLine;
        // Skip to the first newline (the delimiter line itself ends the << line).
        while (pos_ < tokens_.size() && peek().type != LexTokenType::Newline) get();
        if (peek().type == LexTokenType::Newline) get(); // consume it

        while (pos_ < tokens_.size()) {
            Token t = get();
            if (t.type == LexTokenType::Newline) {
                std::string trimmed = currentLine;
                size_t b = trimmed.find_first_not_of(" \t\r");
                size_t e = trimmed.find_last_not_of(" \t\r");
                trimmed = (b == std::string::npos) ? "" : trimmed.substr(b, e - b + 1);
                if (trimmed == delim) break;
                body += currentLine + "\n";
                currentLine.clear();
            } else {
                if (!currentLine.empty() && t.type == LexTokenType::Word) currentLine += " ";
                currentLine += t.value;
            }
        }
        return body;
    }

    // Reconstruct the source text of a brace/paren-delimited block, consuming
    // tokens up to (and including) the matching closer. Handles nesting.
    std::string collect_block_source(const std::string& closer) {
        std::string src;
        int depth = 0;
        LexTokenType openType = (closer == "}") ? LexTokenType::LBrace : LexTokenType::LParen;
        LexTokenType closeType = (closer == "}") ? LexTokenType::RBrace : LexTokenType::RParen;
        while (pos_ < tokens_.size()) {
            Token t = get();
            if (t.type == openType) depth++;
            if (t.type == closeType) {
                if (depth == 0) break; // the matching closer
                depth--;
            }
            if (!src.empty() && t.type != LexTokenType::Newline) src += " ";
            if (t.type == LexTokenType::Newline) src += "\n";
            else src += t.value;
        }
        return src;
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
        } else if (peek().type == LexTokenType::Word && peek().value == "case") {
            get();
            auto case_node = std::make_shared<CaseNode>();
            // case WORD in
            if (peek().type == LexTokenType::Word) case_node->subject = get().value;
            if (peek().type == LexTokenType::Word && to_lower(peek().value) == "in") get();

            // Parse clauses: pattern [| pattern ...] ) body ;; ... esac
            while (pos_ < tokens_.size()) {
                // Skip separators/newlines between clauses.
                while (peek().type == LexTokenType::Newline || peek().type == LexTokenType::Semi) get();
                if (peek().type == LexTokenType::Word && to_lower(peek().value) == "esac") { get(); break; }
                if (pos_ >= tokens_.size()) break;

                // Collect patterns until ')'.
                std::vector<std::string> patterns;
                while (pos_ < tokens_.size()) {
                    Token t = peek();
                    if (t.type == LexTokenType::RParen) { get(); break; }
                    if (t.type == LexTokenType::Word) {
                        // '|' may arrive as a Pipe token between patterns.
                        patterns.push_back(get().value);
                    } else if (t.type == LexTokenType::Pipe) {
                        get(); // pattern alternation
                    } else {
                        break;
                    }
                }

                // Parse the clause body up to ';;' (two consecutive Semi tokens)
                // or 'esac'. We parse commands one at a time and stop when we see
                // the ';;' terminator.
                std::shared_ptr<ASTNode> body;
                while (pos_ < tokens_.size()) {
                    // Detect ';;' = two Semi tokens in a row.
                    if (peek().type == LexTokenType::Semi && pos_ + 1 < tokens_.size() &&
                        tokens_[pos_ + 1].type == LexTokenType::Semi) {
                        break;
                    }
                    if (peek().type == LexTokenType::Word && to_lower(peek().value) == "esac") break;
                    if (peek().type == LexTokenType::Newline) { get(); continue; }

                    auto cmd = parse_pipeline({"esac"});
                    if (!cmd) break;
                    if (!body) {
                        body = cmd;
                    } else {
                        auto chain = std::make_shared<ChainNode>();
                        chain->left = body; chain->right = cmd; chain->op = LexTokenType::Semi;
                        body = chain;
                    }
                    // Consume a single statement separator (but not ';;').
                    if (peek().type == LexTokenType::Semi && pos_ + 1 < tokens_.size() &&
                        tokens_[pos_ + 1].type == LexTokenType::Semi) {
                        break;
                    }
                    if (peek().type == LexTokenType::Semi || peek().type == LexTokenType::Newline) get();
                }
                // Consume the ';;' terminator.
                if (peek().type == LexTokenType::Semi && pos_ + 1 < tokens_.size() &&
                    tokens_[pos_ + 1].type == LexTokenType::Semi) {
                    get(); get();
                }

                if (!patterns.empty()) {
                    case_node->clauses.push_back({patterns, body});
                }
            }
            node = case_node;
        } else if (peek().type == LexTokenType::Word && pos_ + 2 < tokens_.size() &&
                   tokens_[pos_ + 1].type == LexTokenType::LParen && tokens_[pos_ + 2].type == LexTokenType::RParen) {
            // Function definition: name() { body }  or  name() ( body )
            auto fn_node = std::make_shared<FunctionDefNode>();
            fn_node->name = get().value; // function name
            get(); // (
            get(); // )
            // Skip separators before the body opener.
            while (peek().type == LexTokenType::Newline || peek().type == LexTokenType::Semi) get();
            if (peek().type == LexTokenType::LBrace) {
                get(); // {
                fn_node->body_source = collect_block_source("}");
            } else if (peek().type == LexTokenType::LParen) {
                get(); // (
                fn_node->body_source = collect_block_source(")");
            }
            node = fn_node;
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
                else if (t.type == LexTokenType::RedirectErr) {
                    get(); if (peek().type == LexTokenType::Word) cmd->redirect_err = get().value;
                }
                else if (t.type == LexTokenType::RedirectErrToOut) {
                    get(); cmd->err_to_out = true;
                }
                else if (t.type == LexTokenType::RedirectHerestring) {
                    get(); if (peek().type == LexTokenType::Word) cmd->herestring = get().value;
                }
                else if (t.type == LexTokenType::RedirectHeredoc) {
                    get();
                    if (peek().type == LexTokenType::Word) {
                        std::string delim = get().value;
                        cmd->heredoc_body = collect_heredoc(delim);
                    }
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
            } else if (t.type == LexTokenType::RedirectErr) {
                get(); if (peek().type == LexTokenType::Word) node->redirect_err = get().value;
            } else if (t.type == LexTokenType::RedirectErrToOut) {
                get(); node->err_to_out = true;
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

std::string capture_cmd_output(const std::string& cmd, ShellContext& ctx);
