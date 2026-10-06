#include "parser.hpp"
#include "engine.hpp"
#include "expansion.hpp"

string trim_copy(const string& s) {
    size_t b = 0;
    while (b < s.size() && isspace((unsigned char)s[b])) ++b;
    size_t e = s.size();
    while (e > b && isspace((unsigned char)s[e - 1])) --e;
    return s.substr(b, e - b);
}

vector<string> tokenize_words(const string& s) {
    vector<string> tokens;
    string current;
    bool in_dq = false, in_sq = false;
    for (char c : s) {
        if      (c == '"' && !in_sq) in_dq = !in_dq;
        else if (c == '\'' && !in_dq) in_sq = !in_sq;
        else if (isspace(c) && !in_dq && !in_sq) {
            if (!current.empty()) { tokens.push_back(current); current.clear(); }
        } else current += c;
    }
    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

Pipeline parse_pipeline(const string& line) {
    struct ShellToken {
        string raw;
        bool quoted = false;
    };

    Pipeline pl;
    string current = "";
    bool in_dq = false, in_sq = false;
    bool current_started = false;
    bool current_quoted = false;
    int assignment_list_depth = 0;
    int pattern_group_depth = 0;
    int double_bracket_depth = 0;
    string lexical_error;
    vector<ShellToken> raw_tokens;

    auto flush_current = [&]() {
        if (current_started) {
            raw_tokens.push_back({current, current_quoted});
            current.clear();
            current_started = false;
            current_quoted = false;
        }
    };

    auto push_operator = [&](string text) {
        raw_tokens.push_back({std::move(text), false});
    };

    for (size_t i = 0; i < line.length(); ++i) {
        char c = line[i];
        if (c == '\\' && in_dq && i + 1 < line.length() && line[i+1] == '"') {
            current += '\\'; current += '"'; current_started = true; ++i;
        } else if (c == '"' && !in_sq) {
            in_dq = !in_dq; current += c; current_started = true; current_quoted = true;
        }
        else if (c == '\'' && !in_dq) {
            in_sq = !in_sq; current += c; current_started = true; current_quoted = true;
        }
        else if (c == '#' && !in_dq && !in_sq && current.empty()) break; // inline comment
        else if (!in_dq && !in_sq) {
            if (c == '[' && i + 1 < line.length() && line[i + 1] == '[') {
                flush_current();
                push_operator("[[");
                ++double_bracket_depth;
                ++i;
                continue;
            }
            if (c == ']' && i + 1 < line.length() && line[i + 1] == ']' && double_bracket_depth > 0) {
                flush_current();
                push_operator("]]");
                --double_bracket_depth;
                ++i;
                continue;
            }
            if (double_bracket_depth > 0) {
                if (isspace((unsigned char)c)) {
                    flush_current();
                    continue;
                }
                current += c;
                current_started = true;
                continue;
            }
            if (c == '(' && (pattern_group_depth > 0 || (!current.empty() && current.back() == '@'))) {
                ++pattern_group_depth;
                current += c;
                current_started = true;
                continue;
            }
            if (c == ')' && pattern_group_depth > 0) {
                --pattern_group_depth;
                current += c;
                current_started = true;
                continue;
            }
            if (c == '(' && (assignment_list_depth > 0 || (!current.empty() && current.back() == '='))) {
                ++assignment_list_depth;
                current += c;
                current_started = true;
                continue;
            }
            if (c == ')' && assignment_list_depth > 0) {
                --assignment_list_depth;
                current += c;
                current_started = true;
                continue;
            }
            if (c == '$' && i + 1 < line.length() && line[i + 1] == '(') {
                size_t j = i + 2;
                int depth = 1;
                bool csq = false;
                bool cdq = false;
                for (; j < line.length(); ++j) {
                    char cj = line[j];
                    if (cj == '"' && !csq) { cdq = !cdq; continue; }
                    if (cj == '\'' && !cdq) { csq = !csq; continue; }
                    if (csq || cdq) continue;
                    if (cj == '(') { depth++; continue; }
                    if (cj == ')') {
                        depth--;
                        if (depth == 0) { ++j; break; }
                    }
                }
                if (depth != 0) {
                    lexical_error = "unmatched command substitution '('";
                    break;
                }
                current += line.substr(i, j - i);
                current_started = true;
                i = (j == 0) ? i : (j - 1);
                continue;
            }
            if (isspace((unsigned char)c) && assignment_list_depth == 0 && pattern_group_depth == 0) {
                flush_current();
                continue;
            }
            if ((c == '<' || c == '>' || (c == '=' && (current.empty() || !is_valid_env_var_name(current)))) &&
                i + 1 < line.length() && line[i + 1] == '(') {
                size_t j = i + 2;
                int depth = 1;
                bool psq = false;
                bool pdq = false;
                for (; j < line.length(); ++j) {
                    char cj = line[j];
                    if (cj == '"' && !psq) { pdq = !pdq; continue; }
                    if (cj == '\'' && !pdq) { psq = !psq; continue; }
                    if (psq || pdq) continue;
                    if (cj == '(') { depth++; continue; }
                    if (cj == ')') {
                        depth--;
                        if (depth == 0) { ++j; break; }
                    }
                }
                if (depth != 0) {
                    lexical_error = "unmatched process substitution '('";
                    break;
                }
                current += line.substr(i, j - i);
                current_started = true;
                i = (j == 0) ? i : (j - 1);
                continue;
            }
            if (c == '|' && pattern_group_depth == 0) {
                flush_current();
                push_operator("|");
                continue;
            }
            if (c == '&' && i + 1 < line.length() && line[i + 1] == '>') {
                flush_current();
                if (i + 2 < line.length() && (line[i + 2] == '|' || line[i + 2] == '!')) {
                    push_operator("&>|");
                    i += 2;
                } else if (i + 2 < line.length() && line[i + 2] == '>') {
                    push_operator("&>>");
                    i += 2;
                } else {
                    push_operator("&>");
                    ++i;
                }
                continue;
            }
            if (c == '&' && assignment_list_depth == 0 && pattern_group_depth == 0) {
                flush_current();
                push_operator("&");
                continue;
            }
            if (isdigit((unsigned char)c) && i + 2 < line.length() && line[i + 1] == '<' && line[i + 2] == '>') {
                flush_current();
                push_operator(string(1, c) + "<>");
                i += 2;
                continue;
            }
            if (isdigit((unsigned char)c) && i + 2 < line.length() && line[i + 1] == '>' && (line[i + 2] == '|' || line[i + 2] == '!')) {
                flush_current();
                push_operator(string(1, c) + ">|");
                i += 2;
                continue;
            }
            if (isdigit((unsigned char)c) && i + 2 < line.length() && line[i + 1] == '>' && line[i + 2] == '&') {
                flush_current();
                push_operator(string(1, c) + ">&");
                i += 2;
                continue;
            }
            if (isdigit((unsigned char)c) && i + 2 < line.length() && line[i + 1] == '<' && line[i + 2] == '&') {
                flush_current();
                push_operator(string(1, c) + "<&");
                i += 2;
                continue;
            }
            if (isdigit((unsigned char)c) && i + 2 < line.length() && line[i + 1] == '>' && line[i + 2] == '>') {
                flush_current();
                push_operator(string(1, c) + ">>");
                i += 2;
                continue;
            }
            if (isdigit((unsigned char)c) && i + 1 < line.length() && line[i + 1] == '>') {
                flush_current();
                push_operator(string(1, c) + ">");
                i += 1;
                continue;
            }
            if (isdigit((unsigned char)c) && i + 1 < line.length() && line[i + 1] == '<') {
                flush_current();
                push_operator(string(1, c) + "<");
                i += 1;
                continue;
            }
            if (c == '<') {
                flush_current();
                if (i + 1 < line.length() && line[i + 1] == '>') {
                    push_operator("<>");
                    i += 1;
                } else if (i + 2 < line.length() && line[i + 1] == '<' && line[i + 2] == '<') {
                    push_operator("<<<");
                    i += 2;
                } else if (i + 2 < line.length() && line[i + 1] == '<' && line[i + 2] == '-') {
                    push_operator("<<-");
                    i += 2;
                } else if (i + 1 < line.length() && line[i + 1] == '<') {
                    push_operator("<<");
                    i += 1;
                } else if (i + 1 < line.length() && line[i + 1] == '&') {
                    push_operator("<&");
                    i += 1;
                } else {
                    push_operator("<");
                }
                continue;
            }
            if (c == '>') {
                flush_current();
                if (i + 1 < line.length() && (line[i + 1] == '|' || line[i + 1] == '!')) {
                    push_operator(">|");
                    i += 1;
                } else if (i + 2 < line.length() && line[i + 1] == '>' && line[i + 2] == '|') {
                    push_operator(">>|");
                    i += 2;
                } else if (i + 1 < line.length() && line[i + 1] == '>') {
                    push_operator(">>");
                    i += 1;
                } else if (i + 1 < line.length() && line[i + 1] == '&') {
                    push_operator(">&");
                    i += 1;
                } else {
                    push_operator(">");
                }
                continue;
            }
            current += c;
            current_started = true;
        } else {
            current += c;
            current_started = true;
        }
    }
    flush_current();

    if (lexical_error.empty() && in_dq) lexical_error = "unmatched '\"'";
    else if (lexical_error.empty() && in_sq) lexical_error = "unmatched '\''";
    else if (lexical_error.empty() && assignment_list_depth != 0) lexical_error = "unmatched array assignment '('";
    else if (lexical_error.empty() && pattern_group_depth != 0) lexical_error = "unmatched pattern group '('";
    else if (lexical_error.empty() && double_bracket_depth != 0) lexical_error = "unmatched '[['";
    if (!lexical_error.empty()) {
        pl.error = std::move(lexical_error);
        return pl;
    }

    if (!raw_tokens.empty() && raw_tokens.back().raw == "&") {
        pl.background = true; raw_tokens.pop_back();
    }

    auto expand_token = [&](const ShellToken& token) {
        string expanded = g_env.expand_vars(token.raw);
        if (token.raw.size() >= 2 &&
            ((token.raw.front() == '"' && token.raw.back() == '"') ||
             (token.raw.front() == '\'' && token.raw.back() == '\'')) &&
            expanded.size() >= 2 && expanded.front() == token.raw.front() && expanded.back() == token.raw.back()) {
            expanded = expanded.substr(1, expanded.size() - 2);
        }
        return expanded;
    };

    SingleCmd sc;
    auto parse_fd_dup = [](const string& s, int& out_fd, bool& out_close) {
        out_fd = -1;
        out_close = false;
        if (s == "-") { out_close = true; return true; }
        if (s.size() == 1 && isdigit((unsigned char)s[0])) { out_fd = s[0] - '0'; return true; }
        return false;
    };

    auto finish_command = [&]() {
        if (sc.args.empty()) {
            pl.error = "missing pipeline command";
            return false;
        }
        if (sc.args.empty() || sc.args[0] != "[[")
            sc.args = expand_globs(sc.args, &sc.glob_allowed);
        pl.cmds.push_back(std::move(sc));
        sc = SingleCmd();
        return true;
    };

    bool needs_command_after_pipe = false;
    auto append_argument = [&](string value, bool allow_glob) {
        sc.args.push_back(std::move(value));
        sc.glob_allowed.push_back(allow_glob);
        needs_command_after_pipe = false;
    };

    auto parameter_flag_delimiter = [](const string& flags, char flag, string& delimiter) {
        string marker;
        marker += flag;
        marker += ':';
        size_t start = flags.find(marker);
        if (start == string::npos) return false;
        start += marker.size();
        size_t end = flags.find(':', start);
        if (end == string::npos) return false;
        delimiter = flags.substr(start, end - start);
        return true;
    };

    auto append_flagged_parameter = [&](const string& raw, bool quoted) {
        string expression = raw;
        if (expression.size() >= 2 &&
            ((expression.front() == '"' && expression.back() == '"') ||
             (expression.front() == '\'' && expression.back() == '\''))) {
            expression = expression.substr(1, expression.size() - 2);
        }

        smatch match;
        static const regex flagged_parameter_re(
            R"(^\$\{\(([^)]*)\)([A-Za-z_][A-Za-z0-9_]*)(?:\[@\])?\}$)");
        if (!regex_match(expression, match, flagged_parameter_re)) return false;

        string flags = match[1].str();
        string name = match[2].str();
        vector<string> values;
        auto array = g_env.indexed_arrays.find(name);
        if (array != g_env.indexed_arrays.end()) {
            values = array->second;
        } else {
            string value;
            auto scalar = g_env.vars.find(name);
            if (scalar != g_env.vars.end()) value = scalar->second;
            else if (const char* environment_value = getenv(name.c_str())) value = environment_value;
            values.push_back(std::move(value));
        }

        for (auto& value : values) {
            if (flags.find('L') != string::npos) {
                transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(tolower(c)); });
            }
            if (flags.find('U') != string::npos) {
                transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(toupper(c)); });
            }
            if (flags.find('q') != string::npos) {
                string shell_quoted = "'";
                for (char ch : value) shell_quoted += ch == '\'' ? "'\\''" : string(1, ch);
                value = shell_quoted + "'";
            }
        }

        string delimiter;
        if (parameter_flag_delimiter(flags, 'j', delimiter)) {
            string joined;
            for (size_t i = 0; i < values.size(); ++i) {
                if (i > 0) joined += delimiter;
                joined += values[i];
            }
            append_argument(std::move(joined), !quoted);
            return true;
        }
        if (parameter_flag_delimiter(flags, 's', delimiter)) {
            string source = values.empty() ? "" : values.front();
            if (delimiter.empty()) {
                for (char ch : source) append_argument(string(1, ch), !quoted);
            } else {
                size_t start = 0;
                while (true) {
                    size_t end = source.find(delimiter, start);
                    append_argument(source.substr(start, end - start), !quoted);
                    if (end == string::npos) break;
                    start = end + delimiter.size();
                }
            }
            return true;
        }

        for (auto& value : values) append_argument(std::move(value), !quoted);
        return true;
    };

    for (size_t i = 0; i < raw_tokens.size(); ++i) {
        const ShellToken& token = raw_tokens[i];
        const string& t = token.raw;

        bool in_double_bracket = !sc.args.empty() && sc.args[0] == "[[";
        if (in_double_bracket) {
            append_argument(expand_token(token), false);
            continue;
        }

        smatch extra_fd_match;
        static const regex extra_file_redirect(R"(^([0-9])([<>]|<>|>\||>!)$)");
        static const regex extra_dup_redirect(R"(^([0-9])([<>])&$)");
        static const set<string> operand_operators = {
            ">", ">|", ">!", ">>", ">>|", "1>", "1>|", "1>!", "1>>", "1>>|",
            "2>", "2>|", "2>!", "2>>", "2>>|", "<", "<>", "0<", "0<>", "0<<", "<<<",
            "2>&", ">&", "1>&", "0<&", "<&", "&>", "&>|", "&>!", "&>>"
        };
        bool is_extra_redirect = regex_match(t, extra_fd_match, extra_file_redirect) ||
                                 regex_match(t, extra_fd_match, extra_dup_redirect);
        if ((operand_operators.count(t) || is_extra_redirect) && i + 1 >= raw_tokens.size()) {
            pl.error = "missing redirection operand after '" + t + "'";
            return pl;
        }
        if (regex_match(t, extra_fd_match, extra_file_redirect) && i + 1 < raw_tokens.size()) {
            int fd = extra_fd_match[1].str()[0] - '0';
            string op_type = extra_fd_match[2].str();
            if (op_type == ">" || op_type == ">|" || op_type == ">!") {
                if (fd == 1) { sc.output_file = expand_token(raw_tokens[++i]); sc.append_out = false; sc.output_files.push_back({sc.output_file, false}); }
                else if (fd == 2) { sc.error_file = expand_token(raw_tokens[++i]); sc.append_err = false; }
                else sc.extra_output_files[fd] = {expand_token(raw_tokens[++i]), false};
            } else {
                if (fd == 0) { sc.input_file = expand_token(raw_tokens[++i]); }
                else sc.extra_input_files[fd] = expand_token(raw_tokens[++i]);
            }
        } else if (regex_match(t, extra_fd_match, extra_dup_redirect) && i + 1 < raw_tokens.size()) {
            int fd = extra_fd_match[1].str()[0] - '0';
            int source_fd = -1;
            bool close_fd = false;
            if (!parse_fd_dup(raw_tokens[++i].raw, source_fd, close_fd)) {
                pl.error = "invalid file descriptor after '" + t + "'";
                return pl;
            }
            if (fd == 1) {
                if (close_fd) sc.close_stdout = true;
                else sc.dup_stdout_from = source_fd;
            } else if (fd == 2) {
                if (close_fd) sc.close_stderr = true;
                else sc.dup_stderr_from = source_fd;
            } else if (fd == 0) {
                if (close_fd) sc.close_stdin = true;
                else sc.dup_stdin_from = source_fd;
            } else {
                if (close_fd) sc.extra_closed_fds.insert(fd);
                else sc.extra_fd_duplications[fd] = source_fd;
            }
        } else if (t == "|") {
            if (!finish_command()) return pl;
            needs_command_after_pipe = true;
        } else if ((t == ">" || t == ">|" || t == ">!") && i + 1 < raw_tokens.size()) {
            sc.output_file = expand_token(raw_tokens[++i]); sc.append_out = false;
            sc.output_files.push_back({sc.output_file, sc.append_out});
        } else if ((t == ">>" || t == ">>|") && i + 1 < raw_tokens.size()) {
            sc.output_file = expand_token(raw_tokens[++i]); sc.append_out = true;
            sc.output_files.push_back({sc.output_file, sc.append_out});
        } else if ((t == "1>" || t == "1>|" || t == "1>!" || t == "1>>" || t == "1>>|") && i + 1 < raw_tokens.size()) {
            sc.output_file = expand_token(raw_tokens[++i]); sc.append_out = (t.find(">>") != string::npos);
            sc.output_files.push_back({sc.output_file, sc.append_out});
        } else if ((t == "<" || t == "<>") && i + 1 < raw_tokens.size()) {
            sc.input_file = expand_token(raw_tokens[++i]);
        } else if (t == "<<<" && i + 1 < raw_tokens.size()) {
            sc.here_string = expand_token(raw_tokens[++i]);
        } else if ((t == "0<" || t == "0<>" || t == "0<<") && i + 1 < raw_tokens.size()) {
            sc.input_file = expand_token(raw_tokens[++i]);
        } else if ((t == "2>" || t == "2>|" || t == "2>!") && i + 1 < raw_tokens.size()) {
            sc.error_file = expand_token(raw_tokens[++i]);
            sc.append_err = false;
        } else if ((t == "2>>" || t == "2>>|") && i + 1 < raw_tokens.size()) {
            sc.error_file = expand_token(raw_tokens[++i]);
            sc.append_err = true;
        } else if ((t == "&>" || t == "&>|" || t == "&>!" || t == "&>>") && i + 1 < raw_tokens.size()) {
            sc.output_file = expand_token(raw_tokens[++i]);
            sc.append_out = t == "&>>";
            sc.output_files.push_back({sc.output_file, sc.append_out});
            sc.dup_stderr_from = 1;
        } else if (t == "2>&" && i + 1 < raw_tokens.size()) {
            int fd = -1; bool close_fd = false;
            if (parse_fd_dup(raw_tokens[++i].raw, fd, close_fd)) {
                if (close_fd) sc.close_stderr = true;
                else sc.dup_stderr_from = fd;
            }
        } else if ((t == "1>&" || t == ">&") && i + 1 < raw_tokens.size()) {
            int fd = -1; bool close_fd = false;
            if (parse_fd_dup(raw_tokens[++i].raw, fd, close_fd)) {
                if (close_fd) sc.close_stdout = true;
                else sc.dup_stdout_from = fd;
            }
        } else if ((t == "0<&" || t == "<&") && i + 1 < raw_tokens.size()) {
            int fd = -1; bool close_fd = false;
            if (parse_fd_dup(raw_tokens[++i].raw, fd, close_fd)) {
                if (close_fd) sc.close_stdin = true;
                else sc.dup_stdin_from = fd;
            }
        } else {
            vector<string> brace_words = token.quoted ? vector<string>{t} : expand_brace_word(t);
            for (const auto& brace_word : brace_words) {
                bool positional_at = brace_word == "$@" || brace_word == "${@}" ||
                                     brace_word == "\"$@\"" || brace_word == "\"${@}\"";
                if (positional_at) {
                    for (const auto& value : g_env.positional_args) append_argument(value, !token.quoted);
                    continue;
                }

                if (append_flagged_parameter(brace_word, token.quoted)) continue;

                smatch array_match;
                static const regex array_at_re(R"(^\"?\$\{([A-Za-z_][A-Za-z0-9_]*)\[@\]\}\"?$)");
                if (regex_match(brace_word, array_match, array_at_re) && g_env.indexed_arrays.count(array_match[1].str())) {
                    for (const auto& value : g_env.indexed_arrays[array_match[1].str()])
                        append_argument(value, !token.quoted);
                    continue;
                }

                ShellToken expanded_token{brace_word, token.quoted};
                string expanded = expand_token(expanded_token);
                bool split_words = g_env.options.count("shwordsplit") && g_env.options.at("shwordsplit") &&
                                   !token.quoted && brace_word.size() > 1 && brace_word[0] == '$' &&
                                   brace_word.find_first_of(" \t") == string::npos;
                if (split_words) {
                    string separators = g_env.vars.count("IFS") ? g_env.vars["IFS"] : " \t\n";
                    for (auto& field : split_ifs_words(expanded, separators))
                        append_argument(std::move(field), true);
                } else {
                    append_argument(std::move(expanded), !token.quoted);
                }
                needs_command_after_pipe = false;
            }
        }
    }
    if (needs_command_after_pipe) {
        pl.error = "missing command after '|'";
        return pl;
    }
    if (!sc.args.empty() && !finish_command()) return pl;
    return pl;
}

