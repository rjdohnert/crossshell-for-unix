#include "scripting.hpp"
#include "engine.hpp"
#include "parser.hpp"
#include "expansion.hpp"
#include "jobs.hpp"
#include "builtins.hpp"
#include "terminal.hpp"

bool g_script_returning  = false;
int  g_script_return_code = 0;
bool g_in_try_block       = false;
int  g_loop_depth = 0;
int  g_loop_breaking = 0;
int  g_loop_continuing = 0;

LoopGuard::LoopGuard() { ++g_loop_depth; }
LoopGuard::~LoopGuard() {
    --g_loop_depth;
    if (g_loop_depth <= 0) {
        g_loop_depth = 0;
        g_loop_breaking = 0;
        g_loop_continuing = 0;
    }
}

int  g_subshell_depth = 0;
bool g_subshell_exiting = false;
int  g_subshell_exit_code = 0;
unsigned long long g_heredoc_seq = 0;
map<string, string> g_heredoc_payloads;
int  g_exec_recursion_depth = 0;
const int kMaxExecRecursionDepth = 48;

map<int, HANDLE> g_persistent_fds;
map<int, HANDLE> g_active_command_fds;

string capture_command_output(const string& cmd) {
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE rawRead = nullptr;
    HANDLE rawWrite = nullptr;
    if (!CreatePipe(&rawRead, &rawWrite, &sa, 0)) return "";
    unique_handle hRead = make_unique_handle(rawRead);
    unique_handle hWrite = make_unique_handle(rawWrite);
    // Prevent the read end from being inherited by children spawned during capture.
    if (!SetHandleInformation((HANDLE)hRead.get(), HANDLE_FLAG_INHERIT, 0)) {
        return "";
    }

    HANDLE oldStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE oldStdErr = GetStdHandle(STD_ERROR_HANDLE);
    int oldFdOut = _dup(_fileno(stdout));
    int oldFdErr = _dup(_fileno(stderr));

    string output;
    // INVARIANT: `output` is written only by the reader thread below and read by
    // the main thread only after reader.join(). Do not touch it from the main
    // thread between thread creation and join without adding synchronization.
    std::thread reader([&]() {
        char buffer[4096];
        DWORD read_bytes = 0;
        while (ReadFile((HANDLE)hRead.get(), buffer, sizeof(buffer), &read_bytes, NULL) && read_bytes > 0) {
            output.append(buffer, buffer + read_bytes);
        }
    });

    cout.flush();
    cerr.flush();

    SetStdHandle(STD_OUTPUT_HANDLE, (HANDLE)hWrite.get());
    SetStdHandle(STD_ERROR_HANDLE, (HANDLE)hWrite.get());

    HANDLE rawWriteForCrt = INVALID_HANDLE_VALUE;
    if (!DuplicateHandle(GetCurrentProcess(), (HANDLE)hWrite.get(), GetCurrentProcess(), &rawWriteForCrt, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
        rawWriteForCrt = INVALID_HANDLE_VALUE;
    }
    unique_handle hWriteForCrt = make_unique_handle(rawWriteForCrt);

    int writeFd = -1;
    if (hWriteForCrt.get()) {
        writeFd = _open_osfhandle((intptr_t)hWriteForCrt.get(), _O_TEXT);
        if (writeFd != -1) {
            // Ownership moved to CRT fd; _close(writeFd) closes the HANDLE.
            hWriteForCrt.release();
        }
    }

    if (writeFd != -1) {
        _dup2(writeFd, _fileno(stdout));
        _dup2(writeFd, _fileno(stderr));
        _close(writeFd);
    }

    exception_ptr eptr;
    try {
        parse_and_execute(cmd);
    } catch (...) {
        eptr = current_exception();
    }

    cout.flush();
    cerr.flush();

    // Close our pipe write end FIRST so the reader thread sees EOF even on the
    // exception path, then restore std handles, then join the reader.
    hWrite.reset();

    if (oldFdOut != -1) {
        _dup2(oldFdOut, _fileno(stdout));
        _close(oldFdOut);
    }
    if (oldFdErr != -1) {
        _dup2(oldFdErr, _fileno(stderr));
        _close(oldFdErr);
    }

    SetStdHandle(STD_OUTPUT_HANDLE, oldStdOut);
    SetStdHandle(STD_ERROR_HANDLE, oldStdErr);

    if (reader.joinable()) {
        reader.join();
    }

    if (eptr) {
        rethrow_exception(eptr);
    }

    while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) output.pop_back();
    return output;
}

int execute_if_block(const string& block) {
    string s = trim_copy(block);
    if (!starts_with_word_trimmed(s, "if")) return 1;

    size_t cursor = find_top_level_keyword(s, "if", 0);
    if (cursor == string::npos) return 1;
    cursor += 2;

    while (true) {
        size_t then_pos = find_top_level_keyword(s, "then", cursor);
        if (then_pos == string::npos) return 1;

        string cond = strip_optional_trailing_semicolon(s.substr(cursor, then_pos - cursor));
        int cond_status = parse_and_execute(cond);
        g_env.last_exit_code = cond_status;

        size_t body_start = then_pos + 4;
        size_t fi_pos = find_matching_fi(s, body_start);
        if (fi_pos == string::npos) return 1;

        size_t elif_pos = string::npos;
        size_t else_pos = string::npos;
        int depth = 0;
        bool in_sq = false, in_dq = false;
        int p_depth = 0, b_depth = 0;
        for (size_t i = body_start; i < fi_pos; ++i) {
            char ch = s[i];
            if (ch == '"' && !in_sq) { in_dq = !in_dq; continue; }
            if (ch == '\'' && !in_dq) { in_sq = !in_sq; continue; }
            if (in_sq || in_dq) continue;
            if (ch == '(') { p_depth++; continue; }
            if (ch == ')' && p_depth > 0) { p_depth--; continue; }
            if (ch == '{') { b_depth++; continue; }
            if (ch == '}' && b_depth > 0) { b_depth--; continue; }
            if (p_depth != 0 || b_depth != 0) continue;

            if (s.compare(i, 2, "if") == 0 && (i + 2 >= s.size() || !is_word_char(s[i + 2])) && is_statement_boundary_before(s, i)) {
                depth++;
                i += 1;
                continue;
            }
            if (s.compare(i, 2, "fi") == 0 && (i + 2 >= s.size() || !is_word_char(s[i + 2])) && is_statement_boundary_before(s, i)) {
                if (depth > 0) depth--;
                i += 1;
                continue;
            }
            if (depth == 0 && is_statement_boundary_before(s, i)) {
                if (s.compare(i, 4, "elif") == 0 && (i + 4 >= s.size() || !is_word_char(s[i + 4])) && elif_pos == string::npos && else_pos == string::npos) {
                    elif_pos = i;
                } else if (s.compare(i, 4, "else") == 0 && (i + 4 >= s.size() || !is_word_char(s[i + 4])) && else_pos == string::npos) {
                    else_pos = i;
                }
            }
        }

        size_t next_pos = fi_pos;
        string next_kind = "fi";
        if (elif_pos != string::npos && elif_pos < next_pos) { next_pos = elif_pos; next_kind = "elif"; }
        if (else_pos != string::npos && else_pos < next_pos) { next_pos = else_pos; next_kind = "else"; }

        string then_body = trim_copy(s.substr(body_start, next_pos - body_start));
        if (cond_status == 0) {
            return execute_command_line(then_body);
        }

        if (next_kind == "elif") {
            cursor = next_pos + 4;
            continue;
        }

        if (next_kind == "else") {
            string else_body = trim_copy(s.substr(next_pos + 4, fi_pos - (next_pos + 4)));
            return execute_command_line(else_body);
        }

        return 0;
    }
}

int execute_for_loop(const string& block) {
    LoopGuard loop_guard;
    string s = trim_copy(block);
    size_t for_pos = find_top_level_keyword(s, "for", 0);
    size_t in_pos = find_top_level_word(s, "in", for_pos == string::npos ? 0 : for_pos + 3);
    size_t do_pos = find_top_level_word(s, "do", in_pos == string::npos ? 0 : in_pos + 2);
    if (for_pos == string::npos || in_pos == string::npos || do_pos == string::npos) return 1;
    size_t done_pos = find_matching_done(s, do_pos + 2);
    if (done_pos == string::npos) return 1;

    string var_name = trim_copy(s.substr(for_pos + 3, in_pos - (for_pos + 3)));
    string items_str = strip_optional_trailing_semicolon(s.substr(in_pos + 2, do_pos - (in_pos + 2)));
    string do_body = trim_copy(s.substr(do_pos + 2, done_pos - (do_pos + 2)));
    if (var_name.empty()) return 1;

    vector<string> items;
    string items_trimmed = trim_copy(items_str);
    if (items_trimmed == "\"$@\"" || items_trimmed == "$@" ||
        items_trimmed == "\"$*\"" || items_trimmed == "${@}") {
        items = g_env.positional_args;
    } else {
        items = tokenize_words(g_env.expand_vars(items_str));
        items = expand_globs(items);
    }

    int last_rc = 0;
    for (const auto& item : items) {
        g_env.vars[var_name] = item;
        last_rc = execute_command_line(do_body);
        g_env.last_exit_code = last_rc;
        if (g_loop_breaking > 0) {
            --g_loop_breaking;
            break;
        }
        if (g_loop_continuing > 0) {
            --g_loop_continuing;
            if (g_loop_continuing > 0) break;
            continue;
        }
        if (g_script_returning || g_subshell_exiting) break;
    }
    return last_rc;
}