bool is_word_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

bool is_statement_boundary_before(const string& s, size_t pos) {
    if (pos == 0) return true;
    size_t prev = pos;
    while (prev > 0 && (s[prev - 1] == ' ' || s[prev - 1] == '\t' || s[prev - 1] == '\r')) {
        --prev;
    }
    if (prev == 0) return true;
    char prev_ch = s[prev - 1];
    if (prev_ch == '\n' || prev_ch == ';' || prev_ch == '&' || prev_ch == '|' || prev_ch == '(' || prev_ch == '{') return true;
    return false;
}

size_t find_matching_done(const string& s, size_t start) {
    int depth = 1;
    bool in_sq = false, in_dq = false;
    int paren_depth = 0, brace_depth = 0;
    for (size_t i = start; i < s.size(); ++i) {
        char ch = s[i];
        if (ch == '"' && !in_sq) { in_dq = !in_dq; continue; }
        if (ch == '\'' && !in_dq) { in_sq = !in_sq; continue; }
        if (in_sq || in_dq) continue;
        if (ch == '(') { paren_depth++; continue; }
        if (ch == ')' && paren_depth > 0) { paren_depth--; continue; }
        if (ch == '{') { brace_depth++; continue; }
        if (ch == '}' && brace_depth > 0) { brace_depth--; continue; }
        if (paren_depth != 0 || brace_depth != 0) continue;

        if (s.compare(i, 2, "do") == 0 && (i + 2 >= s.size() || !is_word_char(s[i + 2])) && is_statement_boundary_before(s, i)) {
            depth++;
            i += 1;
            continue;
        }
        if (s.compare(i, 4, "done") == 0 && (i + 4 >= s.size() || !is_word_char(s[i + 4])) && is_statement_boundary_before(s, i)) {
            depth--;
            if (depth == 0) return i;
            i += 3;
            continue;
        }
    }
    return string::npos;
}

size_t find_matching_fi(const string& s, size_t start) {
    int depth = 1;
    bool in_sq = false, in_dq = false;
    int paren_depth = 0, brace_depth = 0;
    for (size_t i = start; i < s.size(); ++i) {
        char ch = s[i];
        if (ch == '"' && !in_sq) { in_dq = !in_dq; continue; }
        if (ch == '\'' && !in_dq) { in_sq = !in_sq; continue; }
        if (in_sq || in_dq) continue;
        if (ch == '(') { paren_depth++; continue; }
        if (ch == ')' && paren_depth > 0) { paren_depth--; continue; }
        if (ch == '{') { brace_depth++; continue; }
        if (ch == '}' && brace_depth > 0) { brace_depth--; continue; }
        if (paren_depth != 0 || brace_depth != 0) continue;

        if (s.compare(i, 2, "if") == 0 && (i + 2 >= s.size() || !is_word_char(s[i + 2])) && is_statement_boundary_before(s, i)) {
            depth++;
            i += 1;
            continue;
        }
        if (s.compare(i, 2, "fi") == 0 && (i + 2 >= s.size() || !is_word_char(s[i + 2])) && is_statement_boundary_before(s, i)) {
            depth--;
            if (depth == 0) return i;
            i += 1;
            continue;
        }
    }
    return string::npos;
}