vector<string> split_on_top_level_commas(const string& s) {
    vector<string> out;
    string cur;
    bool in_sq = false, in_dq = false;
    int paren_depth = 0;
    for (char ch : s) {
        if (ch == '"' && !in_sq) { in_dq = !in_dq; cur += ch; continue; }
        if (ch == '\'' && !in_dq) { in_sq = !in_sq; cur += ch; continue; }
        if (!in_sq && !in_dq) {
            if (ch == '(') { paren_depth++; cur += ch; continue; }
            if (ch == ')' && paren_depth > 0) { paren_depth--; cur += ch; continue; }
            if (ch == ',' && paren_depth == 0) {
                out.push_back(trim_copy(cur));
                cur.clear();
                continue;
            }
        }
        cur += ch;
    }
    if (!trim_copy(cur).empty()) out.push_back(trim_copy(cur));
    return out;
}

int execute_cstyle_for_loop(const string& block) {
    LoopGuard loop_guard;
    string s = trim_copy(block);
    if (!starts_with_word_trimmed(s, "for")) return 1;

    size_t open = s.find("((");
    if (open == string::npos) return 1;
    size_t close = s.find("))", open + 2);
    if (close == string::npos) return 1;

    string header = s.substr(open + 2, close - (open + 2));
    vector<string> parts;
    string cur;
    for (char ch : header) {
        if (ch == ';') { parts.push_back(trim_copy(cur)); cur.clear(); }
        else cur += ch;
    }
    parts.push_back(trim_copy(cur));
    if (parts.size() != 3) return 1;

    size_t do_pos = find_top_level_word(s, "do", close + 2);
    if (do_pos == string::npos) return 1;
    size_t done_pos = find_matching_done(s, do_pos + 2);
    if (done_pos == string::npos) return 1;
    string body = trim_copy(s.substr(do_pos + 2, done_pos - (do_pos + 2)));

    for (const auto& expr : split_on_top_level_commas(parts[0])) {
        if (!expr.empty()) parse_and_execute("let " + expr);
    }

    int last_rc = 0;
    int guard = 0;
    while (true) {
        // Evaluate condition each iteration so updates from the previous
        // iteration's increment expressions are visible here.
        long long cond = 1;
        if (!parts[1].empty()) cond = eval_math_expr(g_env.expand_vars(parts[1]));
        if (cond == 0) break;

        last_rc = execute_command_line(body);
        g_env.last_exit_code = last_rc;
        if (g_loop_breaking > 0) {
            --g_loop_breaking;
            break;
        }
        if (g_loop_continuing > 0) {
            --g_loop_continuing;
            if (g_loop_continuing > 0) break;
            for (const auto& expr : split_on_top_level_commas(parts[2])) {
                if (!expr.empty()) parse_and_execute("let " + expr);
            }
            continue;
        }
        if (g_script_returning || g_subshell_exiting) break;

        for (const auto& expr : split_on_top_level_commas(parts[2])) {
            if (!expr.empty()) parse_and_execute("let " + expr);
        }

        if (++guard > 1000000) {
            cerr << "zsh: for((...)): loop guard exceeded\n";
            return 1;
        }
    }
    return last_rc;
}