size_t find_top_level_keyword(const string& s, const string& word, size_t start) {
    bool in_sq = false, in_dq = false;
    int paren_depth = 0, brace_depth = 0;
    for (size_t i = start; i < s.size(); ++i) {
        char ch = s[i];
        if (ch == '"' && !in_sq) { in_dq = !in_dq; continue; }
        if (ch == '\'' && !in_dq) { in_sq = !in_sq; continue; }
        if (in_sq || in_dq) continue;
        if (ch == '(') { paren_depth++; continue; }
        if (ch == ')' && paren_depth > 0) { paren_depth--; continue; }
        if (ch == '{') { brace_depth++; continue; }
        if (ch == '}' && brace_depth > 0) { brace_depth--; continue; }
        if (paren_depth != 0 || brace_depth != 0) continue;
        if (i + word.size() > s.size()) continue;
        if (s.compare(i, word.size(), word) != 0) continue;
        bool right_ok = (i + word.size() >= s.size()) || !is_word_char(s[i + word.size()]);
        if (right_ok && is_statement_boundary_before(s, i)) return i;
    }
    return string::npos;
}

size_t find_top_level_word(const string& s, const string& word, size_t start) {
    bool in_sq = false;
    bool in_dq = false;
    int paren_depth = 0;
    int brace_depth = 0;
    for (size_t i = start; i < s.size(); ++i) {
        char ch = s[i];
        if (ch == '"' && !in_sq) { in_dq = !in_dq; continue; }
        if (ch == '\'' && !in_dq) { in_sq = !in_sq; continue; }
        if (in_sq || in_dq) continue;
        if (ch == '(') { paren_depth++; continue; }
        if (ch == ')' && paren_depth > 0) { paren_depth--; continue; }
        if (ch == '{') { brace_depth++; continue; }
        if (ch == '}' && brace_depth > 0) { brace_depth--; continue; }
        if (paren_depth != 0 || brace_depth != 0) continue;
        if (i + word.size() > s.size()) continue;
        if (s.compare(i, word.size(), word) != 0) continue;
        bool left_ok = (i == 0) || !is_word_char(s[i - 1]);
        bool right_ok = (i + word.size() >= s.size()) || !is_word_char(s[i + word.size()]);
        if (left_ok && right_ok) return i;
    }
    return string::npos;
}