string strip_outer_quotes(string s) {
    s = trim_copy(s);
    if (s.size() >= 2 && ((s.front() == '\'' && s.back() == '\'') || (s.front() == '"' && s.back() == '"'))) {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

int execute_case_block(const string& block) {
    string s = trim_copy(block);
    if (!starts_with_word_trimmed(s, "case")) return 1;

    size_t case_pos = find_top_level_keyword(s, "case", 0);
    size_t in_pos = find_top_level_word(s, "in", case_pos == string::npos ? 0 : case_pos + 4);
    size_t esac_pos = find_top_level_keyword(s, "esac", in_pos == string::npos ? 0 : in_pos + 2);
    if (case_pos == string::npos || in_pos == string::npos || esac_pos == string::npos) return 1;

    string word = strip_outer_quotes(trim_copy(g_env.expand_vars(s.substr(case_pos + 4, in_pos - (case_pos + 4)))));
    string body = s.substr(in_pos + 2, esac_pos - (in_pos + 2));

    struct CaseClause {
        string text;
        enum Term { Stop, FallthroughAction, FallthroughRetest } term = Stop;
    };

    vector<CaseClause> clauses;
    {
        string cur2;
        bool in_sq = false, in_dq = false;
        for (size_t i = 0; i < body.size(); ++i) {
            char ch = body[i];
            if (ch == '"' && !in_sq) { in_dq = !in_dq; cur2 += ch; continue; }
            if (ch == '\'' && !in_dq) { in_sq = !in_sq; cur2 += ch; continue; }

            if (!in_sq && !in_dq && i + 2 < body.size() && body[i] == ';' && body[i + 1] == ';' && body[i + 2] == '&') {
                clauses.push_back({trim_copy(cur2), CaseClause::FallthroughRetest});
                cur2.clear();
                i += 2;
                continue;
            }
            if (!in_sq && !in_dq && i + 1 < body.size() && body[i] == ';' && body[i + 1] == '&') {
                clauses.push_back({trim_copy(cur2), CaseClause::FallthroughAction});
                cur2.clear();
                i += 1;
                continue;
            }
            if (!in_sq && !in_dq && i + 1 < body.size() && body[i] == ';' && body[i + 1] == ';') {
                clauses.push_back({trim_copy(cur2), CaseClause::Stop});
                cur2.clear();
                i += 1;
                continue;
            }
            cur2 += ch;
        }
        if (!trim_copy(cur2).empty()) {
            clauses.push_back({trim_copy(cur2), CaseClause::Stop});
        }
    }

    auto split_patterns = [](const string& pat_expr) {
        vector<string> pats;
        string pcur;
        bool in_sq = false, in_dq = false;
        int paren_depth = 0;
        for (char ch : pat_expr) {
            if (ch == '"' && !in_sq) { in_dq = !in_dq; pcur += ch; continue; }
            if (ch == '\'' && !in_dq) { in_sq = !in_sq; pcur += ch; continue; }
            if (!in_sq && !in_dq && ch == '(') { ++paren_depth; pcur += ch; continue; }
            if (!in_sq && !in_dq && ch == ')' && paren_depth > 0) { --paren_depth; pcur += ch; continue; }
            if (!in_sq && !in_dq && paren_depth == 0 && ch == '|') {
                if (!trim_copy(pcur).empty()) pats.push_back(strip_outer_quotes(trim_copy(pcur)));
                pcur.clear();
                continue;
            }
            pcur += ch;
        }
        if (!trim_copy(pcur).empty()) pats.push_back(strip_outer_quotes(trim_copy(pcur)));
        return pats;
    };

    int last_rc = 0;
    bool executed_any = false;
    bool force_next_action = false;

    for (const auto& clause : clauses) {
        const string& cl = clause.text;
        size_t rp = string::npos;
        bool pattern_sq = false, pattern_dq = false;
        int pattern_depth = 0;
        for (size_t i = 0; i < cl.size(); ++i) {
            char ch = cl[i];
            if (ch == '"' && !pattern_sq) { pattern_dq = !pattern_dq; continue; }
            if (ch == '\'' && !pattern_dq) { pattern_sq = !pattern_sq; continue; }
            if (pattern_sq || pattern_dq) continue;
            if (ch == '(') { ++pattern_depth; continue; }
            if (ch == ')') {
                if (pattern_depth > 0) --pattern_depth;
                else { rp = i; break; }
            }
        }
        if (rp == string::npos) continue;
        string pat_expr = trim_copy(cl.substr(0, rp));
        string action = trim_copy(cl.substr(rp + 1));

        bool matched = force_next_action;
        if (!matched) {
            vector<string> pats = split_patterns(pat_expr);
            for (const auto& p : pats) {
                string pat = g_env.expand_vars(p);
                if (pat == "*" || match_wildcard(pat, word)) { matched = true; break; }
            }
        }

        if (!matched) continue;

        last_rc = execute_command_line(action);
        executed_any = true;

        if (clause.term == CaseClause::Stop) return last_rc;
        if (clause.term == CaseClause::FallthroughAction) {
            force_next_action = true;
        } else {
            force_next_action = false;
        }
    }

    return executed_any ? last_rc : 0;
}

int execute_condition_loop(const string& block, bool until_mode) {
    LoopGuard loop_guard;
    string s = trim_copy(block);
    const string keyword = until_mode ? "until" : "while";
    size_t loop_pos = find_top_level_keyword(s, keyword, 0);
    if (loop_pos == string::npos) return 1;

    size_t after_kw = loop_pos + keyword.size();
    size_t do_pos = find_top_level_word(s, "do", after_kw);
    size_t done_pos = (do_pos != string::npos) ? find_matching_done(s, do_pos + 2) : string::npos;

    string cond;
    string body;

    if (do_pos != string::npos && done_pos != string::npos) {
        cond = strip_optional_trailing_semicolon(s.substr(after_kw, do_pos - after_kw));
        body = trim_copy(s.substr(do_pos + 2, done_pos - (do_pos + 2)));
    } else {
        // Check for brace syntax: until/while <cond> { <body> }
        size_t brace_open = string::npos;
        size_t brace_close = string::npos;
        bool in_sq = false, in_dq = false;
        int p_depth = 0, b_depth = 0;
        for (size_t i = after_kw; i < s.size(); ++i) {
            char ch = s[i];
            if (ch == '"' && !in_sq) { in_dq = !in_dq; continue; }
            if (ch == '\'' && !in_dq) { in_sq = !in_sq; continue; }
            if (in_sq || in_dq) continue;
            if (ch == '(') { ++p_depth; continue; }
            if (ch == ')' && p_depth > 0) { --p_depth; continue; }
            if (p_depth == 0) {
                if (ch == '{') {
                    if (b_depth == 0) brace_open = i;
                    ++b_depth;
                } else if (ch == '}') {
                    if (b_depth > 0) --b_depth;
                    if (b_depth == 0 && brace_open != string::npos) {
                        brace_close = i;
                        break;
                    }
                }
            }
        }
        if (brace_open != string::npos && brace_close != string::npos && brace_close > brace_open) {
            cond = strip_optional_trailing_semicolon(s.substr(after_kw, brace_open - after_kw));
            body = trim_copy(s.substr(brace_open + 1, brace_close - (brace_open + 1)));
        } else {
            cerr << "zsh: " << keyword << ": expected 'do ... done' or '{ ... }'\n";
            return 1;
        }
    }

    cond = trim_copy(cond);
    int last_rc = 0;
    while (true) {
        int cond_rc = parse_and_execute(cond);
        g_env.last_exit_code = cond_rc;
        if ((cond_rc == 0) == until_mode) break;
        last_rc = execute_command_line(body);
        g_env.last_exit_code = last_rc;
        if (g_loop_breaking > 0) {
            --g_loop_breaking;
            break;
        }
        if (g_loop_continuing > 0) {
            --g_loop_continuing;
            if (g_loop_continuing > 0) break;
            continue;
        }
        if (g_script_returning || g_subshell_exiting) break;
    }
    return last_rc;
}

int execute_repeat_loop(const string& block) {
    LoopGuard loop_guard;
    string s = trim_copy(block);
    size_t repeat_pos = find_top_level_keyword(s, "repeat", 0);
    if (repeat_pos == string::npos) return 1;

    size_t after_repeat = repeat_pos + 6;
    size_t do_pos = find_top_level_word(s, "do", after_repeat);
    size_t done_pos = (do_pos != string::npos) ? find_matching_done(s, do_pos + 2) : string::npos;
    if (do_pos != string::npos && done_pos == string::npos) {
        done_pos = find_top_level_word(s, "done", do_pos + 2);
    }

    string count_expr;
    string body;

    if (do_pos != string::npos && done_pos != string::npos) {
        count_expr = strip_optional_trailing_semicolon(s.substr(after_repeat, do_pos - after_repeat));
        body = trim_copy(s.substr(do_pos + 2, done_pos - (do_pos + 2)));
    } else {
        // Check for brace syntax: repeat <count> { <body> }
        size_t brace_open = string::npos;
        size_t brace_close = string::npos;
        bool in_sq = false, in_dq = false;
        int p_depth = 0, b_depth = 0;
        for (size_t i = after_repeat; i < s.size(); ++i) {
            char ch = s[i];
            if (ch == '"' && !in_sq) { in_dq = !in_dq; continue; }
            if (ch == '\'' && !in_dq) { in_sq = !in_sq; continue; }
            if (in_sq || in_dq) continue;
            if (ch == '(') { ++p_depth; continue; }
            if (ch == ')' && p_depth > 0) { --p_depth; continue; }
            if (p_depth == 0) {
                if (ch == '{') {
                    if (b_depth == 0) brace_open = i;
                    ++b_depth;
                } else if (ch == '}') {
                    if (b_depth > 0) --b_depth;
                    if (b_depth == 0 && brace_open != string::npos) {
                        brace_close = i;
                        break;
                    }
                }
            }
        }
        if (brace_open != string::npos && brace_close != string::npos && brace_close > brace_open) {
            count_expr = strip_optional_trailing_semicolon(s.substr(after_repeat, brace_open - after_repeat));
            body = trim_copy(s.substr(brace_open + 1, brace_close - (brace_open + 1)));
        } else {
            // Simple command form: repeat <count> <command...>
            size_t rest_start = after_repeat;
            while (rest_start < s.size() && isspace((unsigned char)s[rest_start])) ++rest_start;
            if (rest_start >= s.size()) {
                cerr << "zsh: repeat: count expected\n";
                return 1;
            }

            size_t count_end = rest_start;
            p_depth = 0;
            in_sq = false; in_dq = false;
            while (count_end < s.size()) {
                char ch = s[count_end];
                if (ch == '"' && !in_sq) { in_dq = !in_dq; ++count_end; continue; }
                if (ch == '\'' && !in_dq) { in_sq = !in_sq; ++count_end; continue; }
                if (!in_sq && !in_dq) {
                    if (ch == '(') { ++p_depth; ++count_end; continue; }
                    if (ch == ')' && p_depth > 0) { --p_depth; ++count_end; continue; }
                    if (p_depth == 0 && (isspace((unsigned char)ch) || ch == ';')) break;
                }
                ++count_end;
            }
            count_expr = trim_copy(s.substr(rest_start, count_end - rest_start));
            body = trim_copy(s.substr(count_end));
            if (body.empty()) {
                cerr << "zsh: repeat: command expected\n";
                return 1;
            }
        }
    }

    count_expr = trim_copy(count_expr);
    long long count = 0;
    if (g_env.vars.count(count_expr)) {
        try { count = stoll(g_env.vars[count_expr]); } catch (...) { count = 0; }
    } else {
        string expanded_count = g_env.expand_vars(count_expr);
        try {
            count = eval_math_expr(expanded_count);
        } catch (...) {
            count = 0;
        }
    }

    if (count <= 0) return 0;
    if (count > 10000000) {
        cerr << "zsh: repeat: count too large: " << count << "\n";
        return 1;
    }

    int last_rc = 0;
    for (long long iteration = 0; iteration < count; ++iteration) {
        last_rc = execute_command_line(body);
        g_env.last_exit_code = last_rc;
        if (g_loop_breaking > 0) {
            --g_loop_breaking;
            break;
        }
        if (g_loop_continuing > 0) {
            --g_loop_continuing;
            if (g_loop_continuing > 0) break;
            continue;
        }
        if (g_script_returning || g_subshell_exiting) break;
    }
    return last_rc;
}

int execute_select_loop(const string& block) {
    LoopGuard loop_guard;
    string s = trim_copy(block);
    size_t select_pos = find_top_level_word(s, "select", 0);
    if (select_pos == string::npos) return 1;

    auto expand_select_items = [](const string& words) {
        string trimmed_words = trim_copy(words);
        if (trimmed_words == "\"$@\"" || trimmed_words == "$@" ||
            trimmed_words == "\"$*\"" || trimmed_words == "${@}") {
            return g_env.positional_args;
        }
        return expand_globs(tokenize_words(g_env.expand_vars(words)));
    };

    size_t after_select = select_pos + 6;
    size_t in_pos = find_top_level_word(s, "in", after_select);
    size_t do_pos = find_top_level_keyword(s, "do", after_select);
    size_t done_pos = (do_pos != string::npos) ? find_matching_done(s, do_pos + 2) : string::npos;

    string var_name;
    vector<string> items;
    string body;

    if (do_pos != string::npos && done_pos != string::npos) {
        if (in_pos != string::npos && in_pos < do_pos) {
            var_name = trim_copy(s.substr(after_select, in_pos - after_select));
            string words_str = strip_optional_trailing_semicolon(s.substr(in_pos + 2, do_pos - (in_pos + 2)));
            items = expand_select_items(words_str);
        } else {
            var_name = strip_optional_trailing_semicolon(trim_copy(s.substr(after_select, do_pos - after_select)));
            items = g_env.positional_args;
        }
        body = trim_copy(s.substr(do_pos + 2, done_pos - (do_pos + 2)));
    } else {
        // Check for brace syntax: select var [in words] { body }
        size_t brace_open = string::npos, brace_close = string::npos;
        bool in_sq = false, in_dq = false;
        int p_depth = 0, b_depth = 0;
        for (size_t i = after_select; i < s.size(); ++i) {
            char ch = s[i];
            if (ch == '"' && !in_sq) { in_dq = !in_dq; continue; }
            if (ch == '\'' && !in_dq) { in_sq = !in_sq; continue; }
            if (in_sq || in_dq) continue;
            if (ch == '(') { ++p_depth; continue; }
            if (ch == ')' && p_depth > 0) { --p_depth; continue; }
            if (p_depth == 0) {
                if (ch == '{') { if (b_depth == 0) brace_open = i; ++b_depth; }
                else if (ch == '}') {
                    if (b_depth > 0) --b_depth;
                    if (b_depth == 0 && brace_open != string::npos) { brace_close = i; break; }
                }
            }
        }
        if (brace_open != string::npos && brace_close != string::npos) {
            string header = trim_copy(s.substr(after_select, brace_open - after_select));
            in_pos = find_top_level_word(header, "in", 0);
            if (in_pos != string::npos) {
                var_name = trim_copy(header.substr(0, in_pos));
                string words_str = strip_optional_trailing_semicolon(header.substr(in_pos + 2));
                items = expand_select_items(words_str);
            } else {
                var_name = strip_optional_trailing_semicolon(header);
                items = g_env.positional_args;
            }
            body = trim_copy(s.substr(brace_open + 1, brace_close - (brace_open + 1)));
        } else {
            cerr << "zsh: select: expected 'do ... done' or '{ ... }'\n";
            return 1;
        }
    }

    var_name = trim_copy(var_name);
    if (!is_valid_env_var_name(var_name)) {
        cerr << "zsh: select: invalid variable name: " << var_name << "\n";
        return 1;
    }
    if (items.empty()) return 0;

    int last_rc = 0;
    while (true) {
        for (size_t i = 0; i < items.size(); ++i) {
            cout << (i + 1) << ") " << items[i] << "\n";
        }
        string prompt = g_env.vars.count("PROMPT3") ? g_env.vars["PROMPT3"] : "?# ";
        cout << prompt;
        cout.flush();

        string line_in;
        if (!getline(cin, line_in)) break;
        line_in = trim_copy(line_in);
        g_env.vars["REPLY"] = line_in;

        long long choice = 0;
        try { choice = stoll(line_in); } catch (...) { choice = 0; }
        if (choice >= 1 && static_cast<size_t>(choice) <= items.size()) {
            g_env.vars[var_name] = items[choice - 1];
        } else {
            g_env.vars[var_name] = "";
        }

        last_rc = execute_command_line(body);
        g_env.last_exit_code = last_rc;
        if (g_loop_breaking > 0) {
            --g_loop_breaking;
            break;
        }
        if (g_loop_continuing > 0) {
            --g_loop_continuing;
            if (g_loop_continuing > 0) break;
            continue;
        }
        if (g_script_returning || g_subshell_exiting) break;
    }
    return last_rc;
}

int execute_timed_command(const string& block) {
    string s = trim_copy(block);
    if (starts_with_word_trimmed(s, "time")) {
        s = trim_copy(s.substr(4));
    }
    if (s.empty()) return 0;
    auto t0 = chrono::high_resolution_clock::now();
    int rc = execute_statement_block_aware(s);
    auto t1 = chrono::high_resolution_clock::now();
    double elapsed = chrono::duration<double>(t1 - t0).count();

    string fmt = g_env.vars.count("TIMEFMT") ? g_env.vars["TIMEFMT"] : "%E total";
    ostringstream oss;
    oss << fixed << setprecision(3) << elapsed << "s";
    string elapsed_str = oss.str();

    auto replace_all = [](string& value, const string& needle, const string& replacement) {
        size_t position = 0;
        while ((position = value.find(needle, position)) != string::npos) {
            value.replace(position, needle.size(), replacement);
            position += replacement.size();
        }
    };
    bool has_elapsed_conversion = fmt.find("%*E") != string::npos || fmt.find("%E") != string::npos;
    string formatted = has_elapsed_conversion ? fmt : elapsed_str + " total";
    if (has_elapsed_conversion) {
        replace_all(formatted, "%*E", elapsed_str);
        replace_all(formatted, "%E", elapsed_str);
    }
    cerr << formatted << "\n";
    return rc;
}

int execute_always_block(const string& statement) {
    string s = trim_copy(statement);
    if (starts_with_word_trimmed(s, "try")) {
        s = trim_copy(s.substr(3));
    }
    size_t always_pos = find_top_level_word(s, "always");
    if (always_pos == string::npos) return 1;

    string try_part = trim_copy(s.substr(0, always_pos));
    string always_part = trim_copy(s.substr(always_pos + 6));

    if (try_part.size() >= 2 && try_part.front() == '{' && try_part.back() == '}') {
        try_part = trim_copy(try_part.substr(1, try_part.size() - 2));
    }
    if (always_part.size() >= 2 && always_part.front() == '{' && always_part.back() == '}') {
        always_part = trim_copy(always_part.substr(1, always_part.size() - 2));
    }

    bool saved_returning = g_script_returning;
    int saved_return_code = g_script_return_code;
    bool saved_exiting = g_subshell_exiting;
    int saved_exit_code = g_subshell_exit_code;

    g_script_returning = false;
    g_subshell_exiting = false;

    bool old_try = g_in_try_block;
    g_in_try_block = true;
    int try_rc = execute_command_line(try_part);
    g_in_try_block = old_try;
    bool try_failed = (try_rc != 0 || g_script_returning || g_subshell_exiting);
    g_env.vars["TRY_BLOCK_ERROR"] = try_failed ? "1" : "0";

    bool after_try_returning = g_script_returning;
    int after_try_return_code = g_script_return_code;
    bool after_try_exiting = g_subshell_exiting;
    int after_try_exit_code = g_subshell_exit_code;

    // Reset return/exit flags so always block runs unconditionally
    g_script_returning = false;
    g_subshell_exiting = false;

    int always_rc = execute_command_line(always_part);

    if (g_env.vars["TRY_BLOCK_ERROR"] != "0") {
        g_script_returning = after_try_returning || saved_returning;
        g_script_return_code = after_try_returning ? after_try_return_code : saved_return_code;
        g_subshell_exiting = after_try_exiting || saved_exiting;
        g_subshell_exit_code = after_try_exiting ? after_try_exit_code : saved_exit_code;
        return try_rc != 0 ? try_rc : 1;
    } else {
        // Error was cleared inside always block
        g_script_returning = saved_returning;
        g_script_return_code = saved_return_code;
        g_subshell_exiting = saved_exiting;
        g_subshell_exit_code = saved_exit_code;
        return always_rc;
    }
}

bool try_execute_anonymous_function(const string& statement, int& rc) {
    string s = trim_copy(statement);
    if (s.empty()) return false;
    size_t body_start = string::npos;
    if (s.rfind("()", 0) == 0) {
        size_t next = 2;
        while (next < s.size() && isspace((unsigned char)s[next])) ++next;
        if (next < s.size() && s[next] == '{') body_start = next;
    } else if (s.rfind("function", 0) == 0 && (s.size() == 8 || isspace((unsigned char)s[8]) || s[8] == '(' || s[8] == '{')) {
        size_t next = 8;
        while (next < s.size() && isspace((unsigned char)s[next])) ++next;
        if (next + 1 < s.size() && s[next] == '(' && s[next + 1] == ')') {
            next += 2;
            while (next < s.size() && isspace((unsigned char)s[next])) ++next;
        }
        if (next < s.size() && s[next] == '{') body_start = next;
    }
    if (body_start == string::npos) return false;

    size_t body_end = string::npos;
    bool in_sq = false, in_dq = false;
    int b_depth = 0, p_depth = 0;
    for (size_t i = body_start; i < s.size(); ++i) {
        char ch = s[i];
        if (ch == '"' && !in_sq) { in_dq = !in_dq; continue; }
        if (ch == '\'' && !in_dq) { in_sq = !in_sq; continue; }
        if (in_sq || in_dq) continue;
        if (ch == '(') { ++p_depth; continue; }
        if (ch == ')' && p_depth > 0) { --p_depth; continue; }
        if (p_depth == 0) {
            if (ch == '{') ++b_depth;
            else if (ch == '}') {
                --b_depth;
                if (b_depth == 0) {
                    body_end = i;
                    break;
                }
            }
        }
    }
    if (body_end == string::npos) return false;

    string body = trim_copy(s.substr(body_start + 1, body_end - (body_start + 1)));
    string args_str = trim_copy(s.substr(body_end + 1));
    vector<string> args;
    if (!args_str.empty()) {
        args = tokenize_words(args_str);
        for (auto& arg : args) arg = g_env.expand_vars(arg);
    }

    vector<string> old_pos = g_env.positional_args;
    bool old_returning = g_script_returning;
    int old_return_code = g_script_return_code;

    if (g_function_local_scopes.size() >= static_cast<size_t>(kMaxExecRecursionDepth)) {
        cerr << "zsh: maximum function recursion depth (" << kMaxExecRecursionDepth << ") exceeded\n";
        g_env.last_exit_code = 1;
        return false;
    }

    g_env.positional_args = args;
    g_script_returning = false;
    g_function_local_scopes.push_back({g_env.vars, g_env.indexed_arrays, g_env.assoc_arrays,
                       g_env.integer_vars, g_env.readonly_vars, g_env.unique_arrays, {}});

    rc = execute_command_line(body);
    if (g_script_returning) {
        rc = g_script_return_code;
        g_script_returning = false;
    }
    if (g_env.vars.count("__trap_RETURN") && !g_env.vars["__trap_RETURN"].empty()) {
        execute_command_line(g_env.vars["__trap_RETURN"]);
    }

    FunctionLocalScope completed_scope = std::move(g_function_local_scopes.back());
    g_function_local_scopes.pop_back();
    restore_function_locals(completed_scope);

    g_env.positional_args = old_pos;
    g_script_returning = old_returning;
    g_script_return_code = old_return_code;
    return true;
}

bool try_define_function(const string& statement) {
    static const regex fn_def_re(R"(^\s*([A-Za-z_][A-Za-z0-9_]*)\s*\(\s*\)\s*\{([\s\S]*)\}\s*$)");
    static const regex fn_kw_re(R"(^\s*function\s+([A-Za-z_][A-Za-z0-9_]*)\s*\{([\s\S]*)\}\s*$)");
    static const regex fn_kw_paren_re(R"(^\s*function\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(\s*\)\s*\{([\s\S]*)\}\s*$)");
    smatch m;
    if (regex_match(statement, m, fn_def_re) || regex_match(statement, m, fn_kw_re) || regex_match(statement, m, fn_kw_paren_re)) {
        string name = m[1].str();
        string body = trim_copy(m[2].str());
        g_env.functions[name] = body;
        return true;
    }
    return false;
}

int execute_subshell_block(const string& statement) {
    string s = trim_copy(statement);
    if (s.size() < 2 || s.front() != '(' || s.back() != ')') return 1;

    if (g_function_local_scopes.size() >= static_cast<size_t>(kMaxExecRecursionDepth)) {
        cerr << "zsh: maximum subshell recursion depth (" << kMaxExecRecursionDepth << ") exceeded\n";
        g_env.last_exit_code = 1;
        return 1;
    }

    ZshEnvironment saved_environment = g_env;
    auto read_process_environment = []() {
        map<string, string> values;
        LPCH block = GetEnvironmentStringsA();
        if (!block) return values;
        for (const char* entry = block; *entry; entry += strlen(entry) + 1) {
            string item(entry);
            size_t equal = item.find('=');
            if (equal != string::npos && equal > 0)
                values[item.substr(0, equal)] = item.substr(equal + 1);
        }
        FreeEnvironmentStringsA(block);
        return values;
    };
    map<string, string> saved_process_environment = read_process_environment();
    error_code cwd_error;
    fs::path saved_cwd = fs::current_path(cwd_error);
    bool saved_returning = g_script_returning;
    int saved_return_code = g_script_return_code;
    bool saved_exiting = g_subshell_exiting;
    int saved_exit_code = g_subshell_exit_code;

    ++g_subshell_depth;
    g_subshell_exiting = false;
    g_subshell_exit_code = 0;
    g_function_local_scopes.push_back({g_env.vars, g_env.indexed_arrays, g_env.assoc_arrays, {}});

    int status = execute_command_line(trim_copy(s.substr(1, s.size() - 2)));
    if (g_subshell_exiting) status = g_subshell_exit_code;

    g_function_local_scopes.pop_back();
    --g_subshell_depth;
    g_env = std::move(saved_environment);
    g_script_returning = saved_returning;
    g_script_return_code = saved_return_code;
    g_subshell_exiting = saved_exiting;
    g_subshell_exit_code = saved_exit_code;
    map<string, string> current_process_environment = read_process_environment();
    for (const auto& [name, value] : current_process_environment) {
        if (!saved_process_environment.count(name)) SetEnvironmentVariableA(name.c_str(), nullptr);
    }
    for (const auto& [name, value] : saved_process_environment)
        SetEnvironmentVariableA(name.c_str(), value.c_str());
    if (!cwd_error) {
        error_code restore_error;
        fs::current_path(saved_cwd, restore_error);
    }
    return status;
}

bool is_shell_pipeline_stage(const SingleCmd& command) {
    if (command.args.empty()) return false;
    string name = command.args[0];
    if (g_env.aliases.count(name)) {
        vector<string> alias_words = tokenize_words(g_env.aliases[name]);
        if (!alias_words.empty()) name = alias_words[0];
    }
    if (is_zsh_builtin_command(name) || g_env.functions.count(name) != 0) return true;
    if (command.args.size() == 1 && g_env.options.count("autocd") && g_env.options.at("autocd")) {
        error_code ec;
        string win_path = normalize_path_to_win(name);
        bool is_drive_letter = (name.size() == 2 && isalpha((unsigned char)name[0]) && name[1] == ':');
        if (is_drive_letter || fs::is_directory(win_path, ec)) return true;
    }
    return false;
}

int execute_mixed_pipeline(const Pipeline& pipeline) {
    Pipeline concurrent = pipeline;
    string shell_executable = current_shell_executable_path();
    if (shell_executable.empty()) {
        cerr << "zsh: unable to locate shell executable for pipeline stage\n";
        return 1;
    }
    vector<string> state_paths;

    for (auto& command : concurrent.cmds) {
        if (!is_shell_pipeline_stage(command)) continue;
        string state_path = create_temp_process_subst_path();
        if (state_path.empty() || !save_pipeline_shell_state(state_path)) {
            cerr << "zsh: unable to save pipeline shell state\n";
            for (const auto& path : state_paths) {
                error_code remove_error;
                fs::remove(normalize_path_to_win(path), remove_error);
            }
            return 1;
        }
        state_paths.push_back(state_path);
        vector<string> child_args{shell_executable, "--pipeline-stage", state_path};
        child_args.insert(child_args.end(), command.args.begin(), command.args.end());
        command.args = std::move(child_args);
    }

    int status = execute_pipeline_native(concurrent);
    if (!pipeline.background) {
        for (const auto& path : state_paths) {
            error_code remove_error;
            fs::remove(normalize_path_to_win(path), remove_error);
        }
    }
    return status;
}

HANDLE shell_fd_handle(int fd) {
    auto active = g_active_command_fds.find(fd);
    if (active != g_active_command_fds.end()) return active->second;
    auto persistent = g_persistent_fds.find(fd);
    if (persistent != g_persistent_fds.end()) return persistent->second;
    if (fd == 0) return GetStdHandle(STD_INPUT_HANDLE);
    if (fd == 1) return GetStdHandle(STD_OUTPUT_HANDLE);
    if (fd == 2) return GetStdHandle(STD_ERROR_HANDLE);
    return INVALID_HANDLE_VALUE;
}

bool duplicate_shell_fd(int source_fd, HANDLE& duplicate) {
    HANDLE source = shell_fd_handle(source_fd);
    duplicate = nullptr;
    return source != INVALID_HANDLE_VALUE && source != nullptr &&
           DuplicateHandle(GetCurrentProcess(), source, GetCurrentProcess(), &duplicate,
                           0, TRUE, DUPLICATE_SAME_ACCESS) != FALSE;
}

bool apply_extra_descriptors(const SingleCmd& command, map<int, HANDLE>& destination) {
    SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    for (const auto& [fd, path] : command.extra_input_files) {
        HANDLE handle = CreateFileW(string_to_wstring(normalize_path_to_win(path)).c_str(), GENERIC_READ,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE, &attributes, OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) return false;
        destination[fd] = handle;
    }
    for (const auto& [fd, target] : command.extra_output_files) {
        DWORD creation = target.second ? OPEN_ALWAYS : CREATE_ALWAYS;
        HANDLE handle = CreateFileW(string_to_wstring(normalize_path_to_win(target.first)).c_str(), GENERIC_WRITE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE, &attributes, creation,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) return false;
        if (target.second) SetFilePointer(handle, 0, nullptr, FILE_END);
        destination[fd] = handle;
    }
    for (const auto& [fd, source_fd] : command.extra_fd_duplications) {
        HANDLE duplicate = nullptr;
        if (!duplicate_shell_fd(source_fd, duplicate)) return false;
        destination[fd] = duplicate;
    }
    return true;
}

void close_fd_map(map<int, HANDLE>& descriptors) {
    for (auto& [fd, handle] : descriptors) close_handle_if_valid(handle);
    descriptors.clear();
}

int apply_persistent_exec_descriptors(const SingleCmd& command) {
    for (int fd : command.extra_closed_fds) {
        auto found = g_persistent_fds.find(fd);
        if (found != g_persistent_fds.end()) {
            close_handle_if_valid(found->second);
            g_persistent_fds.erase(found);
        }
    }
    map<int, HANDLE> replacements;
    if (!apply_extra_descriptors(command, replacements)) {
        close_fd_map(replacements);
        cerr << "exec: unable to open file descriptor\n";
        return 1;
    }
    for (auto& [fd, handle] : replacements) {
        auto existing = g_persistent_fds.find(fd);
        if (existing != g_persistent_fds.end()) close_handle_if_valid(existing->second);
        g_persistent_fds[fd] = handle;
    }
    replacements.clear();
    return 0;
}

bool split_compound_and_redirection(const string& stmt, string& compound_part, string& redir_part) {
    string s = trim_copy(stmt);
    if (s.empty()) return false;
    if (s.size() >= 2 && s[0] == '(' && (s[1] == '(' || s[1] == ')')) return false;
    if (s.rfind("()", 0) == 0 || s.rfind("function", 0) == 0) return false;

    bool starts_compound = false;
    if (s[0] == '{' || s[0] == '(') starts_compound = true;
    else if (starts_with_word_trimmed(s, "if") || starts_with_word_trimmed(s, "case") ||
             starts_with_word_trimmed(s, "for") || starts_with_word_trimmed(s, "while") ||
             starts_with_word_trimmed(s, "until") || starts_with_word_trimmed(s, "select") ||
             starts_with_word_trimmed(s, "repeat") || starts_with_word_trimmed(s, "try")) {
        starts_compound = true;
    }
    if (!starts_compound) return false;

    vector<string> control_stack;
    bool at_command_start = true;
    size_t i = 0;
    size_t n = s.size();
    size_t end_pos = string::npos;

    while (i < n) {
        while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r')) {
            i++;
        }
        if (i >= n) break;

        if (s[i] == '\n') {
            at_command_start = true;
            i++;
            continue;
        }

        if (s[i] == '#' && at_command_start) {
            while (i < n && s[i] != '\n') i++;
            continue;
        }

        if (s[i] == ';') {
            if (i + 1 < n && s[i+1] == ';') {
                i += 2;
                at_command_start = true;
                continue;
            }
            at_command_start = true;
            i++;
            continue;
        }
        if (s[i] == '&') {
            if (i + 1 < n && s[i+1] == '&') {
                i += 2;
                at_command_start = true;
                continue;
            }
            i++;
            at_command_start = true;
            continue;
        }
        if (s[i] == '|') {
            if (i + 1 < n && (s[i+1] == '|' || s[i+1] == '&')) {
                i += 2;
                at_command_start = true;
                continue;
            }
            i++;
            at_command_start = true;
            continue;
        }

        if (s[i] == '{' || s[i] == '}' || s[i] == '(' || s[i] == ')') {
            char ch = s[i];
            i++;
            if (ch == '{') {
                control_stack.push_back("{");
                at_command_start = true;
            } else if (ch == '(') {
                control_stack.push_back("(");
                at_command_start = true;
            } else if (ch == '}') {
                if (!control_stack.empty() && (control_stack.back() == "{" || control_stack.back() == "try")) {
                    control_stack.pop_back();
                }
                if (control_stack.empty()) {
                    size_t next_p = i;
                    while (next_p < n && (s[next_p] == ' ' || s[next_p] == '\t' || s[next_p] == '\r' || s[next_p] == '\n')) next_p++;
                    if (next_p < n && s.compare(next_p, 6, "always") == 0 && (next_p + 6 >= n || !is_word_char(s[next_p + 6]))) {
                        control_stack.push_back("try");
                        i = next_p + 6;
                        at_command_start = true;
                        continue;
                    }
                    end_pos = i;
                    break;
                }
                at_command_start = false;
            } else if (ch == ')') {
                if (!control_stack.empty() && control_stack.back() == "(") {
                    control_stack.pop_back();
                }
                if (control_stack.empty()) {
                    end_pos = i;
                    break;
                }
                at_command_start = false;
            }
            continue;
        }

        string word;
        bool in_sq = false, in_dq = false, escaped = false;
        while (i < n) {
            char c = s[i];
            if (escaped) {
                word += c;
                escaped = false;
                i++;
                continue;
            }
            if (c == '\\' && !in_sq) {
                escaped = true;
                i++;
                continue;
            }
            if (c == '\'' && !in_dq) {
                in_sq = !in_sq;
                word += c;
                i++;
                continue;
            }
            if (c == '"' && !in_sq) {
                in_dq = !in_dq;
                word += c;
                i++;
                continue;
            }
            if (!in_sq && !in_dq) {
                if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ';' || c == '&' || c == '|' || c == '{' || c == '}' || c == '(' || c == ')') {
                    break;
                }
            }
            word += c;
            i++;
        }

        if (at_command_start) {
            if (word == "if") {
                control_stack.push_back("if");
                at_command_start = true;
            } else if (word == "then" || word == "else" || word == "elif") {
                at_command_start = true;
            } else if (word == "fi") {
                if (!control_stack.empty() && control_stack.back() == "if") {
                    control_stack.pop_back();
                }
                if (control_stack.empty()) {
                    end_pos = i;
                    break;
                }
                at_command_start = false;
            } else if (word == "case") {
                control_stack.push_back("case");
                at_command_start = false;
            } else if (word == "esac") {
                if (!control_stack.empty() && control_stack.back() == "case") {
                    control_stack.pop_back();
                }
                if (control_stack.empty()) {
                    end_pos = i;
                    break;
                }
                at_command_start = false;
            } else if (word == "for" || word == "while" || word == "until" || word == "select" || word == "repeat") {
                control_stack.push_back("loop");
                at_command_start = false;
            } else if (word == "do") {
                at_command_start = true;
            } else if (word == "done") {
                if (!control_stack.empty() && control_stack.back() == "loop") {
                    control_stack.pop_back();
                }
                if (control_stack.empty()) {
                    end_pos = i;
                    break;
                }
                at_command_start = false;
            } else if (word == "try") {
                control_stack.push_back("try");
                at_command_start = true;
            } else {
                at_command_start = false;
            }
        } else {
            if (word == "then" || word == "else" || word == "elif" || word == "do") {
                at_command_start = true;
            } else {
                at_command_start = false;
            }
        }
    }

    if (end_pos != string::npos && end_pos < s.size()) {
        compound_part = trim_copy(s.substr(0, end_pos));
        redir_part = trim_copy(s.substr(end_pos));
        return !redir_part.empty();
    }
    return false;
}

int execute_compound_with_redirection(const string& compound_part, const string& redir_part) {
    Pipeline pl = parse_pipeline("dummy " + redir_part);
    if (!pl.error.empty() || pl.cmds.empty()) {
        cerr << "zsh: parse error: " << (pl.error.empty() ? "invalid redirection" : pl.error) << "\n";
        return 1;
    }
    const SingleCmd& sc = pl.cmds[0];

    HANDLE oldStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE oldStdErr = GetStdHandle(STD_ERROR_HANDLE);
    HANDLE oldStdIn = GetStdHandle(STD_INPUT_HANDLE);
    int oldFdOut = _dup(_fileno(stdout));
    int oldFdErr = _dup(_fileno(stderr));
    int oldFdIn = _dup(_fileno(stdin));

    cout.flush();
    cerr.flush();

    unique_handle hFileOut = make_unique_handle();
    unique_handle hFileErr = make_unique_handle();
    unique_handle hFileIn = make_unique_handle();
    unique_handle hHereRead = make_unique_handle();
    unique_handle hHereWrite = make_unique_handle();

    bool error_occurred = false;

    // 1. Input Redirection / Closure
    if (sc.close_stdin) {
        SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
        HANDLE hNul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hNul != INVALID_HANDLE_VALUE) {
            hFileIn = make_unique_handle(hNul);
            SetStdHandle(STD_INPUT_HANDLE, (HANDLE)hFileIn.get());
            HANDLE rawInDup = INVALID_HANDLE_VALUE;
            if (DuplicateHandle(GetCurrentProcess(), (HANDLE)hFileIn.get(), GetCurrentProcess(), &rawInDup, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
                int fd = _open_osfhandle((intptr_t)rawInDup, _O_TEXT);
                if (fd != -1) {
                    _dup2(fd, _fileno(stdin));
                    _close(fd);
                }
            }
        }
    } else if (!sc.here_string.empty()) {
        HANDLE rawR = NULL, rawW = NULL;
        SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
        if (CreatePipe(&rawR, &rawW, &sa, 0)) {
            hHereRead = make_unique_handle(rawR);
            hHereWrite = make_unique_handle(rawW);
            string payload = sc.here_string + "\n";
            DWORD written = 0;
            WriteFile((HANDLE)hHereWrite.get(), payload.data(), static_cast<DWORD>(payload.size()), &written, NULL);
            hHereWrite.reset();
            SetStdHandle(STD_INPUT_HANDLE, (HANDLE)hHereRead.get());
            HANDLE rawInDup = INVALID_HANDLE_VALUE;
            if (DuplicateHandle(GetCurrentProcess(), (HANDLE)hHereRead.get(), GetCurrentProcess(), &rawInDup, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
                int fd = _open_osfhandle((intptr_t)rawInDup, _O_TEXT);
                if (fd != -1) {
                    _dup2(fd, _fileno(stdin));
                    _close(fd);
                }
            }
        }
    } else if (!sc.input_file.empty()) {
        SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
        HANDLE h = CreateFileW(string_to_wstring(normalize_path_to_win(sc.input_file)).c_str(),
                               GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, &sa,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE) {
            cerr << "zsh: cannot open input file: " << sc.input_file << "\n";
            error_occurred = true;
        } else {
            hFileIn = make_unique_handle(h);
            SetStdHandle(STD_INPUT_HANDLE, (HANDLE)hFileIn.get());
            HANDLE rawInDup = INVALID_HANDLE_VALUE;
            if (DuplicateHandle(GetCurrentProcess(), (HANDLE)hFileIn.get(), GetCurrentProcess(), &rawInDup, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
                int fd = _open_osfhandle((intptr_t)rawInDup, _O_TEXT);
                if (fd != -1) {
                    _dup2(fd, _fileno(stdin));
                    _close(fd);
                }
            }
        }
    }

    // 2. Output Redirection / Closure
    if (!error_occurred && sc.close_stdout) {
        SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
        HANDLE hNul = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hNul != INVALID_HANDLE_VALUE) {
            hFileOut = make_unique_handle(hNul);
            SetStdHandle(STD_OUTPUT_HANDLE, (HANDLE)hFileOut.get());
            HANDLE rawOutDup = INVALID_HANDLE_VALUE;
            if (DuplicateHandle(GetCurrentProcess(), (HANDLE)hFileOut.get(), GetCurrentProcess(), &rawOutDup, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
                int fd = _open_osfhandle((intptr_t)rawOutDup, _O_TEXT);
                if (fd != -1) {
                    _dup2(fd, _fileno(stdout));
                    _close(fd);
                }
            }
        }
    } else if (!error_occurred && !sc.output_file.empty()) {
        SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
        DWORD creation = sc.append_out ? OPEN_ALWAYS : CREATE_ALWAYS;
        HANDLE h = CreateFileW(string_to_wstring(normalize_path_to_win(sc.output_file)).c_str(),
                               GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, &sa,
                               creation, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE) {
            cerr << "zsh: cannot open output file: " << sc.output_file << "\n";
            error_occurred = true;
        } else {
            hFileOut = make_unique_handle(h);
            if (sc.append_out) {
                SetFilePointer((HANDLE)hFileOut.get(), 0, NULL, FILE_END);
            }
            SetStdHandle(STD_OUTPUT_HANDLE, (HANDLE)hFileOut.get());
            HANDLE rawOutDup = INVALID_HANDLE_VALUE;
            if (DuplicateHandle(GetCurrentProcess(), (HANDLE)hFileOut.get(), GetCurrentProcess(), &rawOutDup, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
                int fd = _open_osfhandle((intptr_t)rawOutDup, _O_TEXT);
                if (fd != -1) {
                    _dup2(fd, _fileno(stdout));
                    _close(fd);
                }
            }
        }
    }

    // 3. Error Redirection / Closure
    if (!error_occurred && sc.close_stderr) {
        SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
        HANDLE hNul = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hNul != INVALID_HANDLE_VALUE) {
            hFileErr = make_unique_handle(hNul);
            SetStdHandle(STD_ERROR_HANDLE, (HANDLE)hFileErr.get());
            HANDLE rawErrDup = INVALID_HANDLE_VALUE;
            if (DuplicateHandle(GetCurrentProcess(), (HANDLE)hFileErr.get(), GetCurrentProcess(), &rawErrDup, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
                int fd = _open_osfhandle((intptr_t)rawErrDup, _O_TEXT);
                if (fd != -1) {
                    _dup2(fd, _fileno(stderr));
                    _close(fd);
                }
            }
        }
    } else if (!error_occurred && !sc.error_file.empty()) {
        SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
        DWORD creation = sc.append_err ? OPEN_ALWAYS : CREATE_ALWAYS;
        HANDLE h = CreateFileW(string_to_wstring(normalize_path_to_win(sc.error_file)).c_str(),
                               GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, &sa,
                               creation, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE) {
            cerr << "zsh: cannot open error file: " << sc.error_file << "\n";
            error_occurred = true;
        } else {
            hFileErr = make_unique_handle(h);
            if (sc.append_err) {
                SetFilePointer((HANDLE)hFileErr.get(), 0, NULL, FILE_END);
            }
            SetStdHandle(STD_ERROR_HANDLE, (HANDLE)hFileErr.get());
            HANDLE rawErrDup = INVALID_HANDLE_VALUE;
            if (DuplicateHandle(GetCurrentProcess(), (HANDLE)hFileErr.get(), GetCurrentProcess(), &rawErrDup, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
                int fd = _open_osfhandle((intptr_t)rawErrDup, _O_TEXT);
                if (fd != -1) {
                    _dup2(fd, _fileno(stderr));
                    _close(fd);
                }
            }
        }
    } else if (!error_occurred && sc.dup_stderr_from == 1) {
        HANDLE curOut = GetStdHandle(STD_OUTPUT_HANDLE);
        SetStdHandle(STD_ERROR_HANDLE, curOut);
        _dup2(_fileno(stdout), _fileno(stderr));
    } else if (!error_occurred && sc.dup_stdout_from == 2) {
        HANDLE curErr = GetStdHandle(STD_ERROR_HANDLE);
        SetStdHandle(STD_OUTPUT_HANDLE, curErr);
        _dup2(_fileno(stderr), _fileno(stdout));
    }

    int rc = 1;
    if (!error_occurred) {
        rc = execute_statement_block_aware(compound_part);
    }

    cout.flush();
    cerr.flush();

    if (oldFdOut != -1) { _dup2(oldFdOut, _fileno(stdout)); _close(oldFdOut); }
    if (oldFdErr != -1) { _dup2(oldFdErr, _fileno(stderr)); _close(oldFdErr); }
    if (oldFdIn != -1) { _dup2(oldFdIn, _fileno(stdin)); _close(oldFdIn); }

    SetStdHandle(STD_OUTPUT_HANDLE, oldStdOut);
    SetStdHandle(STD_ERROR_HANDLE, oldStdErr);
    SetStdHandle(STD_INPUT_HANDLE, oldStdIn);

    hFileIn.reset();
    hFileOut.reset();
    hFileErr.reset();
    hHereRead.reset();
    hHereWrite.reset();

    return rc;
}

int execute_statement_block_aware(const string& statement) {
    string stmt = trim_copy(statement);
    if (stmt.empty()) return 0;
    int anon_rc = 0;
    if (try_execute_anonymous_function(stmt, anon_rc)) return anon_rc;
    if (try_define_function(stmt)) return 0;
    string compound_part, redir_part;
    if (split_compound_and_redirection(stmt, compound_part, redir_part)) {
        return execute_compound_with_redirection(compound_part, redir_part);
    }
    if (stmt.size() >= 4 && stmt.rfind("((", 0) == 0 && stmt.substr(stmt.size() - 2) == "))") {
        string expr = trim_copy(stmt.substr(2, stmt.size() - 4));
        return parse_and_execute("let " + expr);
    }
    if (starts_with_word_trimmed(stmt, "try") || (stmt.front() == '{' && find_top_level_word(stmt, "always") != string::npos))
        return execute_always_block(stmt);
    if (starts_with_word_trimmed(stmt, "time")) return execute_timed_command(stmt);
    if (stmt.size() >= 2 && stmt.front() == '(' && stmt.back() == ')') return execute_subshell_block(stmt);
    if (stmt.size() >= 2 && stmt.front() == '{' && stmt.back() == '}')
        return execute_command_line(trim_copy(stmt.substr(1, stmt.size() - 2)));
    if (starts_with_word_trimmed(stmt, "if") && find_top_level_word(stmt, "fi") != string::npos) return execute_if_block(stmt);
    if (starts_with_word_trimmed(stmt, "case") && find_top_level_word(stmt, "esac") != string::npos) return execute_case_block(stmt);
    if (starts_with_word_trimmed(stmt, "for") && stmt.find("((") != string::npos && stmt.find("))") != string::npos && find_top_level_word(stmt, "done") != string::npos) return execute_cstyle_for_loop(stmt);
    if (starts_with_word_trimmed(stmt, "for") && find_top_level_word(stmt, "done") != string::npos) return execute_for_loop(stmt);
    if (starts_with_word_trimmed(stmt, "while")) return execute_condition_loop(stmt, false);
    if (starts_with_word_trimmed(stmt, "until")) return execute_condition_loop(stmt, true);
    if (starts_with_word_trimmed(stmt, "repeat")) return execute_repeat_loop(stmt);
    if (starts_with_word_trimmed(stmt, "select")) return execute_select_loop(stmt);
    int rc = execute_single_command(stmt);
    g_env.vars["pipestatus"] = to_string(rc);
    g_env.indexed_arrays["pipestatus"] = { to_string(rc) };
    return rc;
}

int execute_single_command(const string& line) {
    if (line.empty()) return 0;
    Pipeline pl = parse_pipeline(line);
    if (!pl.error.empty()) {
        cerr << "zsh: parse error: " << pl.error << "\n";
        return 1;
    }
    if (pl.cmds.empty()) return 0;

    if (pl.cmds.size() == 1 && pl.cmds[0].args.size() == 1 && pl.cmds[0].args[0] == "exec" &&
        (!pl.cmds[0].extra_input_files.empty() || !pl.cmds[0].extra_output_files.empty() ||
         !pl.cmds[0].extra_fd_duplications.empty() || !pl.cmds[0].extra_closed_fds.empty())) {
        return apply_persistent_exec_descriptors(pl.cmds[0]);
    }

    if (g_env.options.count("multios") && g_env.options.at("multios") &&
        !pl.cmds.empty() && pl.cmds.back().output_files.size() > 1) {
        string shell_executable = current_shell_executable_path();
        if (shell_executable.empty()) return 1;
        SingleCmd tee;
        tee.args = {shell_executable, "--multios-tee"};
        for (const auto& target : pl.cmds.back().output_files) {
            tee.args.push_back(target.second ? "append" : "truncate");
            tee.args.push_back(target.first);
        }
        pl.cmds.back().output_file.clear();
        pl.cmds.back().output_files.clear();
        pl.cmds.push_back(std::move(tee));
    }

    auto needs_launcher = [&](const SingleCmd& c) {
        return !c.input_file.empty() || !c.output_file.empty() || !c.error_file.empty() ||
               !c.here_string.empty() || c.dup_stdin_from >= 0 || c.dup_stdout_from >= 0 || c.dup_stderr_from >= 0 ||
               c.close_stdin || c.close_stdout || c.close_stderr || !c.extra_input_files.empty() ||
               !c.extra_output_files.empty() || !c.extra_fd_duplications.empty() || !c.extra_closed_fds.empty();
    };

    int rc;
    bool has_shell_stage = any_of(pl.cmds.begin(), pl.cmds.end(), is_shell_pipeline_stage);
    bool scoped_extra_descriptors = pl.cmds.size() == 1 && is_shell_pipeline_stage(pl.cmds[0]) &&
                                    (!pl.cmds[0].extra_input_files.empty() || !pl.cmds[0].extra_output_files.empty() ||
                                     !pl.cmds[0].extra_fd_duplications.empty() || !pl.cmds[0].extra_closed_fds.empty());
    if (scoped_extra_descriptors) {
        if (!apply_extra_descriptors(pl.cmds[0], g_active_command_fds)) rc = 1;
        else rc = dispatch_command(pl.cmds[0].args);
        close_fd_map(g_active_command_fds);
    } else if (pl.cmds.size() == 1 && is_shell_pipeline_stage(pl.cmds[0]) && !pl.background && needs_launcher(pl.cmds[0])) {
        string cmd_str;
        for (size_t i = 0; i < pl.cmds[0].args.size(); ++i) {
            if (i > 0) cmd_str += ' ';
            cmd_str += quote_for_shell_path(pl.cmds[0].args[i]);
        }
        string redir_str;
        if (!pl.cmds[0].input_file.empty()) redir_str += " < " + quote_for_shell_path(normalize_path_to_unix(pl.cmds[0].input_file));
        if (!pl.cmds[0].here_string.empty()) redir_str += " <<< " + quote_for_shell_path(pl.cmds[0].here_string);
        if (!pl.cmds[0].output_file.empty()) redir_str += (pl.cmds[0].append_out ? " >> " : " > ") + quote_for_shell_path(normalize_path_to_unix(pl.cmds[0].output_file));
        if (!pl.cmds[0].error_file.empty()) redir_str += " 2> " + quote_for_shell_path(normalize_path_to_unix(pl.cmds[0].error_file));
        if (pl.cmds[0].dup_stderr_from == 1) redir_str += " 2>&1";
        if (pl.cmds[0].dup_stdout_from == 2) redir_str += " 1>&2";
        rc = execute_compound_with_redirection(cmd_str, redir_str);
    } else if (has_shell_stage &&
               (pl.background || pl.cmds.size() > 1 || needs_launcher(pl.cmds[0]))) {
        rc = execute_mixed_pipeline(pl);
    } else if (pl.background || pl.cmds.size() > 1 || needs_launcher(pl.cmds[0])) {
        rc = execute_pipeline_native(pl);
    } else {
        rc = dispatch_command(pl.cmds[0].args);
    }

    // Complete deferred >(command) sinks after the producer command exits.
    if (g_process_subst_eval_depth == 0 && !g_process_subst_sinks.empty()) {
        auto sinks = g_process_subst_sinks;
        g_process_subst_sinks.clear();
        for (const auto& s : sinks) {
            parse_and_execute(s.command + " < " + quote_for_shell_path(normalize_path_to_unix(s.path)));
        }
    }

    // Cleanup all temp files created for this command's process substitutions.
    if (g_process_subst_eval_depth == 0 && !g_process_subst_temp_files.empty()) {
        auto files = g_process_subst_temp_files;
        g_process_subst_temp_files.clear();
        for (const auto& p : files) {
            error_code ec;
            fs::remove(normalize_path_to_win(p), ec);
            if (ec) cerr << "zsh: warning: could not remove process-substitution temp file: " << p << "\n";
        }
    }

    return rc;
}

int execute_command_line(const string& line) {
    // Recursion guard at the true recursion point (function calls and command
    // substitutions re-enter execute_command_line, not parse_and_execute).
    if (g_exec_recursion_depth >= kMaxExecRecursionDepth) {
        cerr << "zsh: maximum execution recursion depth exceeded\n";
        g_env.last_exit_code = 1;
        return 1;
    }
    struct ExecDepthGuard {
        ExecDepthGuard()  { ++g_exec_recursion_depth; }
        ~ExecDepthGuard() { --g_exec_recursion_depth; }
    } exec_depth_guard;

    CommandListAst ast = parse_command_list_ast(line);
    if (!ast.error.empty()) {
        cerr << "zsh: parse error: " << ast.error << "\n";
        g_env.last_exit_code = 1;
        return 1;
    }
    if (ast.nodes.empty()) return 0;

    static bool in_trap_debug = false;
    static bool in_trap_zerr = false;

    int last_status = 0;
    for (size_t i = 0; i < ast.nodes.size(); ++i) {
        const CommandListNode& node = ast.nodes[i];
        bool should_run = i == 0 || node.connector == CommandListConnector::Always ||
                          (node.connector == CommandListConnector::And && last_status == 0) ||
                          (node.connector == CommandListConnector::Or && last_status != 0);
        if (should_run) {
            if (!in_trap_debug && g_env.vars.count("__trap_DEBUG") && !g_env.vars["__trap_DEBUG"].empty()) {
                in_trap_debug = true;
                execute_command_line(g_env.vars["__trap_DEBUG"]);
                in_trap_debug = false;
            }
            last_status = execute_statement_block_aware(node.source);
            // Keep $? in sync after each statement so intermediate expansions see it.
            g_env.last_exit_code = last_status;
            if (last_status != 0 && !in_trap_zerr && g_env.vars.count("__trap_ZERR") && !g_env.vars["__trap_ZERR"].empty()) {
                in_trap_zerr = true;
                execute_command_line(g_env.vars["__trap_ZERR"]);
                in_trap_zerr = false;
            }
            if (g_in_try_block && last_status != 0 && (node.connector == CommandListConnector::Always || node.connector == CommandListConnector::And)) break;
            if (g_subshell_exiting || g_script_returning || g_loop_breaking > 0 || g_loop_continuing > 0) break;
        }
    }
    return last_status;
}

int parse_and_execute(const string& line) {
    process_pending_traps();
    int rc = execute_command_line(line);
    g_env.last_exit_code = rc;
    if (g_loop_depth == 0) {
        g_loop_breaking = 0;
        g_loop_continuing = 0;
    }
    return rc;
}

string register_heredoc_payload(const string& content) {
    static const size_t kMaxHeredocPayloads = 4096;
    if (g_heredoc_payloads.size() >= kMaxHeredocPayloads) {
        g_heredoc_payloads.erase(g_heredoc_payloads.begin());
    }
    string marker = "__HEREDOC_MEM_" + to_string(GetCurrentProcessId()) + "_" + to_string(++g_heredoc_seq) + "__";
    g_heredoc_payloads[marker] = content;
    return marker;
}

string materialize_heredoc_block(const string& block) {
    vector<string> lines = split_lines_preserve_empty(block);
    if (lines.empty()) return block;

    vector<HeredocSpec> specs;
    string header_rewritten;
    if (!parse_heredoc_specs(lines[0], specs, header_rewritten)) return block;

    size_t cursor = 1;
    vector<string> markers;
    for (const auto& spec : specs) {
        string body;
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
            string row = lines[cursor];
            if (spec.strip_tabs) {
                size_t p = 0;
                while (p < row.size() && row[p] == '\t') ++p;
                row = row.substr(p);
            }
            if (spec.expand_body) row = g_env.expand_vars(row);
            body += row;
            body += "\n";
        }
        if (!found_end) return block;

        markers.push_back(register_heredoc_payload(body));
    }

    string out = header_rewritten;
    for (size_t i = 0; i < markers.size(); ++i) {
        string ph = "__HEREDOC_" + to_string(i) + "__";
        string rp = markers[i];
        size_t p = 0;
        while ((p = out.find(ph, p)) != string::npos) {
            out.replace(p, ph.size(), rp);
            p += rp.size();
        }
    }

    while (cursor < lines.size()) {
        string tail = trim_copy(lines[cursor]);
        if (!tail.empty()) {
            out += " ; ";
            out += lines[cursor];
        }
        ++cursor;
    }
    return out;
}

int execute_script(const string& filepath, bool trace, bool errexit) {
    static int g_script_source_depth = 0;
    if (g_script_source_depth >= kMaxExecRecursionDepth) {
        cerr << "zsh: maximum source recursion depth (" << kMaxExecRecursionDepth << ") exceeded\n";
        g_env.last_exit_code = 1;
        return 1;
    }
    struct ScriptSourceGuard {
        ScriptSourceGuard()  { ++g_script_source_depth; }
        ~ScriptSourceGuard() { --g_script_source_depth; }
    } script_source_guard;

    if (filepath.empty()) {
        cerr << "zsh: source: filename argument required\n";
        return 1;
    }
    fs::path p = normalize_path_to_win(filepath);
    if (!fs::exists(p) || fs::is_directory(p)) {
        cerr << "zsh: no such file or directory: " << filepath << "\n";
        return 127;
    }
    ifstream file(p);
    if (!file.is_open()) return 127;

    auto count_word = [](const string& s, const string& word) {
        int count = 0;
        bool in_sq = false, in_dq = false;
        for (size_t i = 0; i < s.size(); ++i) {
            char ch = s[i];
            if (ch == '"' && !in_sq) { in_dq = !in_dq; continue; }
            if (ch == '\'' && !in_dq) { in_sq = !in_sq; continue; }
            if (in_sq || in_dq) continue;
            if (i + word.size() > s.size()) continue;
            if (s.compare(i, word.size(), word) != 0) continue;
            bool left_ok = (i == 0) || !is_word_char(s[i - 1]);
            bool right_ok = (i + word.size() >= s.size()) || !is_word_char(s[i + word.size()]);
            if (left_ok && right_ok) count++;
        }
        return count;
    };

    auto count_char_top_level = [](const string& s, char target) {
        int count = 0;
        bool in_sq = false, in_dq = false;
        for (char ch : s) {
            if (ch == '"' && !in_sq) { in_dq = !in_dq; continue; }
            if (ch == '\'' && !in_dq) { in_sq = !in_sq; continue; }
            if (in_sq || in_dq) continue;
            if (ch == target) count++;
        }
        return count;
    };

    string line;
    string block_buffer;
    int status = 0;
    int block_depth = 0;
    g_script_returning = false;

    while (getline(file, line)) {
        string trimmed = trim_copy(line);
        if (block_depth == 0 && (trimmed.empty() || trimmed[0] == '#')) continue;

        if (!block_buffer.empty()) block_buffer += "\n";
        block_buffer += line;

        int opens = 0;
        if (starts_with_word_trimmed(line, "if")) opens++;
        if (starts_with_word_trimmed(line, "for")) opens++;
        if (starts_with_word_trimmed(line, "while") && find_top_level_word(line, "do") != string::npos) opens++;
        if (starts_with_word_trimmed(line, "until") && find_top_level_word(line, "do") != string::npos) opens++;
        if (starts_with_word_trimmed(line, "repeat") && find_top_level_word(line, "do") != string::npos) opens++;
        if (starts_with_word_trimmed(line, "select") && find_top_level_word(line, "do") != string::npos) opens++;
        if (starts_with_word_trimmed(line, "case")) opens++;
        opens += count_char_top_level(line, '{');

        int closes = 0;
        closes += count_word(line, "fi");
        closes += count_word(line, "done");
        closes += count_word(line, "esac");
        closes += count_char_top_level(line, '}');

        block_depth += (opens - closes);
        if (block_depth < 0) block_depth = 0;

        if (block_depth == 0) {
            if (has_unterminated_heredoc(block_buffer)) {
                continue;
            }
            string prepared = materialize_heredoc_block(block_buffer);
            if (trace) cerr << "+ " << block_buffer << "\n";
            status = parse_and_execute(prepared);
            block_buffer.clear();
            if (g_script_returning) {
                g_script_returning = false;
                return g_script_return_code;
            }
            if (errexit && status != 0) return status;
        }
    }

    if (!block_buffer.empty()) {
        string prepared = materialize_heredoc_block(block_buffer);
        if (trace) cerr << "+ " << block_buffer << "\n";
        status = parse_and_execute(prepared);
    }
    // Note: do NOT fire the EXIT trap here — execute_script is also used to load
    // .zshrc at startup, and the EXIT trap must only fire when the shell/script
    // actually exits (handled in main).
    return status;
}