bool starts_with_word_trimmed(const string& s, const string& word) {
    string t = trim_copy(s);
    if (t.size() < word.size()) return false;
    if (t.compare(0, word.size(), word) != 0) return false;
    return t.size() == word.size() || !is_word_char(t[word.size()]);
}

string strip_optional_trailing_semicolon(string s) {
    s = trim_copy(s);
    if (!s.empty() && s.back() == ';') {
        s.pop_back();
        s = trim_copy(s);
    }
    return s;
}

vector<string> split_lines_preserve_empty(const string& s) {
    vector<string> lines;
    size_t start = 0;
    while (start <= s.size()) {
        size_t nl = s.find('\n', start);
        if (nl == string::npos) {
            lines.push_back(s.substr(start));
            break;
        }
        lines.push_back(s.substr(start, nl - start));
        start = nl + 1;
    }
    return lines;
}

bool parse_heredoc_specs(const string& header, vector<HeredocSpec>& specs, string& rewritten_header) {
    rewritten_header.clear();
    specs.clear();

    bool in_sq = false, in_dq = false;
    for (size_t i = 0; i < header.size(); ++i) {
        char ch = header[i];
        if (ch == '"' && !in_sq) { in_dq = !in_dq; rewritten_header += ch; continue; }
        if (ch == '\'' && !in_dq) { in_sq = !in_sq; rewritten_header += ch; continue; }

        if (!in_sq && !in_dq && ch == '<' && i + 1 < header.size() && header[i + 1] == '<') {
            bool strip_tabs = false;
            i += 2;
            if (i < header.size() && header[i] == '-') { strip_tabs = true; ++i; }
            while (i < header.size() && isspace((unsigned char)header[i])) ++i;
            if (i >= header.size()) return false;

            string delim;
            bool quoted_delim = false;
            if (header[i] == '\'' || header[i] == '"') {
                quoted_delim = true;
                char q = header[i++];
                while (i < header.size() && header[i] != q) delim += header[i++];
                if (i < header.size() && header[i] == q) ++i;
            } else {
                while (i < header.size() && !isspace((unsigned char)header[i]) && header[i] != ';' && header[i] != '|' && header[i] != '&') {
                    delim += header[i++];
                }
            }
            if (delim.empty()) return false;

            specs.push_back({delim, strip_tabs, !quoted_delim});
            rewritten_header += " < __HEREDOC_" + to_string(specs.size() - 1) + "__ ";
            --i;
            continue;
        }

        rewritten_header += ch;
    }
    return !specs.empty();
}

bool has_unterminated_heredoc(const string& block) {
    vector<string> lines = split_lines_preserve_empty(block);
    if (lines.empty()) return false;

    vector<HeredocSpec> specs;
    string header_rewritten;
    if (!parse_heredoc_specs(lines[0], specs, header_rewritten)) return false;

    size_t cursor = 1;
    for (const auto& spec : specs) {
        bool found_end = false;
        for (; cursor < lines.size(); ++cursor) {
            string cmp = lines[cursor];
            if (spec.strip_tabs) {
                size_t p = 0;
                while (p < cmp.size() && cmp[p] == '\t') ++p;
                cmp = cmp.substr(p);
            }
            if (cmp == spec.delim) {
                found_end = true;
                ++cursor;
                break;
            }
        }
        if (!found_end) return true;
    }
    return false;
}

CommandListAst parse_command_list_ast(const string& line) {
    CommandListAst ast;
    string current;
    CommandListConnector pending_connector = CommandListConnector::Always;
    bool in_sq = false, in_dq = false;
    bool escaped = false;
    int paren_depth = 0;
    int brace_depth = 0;
    vector<string> control_stack; // tracks opener keywords for pairing validation

    auto parse_word_at = [&](size_t i, string& word_out) {
        if (i >= line.size() || !is_word_char(line[i])) return false;
        size_t j = i;
        while (j < line.size() && is_word_char(line[j])) ++j;
        bool left_ok = (i == 0) || !is_word_char(line[i - 1]);
        if (!left_ok) return false;
        word_out = line.substr(i, j - i);
        return true;
    };

    auto segment_has_nonspace = [&]() {
        for (char c : current) {
            if (!isspace((unsigned char)c)) return true;
        }
        return false;
    };
    auto push_current = [&]() {
        string statement = trim_copy(current);
        if (statement.empty()) return false;
        ast.nodes.push_back({pending_connector, std::move(statement)});
        current.clear();
        pending_connector = CommandListConnector::Always;
        return true;
    };
    auto set_missing_operand_error = [&](const string& op) {
        ast.error = "missing command before '" + op + "'";
    };

    for (size_t i = 0; i < line.size(); ++i) {
        char ch = line[i];
        if (escaped) {
            current += ch;
            escaped = false;
            continue;
        }
        if (ch == '\\' && !in_sq) {
            current += ch;
            escaped = true;
            continue;
        }
        if (ch == '"' && !in_sq) { in_dq = !in_dq; current += ch; continue; }
        if (ch == '\'' && !in_dq) { in_sq = !in_sq; current += ch; continue; }

        if (!in_sq && !in_dq) {
            string word;
            if (parse_word_at(i, word)) {
                auto is_keyword_boundary = [&]() {
                    if (i == 0) return true;
                    size_t prev = i;
                    while (prev > 0 && (line[prev - 1] == ' ' || line[prev - 1] == '\t' || line[prev - 1] == '\r')) {
                        --prev;
                    }
                    if (prev == 0) return true;
                    char prev_ch = line[prev - 1];
                    if (prev_ch == '\n' || prev_ch == ';' || prev_ch == '&' || prev_ch == '|' || prev_ch == '(' || prev_ch == '{') return true;
                    size_t w_end = prev;
                    size_t w_start = w_end;
                    while (w_start > 0 && is_word_char(line[w_start - 1])) {
                        --w_start;
                    }
                    if (w_start < w_end) {
                        string prev_word = line.substr(w_start, w_end - w_start);
                        if (prev_word == "do" || prev_word == "then" || prev_word == "else" || prev_word == "elif" || prev_word == "always") {
                            return true;
                        }
                    }
                    return false;
                };
                bool at_statement_start = is_keyword_boundary();
                // Track opener->closer pairing so mismatched keywords (e.g. for...fi)
                // are caught instead of silently mis-parsing downstream.
                static const unordered_map<string, string> kCloserFor = {
                    {"if", "fi"}, {"for", "done"}, {"while", "done"}, {"until", "done"},
                    {"repeat", "done"}, {"select", "done"}, {"case", "esac"}
                };
                static const unordered_map<string, string> kOpenerFor = {
                    {"fi", "if"}, {"done", "for"}, {"esac", "case"}
                };
                bool needs_closer = true;
                if (word == "repeat" || word == "until" || word == "while" || word == "select") {
                    needs_closer = (find_top_level_word(line, "do", i) != string::npos);
                }
                if (kCloserFor.count(word) && at_statement_start && brace_depth == 0 && paren_depth == 0 && needs_closer) {
                    control_stack.push_back(word);
                } else if (kOpenerFor.count(word) && at_statement_start && !control_stack.empty() && brace_depth == 0 && paren_depth == 0) {
                    const string& expected_closer = kCloserFor.at(control_stack.back());
                    bool ok = expected_closer == word;
                    if (!ok) {
                        ast.error = "'" + word + "' does not match '" + control_stack.back() +
                                    "' (expected '" + expected_closer + "')";
                        return ast;
                    }
                    control_stack.pop_back();
                }
            }

            if (ch == '[' && i + 1 < line.size() && line[i + 1] == '[') {
                brace_depth += 1000;
                current += "[[";
                ++i;
                continue;
            }
            if (ch == ']' && i + 1 < line.size() && line[i + 1] == ']' && brace_depth >= 1000) {
                brace_depth -= 1000;
                current += "]]";
                ++i;
                continue;
            }

            if (ch == '{') { brace_depth++; current += ch; continue; }
            if (ch == '}') {
                if (brace_depth == 0) { ast.error = "unmatched '}'"; return ast; }
                brace_depth--; current += ch; continue;
            }
            if (ch == '(') { paren_depth++; current += ch; continue; }
            if (ch == ')') {
                if (paren_depth == 0 && !control_stack.empty() && control_stack.back() == "case") {
                    current += ch;
                    continue;
                }
                if (paren_depth == 0) { ast.error = "unmatched ')'"; return ast; }
                paren_depth--; current += ch; continue;
            }

            if (paren_depth == 0 && brace_depth == 0 && control_stack.empty()) {
                if (ch == ';' || ch == '\n') {
                    if (!push_current() && ch == ';') {
                        set_missing_operand_error(";");
                        return ast;
                    }
                    continue;
                }
                if (i + 1 < line.size() && line[i] == '&' && line[i + 1] == '&') {
                    if (!push_current()) { set_missing_operand_error("&&"); return ast; }
                    pending_connector = CommandListConnector::And;
                    ++i;
                    continue;
                }
                if (i + 1 < line.size() && line[i] == '|' && line[i + 1] == '|') {
                    if (!push_current()) { set_missing_operand_error("||"); return ast; }
                    pending_connector = CommandListConnector::Or;
                    ++i;
                    continue;
                }
                if (ch == '&' && (i == 0 || line[i - 1] != '>') &&
                    (i + 1 >= line.size() || line[i + 1] != '>')) {
                    string statement = trim_copy(current);
                    if (statement.empty()) { set_missing_operand_error("&"); return ast; }
                    statement += " &";
                    ast.nodes.push_back({pending_connector, std::move(statement)});
                    current.clear();
                    pending_connector = CommandListConnector::Always;
                    continue;
                }
            }
        }
        current += ch;
    }

    if (in_dq) ast.error = "unmatched '\"'";
    else if (in_sq) ast.error = "unmatched '\''";
    else if (paren_depth != 0) ast.error = "unmatched '('";
    else if (brace_depth != 0) ast.error = "unmatched '{'";
    else if (!control_stack.empty()) ast.error = "'" + control_stack.back() + "' not closed";
    if (!ast.error.empty()) return ast;

    if (!push_current() && pending_connector != CommandListConnector::Always)
        ast.error = "missing command after logical operator";
    return ast;
}
