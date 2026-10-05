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

// Script context section: current script name and argument helpers.
const std::vector<std::wstring>& current_script_args() {
    static const std::vector<std::wstring> empty_args;
    if (g_script_context_stack.empty()) {
        return empty_args;
    }
    return g_script_context_stack.back().args;
}

std::wstring current_script_name() {
    if (g_script_context_stack.empty()) {
        return L"";
    }
    return g_script_context_stack.back().script_name;
}

std::wstring join_script_args(const std::vector<std::wstring>& args) {
    std::wstring joined;
    for (size_t i = 0; i < args.size(); ++i) {
        joined += args[i];
        if (i + 1 < args.size()) {
            joined += L" ";
        }
    }
    return joined;
}

std::wstring quote_command_argument(const std::wstring& arg) {
    if (arg.empty()) {
        return L"\"\"";
    }

    bool needs_quotes = false;
    for (wchar_t ch : arg) {
        if (std::iswspace(ch) || ch == L'"') {
            needs_quotes = true;
            break;
        }
    }

    if (!needs_quotes) {
        return arg;
    }

    std::wstring escaped;
    escaped.reserve(arg.size() + 8);
    escaped.push_back(L'"');

    size_t backslash_run = 0;
    for (wchar_t ch : arg) {
        if (ch == L'\\') {
            backslash_run++;
            continue;
        }

        if (ch == L'"') {
            escaped.append(backslash_run * 2 + 1, L'\\');
            escaped.push_back(L'"');
            backslash_run = 0;
            continue;
        }

        if (backslash_run > 0) {
            escaped.append(backslash_run, L'\\');
            backslash_run = 0;
        }

        escaped.push_back(ch);
    }

    if (backslash_run > 0) {
        escaped.append(backslash_run * 2, L'\\');
    }

    escaped.push_back(L'"');
    return escaped;
}

int count_unquoted_brace_delta(const std::wstring& line) {
    int delta = 0;
    bool in_single_quotes = false;
    bool in_double_quotes = false;
    bool escaped = false;

    for (wchar_t ch : line) {
        if (escaped) {
            escaped = false;
            continue;
        }

        if (ch == L'\\' && !in_single_quotes) {
            escaped = true;
            continue;
        }

        if (ch == L'\'' && !in_double_quotes) {
            in_single_quotes = !in_single_quotes;
            continue;
        }

        if (ch == L'"' && !in_single_quotes) {
            in_double_quotes = !in_double_quotes;
            continue;
        }

        if (in_single_quotes || in_double_quotes) {
            continue;
        }

        if (ch == L'#') {
            break;
        }

        if (ch == L'{') {
            delta++;
        } else if (ch == L'}') {
            delta--;
        }
    }

    return delta;
}

bool parse_function_header(const std::wstring& trimmed_line, std::wstring& function_name, bool& has_open_brace) {
    function_name.clear();
    has_open_brace = false;

    if (trimmed_line.empty() || trimmed_line.size() > kMaxFunctionHeaderLineLength) {
        return false;
    }

    std::wstring_view sv(trimmed_line);
    size_t pos = 0;
    while (pos < sv.size() && (sv[pos] == L' ' || sv[pos] == L'\t')) pos++;
    if (pos >= sv.size()) return false;

    bool keyword_mode = false;
    if (sv.size() - pos >= 8 && starts_with_case_insensitive(sv.substr(pos), L"function")) {
        size_t after_fn = pos + 8;
        if (after_fn < sv.size() && (sv[after_fn] == L' ' || sv[after_fn] == L'\t')) {
            keyword_mode = true;
            pos = after_fn;
            while (pos < sv.size() && (sv[pos] == L' ' || sv[pos] == L'\t')) pos++;
        }
    }

    if (pos >= sv.size()) return false;

    wchar_t first_char = sv[pos];
    if (!(std::iswalpha(first_char) || first_char == L'_')) {
        return false;
    }

    size_t ident_start = pos;
    pos++;
    while (pos < sv.size()) {
        wchar_t ch = sv[pos];
        if (std::iswalnum(ch) || ch == L'_' || ch == L'.') {
            pos++;
        } else {
            break;
        }
    }
    std::wstring_view ident_sv = sv.substr(ident_start, pos - ident_start);

    while (pos < sv.size() && (sv[pos] == L' ' || sv[pos] == L'\t')) pos++;

    bool has_parens = false;
    if (pos < sv.size() && sv[pos] == L'(') {
        pos++;
        while (pos < sv.size() && (sv[pos] == L' ' || sv[pos] == L'\t')) pos++;
        if (pos >= sv.size() || sv[pos] != L')') {
            return false;
        }
        pos++;
        has_parens = true;
        while (pos < sv.size() && (sv[pos] == L' ' || sv[pos] == L'\t')) pos++;
    }

    if (!keyword_mode && !has_parens) {
        return false;
    }

    if (pos < sv.size() && sv[pos] == L'{') {
        has_open_brace = true;
        pos++;
        while (pos < sv.size() && (sv[pos] == L' ' || sv[pos] == L'\t')) pos++;
    }

    if (pos < sv.size()) {
        return false;
    }

    function_name = std::wstring(ident_sv);
    return true;
}

bool capture_function_definition(
    const std::vector<std::wstring>& lines,
    size_t& line_index,
    size_t end_index,
    std::wstring& error_message,
    size_t& error_line_index) {
    error_message.clear();
    error_line_index = line_index;

    std::wstring function_name;
    bool has_open_brace = false;
    const std::wstring trimmed = trim_copy(lines[line_index]);
    if (!parse_function_header(trimmed, function_name, has_open_brace)) {
        return false;
    }

    size_t body_start = line_index + 1;
    if (!has_open_brace) {
        size_t probe = line_index + 1;
        bool found_open = false;
        while (probe < end_index) {
            std::wstring probe_trimmed = trim_copy(lines[probe]);
            if (probe_trimmed.empty() || probe_trimmed[0] == L'#') {
                probe++;
                continue;
            }

            if (probe_trimmed == L"{") {
                found_open = true;
                body_start = probe + 1;
                break;
            }

            error_message = L"expected '{' to start function body";
            error_line_index = probe;
            return false;
        }

        if (!found_open) {
            error_message = L"missing '{' for function body";
            error_line_index = line_index;
            return false;
        }
    }

    std::vector<std::wstring> body_lines;
    int brace_depth = 1;
    size_t probe = body_start;
    bool found_close = false;

    while (probe < end_index) {
        const std::wstring current_line = lines[probe];
        const std::wstring current_trimmed = trim_copy(current_line);
        const int delta = count_unquoted_brace_delta(current_line);

        if (brace_depth == 1 && current_trimmed == L"}" && delta == -1) {
            brace_depth = 0;
            found_close = true;
            break;
        }

        body_lines.push_back(current_line);
        brace_depth += delta;
        if (brace_depth < 0) {
            error_message = L"unexpected '}' while parsing function";
            error_line_index = probe;
            return false;
        }

        if (brace_depth == 0) {
            body_lines.pop_back();
            found_close = true;
            break;
        }

        probe++;
    }

    if (!found_close) {
        error_message = L"missing '}' for function body";
        error_line_index = line_index;
        return false;
    }

    ShellFunctionDefinition definition;
    definition.body_lines = body_lines;

    // Enforce per-session function table cap (new entries only).
    if (g_shell_functions.find(function_name) == g_shell_functions.end() &&
        g_shell_functions.size() >= kMaxShellFunctions) {
        std::wcerr << L"ksh: function table limit reached, cannot define: " << function_name << L"\n";
        return false;
    }
    g_shell_functions[function_name] = definition;

    line_index = probe + 1;
    return true;
}

bool is_script_empty_or_comment_line(const std::wstring& trimmed) {
    return trimmed.empty() || trimmed[0] == L'#';
}

bool starts_with_if_control_keyword(const std::wstring& trimmed) {
    if (trimmed.size() < 2) {
        return false;
    }

    if (std::towlower(trimmed[0]) != L'i' || std::towlower(trimmed[1]) != L'f') {
        return false;
    }

    if (trimmed.size() == 2) {
        return true;
    }

    return std::iswspace(trimmed[2]) != 0;
}

bool starts_with_elif_control_keyword(const std::wstring& trimmed) {
    if (trimmed.size() < 4) {
        return false;
    }

    if (std::towlower(trimmed[0]) != L'e' ||
        std::towlower(trimmed[1]) != L'l' ||
        std::towlower(trimmed[2]) != L'i' ||
        std::towlower(trimmed[3]) != L'f') {
        return false;
    }

    if (trimmed.size() == 4) {
        return true;
    }

    return std::iswspace(trimmed[4]) != 0;
}

bool starts_with_while_control_keyword(const std::wstring& trimmed) {
    if (trimmed.size() < 5) {
        return false;
    }

    if (std::towlower(trimmed[0]) != L'w' ||
        std::towlower(trimmed[1]) != L'h' ||
        std::towlower(trimmed[2]) != L'i' ||
        std::towlower(trimmed[3]) != L'l' ||
        std::towlower(trimmed[4]) != L'e') {
        return false;
    }

    if (trimmed.size() == 5) {
        return true;
    }

    return std::iswspace(trimmed[5]) != 0;
}

bool starts_with_until_control_keyword(const std::wstring& trimmed) {
    if (trimmed.size() < 5) {
        return false;
    }

    if (std::towlower(trimmed[0]) != L'u' ||
        std::towlower(trimmed[1]) != L'n' ||
        std::towlower(trimmed[2]) != L't' ||
        std::towlower(trimmed[3]) != L'i' ||
        std::towlower(trimmed[4]) != L'l') {
        return false;
    }

    if (trimmed.size() == 5) {
        return true;
    }

    return std::iswspace(trimmed[5]) != 0;
}

bool starts_with_for_control_keyword(const std::wstring& trimmed) {
    if (trimmed.size() < 3) {
        return false;
    }

    if (std::towlower(trimmed[0]) != L'f' ||
        std::towlower(trimmed[1]) != L'o' ||
        std::towlower(trimmed[2]) != L'r') {
        return false;
    }

    if (trimmed.size() == 3) {
        return true;
    }

    return std::iswspace(trimmed[3]) != 0;
}

bool starts_with_case_control_keyword(const std::wstring& trimmed) {
    if (trimmed.size() < 4) {
        return false;
    }

    if (std::towlower(trimmed[0]) != L'c' ||
        std::towlower(trimmed[1]) != L'a' ||
        std::towlower(trimmed[2]) != L's' ||
        std::towlower(trimmed[3]) != L'e') {
        return false;
    }

    if (trimmed.size() == 4) {
        return true;
    }

    return std::iswspace(trimmed[4]) != 0;
}

bool starts_with_select_control_keyword(const std::wstring& trimmed) {
    if (trimmed.size() < 6) {
        return false;
    }
    if (std::towlower(trimmed[0]) != L's' ||
        std::towlower(trimmed[1]) != L'e' ||
        std::towlower(trimmed[2]) != L'l' ||
        std::towlower(trimmed[3]) != L'e' ||
        std::towlower(trimmed[4]) != L'c' ||
        std::towlower(trimmed[5]) != L't') {
        return false;
    }
    if (trimmed.size() == 6) {
        return true;
    }
    return std::iswspace(trimmed[6]) != 0;
}

bool starts_with_loop_control_keyword(const std::wstring& trimmed) {
    return starts_with_while_control_keyword(trimmed) ||
           starts_with_until_control_keyword(trimmed) ||
           starts_with_for_control_keyword(trimmed) ||
           starts_with_select_control_keyword(trimmed);
}

void report_script_syntax_error(size_t line_index, const std::wstring& message) {
    std::wcerr << L"ksh: script: line " << (line_index + 1) << L": " << message << L"\n";
}

bool parse_if_header_condition(
    const std::vector<std::wstring>& lines,
    size_t header_line_index,
    size_t end_index,
    size_t keyword_length,
    const std::wstring& header_label,
    std::wstring& condition_command,
    size_t& body_start,
    std::wstring& error_message,
    size_t& error_line_index) {
    error_message.clear();
    error_line_index = header_line_index;

    const std::wstring header_line = trim_copy(lines[header_line_index]);
    condition_command = trim_copy(header_line.substr(keyword_length));

    bool inline_then = false;
    if (ends_with_case_insensitive(condition_command, L"; then")) {
        condition_command = trim_copy(condition_command.substr(0, condition_command.size() - 6));
        inline_then = true;
    }

    if (condition_command.empty()) {
        error_message = header_label + L" requires a condition command";
        return false;
    }

    if (inline_then) {
        body_start = header_line_index + 1;
        return true;
    }

    size_t probe = header_line_index + 1;
    while (probe < end_index) {
        const std::wstring probe_trimmed = trim_copy(lines[probe]);
        if (is_script_empty_or_comment_line(probe_trimmed)) {
            probe++;
            continue;
        }

        if (to_lower_copy(probe_trimmed) == L"then") {
            body_start = probe + 1;
            return true;
        }

        error_message = L"expected then";
        error_line_index = probe;
        return false;
    }

    error_message = L"missing then for " + header_label + L" block";
    error_line_index = header_line_index;
    return false;
}

struct ScriptFlowControl {
    int break_levels = 0;
    int continue_levels = 0;
};

bool parse_do_block_start(
    const std::vector<std::wstring>& lines,
    size_t header_line_index,
    size_t end_index,
    std::wstring& trailing,
    const std::wstring& header_label,
    size_t& body_start,
    std::wstring& error_message,
    size_t& error_line_index) {
    error_message.clear();
    error_line_index = header_line_index;

    std::wstring normalized = trim_copy(trailing);
    bool inline_do = false;
    if (ends_with_case_insensitive(normalized, L"; do")) {
        normalized = trim_copy(normalized.substr(0, normalized.size() - 4));
        inline_do = true;
    }

    trailing = normalized;

    if (normalized.empty()) {
        error_message = header_label + L" requires a command";
        return false;
    }

    if (inline_do) {
        body_start = header_line_index + 1;
        return true;
    }

    size_t probe = header_line_index + 1;
    while (probe < end_index) {
        const std::wstring probe_trimmed = trim_copy(lines[probe]);
        if (is_script_empty_or_comment_line(probe_trimmed)) {
            probe++;
            continue;
        }

        if (to_lower_copy(probe_trimmed) == L"do") {
            body_start = probe + 1;
            return true;
        }

        error_message = L"expected do";
        error_line_index = probe;
        return false;
    }

    error_message = L"missing do for " + header_label + L" block";
    error_line_index = header_line_index;
    return false;
}

bool is_loop_done_keyword(const std::wstring& line, std::wstring& redirection_part) {
    std::wstring trimmed = trim_copy(line);
    std::wstring lowered = to_lower_copy(trimmed);
    if (lowered == L"done") {
        redirection_part = L"";
        return true;
    }
    if (lowered.rfind(L"done", 0) == 0 && lowered.size() > 4) {
        wchar_t next = lowered[4];
        if (next == L' ' || next == L'\t' || next == L'<' || next == L'>' || next == L'|' || next == L'&' || next == L';') {
            size_t done_pos = line.find(trimmed);
            redirection_part = trim_copy(line.substr(done_pos + 4));
            return true;
        }
    }
    return false;
}

bool find_matching_done(
    const std::vector<std::wstring>& lines,
    size_t body_start,
    size_t end_index,
    size_t& done_line_index) {
    done_line_index = std::wstring::npos;
    int nested_if_depth = 0;
    int nested_loop_depth = 0;

    for (size_t probe = body_start; probe < end_index; ++probe) {
        const std::wstring probe_trimmed = trim_copy(lines[probe]);
        if (is_script_empty_or_comment_line(probe_trimmed)) {
            continue;
        }

        if (starts_with_if_control_keyword(probe_trimmed)) {
            nested_if_depth++;
            continue;
        }

        if (starts_with_loop_control_keyword(probe_trimmed)) {
            nested_loop_depth++;
            continue;
        }

        const std::wstring lowered = to_lower_copy(probe_trimmed);
        if (lowered == L"fi") {
            if (nested_if_depth > 0) {
                nested_if_depth--;
            }
            continue;
        }

        std::wstring dummy_redir;
        if (is_loop_done_keyword(lines[probe], dummy_redir)) {
            if (nested_if_depth > 0) {
                continue;
            }

            if (nested_loop_depth > 0) {
                nested_loop_depth--;
                continue;
            }

            done_line_index = probe;
            return true;
        }
    }

    return false;
}

bool parse_for_header(
    const std::vector<std::wstring>& lines,
    size_t header_line_index,
    size_t end_index,
    std::wstring& variable_name,
    std::vector<std::wstring>& items,
    size_t& body_start,
    std::wstring& error_message,
    size_t& error_line_index);

void parse_and_replace_process_substitutions(std::wstring& line);

class BlockRedirector {
    HANDLE hSavedIn, hSavedOut;
    HANDLE hNewIn, hNewOut;
    bool activeIn, activeOut;
public:
    BlockRedirector(const RedirectionSpec& r) :
        activeIn(false), activeOut(false),
        hSavedIn(INVALID_HANDLE_VALUE), hSavedOut(INVALID_HANDLE_VALUE),
        hNewIn(INVALID_HANDLE_VALUE), hNewOut(INVALID_HANDLE_VALUE) {
        
        if (r.has_stdin) {
            hSavedIn = GetStdHandle(STD_INPUT_HANDLE);
            if (open_redirection_file(r.stdin_path, GENERIC_READ, OPEN_EXISTING, hNewIn)) {
                SetStdHandle(STD_INPUT_HANDLE, hNewIn);
                activeIn = true;
            }
        }
        if (r.has_stdout) {
            hSavedOut = GetStdHandle(STD_OUTPUT_HANDLE);
            DWORD creation = r.append_stdout ? OPEN_ALWAYS : CREATE_ALWAYS;
            DWORD access = GENERIC_WRITE;
            if (r.append_stdout) access |= FILE_APPEND_DATA;
            if (open_redirection_file(r.stdout_path, access, creation, hNewOut)) {
                if (r.append_stdout) {
                    SetFilePointer(hNewOut, 0, NULL, FILE_END);
                }
                SetStdHandle(STD_OUTPUT_HANDLE, hNewOut);
                activeOut = true;
            }
        }
    }
    ~BlockRedirector() {
        if (activeIn) {
            SetStdHandle(STD_INPUT_HANDLE, hSavedIn);
            CloseHandle(hNewIn);
        }
        if (activeOut) {
            SetStdHandle(STD_OUTPUT_HANDLE, hSavedOut);
            CloseHandle(hNewOut);
        }
    }
    bool is_valid(const RedirectionSpec& r) const {
        if (r.has_stdin && !activeIn) return false;
        if (r.has_stdout && !activeOut) return false;
        return true;
    }
};

bool execute_script_while_until_block(
    const std::vector<std::wstring>& lines,
    size_t& line_index,
    size_t end_index,
    bool until_mode,
    bool& should_exit_shell,
    ScriptFlowControl* flow_control);

bool execute_script_select_block(
    const std::vector<std::wstring>& lines,
    size_t& line_index,
    size_t end_index,
    bool& should_exit_shell,
    ScriptFlowControl* flow_control);

bool execute_script_for_block(
    const std::vector<std::wstring>& lines,
    size_t& line_index,
    size_t end_index,
    bool& should_exit_shell,
    ScriptFlowControl* flow_control);

bool execute_script_case_block(
    const std::vector<std::wstring>& lines,
    size_t& line_index,
    size_t end_index,
    bool& should_exit_shell,
    ScriptFlowControl* flow_control);

bool execute_script_lines_range(
    const std::vector<std::wstring>& lines,
    size_t start_index,
    size_t end_index,
    bool& should_exit_shell,
    ScriptFlowControl* flow_control = nullptr);

bool execute_script_if_block(
    const std::vector<std::wstring>& lines,
    size_t& line_index,
    size_t end_index,
    bool& should_exit_shell,
    ScriptFlowControl* flow_control) {
    struct ConditionalBranch {
        bool has_condition = true;
        std::wstring condition_command;
        size_t marker_line_index = 0;
        size_t body_start = 0;
        size_t body_end = 0;
    };

    const size_t if_line_index = line_index;

    std::wstring if_condition;
    size_t if_body_start = if_line_index + 1;
    std::wstring parse_error;
    size_t parse_error_line = if_line_index;
    if (!parse_if_header_condition(lines, if_line_index, end_index, 2, L"if", if_condition, if_body_start, parse_error, parse_error_line)) {
        report_script_syntax_error(parse_error_line, parse_error);
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    std::vector<size_t> top_level_elif_lines;
    size_t else_line_index = std::wstring::npos;
    size_t fi_line_index = std::wstring::npos;
    int nested_if_depth = 0;

    for (size_t probe = if_body_start; probe < end_index; ++probe) {
        const std::wstring probe_trimmed = trim_copy(lines[probe]);
        if (is_script_empty_or_comment_line(probe_trimmed)) {
            continue;
        }

        if (starts_with_if_control_keyword(probe_trimmed)) {
            nested_if_depth++;
            continue;
        }

        if (nested_if_depth > 0) {
            if (to_lower_copy(probe_trimmed) == L"fi") {
                nested_if_depth--;
            }
            continue;
        }

        if (starts_with_elif_control_keyword(probe_trimmed)) {
            if (else_line_index != std::wstring::npos) {
                report_script_syntax_error(probe, L"unexpected elif after else");
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            top_level_elif_lines.push_back(probe);
            continue;
        }

        const std::wstring lowered = to_lower_copy(probe_trimmed);
        if (lowered == L"else") {
            if (else_line_index != std::wstring::npos) {
                report_script_syntax_error(probe, L"multiple else blocks in if statement");
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            else_line_index = probe;
            continue;
        }

        if (lowered == L"fi") {
            fi_line_index = probe;
            break;
        }
    }

    if (fi_line_index == std::wstring::npos) {
        report_script_syntax_error(if_line_index, L"missing fi for if block");
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    std::vector<ConditionalBranch> branches;

    ConditionalBranch if_branch;
    if_branch.has_condition = true;
    if_branch.condition_command = if_condition;
    if_branch.marker_line_index = if_line_index;
    if_branch.body_start = if_body_start;
    branches.push_back(if_branch);

    for (size_t elif_line : top_level_elif_lines) {
        ConditionalBranch elif_branch;
        elif_branch.has_condition = true;
        elif_branch.marker_line_index = elif_line;
        if (!parse_if_header_condition(lines, elif_line, end_index, 4, L"elif", elif_branch.condition_command, elif_branch.body_start, parse_error, parse_error_line)) {
            report_script_syntax_error(parse_error_line, parse_error);
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        branches.push_back(elif_branch);
    }

    if (else_line_index != std::wstring::npos) {
        ConditionalBranch else_branch;
        else_branch.has_condition = false;
        else_branch.marker_line_index = else_line_index;
        else_branch.body_start = else_line_index + 1;
        branches.push_back(else_branch);
    }

    for (size_t idx = 0; idx < branches.size(); ++idx) {
        size_t end_marker = fi_line_index;
        if (idx + 1 < branches.size()) {
            end_marker = branches[idx + 1].marker_line_index;
        }
        branches[idx].body_end = end_marker;

        if (branches[idx].body_start > branches[idx].body_end) {
            report_script_syntax_error(branches[idx].marker_line_index, L"invalid branch body range");
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
    }

    for (const ConditionalBranch& branch : branches) {
        bool should_run_branch = false;

        if (!branch.has_condition) {
            should_run_branch = true;
        } else {
            g_errexit_disabled_depth++;
            bool cond_ok = execute_command_line(branch.condition_command, should_exit_shell);
            g_errexit_disabled_depth--;
            if (!cond_ok) {
                return false;
            }
            if (should_exit_shell) {
                line_index = fi_line_index + 1;
                return true;
            }
            if (is_function_return_requested()) {
                line_index = fi_line_index + 1;
                return true;
            }

            std::map<std::wstring, std::wstring>::const_iterator status_it = ksh_env.variables.find(L"?");
            should_run_branch = (status_it != ksh_env.variables.end() && status_it->second == L"0");
        }

        if (should_run_branch) {
            if (!execute_script_lines_range(lines, branch.body_start, branch.body_end, should_exit_shell, flow_control)) {
                return false;
            }
            if (is_function_return_requested()) {
                line_index = fi_line_index + 1;
                return true;
            }
            break;
        }
    }

    line_index = fi_line_index + 1;
    return true;
}

bool execute_script_while_until_block(
    const std::vector<std::wstring>& lines,
    size_t& line_index,
    size_t end_index,
    bool until_mode,
    bool& should_exit_shell,
    ScriptFlowControl* flow_control) {
    const size_t header_line_index = line_index;
    const std::wstring header_line = trim_copy(lines[header_line_index]);
    const std::wstring header_label = until_mode ? L"until" : L"while";

    std::wstring condition_command = trim_copy(header_line.substr(5));
    std::wstring parse_error;
    size_t parse_error_line = header_line_index;
    size_t body_start = header_line_index + 1;

    if (!parse_do_block_start(lines, header_line_index, end_index, condition_command, header_label, body_start, parse_error, parse_error_line)) {
        report_script_syntax_error(parse_error_line, parse_error);
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    size_t done_line_index = std::wstring::npos;
    if (!find_matching_done(lines, body_start, end_index, done_line_index)) {
        report_script_syntax_error(header_line_index, L"missing done for " + header_label + L" block");
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    std::wstring redirection_part;
    RedirectionSpec loop_redir;
    bool has_redir = false;
    if (is_loop_done_keyword(lines[done_line_index], redirection_part) && !redirection_part.empty()) {
        parse_and_replace_process_substitutions(redirection_part);
        std::vector<std::wstring> redir_tokens = ksh_tokenize_preserve_quotes(redirection_part);
        redir_tokens = remove_quotes_and_escapes_from_tokens(redir_tokens);
        std::wstring err;
        if (parse_redirections(redir_tokens, loop_redir, err)) {
            has_redir = true;
        } else {
            std::wcerr << L"ksh: while: loop redirection parse error: " << err << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
    }

    std::unique_ptr<BlockRedirector> block_redir;
    if (has_redir) {
        block_redir = std::make_unique<BlockRedirector>(loop_redir);
        if (!block_redir->is_valid(loop_redir)) {
            std::wcerr << L"ksh: while: loop redirection setup failed\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
    }

    while (true) {
        g_errexit_disabled_depth++;
        bool cond_ok = execute_command_line(condition_command, should_exit_shell);
        g_errexit_disabled_depth--;
        if (should_exit_shell) {
            line_index = done_line_index + 1;
            return true;
        }
        if (is_function_return_requested()) {
            line_index = done_line_index + 1;
            return true;
        }

        std::map<std::wstring, std::wstring>::const_iterator status_it = ksh_env.variables.find(L"?");
        const bool condition_true = cond_ok && (status_it != ksh_env.variables.end() && status_it->second == L"0");
        const bool run_body = until_mode ? !condition_true : condition_true;
        if (!run_body) {
            break;
        }

        ScriptFlowControl local_flow;
        ScriptFlowControl* body_flow = (flow_control != nullptr) ? flow_control : &local_flow;
        if (!execute_script_lines_range(lines, body_start, done_line_index, should_exit_shell, body_flow)) {
            return false;
        }
        if (should_exit_shell) {
            line_index = done_line_index + 1;
            return true;
        }
        if (is_function_return_requested()) {
            line_index = done_line_index + 1;
            return true;
        }

        if (body_flow->break_levels > 0) {
            body_flow->break_levels--;
            ksh_env.variables[L"?"] = L"0";
            line_index = done_line_index + 1;
            return true;
        }

        if (body_flow->continue_levels > 0) {
            body_flow->continue_levels--;
            if (body_flow->continue_levels > 0) {
                line_index = done_line_index + 1;
                return true;
            }
            continue;
        }
    }

    ksh_env.variables[L"?"] = L"0";
    line_index = done_line_index + 1;
    return true;
}

bool execute_script_case_block(
    const std::vector<std::wstring>& lines,
    size_t& line_index,
    size_t end_index,
    bool& should_exit_shell,
    ScriptFlowControl* flow_control) {
    const size_t header_line_index = line_index;
    const std::wstring header_line = trim_copy(lines[header_line_index]);
    std::wstring trailing = trim_copy(header_line.substr(4));

    std::wstring case_word;
    size_t body_start = header_line_index + 1;

    size_t in_pos = trailing.find(L" in");
    if (in_pos != std::wstring::npos) {
        case_word = trim_copy(trailing.substr(0, in_pos));
        body_start = header_line_index + 1;
    } else {
        case_word = trim_copy(trailing);
        size_t probe = header_line_index + 1;
        while (probe < end_index) {
            const std::wstring probe_trimmed = trim_copy(lines[probe]);
            if (is_script_empty_or_comment_line(probe_trimmed)) {
                probe++;
                continue;
            }
            if (to_lower_copy(probe_trimmed) == L"in") {
                body_start = probe + 1;
                break;
            }
            report_script_syntax_error(probe, L"expected 'in' for case block");
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
    }

    if (case_word.empty()) {
        report_script_syntax_error(header_line_index, L"case requires a word");
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    std::wstring expanded_case_word = expand_substitutions_left_to_right(case_word);
    if (g_expansion_error) {
        ksh_env.variables[L"?"] = L"1";
        return false;
    }
    expanded_case_word = remove_quotes_and_escapes_from_token(expanded_case_word);

    size_t esac_line_index = std::wstring::npos;
    int case_nesting_depth = 0;
    for (size_t probe = body_start; probe < end_index; ++probe) {
        const std::wstring probe_trimmed = trim_copy(lines[probe]);
        if (is_script_empty_or_comment_line(probe_trimmed)) {
            continue;
        }

        if (starts_with_case_control_keyword(probe_trimmed)) {
            case_nesting_depth++;
            continue;
        }

        if (to_lower_copy(probe_trimmed) == L"esac") {
            if (case_nesting_depth > 0) {
                case_nesting_depth--;
                continue;
            }
            esac_line_index = probe;
            break;
        }
    }

    if (esac_line_index == std::wstring::npos) {
        report_script_syntax_error(header_line_index, L"missing esac for case block");
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    bool execute_following_clauses = false;
    bool matched_any = false;
    size_t probe = body_start;
    while (probe < esac_line_index) {
        const std::wstring clause_line = trim_copy(lines[probe]);
        if (is_script_empty_or_comment_line(clause_line)) {
            probe++;
            continue;
        }

        const size_t close_paren = clause_line.find(L')');
        if (close_paren == std::wstring::npos) {
            report_script_syntax_error(probe, L"invalid case pattern clause");
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        std::wstring patterns_text = trim_copy(clause_line.substr(0, close_paren));
        std::wstring clause_remainder = trim_copy(clause_line.substr(close_paren + 1));

        std::vector<std::wstring> patterns;
        std::wstring current_pattern;
        bool in_single_quotes = false;
        bool in_double_quotes = false;
        bool escaped = false;
        for (wchar_t ch : patterns_text) {
            if (escaped) {
                current_pattern.push_back(ch);
                escaped = false;
                continue;
            }
            if (ch == L'\\' && !in_single_quotes) {
                current_pattern.push_back(ch);
                escaped = true;
                continue;
            }
            if (ch == L'\'' && !in_double_quotes) {
                in_single_quotes = !in_single_quotes;
                current_pattern.push_back(ch);
                continue;
            }
            if (ch == L'"' && !in_single_quotes) {
                in_double_quotes = !in_double_quotes;
                current_pattern.push_back(ch);
                continue;
            }
            if (ch == L'|' && !in_single_quotes && !in_double_quotes) {
                patterns.push_back(trim_copy(current_pattern));
                current_pattern.clear();
                continue;
            }
            current_pattern.push_back(ch);
        }
        if (!current_pattern.empty()) {
            patterns.push_back(trim_copy(current_pattern));
        }

        bool clause_matches = execute_following_clauses;
        if (!clause_matches) {
            for (const std::wstring& raw_pattern : patterns) {
                std::wstring expanded_pattern = expand_substitutions_left_to_right(raw_pattern);
                if (g_expansion_error) {
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
                expanded_pattern = remove_quotes_and_escapes_from_token(expanded_pattern);
                if (match_glob_pattern(expanded_pattern, expanded_case_word)) {
                    clause_matches = true;
                    matched_any = true;
                    break;
                }
            }
        }

        std::vector<std::wstring> clause_body_lines;
        std::wstring inline_terminator;
        if (!clause_remainder.empty()) {
            size_t term_pos = std::wstring::npos;
            std::wstring term;
            if ((term_pos = clause_remainder.find(L";;&")) != std::wstring::npos) {
                term = L";;&";
            } else if ((term_pos = clause_remainder.find(L";&")) != std::wstring::npos) {
                term = L";&";
            } else if ((term_pos = clause_remainder.find(L";;")) != std::wstring::npos) {
                term = L";;";
            }

            if (!term.empty()) {
                std::wstring inline_body = trim_copy(clause_remainder.substr(0, term_pos));
                if (!inline_body.empty()) {
                    clause_body_lines.push_back(inline_body);
                }
                inline_terminator = term;
            } else {
                clause_body_lines.push_back(clause_remainder);
            }
        }

        size_t terminator_line = probe;
        std::wstring terminator = L";;";
        if (!inline_terminator.empty()) {
            terminator = inline_terminator;
        } else {
            bool found_terminator = false;
            size_t scan = probe + 1;
            int nested_case_depth = 0;
            while (scan < esac_line_index) {
                const std::wstring body_line = trim_copy(lines[scan]);
                if (starts_with_case_control_keyword(body_line)) {
                    nested_case_depth++;
                    clause_body_lines.push_back(lines[scan]);
                    scan++;
                    continue;
                }

                const std::wstring lowered_body = to_lower_copy(body_line);
                if (lowered_body == L"esac" && nested_case_depth > 0) {
                    nested_case_depth--;
                    clause_body_lines.push_back(lines[scan]);
                    scan++;
                    continue;
                }

                if (nested_case_depth == 0 && (body_line == L";;" || body_line == L";&" || body_line == L";;&")) {
                    terminator_line = scan;
                    terminator = body_line;
                    found_terminator = true;
                    break;
                }
                clause_body_lines.push_back(lines[scan]);
                scan++;
            }
            if (!found_terminator) {
                report_script_syntax_error(probe, L"missing case clause terminator (;;, ;&, or ;;&)");
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
        }

        if (clause_matches) {
            if (!execute_script_lines_range(clause_body_lines, 0, clause_body_lines.size(), should_exit_shell, flow_control)) {
                return false;
            }
            if (should_exit_shell || is_function_return_requested()) {
                line_index = esac_line_index + 1;
                return true;
            }
        }

        if (terminator == L";;") {
            if (clause_matches) {
                line_index = esac_line_index + 1;
                return true;
            }
            execute_following_clauses = false;
        } else if (terminator == L";&") {
            execute_following_clauses = clause_matches;
        } else {
            // ;;& keeps pattern testing in following clauses.
            execute_following_clauses = false;
        }

        probe = terminator_line + 1;
    }

    if (!matched_any) {
        ksh_env.variables[L"?"] = L"0";
    }

    line_index = esac_line_index + 1;
    return true;
}

bool parse_for_header(
    const std::vector<std::wstring>& lines,
    size_t header_line_index,
    size_t end_index,
    std::wstring& variable_name,
    std::vector<std::wstring>& items,
    size_t& body_start,
    std::wstring& error_message,
    size_t& error_line_index) {
    error_message.clear();
    error_line_index = header_line_index;
    variable_name.clear();
    items.clear();

    const std::wstring header_line = trim_copy(lines[header_line_index]);
    std::wstring trailing = trim_copy(header_line.substr(3));

    std::wstring normalized = trailing;
    bool inline_do = false;
    if (ends_with_case_insensitive(normalized, L"; do")) {
        normalized = trim_copy(normalized.substr(0, normalized.size() - 4));
        inline_do = true;
    }

    std::vector<std::wstring> tokens = ksh_tokenize_preserve_quotes(normalized);
    if (tokens.empty()) {
        error_message = L"for requires a loop variable";
        return false;
    }

    variable_name = tokens[0];
    if (variable_name.empty() ||
        !(std::iswalpha(variable_name[0]) || variable_name[0] == L'_') ||
        !std::all_of(variable_name.begin(), variable_name.end(), [](wchar_t ch) {
            return std::iswalnum(ch) != 0 || ch == L'_';
        })) {
        error_message = L"invalid for loop variable: " + variable_name;
        return false;
    }

    if (tokens.size() == 1) {
        items = current_script_args();
    } else {
        if (to_lower_copy(tokens[1]) != L"in") {
            error_message = L"for header must use 'in' list form";
            return false;
        }

        for (size_t i = 2; i < tokens.size(); ++i) {
            std::wstring expanded_item = expand_substitutions_left_to_right(tokens[i]);
            if (g_expansion_error) {
                error_message = L"failed to expand for-loop item";
                return false;
            }

            std::vector<std::wstring> expanded_tokens = split_fields_by_ifs(expanded_item);
            expanded_tokens = expand_tilde_for_tokens(expanded_tokens);
            expanded_tokens = expand_globs_for_tokens(expanded_tokens, expanded_tokens);
            expanded_tokens = remove_quotes_and_escapes_from_tokens(expanded_tokens);
            if (expanded_tokens.empty()) {
                items.push_back(L"");
            } else {
                items.insert(items.end(), expanded_tokens.begin(), expanded_tokens.end());
            }
        }
    }

    if (inline_do) {
        body_start = header_line_index + 1;
        return true;
    }

    size_t probe = header_line_index + 1;
    while (probe < end_index) {
        const std::wstring probe_trimmed = trim_copy(lines[probe]);
        if (is_script_empty_or_comment_line(probe_trimmed)) {
            probe++;
            continue;
        }

        if (to_lower_copy(probe_trimmed) == L"do") {
            body_start = probe + 1;
            return true;
        }

        error_message = L"expected do";
        error_line_index = probe;
        return false;
    }

    error_message = L"missing do for for block";
    error_line_index = header_line_index;
    return false;
}

bool execute_script_select_block(
    const std::vector<std::wstring>& lines,
    size_t& line_index,
    size_t end_index,
    bool& should_exit_shell,
    ScriptFlowControl* flow_control) {
    const size_t header_line_index = line_index;
    std::wstring variable_name;
    std::vector<std::wstring> items;
    size_t body_start = header_line_index + 1;
    std::wstring parse_error;
    
    const std::wstring header_line = trim_copy(lines[header_line_index]);
    std::wstring trailing = trim_copy(header_line.substr(6));
    
    std::wstring normalized = trailing;
    bool inline_do = false;
    if (ends_with_case_insensitive(normalized, L"; do")) {
        normalized = trim_copy(normalized.substr(0, normalized.size() - 4));
        inline_do = true;
    }
    
    std::vector<std::wstring> tokens = ksh_tokenize_preserve_quotes(normalized);
    if (tokens.empty()) {
        report_script_syntax_error(header_line_index, L"select requires a loop variable");
        ksh_env.variables[L"?"] = L"1";
        return false;
    }
    
    variable_name = tokens[0];
    if (variable_name.empty() ||
        !(std::iswalpha(variable_name[0]) || variable_name[0] == L'_') ||
        !std::all_of(variable_name.begin(), variable_name.end(), [](wchar_t ch) {
            return std::iswalnum(ch) != 0 || ch == L'_';
        })) {
        report_script_syntax_error(header_line_index, L"invalid select loop variable: " + variable_name);
        ksh_env.variables[L"?"] = L"1";
        return false;
    }
    
    if (tokens.size() == 1) {
        items = current_script_args();
    } else {
        if (to_lower_copy(tokens[1]) != L"in") {
            report_script_syntax_error(header_line_index, L"select header must use 'in' list form");
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        
        for (size_t i = 2; i < tokens.size(); ++i) {
            std::wstring expanded_item = expand_substitutions_left_to_right(tokens[i]);
            if (g_expansion_error) {
                report_script_syntax_error(header_line_index, L"failed to expand select-loop item");
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            
            std::vector<std::wstring> expanded_tokens = split_fields_by_ifs(expanded_item);
            expanded_tokens = expand_tilde_for_tokens(expanded_tokens);
            expanded_tokens = expand_globs_for_tokens(expanded_tokens, expanded_tokens);
            expanded_tokens = remove_quotes_and_escapes_from_tokens(expanded_tokens);
            if (expanded_tokens.empty()) {
                items.push_back(L"");
            } else {
                items.insert(items.end(), expanded_tokens.begin(), expanded_tokens.end());
            }
        }
    }
    
    if (inline_do) {
        body_start = header_line_index + 1;
    } else {
        size_t probe = header_line_index + 1;
        bool found_do = false;
        while (probe < end_index) {
            const std::wstring probe_trimmed = trim_copy(lines[probe]);
            if (is_script_empty_or_comment_line(probe_trimmed)) {
                probe++;
                continue;
            }
            if (to_lower_copy(probe_trimmed) == L"do") {
                body_start = probe + 1;
                found_do = true;
                break;
            }
            report_script_syntax_error(probe, L"expected do");
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        if (!found_do) {
            report_script_syntax_error(header_line_index, L"missing do for select block");
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
    }
    
    size_t done_line_index = std::wstring::npos;
    if (!find_matching_done(lines, body_start, end_index, done_line_index)) {
        report_script_syntax_error(header_line_index, L"missing done for select block");
        ksh_env.variables[L"?"] = L"1";
        return false;
    }
    
    auto read_line_from_stdin = [](std::wstring& out_line) -> bool {
        out_line.clear();
        if (_isatty(_fileno(stdin))) {
            if (std::getline(std::wcin, out_line)) {
                return true;
            }
            return false;
        } else {
            HANDLE hStdin = (g_pipeline_stdin != INVALID_HANDLE_VALUE) ? g_pipeline_stdin : GetStdHandle(STD_INPUT_HANDLE);
            std::string line;
            char ch;
            DWORD bytes_read = 0;
            bool read_any = false;
            while (ReadFile(hStdin, &ch, 1, &bytes_read, nullptr) && bytes_read > 0) {
                read_any = true;
                if (ch == '\n') {
                    break;
                }
                if (ch != '\r') {
                    line.push_back(ch);
                }
            }
            if (!read_any) {
                return false;
            }
            int wlen = MultiByteToWideChar(CP_UTF8, 0, line.c_str(), -1, nullptr, 0);
            if (wlen > 0) {
                std::vector<wchar_t> wbuf(wlen);
                MultiByteToWideChar(CP_UTF8, 0, line.c_str(), -1, wbuf.data(), wlen);
                out_line = std::wstring(wbuf.data());
            }
            return true;
        }
    };

    while (true) {
        for (size_t idx = 0; idx < items.size(); ++idx) {
            std::wcerr << (idx + 1) << L") " << items[idx] << L"\n";
        }
        std::wcerr.flush();
        
        std::wstring prompt = L"#? ";
        wchar_t ps3[256];
        if (GetEnvironmentVariableW(L"PS3", ps3, 256) > 0) {
            prompt = ps3;
        } else {
            auto it = ksh_env.variables.find(L"PS3");
            if (it != ksh_env.variables.end()) {
                prompt = it->second;
            }
        }
        std::wcerr << prompt;
        std::wcerr.flush();
        
        std::wstring input;
        if (!read_line_from_stdin(input)) {
            break;
        }
        
        std::wstring reply = trim_copy(input);
        ksh_env.variables[L"REPLY"] = reply;
        SetEnvironmentVariableW(L"REPLY", reply.c_str());
        
        std::wstring selected_value;
        int choice = 0;
        if (try_parse_int_strict(reply, choice) && choice >= 1 && choice <= static_cast<int>(items.size())) {
            selected_value = items[choice - 1];
        }
        
        ksh_env.variables[variable_name] = selected_value;
        SetEnvironmentVariableW(variable_name.c_str(), selected_value.c_str());
        
        ScriptFlowControl local_flow;
        ScriptFlowControl* body_flow = (flow_control != nullptr) ? flow_control : &local_flow;
        if (!execute_script_lines_range(lines, body_start, done_line_index, should_exit_shell, body_flow)) {
            return false;
        }
        
        if (should_exit_shell) {
            line_index = done_line_index + 1;
            return true;
        }
        if (is_function_return_requested()) {
            line_index = done_line_index + 1;
            return true;
        }
        
        if (body_flow->break_levels > 0) {
            body_flow->break_levels--;
            break;
        }
        if (body_flow->continue_levels > 0) {
            body_flow->continue_levels--;
        }
    }
    
    line_index = done_line_index + 1;
    return true;
}

bool execute_script_for_block(
    const std::vector<std::wstring>& lines,
    size_t& line_index,
    size_t end_index,
    bool& should_exit_shell,
    ScriptFlowControl* flow_control) {
    const size_t header_line_index = line_index;
    std::wstring variable_name;
    std::vector<std::wstring> items;
    size_t body_start = header_line_index + 1;
    std::wstring parse_error;
    size_t parse_error_line = header_line_index;

    if (!parse_for_header(lines, header_line_index, end_index, variable_name, items, body_start, parse_error, parse_error_line)) {
        report_script_syntax_error(parse_error_line, parse_error);
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    size_t done_line_index = std::wstring::npos;
    if (!find_matching_done(lines, body_start, end_index, done_line_index)) {
        report_script_syntax_error(header_line_index, L"missing done for for block");
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    std::wstring redirection_part;
    RedirectionSpec loop_redir;
    bool has_redir = false;
    if (is_loop_done_keyword(lines[done_line_index], redirection_part) && !redirection_part.empty()) {
        parse_and_replace_process_substitutions(redirection_part);
        std::vector<std::wstring> redir_tokens = ksh_tokenize_preserve_quotes(redirection_part);
        redir_tokens = remove_quotes_and_escapes_from_tokens(redir_tokens);
        std::wstring err;
        if (parse_redirections(redir_tokens, loop_redir, err)) {
            has_redir = true;
        } else {
            std::wcerr << L"ksh: for: loop redirection parse error: " << err << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
    }

    std::unique_ptr<BlockRedirector> block_redir;
    if (has_redir) {
        block_redir = std::make_unique<BlockRedirector>(loop_redir);
        if (!block_redir->is_valid(loop_redir)) {
            std::wcerr << L"ksh: for: loop redirection setup failed\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
    }

    for (const std::wstring& item : items) {
        ksh_env.variables[variable_name] = item;
        SetEnvironmentVariableW(variable_name.c_str(), item.c_str());

        ScriptFlowControl local_flow;
        ScriptFlowControl* body_flow = (flow_control != nullptr) ? flow_control : &local_flow;
        if (!execute_script_lines_range(lines, body_start, done_line_index, should_exit_shell, body_flow)) {
            return false;
        }
        if (should_exit_shell) {
            line_index = done_line_index + 1;
            return true;
        }
        if (is_function_return_requested()) {
            line_index = done_line_index + 1;
            return true;
        }

        if (body_flow->break_levels > 0) {
            body_flow->break_levels--;
            line_index = done_line_index + 1;
            return true;
        }

        if (body_flow->continue_levels > 0) {
            body_flow->continue_levels--;
            if (body_flow->continue_levels > 0) {
                line_index = done_line_index + 1;
                return true;
            }
            continue;
        }
    }

    line_index = done_line_index + 1;
    return true;
}

bool execute_script_lines_range(
    const std::vector<std::wstring>& lines,
    size_t start_index,
    size_t end_index,
    bool& should_exit_shell,
    ScriptFlowControl* flow_control) {
    size_t line_index = start_index;
    while (line_index < end_index) {
        const std::wstring trimmed = trim_copy(lines[line_index]);
        if (is_script_empty_or_comment_line(trimmed)) {
            line_index++;
            continue;
        }

        std::wstring function_parse_error;
        size_t function_parse_error_line = line_index;
        size_t function_line = line_index;
        if (capture_function_definition(lines, line_index, end_index, function_parse_error, function_parse_error_line)) {
            if (line_index <= function_line) {
                report_script_syntax_error(function_line, L"failed to advance after function definition");
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            continue;
        }
        if (!function_parse_error.empty()) {
            report_script_syntax_error(function_parse_error_line, function_parse_error);
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        if (starts_with_if_control_keyword(trimmed)) {
            if (!execute_script_if_block(lines, line_index, end_index, should_exit_shell, flow_control)) {
                return false;
            }
            if (should_exit_shell) {
                return true;
            }
            if (is_function_return_requested()) {
                return true;
            }
            if (flow_control != nullptr && (flow_control->break_levels > 0 || flow_control->continue_levels > 0)) {
                return true;
            }
            continue;
        }

        if (starts_with_while_control_keyword(trimmed)) {
            if (!execute_script_while_until_block(lines, line_index, end_index, false, should_exit_shell, flow_control)) {
                return false;
            }
            if (should_exit_shell) {
                return true;
            }
            if (is_function_return_requested()) {
                return true;
            }
            if (flow_control != nullptr && (flow_control->break_levels > 0 || flow_control->continue_levels > 0)) {
                return true;
            }
            continue;
        }

        if (starts_with_until_control_keyword(trimmed)) {
            if (!execute_script_while_until_block(lines, line_index, end_index, true, should_exit_shell, flow_control)) {
                return false;
            }
            if (should_exit_shell) {
                return true;
            }
            if (is_function_return_requested()) {
                return true;
            }
            if (flow_control != nullptr && (flow_control->break_levels > 0 || flow_control->continue_levels > 0)) {
                return true;
            }
            continue;
        }

        if (starts_with_select_control_keyword(trimmed)) {
            if (!execute_script_select_block(lines, line_index, end_index, should_exit_shell, flow_control)) {
                return false;
            }
            if (should_exit_shell) {
                return true;
            }
            if (is_function_return_requested()) {
                return true;
            }
            if (flow_control != nullptr && (flow_control->break_levels > 0 || flow_control->continue_levels > 0)) {
                return true;
            }
            continue;
        }

        if (starts_with_for_control_keyword(trimmed)) {
            if (!execute_script_for_block(lines, line_index, end_index, should_exit_shell, flow_control)) {
                return false;
            }
            if (should_exit_shell) {
                return true;
            }
            if (is_function_return_requested()) {
                return true;
            }
            if (flow_control != nullptr && (flow_control->break_levels > 0 || flow_control->continue_levels > 0)) {
                return true;
            }
            continue;
        }

        if (starts_with_case_control_keyword(trimmed)) {
            if (!execute_script_case_block(lines, line_index, end_index, should_exit_shell, flow_control)) {
                return false;
            }
            if (should_exit_shell) {
                return true;
            }
            if (is_function_return_requested()) {
                return true;
            }
            if (flow_control != nullptr && (flow_control->break_levels > 0 || flow_control->continue_levels > 0)) {
                return true;
            }
            continue;
        }

        const std::wstring lowered = to_lower_copy(trimmed);
        if (lowered == L"then" || lowered == L"else" || lowered == L"fi" || lowered == L"do" || lowered == L"done" || lowered == L"elif" || lowered == L"in" || lowered == L"esac") {
            report_script_syntax_error(line_index, L"unexpected control keyword: " + lowered);
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        if (lowered.rfind(L"break", 0) == 0 || lowered.rfind(L"continue", 0) == 0) {
            std::vector<std::wstring> flow_tokens = ksh_tokenize(trimmed);
            if (flow_tokens.empty()) {
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            const std::wstring command = to_lower_copy(flow_tokens[0]);
            if (command == L"break" || command == L"continue") {
                if (flow_control == nullptr) {
                    report_script_syntax_error(line_index, command + L" is only valid inside a loop");
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }

                int level_count = 1;
                if (flow_tokens.size() > 2) {
                    report_script_syntax_error(line_index, command + L" takes at most one numeric argument");
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
                if (flow_tokens.size() == 2) {
                    int parsed = 0;
                    if (!try_parse_positive_int_strict(flow_tokens[1], parsed)) {
                        report_script_syntax_error(line_index, command + L" requires a positive integer argument");
                        ksh_env.variables[L"?"] = L"1";
                        return false;
                    }
                    level_count = parsed;
                }

                if (command == L"break") {
                    flow_control->break_levels = level_count;
                } else {
                    flow_control->continue_levels = level_count;
                }
                ksh_env.variables[L"?"] = L"0";
                return true;
            }
        }

        std::wstring command_to_execute = trimmed;
        std::wstring heredoc_temp_file;
        bool consume_heredoc_lines = false;
        size_t heredoc_end_line = line_index;

        std::vector<std::wstring> heredoc_tokens = ksh_tokenize_preserve_quotes(trimmed);
        size_t heredoc_op_index = std::wstring::npos;
        bool heredoc_strip_tabs = false;
        std::wstring heredoc_delimiter;
        bool heredoc_disable_expansion = false;

        for (size_t t = 0; t < heredoc_tokens.size(); ++t) {
            const std::wstring& token = heredoc_tokens[t];
            bool is_op = false;
            size_t op_len = 0;
            if (token == L"<<") {
                is_op = true;
                op_len = 2;
            } else if (token == L"<<-") {
                is_op = true;
                op_len = 3;
                heredoc_strip_tabs = true;
            } else if (token.rfind(L"<<-", 0) == 0) {
                is_op = true;
                op_len = 3;
                heredoc_strip_tabs = true;
            } else if (token.rfind(L"<<", 0) == 0) {
                is_op = true;
                op_len = 2;
            }

            if (!is_op) {
                continue;
            }

            heredoc_op_index = t;
            if (token.size() > op_len) {
                heredoc_delimiter = token.substr(op_len);
            } else if (t + 1 < heredoc_tokens.size()) {
                heredoc_delimiter = heredoc_tokens[t + 1];
            }
            break;
        }

        if (heredoc_op_index != std::wstring::npos) {
            if (heredoc_delimiter.empty()) {
                report_script_syntax_error(line_index, L"missing here-doc delimiter");
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            std::wstring raw_delimiter = heredoc_delimiter;
            if ((raw_delimiter.size() >= 2) &&
                ((raw_delimiter.front() == L'"' && raw_delimiter.back() == L'"') ||
                 (raw_delimiter.front() == L'\'' && raw_delimiter.back() == L'\''))) {
                heredoc_disable_expansion = true;
                raw_delimiter = raw_delimiter.substr(1, raw_delimiter.size() - 2);
            }

            std::wstring heredoc_content;
            size_t probe = line_index + 1;
            bool found_delimiter = false;
            for (; probe < end_index; ++probe) {
                std::wstring candidate = lines[probe];
                std::wstring compare = candidate;
                if (heredoc_strip_tabs) {
                    while (!compare.empty() && compare.front() == L'\t') {
                        compare.erase(compare.begin());
                    }
                }

                if (compare == raw_delimiter) {
                    found_delimiter = true;
                    break;
                }

                std::wstring content_line = heredoc_strip_tabs ? compare : candidate;
                if (!heredoc_disable_expansion) {
                    content_line = expand_substitutions_left_to_right(content_line);
                    if (g_expansion_error) {
                        ksh_env.variables[L"?"] = L"1";
                        return false;
                    }
                }

                if (heredoc_content.size() + content_line.size() + 1 > kMaxHereDocContentBytes) {
                    report_script_syntax_error(line_index, L"here-doc content exceeds configured limit");
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }

                heredoc_content += content_line;
                heredoc_content += L"\n";
            }

            if (!found_delimiter) {
                report_script_syntax_error(line_index, L"missing here-doc terminator: " + raw_delimiter);
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            wchar_t temp_dir[MAX_PATH];
            DWORD dir_len = GetTempPathW(MAX_PATH, temp_dir);
            if (dir_len == 0 || dir_len >= MAX_PATH) {
                report_script_syntax_error(line_index, L"failed to allocate here-doc temp path");
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            wchar_t temp_name[MAX_PATH];
            if (GetTempFileNameW(temp_dir, L"ksh", 0, temp_name) == 0) {
                report_script_syntax_error(line_index, L"failed to create here-doc temp file");
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            heredoc_temp_file = temp_name;
            std::wofstream out(heredoc_temp_file.c_str(), std::ios::binary | std::ios::trunc);
            if (!out.is_open()) {
                DeleteFileW(heredoc_temp_file.c_str());
                report_script_syntax_error(line_index, L"failed to write here-doc temp file");
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            out << heredoc_content;
            out.close();

            std::vector<std::wstring> rebuilt_tokens;
            for (size_t t = 0; t < heredoc_tokens.size(); ++t) {
                if (t == heredoc_op_index) {
                    const std::wstring& op_token = heredoc_tokens[t];
                    if (op_token == L"<<" || op_token == L"<<-") {
                        if ((t + 1) < heredoc_tokens.size()) {
                            t++;
                        }
                    }
                    continue;
                }

                if ((t == heredoc_op_index + 1) &&
                    (heredoc_tokens[heredoc_op_index] == L"<<" || heredoc_tokens[heredoc_op_index] == L"<<-")) {
                    continue;
                }

                rebuilt_tokens.push_back(heredoc_tokens[t]);
            }
            rebuilt_tokens.push_back(L"<");
            rebuilt_tokens.push_back(L"\"" + heredoc_temp_file + L"\"");
            command_to_execute.clear();
            for (size_t i = 0; i < rebuilt_tokens.size(); ++i) {
                if (!command_to_execute.empty()) {
                    command_to_execute += L" ";
                }
                command_to_execute += rebuilt_tokens[i];
            }
            consume_heredoc_lines = true;
            heredoc_end_line = probe;
        }

        if (!execute_command_line(command_to_execute, should_exit_shell)) {
            if (!heredoc_temp_file.empty()) {
                DeleteFileW(heredoc_temp_file.c_str());
            }
            return false;
        }
        if (!heredoc_temp_file.empty()) {
            DeleteFileW(heredoc_temp_file.c_str());
        }
        if (should_exit_shell) {
            return true;
        }
        if (is_function_return_requested()) {
            return true;
        }

        if (consume_heredoc_lines) {
            line_index = heredoc_end_line;
        }

        line_index++;
    }

    return true;
}

std::wstring decode_script_text(const std::vector<char>& bytes) {
    if (bytes.empty()) {
        return L"";
    }

    if (bytes.size() >= 3 &&
        static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB &&
        static_cast<unsigned char>(bytes[2]) == 0xBF) {
        return decode_multibyte(std::string(bytes.begin() + 3, bytes.end()), CP_UTF8);
    }

    if (bytes.size() >= 2 &&
        static_cast<unsigned char>(bytes[0]) == 0xFF &&
        static_cast<unsigned char>(bytes[1]) == 0xFE) {
        const size_t byte_count = bytes.size() - 2;
        const size_t wchar_count = byte_count / sizeof(wchar_t);
        std::wstring text(wchar_count, L'\0');
        if (wchar_count > 0) {
            std::memcpy(&text[0], bytes.data() + 2, wchar_count * sizeof(wchar_t));
        }
        return text;
    }

    if (bytes.size() >= 2 &&
        static_cast<unsigned char>(bytes[0]) == 0xFE &&
        static_cast<unsigned char>(bytes[1]) == 0xFF) {
        std::wstring text;
        for (size_t i = 2; i + 1 < bytes.size(); i += 2) {
            const unsigned char hi = static_cast<unsigned char>(bytes[i]);
            const unsigned char lo = static_cast<unsigned char>(bytes[i + 1]);
            wchar_t ch = static_cast<wchar_t>((hi << 8) | lo);
            text.push_back(ch);
        }
        return text;
    }

    std::wstring utf8 = decode_multibyte(std::string(bytes.begin(), bytes.end()), CP_UTF8);
    if (!utf8.empty()) {
        return utf8;
    }
    return decode_multibyte(std::string(bytes.begin(), bytes.end()), CP_ACP);
}

std::vector<std::wstring> split_script_lines(const std::wstring& text) {
    std::vector<std::wstring> lines;
    std::wstring current;
    for (wchar_t ch : text) {
        if (ch == L'\r') {
            continue;
        }
        if (ch == L'\n') {
            lines.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty()) {
        lines.push_back(current);
    }
    return lines;
}

std::vector<std::wstring> split_top_level_script_commands(const std::wstring& line) {
    std::vector<std::wstring> commands;
    std::wstring current;
    bool in_single_quotes = false;
    bool in_double_quotes = false;
    bool escaped = false;
    int paren_depth = 0;

    for (size_t i = 0; i < line.size(); ++i) {
        const wchar_t ch = line[i];
        if (escaped) {
            current += ch;
            escaped = false;
            continue;
        }
        if (ch == L'\\' && !in_single_quotes) {
            current += ch;
            escaped = true;
            continue;
        }
        if (ch == L'\'' && !in_double_quotes) {
            in_single_quotes = !in_single_quotes;
            current += ch;
            continue;
        }
        if (ch == L'"' && !in_single_quotes) {
            in_double_quotes = !in_double_quotes;
            current += ch;
            continue;
        }
        if (!in_single_quotes && !in_double_quotes) {
            if (ch == L'(') {
                ++paren_depth;
            } else if (ch == L')' && paren_depth > 0) {
                --paren_depth;
            }
            if (ch == L';' && paren_depth == 0) {
                if (i + 2 < line.size() && line[i + 1] == L';' && line[i + 2] == L'&') {
                    current += L";;&";
                    i += 2;
                    continue;
                }
                if (i + 1 < line.size() && (line[i + 1] == L';' || line[i + 1] == L'&')) {
                    current += L';';
                    current += line[i + 1];
                    ++i;
                    continue;
                }
                const std::wstring command = trim_copy(current);
                if (!command.empty()) {
                    commands.push_back(command);
                }
                current.clear();
                continue;
            }
        }
        current += ch;
    }

    const std::wstring command = trim_copy(current);
    if (!command.empty()) {
        commands.push_back(command);
    }
    return commands;
}

std::vector<std::wstring> prepare_script_lines(const std::wstring& text) {
    const std::vector<std::wstring> physical_lines = split_script_lines(text);
    std::vector<std::wstring> logical_lines;
    std::wstring continued;

    for (const std::wstring& physical_line : physical_lines) {
        std::wstring line = physical_line;
        size_t trailing = line.size();
        while (trailing > 0 && (line[trailing - 1] == L' ' || line[trailing - 1] == L'\t')) {
            --trailing;
        }
        const bool escaped_newline = trailing > 0 && line[trailing - 1] == L'\\';
        const std::wstring line_trimmed = trim_copy(line);
        const size_t line_command_end = line_trimmed.find_first_of(L" \t");
        const bool is_cd_path = line_trimmed.substr(0, line_command_end) == L"cd" &&
            line_trimmed.size() > line_command_end;
        if (escaped_newline && !is_cd_path) {
            line.erase(trailing - 1);
            continued += line;
            continue;
        }

        if (!continued.empty()) {
            line = continued + line;
            continued.clear();
        }

        const std::wstring trimmed = trim_copy(line);
        const bool trailing_pipe = !trimmed.empty() && trimmed.back() == L'|';
        if (trailing_pipe) {
            continued = line + L' ';
            continue;
        }

        const std::wstring lowered_line = to_lower_copy(trimmed);
        if (starts_with_for_control_keyword(trimmed)) {
            const size_t do_pos = lowered_line.find(L"; do");
            const size_t done_pos = lowered_line.rfind(L"; done");
            if (do_pos != std::wstring::npos && done_pos != std::wstring::npos && done_pos > do_pos) {
                logical_lines.push_back(trim_copy(trimmed.substr(0, do_pos)));
                logical_lines.push_back(L"do");
                const std::wstring body = trim_copy(trimmed.substr(do_pos + 5, done_pos - (do_pos + 5)));
                const std::vector<std::wstring> body_commands = split_top_level_script_commands(body);
                logical_lines.insert(logical_lines.end(), body_commands.begin(), body_commands.end());
                logical_lines.push_back(L"done" + trimmed.substr(done_pos + 6));
                continue;
            }
        }

        const std::vector<std::wstring> commands = split_top_level_script_commands(line);
        for (const std::wstring& command : commands) {
            std::wstring compact = trim_copy(command);
            std::wstring lowered = to_lower_copy(compact);
            if (starts_with_for_control_keyword(compact)) {
                const size_t do_pos = lowered.find(L"; do");
                const size_t done_pos = lowered.rfind(L"; done");
                if (do_pos != std::wstring::npos && done_pos != std::wstring::npos && done_pos > do_pos) {
                    logical_lines.push_back(trim_copy(compact.substr(0, do_pos)));
                    logical_lines.push_back(L"do");
                    const std::wstring body = trim_copy(compact.substr(do_pos + 5, done_pos - (do_pos + 5)));
                    const std::vector<std::wstring> body_commands = split_top_level_script_commands(body);
                    logical_lines.insert(logical_lines.end(), body_commands.begin(), body_commands.end());
                    logical_lines.push_back(L"done" + compact.substr(done_pos + 6));
                    continue;
                }
            }
            logical_lines.push_back(compact);
        }
    }

    if (!continued.empty()) {
        const std::vector<std::wstring> commands = split_top_level_script_commands(continued);
        logical_lines.insert(logical_lines.end(), commands.begin(), commands.end());
    }
    return logical_lines;
}

bool execute_script_file(const std::wstring& script_path, const std::vector<std::wstring>& script_args, bool& should_exit_shell) {
    ScriptContext context;
    context.script_name = script_path;
    context.args = script_args;
    g_script_context_stack.push_back(context);

    std::ifstream script_file(script_path.c_str(), std::ios::binary);
    if (!script_file.is_open()) {
        std::wcerr << L"ksh: cannot open script: " << script_path << L"\n";
        ksh_env.variables[L"?"] = L"1";
        g_script_context_stack.pop_back();
        return false;
    }

    script_file.seekg(0, std::ios::end);
    std::streampos script_size = script_file.tellg();
    script_file.seekg(0, std::ios::beg);
    if (script_size < 0 || static_cast<size_t>(script_size) > kMaxScriptFileSizeBytes) {
        std::wcerr << L"ksh: script file too large: " << script_path << L"\n";
        ksh_env.variables[L"?"] = L"1";
        g_script_context_stack.pop_back();
        return false;
    }

    std::vector<char> bytes((std::istreambuf_iterator<char>(script_file)), std::istreambuf_iterator<char>());
    std::wstring script_text = decode_script_text(bytes);
    std::vector<std::wstring> lines = prepare_script_lines(script_text);

    size_t start_index = 0;
    if (!lines.empty() && lines[0].rfind(L"#!", 0) == 0) {
        start_index = 1;
    }

    if (!execute_script_lines_range(lines, start_index, lines.size(), should_exit_shell)) {
        g_script_context_stack.pop_back();
        return false;
    }
    if (should_exit_shell) {
        g_script_context_stack.pop_back();
        return true;
    }

    g_script_context_stack.pop_back();
    return true;
}

// Function execution and scope restore.
bool execute_defined_function(const std::wstring& function_name, const std::vector<std::wstring>& function_args, bool& should_exit_shell) {
    std::map<std::wstring, ShellFunctionDefinition>::const_iterator fn_it = g_shell_functions.find(function_name);
    if (fn_it == g_shell_functions.end()) {
        return false;
    }

    if (static_cast<int>(g_function_scope_stack.size()) >= kMaxFunctionCallDepth) {
        std::wcerr << L"ksh: " << function_name << L": maximum function call depth exceeded\n";
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    g_function_scope_stack.emplace_back();
    g_function_scope_stack.back().locals.reserve(function_args.size() + 8);

    g_script_context_stack.emplace_back();
    g_script_context_stack.back().script_name = function_name;
    g_script_context_stack.back().args = function_args;

    bool ok = execute_script_lines_range(fn_it->second.body_lines, 0, fn_it->second.body_lines.size(), should_exit_shell, nullptr);

    FunctionScopeContext finished_scope = std::move(g_function_scope_stack.back());
    g_function_scope_stack.pop_back();
    restore_function_scope_variables(finished_scope);

    g_script_context_stack.pop_back();

    if (!ok) {
        if (ksh_env.variables.find(L"?") == ksh_env.variables.end() || ksh_env.variables[L"?"] == L"0") {
            ksh_env.variables[L"?"] = L"1";
        }
        return false;
    }

    if (finished_scope.return_requested) {
        ksh_env.variables[L"?"] = std::to_wstring(finished_scope.return_status);
        return true;
    }

    if (ksh_env.variables.find(L"?") == ksh_env.variables.end()) {
        ksh_env.variables[L"?"] = L"0";
    }

    return true;
}

struct PipeRelayParams {
    HANDLE hChild;
    HANDLE hServerPipe;
    HANDLE hEvent;
    OVERLAPPED* ov;
    bool is_output;
    HANDLE hProcess;
    HANDLE hThread;
};

DWORD get_process_substitution_connect_timeout_ms() {
    std::wstring raw_value = trim_copy(get_environment_value(L"ksh_PROC_SUB_CONNECT_TIMEOUT_MS"));
    if (raw_value.empty()) {
        return kDefaultProcessSubstitutionConnectTimeoutMs;
    }

    wchar_t* end_ptr = nullptr;
    errno = 0;
    unsigned long parsed = std::wcstoul(raw_value.c_str(), &end_ptr, 10);
    if (errno != 0 || end_ptr == raw_value.c_str() || (end_ptr != nullptr && *end_ptr != L'\0')) {
        return kDefaultProcessSubstitutionConnectTimeoutMs;
    }

    DWORD timeout_ms = static_cast<DWORD>(parsed);
    if (timeout_ms < kMinProcessSubstitutionConnectTimeoutMs) {
        return kMinProcessSubstitutionConnectTimeoutMs;
    }
    if (timeout_ms > kMaxProcessSubstitutionConnectTimeoutMs) {
        return kMaxProcessSubstitutionConnectTimeoutMs;
    }
    return timeout_ms;
}

std::wstring make_process_substitution_pipe_name(LONG id) {
    LARGE_INTEGER qpc;
    if (!QueryPerformanceCounter(&qpc)) {
        qpc.QuadPart = static_cast<LONGLONG>(GetTickCount64());
    }

    const unsigned long long entropy =
        (static_cast<unsigned long long>(qpc.QuadPart) << 1) ^
        (static_cast<unsigned long long>(GetTickCount64()) << 17) ^
        (static_cast<unsigned long long>(GetCurrentProcessId()) << 33) ^
        (static_cast<unsigned long long>(GetCurrentThreadId()) << 7) ^
        static_cast<unsigned long long>(id);

    return L"\\\\.\\pipe\\ksh_ps_" +
        std::to_wstring(GetCurrentProcessId()) + L"_" +
        std::to_wstring(id) + L"_" +
        std::to_wstring(entropy);
}

DWORD WINAPI PipeRelayThread(LPVOID lpParam) {
    PipeRelayParams* params = static_cast<PipeRelayParams*>(lpParam);
    if (params == nullptr) {
        return 0;
    }

    HANDLE hChild = params->hChild;
    HANDLE hServer = params->hServerPipe;
    HANDLE hEvent = params->hEvent;
    OVERLAPPED* ov = params->ov;
    bool is_output = params->is_output;
    HANDLE hProcess = params->hProcess;
    HANDLE hThread = params->hThread;
    delete params;

    bool child_terminated = false;
    auto wait_for_pipe_io = [&](HANDLE io_event, OVERLAPPED* io_ov, DWORD timeout_ms, DWORD& transferred) -> bool {
        HANDLE wait_handles[2] = { io_event, hProcess };
        DWORD wait_count = (hProcess != nullptr && hProcess != INVALID_HANDLE_VALUE) ? 2 : 1;
        DWORD wait_result = WaitForMultipleObjects(wait_count, wait_handles, FALSE, timeout_ms);
        if (wait_result == WAIT_OBJECT_0) {
            return GetOverlappedResult(hServer, io_ov, &transferred, FALSE) != FALSE;
        }
        if (wait_result == WAIT_OBJECT_0 + 1) {
            child_terminated = true;
            CancelIoEx(hServer, io_ov);
            GetOverlappedResult(hServer, io_ov, &transferred, TRUE);
            return false;
        }
        CancelIoEx(hServer, io_ov);
        GetOverlappedResult(hServer, io_ov, &transferred, TRUE);
        return false;
    };

    DWORD dwWait = WAIT_FAILED;
    if (hEvent != nullptr && hEvent != INVALID_HANDLE_VALUE) {
        HANDLE connect_handles[2] = { hEvent, hProcess };
        DWORD connect_count = (hProcess != nullptr && hProcess != INVALID_HANDLE_VALUE) ? 2 : 1;
        dwWait = WaitForMultipleObjects(connect_count, connect_handles, FALSE, get_process_substitution_connect_timeout_ms());
        if (dwWait == WAIT_OBJECT_0 + 1) {
            child_terminated = true;
            if (is_output) {
                // Child finished producing output; still wait for reader to connect and drain buffer
                dwWait = WaitForSingleObject(hEvent, get_process_substitution_connect_timeout_ms());
            }
        }
        if (dwWait != WAIT_OBJECT_0 && hServer != nullptr && hServer != INVALID_HANDLE_VALUE && ov != nullptr) {
            CancelIoEx(hServer, ov);
            DWORD transferred = 0;
            GetOverlappedResult(hServer, ov, &transferred, TRUE);
        }
        CloseHandle(hEvent);
    }

    if (ov != nullptr) {
        delete ov;
    }

    if (dwWait == WAIT_OBJECT_0) {
        char buffer[65536];
        DWORD bytesRead = 0;
        DWORD bytesWritten = 0;
        HANDLE hIoEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

        if (hIoEvent != nullptr) {
            if (is_output) {
                // <(cmd): child outputs data, relay forwards it to named pipe (hServer)
                bool early_reader_exit = false;
                while (ReadFile(hChild, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
                    OVERLAPPED w_ov;
                    ZeroMemory(&w_ov, sizeof(w_ov));
                    ResetEvent(hIoEvent);
                    w_ov.hEvent = hIoEvent;

                    bytesWritten = 0;
                    BOOL bWrite = WriteFile(hServer, buffer, bytesRead, &bytesWritten, &w_ov);
                    if (!bWrite) {
                        DWORD err = GetLastError();
                        if (err == ERROR_IO_PENDING) {
                            if (!wait_for_pipe_io(hIoEvent, &w_ov, INFINITE, bytesWritten)) {
                                early_reader_exit = true;
                                break;
                            }
                        } else {
                            // ERROR_BROKEN_PIPE, ERROR_PIPE_NOT_CONNECTED, ERROR_NO_DATA, etc.
                            early_reader_exit = true;
                            break;
                        }
                    }

                    if (bytesWritten != bytesRead) {
                        early_reader_exit = true;
                        break;
                    }
                }

                if (!early_reader_exit) {
                    FlushFileBuffers(hServer);
                } else if (hProcess != nullptr && hProcess != INVALID_HANDLE_VALUE) {
                    // Downstream reader exited early (e.g. head -n 1 <(cmd)).
                    // Close hChild immediately to signal broken pipe to child.
                    if (hChild != nullptr && hChild != INVALID_HANDLE_VALUE) {
                        CloseHandle(hChild);
                        hChild = INVALID_HANDLE_VALUE;
                    }
                    // Give child a brief grace period to terminate upon broken pipe, otherwise terminate it
                    if (WaitForSingleObject(hProcess, 50) != WAIT_OBJECT_0) {
                        TerminateProcess(hProcess, 0);
                        WaitForSingleObject(hProcess, 1000);
                    }
                    child_terminated = true;
                }
            } else {
                // >(cmd): main command writes to named pipe (hServer), relay forwards to child stdin (hChild)
                while (true) {
                    OVERLAPPED r_ov;
                    ZeroMemory(&r_ov, sizeof(r_ov));
                    ResetEvent(hIoEvent);
                    r_ov.hEvent = hIoEvent;

                    BOOL bRead = ReadFile(hServer, buffer, sizeof(buffer), &bytesRead, &r_ov);
                    if (!bRead) {
                        DWORD err = GetLastError();
                        if (err == ERROR_IO_PENDING) {
                            bRead = wait_for_pipe_io(hIoEvent, &r_ov, INFINITE, bytesRead);
                        } else {
                            // Writer closed pipe (clean EOF) or error
                            break;
                        }
                    }

                    if (!bRead || bytesRead == 0 || child_terminated) {
                        break;
                    }

                    if (!WriteFile(hChild, buffer, bytesRead, &bytesWritten, NULL) || bytesWritten != bytesRead) {
                        break;
                    }
                }

                // EOF reached on input pipe -> Close child's stdin so child sees EOF and finishes execution
                if (hChild != nullptr && hChild != INVALID_HANDLE_VALUE) {
                    CloseHandle(hChild);
                    hChild = INVALID_HANDLE_VALUE;
                }
                // Allow child process up to 10s to complete processing remaining input
                if (!child_terminated && hProcess != nullptr && hProcess != INVALID_HANDLE_VALUE) {
                    WaitForSingleObject(hProcess, 10000);
                }
            }
            CloseHandle(hIoEvent);
        }
    } else if (!child_terminated && hProcess != nullptr && hProcess != INVALID_HANDLE_VALUE) {
        TerminateProcess(hProcess, 1);
        WaitForSingleObject(hProcess, 5000);
    }

    if (hServer != nullptr && hServer != INVALID_HANDLE_VALUE) {
        DisconnectNamedPipe(hServer);
    }

    if (hChild != nullptr && hChild != INVALID_HANDLE_VALUE) {
        CloseHandle(hChild);
    }
    if (hServer != nullptr && hServer != INVALID_HANDLE_VALUE) {
        CloseHandle(hServer);
    }
    if (hProcess != nullptr && hProcess != INVALID_HANDLE_VALUE) {
        CloseHandle(hProcess);
    }
    if (hThread != nullptr && hThread != INVALID_HANDLE_VALUE) {
        CloseHandle(hThread);
    }
    return 0;
}

std::wstring escape_quotes_for_cmd_line(const std::wstring& str) {
    std::wstring result;
    for (wchar_t ch : str) {
        if (ch == L'"') {
            result += L"\\\"";
        } else {
            result += ch;
        }
    }
    return result;
}

std::wstring handle_single_process_substitution(const std::wstring& inner_cmd, bool is_in) {
    static volatile LONG g_proc_sub_id = 0;
    LONG id = InterlockedIncrement(&g_proc_sub_id);
    std::wstring pipeName = make_process_substitution_pipe_name(id);

    PSECURITY_DESCRIPTOR pipe_sd = nullptr;
    const wchar_t* pipe_sddl = L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;OW)";
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            pipe_sddl,
            SDDL_REVISION_1,
            &pipe_sd,
            nullptr)) {
        pipe_sd = nullptr;
    }

    SECURITY_ATTRIBUTES pipe_sa;
    pipe_sa.nLength = sizeof(pipe_sa);
    pipe_sa.lpSecurityDescriptor = pipe_sd;
    pipe_sa.bInheritHandle = FALSE;
    
    HANDLE hServerPipe = CreateNamedPipeW(
        pipeName.c_str(),
        PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
        1, 65536, 65536, 0, &pipe_sa
    );

    if (pipe_sd != nullptr) {
        LocalFree(pipe_sd);
    }
    
    if (hServerPipe == INVALID_HANDLE_VALUE) {
        return L"";
    }
    
    OVERLAPPED* ov = new OVERLAPPED();
    ZeroMemory(ov, sizeof(OVERLAPPED));
    ov->hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (ov->hEvent == nullptr) {
        delete ov;
        CloseHandle(hServerPipe);
        return L"";
    }
    
    BOOL bConn = ConnectNamedPipe(hServerPipe, ov);
    if (!bConn && GetLastError() != ERROR_IO_PENDING && GetLastError() != ERROR_PIPE_CONNECTED) {
        CloseHandle(ov->hEvent);
        delete ov;
        CloseHandle(hServerPipe);
        return L"";
    }
    
    if (bConn || GetLastError() == ERROR_PIPE_CONNECTED) {
        SetEvent(ov->hEvent);
    }
    
    HANDLE hChildRead = nullptr;
    HANDLE hChildWrite = nullptr;
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;
    
    if (!CreatePipe(&hChildRead, &hChildWrite, &sa, 0)) {
        CloseHandle(ov->hEvent);
        delete ov;
        CloseHandle(hServerPipe);
        return L"";
    }
    
    if (is_in) {
        if (!SetHandleInformation(hChildRead, HANDLE_FLAG_INHERIT, 0)) {
            CloseHandle(hChildRead);
            CloseHandle(hChildWrite);
            CancelIoEx(hServerPipe, ov);
            CloseHandle(ov->hEvent);
            delete ov;
            CloseHandle(hServerPipe);
            return L"";
        }
    } else {
        if (!SetHandleInformation(hChildWrite, HANDLE_FLAG_INHERIT, 0)) {
            CloseHandle(hChildRead);
            CloseHandle(hChildWrite);
            CancelIoEx(hServerPipe, ov);
            CloseHandle(ov->hEvent);
            delete ov;
            CloseHandle(hServerPipe);
            return L"";
        }
    }
    
    wchar_t ksh_path[MAX_PATH];
    DWORD path_len = GetModuleFileNameW(nullptr, ksh_path, MAX_PATH);
    if (path_len == 0 || path_len >= MAX_PATH) {
        CloseHandle(hChildRead);
        CloseHandle(hChildWrite);
        CancelIoEx(hServerPipe, ov);
        CloseHandle(ov->hEvent);
        delete ov;
        CloseHandle(hServerPipe);
        return L"";
    }
    
    std::wstring command_line = quote_command_argument(ksh_path) + L" -c " + quote_command_argument(inner_cmd);
    
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    
    if (is_in) {
        si.hStdOutput = hChildWrite;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    } else {
        si.hStdInput = hChildRead;
        si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    }
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    
    std::vector<wchar_t> cmdBuf(command_line.size() + 1, 0);
    wcsncpy_s(cmdBuf.data(), cmdBuf.size(), command_line.c_str(), command_line.size());
    
    std::vector<HANDLE> inherited_handles = { si.hStdInput, si.hStdOutput, si.hStdError };
    if (create_process_with_handle_list(command_line, si, inherited_handles, 0, pi)) {
        if (is_in) {
            CloseHandle(hChildWrite);
        } else {
            CloseHandle(hChildRead);
        }
        
        PipeRelayParams* params = new PipeRelayParams();
        params->hChild = is_in ? hChildRead : hChildWrite;
        params->hServerPipe = hServerPipe;
        params->hEvent = ov->hEvent;
        params->ov = ov;
        params->is_output = is_in;
        params->hProcess = pi.hProcess;
        params->hThread = pi.hThread;
        
        HANDLE relay_thread = CreateThread(NULL, 0, PipeRelayThread, params, 0, NULL);
        if (relay_thread == nullptr) {
            TerminateProcess(pi.hProcess, 1);
            WaitForSingleObject(pi.hProcess, 5000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            if (params->hChild != nullptr && params->hChild != INVALID_HANDLE_VALUE) {
                CloseHandle(params->hChild);
            }
            if (params->hServerPipe != nullptr && params->hServerPipe != INVALID_HANDLE_VALUE) {
                CancelIoEx(params->hServerPipe, params->ov);
                DisconnectNamedPipe(params->hServerPipe);
                CloseHandle(params->hServerPipe);
            }
            if (params->hEvent != nullptr && params->hEvent != INVALID_HANDLE_VALUE) {
                CloseHandle(params->hEvent);
            }
            delete params;
            delete ov;
            return L"";
        }
        CloseHandle(relay_thread);
    } else {
        CloseHandle(hChildRead);
        CloseHandle(hChildWrite);
        CancelIoEx(hServerPipe, ov);
        CloseHandle(ov->hEvent);
        delete ov;
        CloseHandle(hServerPipe);
    }
    
    return pipeName;
}

void parse_and_replace_process_substitutions(std::wstring& line) {
    if (line.find(L"<(") == std::wstring::npos && line.find(L">(") == std::wstring::npos) {
        return;
    }
    size_t pos = 0;
    while (pos < line.size()) {
        bool in_single = false;
        bool in_double = false;
        bool escaped = false;
        size_t start = std::wstring::npos;
        bool is_in = false;

        for (size_t k = pos; k < line.size(); ++k) {
            wchar_t c = line[k];
            if (escaped) {
                escaped = false;
                continue;
            }
            if (c == L'\\' && !in_single) {
                escaped = true;
                continue;
            }
            if (c == L'\'' && !in_double) {
                in_single = !in_single;
                continue;
            }
            if (c == L'"' && !in_single) {
                in_double = !in_double;
                continue;
            }
            if (!in_single && !in_double) {
                if ((c == L'<' || c == L'>') && k + 1 < line.size() && line[k + 1] == L'(') {
                    start = k;
                    is_in = (c == L'<');
                    break;
                }
            }
        }

        if (start == std::wstring::npos) {
            break;
        }

        size_t i = start + 2;
        int depth = 1;
        bool inner_single = false;
        bool inner_double = false;
        bool inner_escaped = false;

        while (i < line.size() && depth > 0) {
            wchar_t c = line[i];
            if (inner_escaped) {
                inner_escaped = false;
                i++;
                continue;
            }
            if (c == L'\\' && !inner_single) {
                inner_escaped = true;
                i++;
                continue;
            }
            if (c == L'\'' && !inner_double) {
                inner_single = !inner_single;
                i++;
                continue;
            }
            if (c == L'"' && !inner_single) {
                inner_double = !inner_double;
                i++;
                continue;
            }
            if (!inner_single && !inner_double) {
                if (c == L'(') depth++;
                else if (c == L')') {
                    depth--;
                    if (depth == 0) break;
                }
            }
            i++;
        }

        if (depth == 0 && i < line.size()) {
            std::wstring inner = line.substr(start + 2, i - (start + 2));
            parse_and_replace_process_substitutions(inner);

            std::wstring pipePath = handle_single_process_substitution(inner, is_in);
            if (pipePath.empty()) {
                std::wcerr << L"ksh: process substitution setup failed\n";
                g_expansion_error = true;
                return;
            }

            std::wstring escapedPipePath;
            for (wchar_t ch : pipePath) {
                if (ch == L'\\') {
                    escapedPipePath += L"\\\\";
                } else {
                    escapedPipePath += ch;
                }
            }

            line.replace(start, i + 1 - start, escapedPipePath);
            pos = start + escapedPipePath.size();
        } else {
            pos = start + 2;
        }
    }
}

bool execute_command_line(const std::wstring& input_line, bool& should_exit_shell, bool bypass_aliases, bool bypass_function_lookup) {
    std::wstring line = input_line;
    g_expansion_error = false;
    parse_and_replace_process_substitutions(line);
    if (g_expansion_error) {
        ksh_env.variables[L"?"] = L"1";
        return false;
    }
    
    bool result = execute_command_line_impl(line, should_exit_shell, bypass_aliases, bypass_function_lookup);
    
    if (g_errexit_enabled && !g_is_interactive_session && g_errexit_disabled_depth == 0) {
        std::map<std::wstring, std::wstring>::const_iterator status_it = ksh_env.variables.find(L"?");
        if (status_it != ksh_env.variables.end() && status_it->second != L"0") {
            should_exit_shell = true;
        }
    }
    return result;
}

// Main command execution pipeline.
bool execute_command_line_impl(const std::wstring& input_line, bool& should_exit_shell, bool bypass_aliases, bool bypass_function_lookup) {
    const bool perf_trace = is_performance_telemetry_enabled();
    const ULONGLONG command_start = perf_trace ? GetTickCount64() : 0;

    // Execution phase: trap dispatch gate. Pending INT/TERM traps are processed before parsing.
    process_pending_traps(should_exit_shell);
    if (should_exit_shell) {
        return true;
    }

    if (input_line.empty()) {
        return true;
    }

    std::wstring lexical_error;
    if (!validate_shell_lexical_state(input_line, lexical_error)) {
        std::wcerr << lexical_error << L"\n";
        ksh_env.variables[L"?"] = L"2";
        return false;
    }

    auto is_double_parentheses = [](const std::wstring& text, std::wstring& inner) -> bool {
        const std::wstring trimmed = trim_copy(text);
        if (trimmed.size() < 4 || trimmed.substr(0, 2) != L"((" || trimmed.substr(trimmed.size() - 2) != L"))") {
            return false;
        }

        bool in_single_quotes = false;
        bool in_double_quotes = false;
        bool escaped = false;
        int depth = 0;

        for (size_t i = 0; i < trimmed.size(); ++i) {
            wchar_t ch = trimmed[i];
            if (escaped) {
                escaped = false;
                continue;
            }

            if (ch == L'\\' && !in_single_quotes) {
                escaped = true;
                continue;
            }

            if (ch == L'\'' && !in_double_quotes) {
                in_single_quotes = !in_single_quotes;
                continue;
            }

            if (ch == L'"' && !in_single_quotes) {
                in_double_quotes = !in_double_quotes;
                continue;
            }

            if (in_single_quotes || in_double_quotes) {
                continue;
            }

            if (ch == L'(') {
                depth++;
            } else if (ch == L')') {
                depth--;
                if (depth == 0 && i + 1 != trimmed.size()) {
                    return false;
                }
            }

            if (depth < 0) {
                return false;
            }
        }

        if (depth != 0) {
            return false;
        }

        inner = trim_copy(trimmed.substr(2, trimmed.size() - 4));
        return true;
    };

    auto is_enclosed_by_top_level_pair = [](const std::wstring& text, wchar_t open_ch, wchar_t close_ch, std::wstring& inner) -> bool {
        const std::wstring trimmed = trim_copy(text);
        if (trimmed.size() < 2 || trimmed.front() != open_ch || trimmed.back() != close_ch) {
            return false;
        }

        bool in_single_quotes = false;
        bool in_double_quotes = false;
        bool escaped = false;
        int depth = 0;

        for (size_t i = 0; i < trimmed.size(); ++i) {
            wchar_t ch = trimmed[i];
            if (escaped) {
                escaped = false;
                continue;
            }

            if (ch == L'\\' && !in_single_quotes) {
                escaped = true;
                continue;
            }

            if (ch == L'\'' && !in_double_quotes) {
                in_single_quotes = !in_single_quotes;
                continue;
            }

            if (ch == L'"' && !in_single_quotes) {
                in_double_quotes = !in_double_quotes;
                continue;
            }

            if (in_single_quotes || in_double_quotes) {
                continue;
            }

            if (ch == open_ch) {
                depth++;
            } else if (ch == close_ch) {
                depth--;
                if (depth == 0 && i + 1 != trimmed.size()) {
                    return false;
                }
            }

            if (depth < 0) {
                return false;
            }
        }

        if (depth != 0) {
            return false;
        }

        inner = trim_copy(trimmed.substr(1, trimmed.size() - 2));
        return true;
    };

    auto split_top_level_command_list = [](const std::wstring& text, std::vector<std::wstring>& segments, std::vector<std::wstring>& separators) {
        segments.clear();
        separators.clear();

        bool in_single_quotes = false;
        bool in_double_quotes = false;
        bool escaped = false;
        int paren_depth = 0;
        int brace_depth = 0;
        std::wstring current;

        for (size_t i = 0; i < text.size(); ++i) {
            wchar_t ch = text[i];

            if (escaped) {
                current.push_back(ch);
                escaped = false;
                continue;
            }

            if (ch == L'\\' && !in_single_quotes) {
                current.push_back(ch);
                escaped = true;
                continue;
            }

            if (ch == L'\'' && !in_double_quotes) {
                in_single_quotes = !in_single_quotes;
                current.push_back(ch);
                continue;
            }

            if (ch == L'"' && !in_single_quotes) {
                in_double_quotes = !in_double_quotes;
                current.push_back(ch);
                continue;
            }

            if (!in_single_quotes && !in_double_quotes) {
                if (ch == L'(') {
                    paren_depth++;
                    current.push_back(ch);
                    continue;
                }
                if (ch == L')' && paren_depth > 0) {
                    paren_depth--;
                    current.push_back(ch);
                    continue;
                }
                if (ch == L'{') {
                    brace_depth++;
                    current.push_back(ch);
                    continue;
                }
                if (ch == L'}' && brace_depth > 0) {
                    brace_depth--;
                    current.push_back(ch);
                    continue;
                }

                if (paren_depth == 0 && brace_depth == 0) {
                    if (ch == L'[' && (i + 1) < text.size() && text[i + 1] == L'[') {
                        current += L"[[";
                        i += 2;

                        bool cond_single_quotes = false;
                        bool cond_double_quotes = false;
                        bool cond_escaped = false;
                        for (; i < text.size(); ++i) {
                            wchar_t cond_ch = text[i];
                            current.push_back(cond_ch);

                            if (cond_escaped) {
                                cond_escaped = false;
                                continue;
                            }

                            if (cond_ch == L'\\' && !cond_single_quotes) {
                                cond_escaped = true;
                                continue;
                            }

                            if (cond_ch == L'\'' && !cond_double_quotes) {
                                cond_single_quotes = !cond_single_quotes;
                                continue;
                            }

                            if (cond_ch == L'"' && !cond_single_quotes) {
                                cond_double_quotes = !cond_double_quotes;
                                continue;
                            }

                            if (!cond_single_quotes && !cond_double_quotes && cond_ch == L']' &&
                                (i + 1) < text.size() && text[i + 1] == L']') {
                                current.push_back(L']');
                                i++;
                                break;
                            }
                        }

                        continue;
                    }

                    if (ch == L';') {
                        segments.push_back(trim_copy(current));
                        separators.push_back(L";");
                        current.clear();
                        continue;
                    }

                    if (ch == L'&' && (i + 1) < text.size() && text[i + 1] == L'&') {
                        segments.push_back(trim_copy(current));
                        separators.push_back(L"&&");
                        current.clear();
                        i++;
                        continue;
                    }

                    if (ch == L'|' && (i + 1) < text.size() && text[i + 1] == L'|') {
                        segments.push_back(trim_copy(current));
                        separators.push_back(L"||");
                        current.clear();
                        i++;
                        continue;
                    }
                }
            }

            current.push_back(ch);
        }

        segments.push_back(trim_copy(current));
    };

    // structural parsing. Handle { ... }, ( ... ), and top-level command lists first.
    // separators (;, &&, ||) are recognized only at top level, never inside quotes/[[...]].
    const std::wstring early_trimmed = trim_copy(input_line);
    std::wstring enclosed_inner;
    if (is_double_parentheses(early_trimmed, enclosed_inner)) {
        std::wstring expanded = expand_substitutions_left_to_right(enclosed_inner);
        if (g_expansion_error) {
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        double val = 0.0;
        bool parse_ok = true;
        try {
            val = evaluate_arithmetic_double(expanded);
            // parsed
        } catch (const std::exception& ex) {
            std::wstring wmsg = L"ksh: ((" + expanded + L")): " + std::wstring(ex.what(), ex.what() + strlen(ex.what()));
            std::wcerr << wmsg << L"\n";
            parse_ok = false;
        } catch (...) {
            std::wcerr << L"ksh: ((" << expanded << L")): arithmetic parse error\n";
            parse_ok = false;
        }

        if (!parse_ok) {
            ksh_env.variables[L"?"] = L"1";
            return true;
        }

        bool exit_success = (val != 0.0);
        ksh_env.variables[L"?"] = exit_success ? L"0" : L"1";
        return true;
    }
    if (is_enclosed_by_top_level_pair(early_trimmed, L'{', L'}', enclosed_inner)) {
        return execute_command_line(enclosed_inner, should_exit_shell);
    }
    if (is_enclosed_by_top_level_pair(early_trimmed, L'(', L')', enclosed_inner)) {
        const ULONGLONG subshell_start = GetTickCount64();
        bool stateless_fast_path = false;
        if (!has_unquoted_shell_metacharacters(enclosed_inner)) {
            std::vector<std::wstring> subshell_tokens = ksh_tokenize_preserve_quotes(enclosed_inner);
            stateless_fast_path = can_capture_builtin_command_substitution(subshell_tokens);
        }

        if (stateless_fast_path) {
            bool ok = execute_command_line(enclosed_inner, should_exit_shell);
            const ULONGLONG elapsed_ms = GetTickCount64() - subshell_start;
            record_parenthesized_subshell_path_perf(true, false, elapsed_ms);
            if (perf_trace) {
                std::wstring detail = L"path=fast count=" + std::to_wstring(g_subshell_path_perf_counters.parenthesized_fast_count) +
                    L" total_ms=" + std::to_wstring(g_subshell_path_perf_counters.parenthesized_fast_ms);
                log_performance_trace(L"subshell.parenthesized", elapsed_ms, detail);
            }
            return ok;
        }

        const bool require_state_snapshot = command_requires_environment_snapshot(enclosed_inner);
        ScopedSubshellDepthGuard subshell_depth_guard;
        std::optional<ShellStateSnapshot> saved_snapshot;
        ScopedSnapshotRestoreGuard snapshot_guard(saved_snapshot, L"1");

        if (require_state_snapshot) {
            saved_snapshot.emplace(capture_shell_state_snapshot());
        }

        bool ok = execute_command_line(enclosed_inner, should_exit_shell);
        const bool used_snapshot_restore = saved_snapshot.has_value();
        restore_shell_state_snapshot_if_present(saved_snapshot, L"1");
        snapshot_guard.dismiss();

        const ULONGLONG elapsed_ms = GetTickCount64() - subshell_start;
        record_parenthesized_subshell_path_perf(false, used_snapshot_restore, elapsed_ms);
        if (perf_trace) {
            std::wstring detail;
            if (used_snapshot_restore) {
                detail = L"path=snapshot count=" + std::to_wstring(g_subshell_path_perf_counters.parenthesized_snapshot_count) +
                    L" total_ms=" + std::to_wstring(g_subshell_path_perf_counters.parenthesized_snapshot_ms);
            } else {
                detail = L"path=direct_no_snapshot count=" + std::to_wstring(g_subshell_path_perf_counters.parenthesized_direct_count) +
                    L" total_ms=" + std::to_wstring(g_subshell_path_perf_counters.parenthesized_direct_ms);
            }
            log_performance_trace(L"subshell.parenthesized", elapsed_ms, detail);
        }

        return ok;
    }

    std::vector<std::wstring> list_segments;
    std::vector<std::wstring> list_separators;
    split_top_level_command_list(early_trimmed, list_segments, list_separators);
    if (!list_separators.empty()) {
        bool last_result = true;
        bool have_executed = false;

        for (size_t i = 0; i < list_segments.size(); ++i) {
            const std::wstring segment = trim_copy(list_segments[i]);
            if (segment.empty()) {
                continue;
            }

            bool should_run = true;
            if (have_executed && i > 0) {
                const std::wstring& sep = list_separators[i - 1];
                if (sep == L"&&") {
                    should_run = last_result;
                } else if (sep == L"||") {
                    should_run = !last_result;
                }
            }

            if (!should_run) {
                continue;
            }

            bool disable_err = (i < list_separators.size() && (list_separators[i] == L"&&" || list_separators[i] == L"||"));
            if (disable_err) {
                g_errexit_disabled_depth++;
            }
            bool invoke_ok = execute_command_line(segment, should_exit_shell);
            if (disable_err) {
                g_errexit_disabled_depth--;
            }
            (void)invoke_ok;
            std::map<std::wstring, std::wstring>::const_iterator status_it = ksh_env.variables.find(L"?");
            last_result = (status_it != ksh_env.variables.end() && status_it->second == L"0");
            have_executed = true;
            if (should_exit_shell || is_function_return_requested()) {
                return last_result;
            }
        }

        if (!have_executed) {
            ksh_env.variables[L"?"] = L"0";
            return true;
        }
        return true;
    }

    // Execution phase: history + alias preprocessing.
    // alias expansion applies only to the leading token and is recursion-capped.
    std::wstring resolved_input;
    if (!resolve_history_recall(input_line, resolved_input)) {
        ksh_env.variables[L"?"] = L"1";
        return false;
    }
    if (resolved_input != input_line) {
        std::wcout << resolved_input << L"\n";
    }
    append_history_entry_to_file(resolved_input);

    if (!bypass_aliases) {
        // Expand aliases on the leading command token with a small recursion cap.
        for (int alias_depth = 0; alias_depth < 16; ++alias_depth) {
            std::vector<std::wstring> alias_tokens = ksh_tokenize_preserve_quotes(resolved_input);
            if (alias_tokens.empty()) {
                break;
            }

            const std::wstring alias_name = alias_tokens[0];
            if (alias_name == L"alias" || alias_name == L"unalias") {
                break;
            }

            std::map<std::wstring, std::wstring>::const_iterator alias_it = g_aliases.find(alias_name);
            if (alias_it == g_aliases.end()) {
                break;
            }

            std::wstring remainder = join_tokens_with_spaces(alias_tokens, 1);
            resolved_input = alias_it->second;
            if (!remainder.empty()) {
                resolved_input += L" ";
                resolved_input += remainder;
            }
        }
    }

    // substitutions (parameter, command, arithmetic) on raw command text.
    // expansion happens before field splitting and pathname expansion.
    g_expansion_error = false;
    const ULONGLONG expand_start = perf_trace ? GetTickCount64() : 0;
    std::wstring expanded = expand_substitutions_left_to_right(resolved_input);
    if (perf_trace) {
        log_performance_trace(L"execute.expand_substitutions", GetTickCount64() - expand_start);
    }
    if (g_expansion_error) {
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    std::wstring pipeline_source;
    std::wstring coprocess_source;
    if (strip_trailing_unquoted_coprocess_marker(expanded, coprocess_source)) {
        std::vector<std::wstring> coprocess_tokens = ksh_tokenize_preserve_quotes(coprocess_source);
        std::vector<std::wstring> staged_tokens;
        for (const std::wstring& raw_token : coprocess_tokens) {
            std::vector<std::wstring> split_tokens = split_fields_by_ifs(raw_token);
            staged_tokens.insert(
                staged_tokens.end(),
                std::make_move_iterator(split_tokens.begin()),
                std::make_move_iterator(split_tokens.end()));
        }
        staged_tokens = expand_tilde_for_tokens(staged_tokens);
        staged_tokens = expand_globs_for_tokens(staged_tokens, staged_tokens);
        staged_tokens = remove_quotes_and_escapes_from_tokens(staged_tokens);
        if (staged_tokens.empty()) {
            std::wcerr << L"ksh: |& requires a command\n";
            ksh_env.variables[L"?"] = L"2";
            return false;
        }
        staged_tokens.insert(staged_tokens.begin(), L"coproc");
        return execute_builtin_coproc(staged_tokens);
    }

    bool pipeline_background = strip_trailing_unquoted_background_marker(expanded, pipeline_source);

    // pipeline routing.
    // pipeline splitting occurs only for top-level unquoted '|'.
    const ULONGLONG pipeline_split_start = perf_trace ? GetTickCount64() : 0;
    std::vector<std::wstring> pipeline_segments = split_pipeline_segments(pipeline_source);
    if (perf_trace) {
        log_performance_trace(L"execute.pipeline_split", GetTickCount64() - pipeline_split_start, L"segments=" + std::to_wstring(pipeline_segments.size()));
    }
    if (pipeline_segments.size() > 1) {
        DWORD pipeline_exit = 1;
        HANDLE background_handle = nullptr;
        DWORD background_pid = 0;
        std::vector<HANDLE> background_handles;
        std::vector<DWORD> background_pids;
        std::vector<std::wstring> pipeline_temp_files;
        const ULONGLONG pipeline_exec_start = perf_trace ? GetTickCount64() : 0;
        if (!execute_pipeline_segments(
                pipeline_segments,
                pipeline_background,
                pipeline_exit,
                background_handle,
                background_pid,
                &background_handles,
                &background_pids,
                &pipeline_temp_files)) {
            if (perf_trace) {
                log_performance_trace(L"execute.pipeline_run", GetTickCount64() - pipeline_exec_start, L"failed");
            }
            if (!g_last_pipeline_error_detail.empty()) {
                std::wcerr << g_last_pipeline_error_detail << L"\n";
            } else {
                std::wcerr << L"ksh: pipeline execution failed\n";
            }
            ksh_env.variables[L"?"] = std::to_wstring(pipeline_exit);
            return false;
        }
        if (perf_trace) {
            log_performance_trace(L"execute.pipeline_run", GetTickCount64() - pipeline_exec_start, L"exit=" + std::to_wstring(pipeline_exit));
        }

        if (pipeline_background) {
            BackgroundJob job;
            job.id = g_next_job_id++;
            job.pid = background_pid;
            job.process_handle = background_handle;
            job.pids = background_pids;
            job.process_handles = background_handles;
            job.command = trim_copy(pipeline_source);
            job.completed = false;
            job.exit_code = STILL_ACTIVE;
            job.completion_reported = false;
            job.temp_file_paths = std::move(pipeline_temp_files);
            g_background_jobs.push_back(job);

            std::wcout << L"[" << job.id << L"] " << job.pid << L"\n";
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        ksh_env.variables[L"?"] = std::to_wstring(pipeline_exit);
        return pipeline_exit == 0;
    }

    // token staging.
    // field splitting -> glob expansion -> quote/escape removal.
    // Quoted text is preserved through split/glob phases and stripped only at the final step.
    const ULONGLONG token_stage_start = perf_trace ? GetTickCount64() : 0;
    std::vector<std::wstring> lexed_tokens = ksh_tokenize_preserve_quotes(expanded);
    std::vector<std::wstring> tokens;
    tokens.reserve(lexed_tokens.size());
    for (const std::wstring& raw_token : lexed_tokens) {
        std::vector<std::wstring> split_tokens = split_fields_by_ifs(raw_token);
        if (split_tokens.empty()) {
            continue;
        }
        tokens.insert(tokens.end(), std::make_move_iterator(split_tokens.begin()), std::make_move_iterator(split_tokens.end()));
    }
    tokens = expand_tilde_for_tokens(tokens);
    tokens = expand_globs_for_tokens(tokens, tokens);
    if (perf_trace) {
        log_performance_trace(L"execute.token_stage", GetTickCount64() - token_stage_start, L"tokens=" + std::to_wstring(tokens.size()));
    }
    if (tokens.empty()) {
        return true;
    }

    bool run_in_background = false;
    if (!tokens.empty() && tokens.back() == L"&") {
        run_in_background = true;
        tokens.pop_back();
        if (tokens.empty()) {
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
    }

    RedirectionSpec redir;
    std::wstring redirection_error;
    // redirection parse after token staging.
    // redirection operators are parsed from staged tokens and removed from argv tokens.
    const ULONGLONG redirection_parse_start = perf_trace ? GetTickCount64() : 0;
    if (!parse_redirections(tokens, redir, redirection_error)) {
        if (perf_trace) {
            log_performance_trace(L"execute.redirection_parse", GetTickCount64() - redirection_parse_start, L"failed");
        }
        std::wcerr << redirection_error << L"\n";
        ksh_env.variables[L"?"] = L"1";
        return false;
    }
    redir.stdin_path = remove_quotes_and_escapes_from_token(redir.stdin_path);
    redir.stdout_path = remove_quotes_and_escapes_from_token(redir.stdout_path);
    redir.stderr_path = remove_quotes_and_escapes_from_token(redir.stderr_path);
    for (RedirectionSpec::CustomFdAction& action : redir.custom_fd_actions) {
        action.path = remove_quotes_and_escapes_from_token(action.path);
    }
    tokens = remove_quotes_and_escapes_from_tokens(tokens);
    if (perf_trace) {
        log_performance_trace(L"execute.redirection_parse", GetTickCount64() - redirection_parse_start);
    }

    if (!redir.custom_fd_actions.empty() && (tokens.empty() || tokens[0] != L"exec")) {
        std::wcerr << L"ksh: custom fd redirection (3-9) is currently supported only with exec\n";
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    if (tokens.empty()) {
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    struct StdinRedirector {
        HANDLE hOld;
        HANDLE hNew;
        bool active;
        StdinRedirector(const RedirectionSpec& r) : active(false), hOld(INVALID_HANDLE_VALUE), hNew(INVALID_HANDLE_VALUE) {
            if (r.has_stdin) {
                hOld = GetStdHandle(STD_INPUT_HANDLE);
                if (open_redirection_file(r.stdin_path, GENERIC_READ, OPEN_EXISTING, hNew)) {
                    SetStdHandle(STD_INPUT_HANDLE, hNew);
                    active = true;
                }
            }
        }
        ~StdinRedirector() {
            if (active) {
                SetStdHandle(STD_INPUT_HANDLE, hOld);
                CloseHandle(hNew);
            }
        }
        bool is_valid(const RedirectionSpec& r) const {
            return !r.has_stdin || active;
        }
    };

    std::wstring check_cmd = tokens[0];
    const std::vector<std::wstring>& builtins_list = builtin_commands();
    bool is_builtin_cmd = (std::find(builtins_list.begin(), builtins_list.end(), check_cmd) != builtins_list.end());
    bool is_function_cmd = (g_shell_functions.find(check_cmd) != g_shell_functions.end());

    std::unique_ptr<StdinRedirector> stdin_redir;
    if (is_builtin_cmd || is_function_cmd) {
        stdin_redir = std::make_unique<StdinRedirector>(redir);
        if (!stdin_redir->is_valid(redir)) {
            std::wcerr << L"ksh: redirection setup failed\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
    }

    bool builtin_output_started = false;
    auto write_builtin_output = [&](const std::wstring& text) -> bool {
        if (g_builtin_capture_output != nullptr && !redir.has_stdout && !redir.stdout_to_stderr) {
            g_builtin_capture_output->append(text);
            return true;
        }

        if (!redir.has_stdout && !redir.stdout_to_stderr) {
            if (g_pipeline_stdout != INVALID_HANDLE_VALUE) {
                std::string utf8;
                int len = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), NULL, 0, NULL, NULL);
                if (len > 0) {
                    utf8.resize(len);
                    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), &utf8[0], len, NULL, NULL);
                }
                DWORD written = 0;
                WriteFile(g_pipeline_stdout, utf8.data(), (DWORD)utf8.size(), &written, NULL);
                return true;
            }
            if (g_subshell_stdout != INVALID_HANDLE_VALUE) {
                std::string utf8;
                int len = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), NULL, 0, NULL, NULL);
                if (len > 0) {
                    utf8.resize(len);
                    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), &utf8[0], len, NULL, NULL);
                }
                DWORD written = 0;
                WriteFile(g_subshell_stdout, utf8.data(), (DWORD)utf8.size(), &written, NULL);
                return true;
            }
            if (!_isatty(_fileno(stdout))) {
                std::string utf8;
                int len = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), NULL, 0, NULL, NULL);
                if (len > 0) {
                    utf8.resize(len);
                    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), &utf8[0], len, NULL, NULL);
                }
                _write(_fileno(stdout), utf8.data(), (unsigned int)utf8.size());
                return true;
            }
            std::wcout << text;
            return true;
        }

        RedirectionSpec output_redir = redir;
        if (builtin_output_started) {
            output_redir.append_stdout = true;
        }

        if (!write_text_with_stdout_redirection(text, output_redir)) {
            return false;
        }

        builtin_output_started = true;
        return true;
    };

    std::wstring command_to_run = join_tokens_as_command_line(tokens);
    if (g_xtrace_enabled) {
        std::wcerr << L"+ " << command_to_run << L"\n";
        std::wcerr.flush();
    }

    std::wstring cmd = tokens[0];
    std::wstring external_command_to_run = command_to_run;
    ScriptInterpreterResolution interpreter_resolution = build_script_interpreter_command(tokens, external_command_to_run);
    if (interpreter_resolution == ScriptInterpreterResolution::Failed) {
        std::wcerr << L"ksh: failed to resolve script interpreter\n";
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    if (g_custom_types.find(cmd) != g_custom_types.end()) {
        for (size_t i = 1; i < tokens.size(); ++i) {
            std::wstring token = tokens[i];
            std::wstring var_name = token;
            std::wstring assign_part;
            size_t eq = token.find(L'=');
            if (eq != std::wstring::npos) {
                var_name = token.substr(0, eq);
                assign_part = token.substr(eq + 1);
            }
            
            instantiate_custom_type(cmd, var_name);
            
            if (!assign_part.empty()) {
                if (assign_part.front() == L'(' && assign_part.back() == L')') {
                    std::wstring inner = assign_part.substr(1, assign_part.size() - 2);
                    std::vector<std::wstring> sub_tokens = ksh_tokenize_preserve_quotes(inner);
                    for (const auto& sub_t : sub_tokens) {
                        size_t sub_eq = sub_t.find(L'=');
                        if (sub_eq != std::wstring::npos) {
                            std::wstring member = sub_t.substr(0, sub_eq);
                            std::wstring val = sub_t.substr(sub_eq + 1);
                            std::wstring full_name = var_name + L"." + member;
                            ksh_env.variables[full_name] = val;
                            SetEnvironmentVariableW(full_name.c_str(), val.c_str());
                        }
                    }
                }
            }
        }
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    if (cmd == L"exit") {
        int status_code = 0;
        if (tokens.size() > 2) {
            std::wcerr << L"ksh: exit: too many arguments\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        if (tokens.size() == 2 && !try_parse_int_strict(tokens[1], status_code)) {
            std::wcerr << L"ksh: exit: numeric argument required\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        should_exit_shell = true;
        ksh_env.variables[L"?"] = std::to_wstring(status_code);
        return true;
    }

    if (cmd == L"logout") {
        int status_code = 0;
        if (tokens.size() > 2) {
            std::wcerr << L"ksh: logout: too many arguments\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        if (tokens.size() == 2) {
            if (!try_parse_int_strict(tokens[1], status_code)) {
                std::wcerr << L"ksh: logout: numeric argument required\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
        }

        should_exit_shell = true;
        ksh_env.variables[L"?"] = std::to_wstring(status_code);
        return true;
    }

    if (cmd == L"true") {
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    if (cmd == L"false") {
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    if (cmd == L"dirs") {
        bool clear_stack = false;
        bool vertical = false;
        bool one_per_line = false;
        bool long_format = false;
        int print_index = -1;
        bool index_from_left = true;
        bool has_index = false;

        for (size_t i = 1; i < tokens.size(); ++i) {
            const std::wstring& arg = tokens[i];
            if (arg == L"-c") {
                clear_stack = true;
            } else if (arg == L"-v") {
                vertical = true;
            } else if (arg == L"-p") {
                one_per_line = true;
            } else if (arg == L"-l") {
                long_format = true;
            } else if (arg.size() > 1 && (arg[0] == L'+' || arg[0] == L'-')) {
                int parsed = 0;
                if (try_parse_int_strict(arg.substr(1), parsed)) {
                    has_index = true;
                    index_from_left = (arg[0] == L'+');
                    print_index = parsed;
                } else if (arg[0] == L'-') {
                    for (size_t j = 1; j < arg.size(); ++j) {
                        if (arg[j] == L'c') clear_stack = true;
                        else if (arg[j] == L'v') vertical = true;
                        else if (arg[j] == L'p') one_per_line = true;
                        else if (arg[j] == L'l') long_format = true;
                        else {
                            std::wcerr << L"ksh: dirs: invalid option: -" << arg[j] << L"\n";
                            ksh_env.variables[L"?"] = L"1";
                            return false;
                        }
                    }
                } else {
                    std::wcerr << L"ksh: dirs: invalid index: " << arg << L"\n";
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
            } else {
                std::wcerr << L"ksh: dirs: invalid argument: " << arg << L"\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
        }

        if (clear_stack) {
            g_directory_stack.clear();
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        std::wstring current_directory;
        if (!get_current_directory_path(current_directory)) {
            std::wcerr << L"ksh: dirs: unable to determine current directory\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        std::vector<std::wstring> all_dirs;
        all_dirs.push_back(current_directory);
        for (size_t i = g_directory_stack.size(); i > 0; --i) {
            all_dirs.push_back(g_directory_stack[i - 1]);
        }

        std::wstring home = get_environment_value(L"HOME");
        if (home.empty()) home = get_environment_value(L"USERPROFILE");
        auto format_entry = [&](const std::wstring& dir) -> std::wstring {
            if (!long_format && !home.empty() && starts_with_case_insensitive(dir, home)) {
                return L"~" + dir.substr(home.size());
            }
            return dir;
        };

        if (has_index) {
            size_t target_idx = 0;
            if (index_from_left) {
                target_idx = static_cast<size_t>(print_index);
            } else {
                if (static_cast<size_t>(print_index) >= all_dirs.size()) {
                    target_idx = all_dirs.size();
                } else {
                    target_idx = all_dirs.size() - 1 - static_cast<size_t>(print_index);
                }
            }
            if (target_idx >= all_dirs.size()) {
                std::wcerr << L"ksh: dirs: directory stack index out of range\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            write_builtin_output(format_entry(all_dirs[target_idx]) + L"\n");
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        if (vertical) {
            std::wstring out;
            for (size_t i = 0; i < all_dirs.size(); ++i) {
                out += std::to_wstring(i) + L"\t" + format_entry(all_dirs[i]) + L"\n";
            }
            write_builtin_output(out);
        } else if (one_per_line) {
            std::wstring out;
            for (size_t i = 0; i < all_dirs.size(); ++i) {
                out += format_entry(all_dirs[i]) + L"\n";
            }
            write_builtin_output(out);
        } else {
            std::wstring out;
            for (size_t i = 0; i < all_dirs.size(); ++i) {
                if (i > 0) out += L" ";
                out += format_entry(all_dirs[i]);
            }
            out += L"\n";
            write_builtin_output(out);
        }

        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    if (cmd == L"pushd") {
        if (tokens.size() > 2) {
            std::wcerr << L"ksh: pushd: too many arguments\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        std::wstring current_directory;
        if (!get_current_directory_path(current_directory)) {
            std::wcerr << L"ksh: pushd: unable to determine current directory\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        std::wstring target_directory;
        bool use_stack_entry = false;
        if (tokens.size() == 1) {
            if (g_directory_stack.empty()) {
                std::wcerr << L"ksh: pushd: directory stack empty\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            target_directory = g_directory_stack.back();
            use_stack_entry = true;
        } else {
            target_directory = normalize_cd_path(remove_quotes_and_escapes_from_token(tokens[1]));
        }

        if (!SetCurrentDirectoryW(target_directory.c_str())) {
            std::wcerr << L"ksh: pushd failed: " << target_directory << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        if (use_stack_entry) {
            g_directory_stack.pop_back();
        }
        g_directory_stack.push_back(current_directory);
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    if (cmd == L"popd") {
        if (tokens.size() != 1) {
            std::wcerr << L"ksh: popd: too many arguments\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        if (g_directory_stack.empty()) {
            std::wcerr << L"ksh: popd: directory stack empty\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        std::wstring target_directory = g_directory_stack.back();
        if (!SetCurrentDirectoryW(target_directory.c_str())) {
            std::wcerr << L"ksh: popd failed: " << target_directory << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        g_directory_stack.pop_back();
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    if (cmd == L"builtin") {
        if (tokens.size() == 1) {
            const std::vector<std::wstring>& builtins = builtin_commands();
            for (const std::wstring& builtin_name : builtins) {
                if (!write_builtin_output(builtin_name + L"\n")) {
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        size_t start = 1;
        if (tokens[1] == L"--") {
            start = 2;
        }

        if (tokens.size() <= start) {
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        const std::wstring builtin_name = tokens[start];
        std::wstring detail;
        if (resolve_command_kind(builtin_name, detail) != CommandResolutionKind::Builtin) {
            std::wcerr << L"ksh: builtin: " << builtin_name << L": not a shell builtin\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        std::vector<std::wstring> builtin_tokens(tokens.begin() + static_cast<std::ptrdiff_t>(start), tokens.end());
        std::wstring builtin_line = join_tokens_with_spaces(builtin_tokens);
        return execute_command_line(builtin_line, should_exit_shell, true, true);
    }

    if (cmd == L"read") {
        return execute_builtin_read(tokens);
    }

    if (cmd == L"help") {
        return execute_builtin_help(tokens, write_builtin_output);
    }

    if (cmd == L"version") {
        print_version_text(write_builtin_output);
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    if (cmd == L"clear") {
        if (tokens.size() != 1) {
            std::wcerr << L"ksh: clear: too many arguments\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        if (!clear_console_screen()) {
            std::wcerr << L"ksh: clear: unable to clear console\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    if (cmd == L"return") {
        if (!is_executing_function_scope()) {
            std::wcerr << L"ksh: return: not in a function\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        int status_code = 0;
        if (tokens.size() == 1) {
            std::map<std::wstring, std::wstring>::const_iterator status_it = ksh_env.variables.find(L"?");
            if (status_it != ksh_env.variables.end()) {
                if (!try_parse_int_strict(status_it->second, status_code)) {
                    status_code = 1;
                }
            }
        } else if (tokens.size() == 2) {
            if (!try_parse_int_strict(tokens[1], status_code)) {
                std::wcerr << L"ksh: return: numeric argument required\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
        } else {
            std::wcerr << L"ksh: return: too many arguments\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        request_function_return(status_code);
        ksh_env.variables[L"?"] = std::to_wstring(status_code);
        return true;
    }

    if (cmd == L"hash") {
        if (tokens.size() == 1) {
            if (g_command_hash_table.empty()) {
                ksh_env.variables[L"?"] = L"0";
                return true;
            }
            std::wstring out = L"hits\tcommand\n";
            for (const auto& [name, entry] : g_command_hash_table) {
                out += std::to_wstring(entry.hits) + L"\t" + entry.path + L"\n";
            }
            write_builtin_output(out);
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        bool verbose = false;
        bool refresh_only = false;
        bool delete_mode = false;
        bool print_path_only = false;
        size_t start = 1;

        while (start < tokens.size()) {
            if (tokens[start] == L"-r") {
                refresh_only = true;
                start++;
                continue;
            }
            if (tokens[start] == L"-v") {
                verbose = true;
                start++;
                continue;
            }
            if (tokens[start] == L"-d") {
                delete_mode = true;
                start++;
                continue;
            }
            if (tokens[start] == L"-t") {
                print_path_only = true;
                start++;
                continue;
            }
            if (tokens[start] == L"--") {
                start++;
                break;
            }
            break;
        }

        if (refresh_only) {
            g_command_hash_table.clear();
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        if (delete_mode) {
            bool ok = true;
            for (size_t i = start; i < tokens.size(); ++i) {
                if (g_command_hash_table.erase(tokens[i]) == 0) {
                    std::wcerr << L"ksh: hash: " << tokens[i] << L": not found\n";
                    ok = false;
                }
            }
            ksh_env.variables[L"?"] = ok ? L"0" : L"1";
            return ok;
        }

        if (print_path_only) {
            bool ok = true;
            for (size_t i = start; i < tokens.size(); ++i) {
                auto it = g_command_hash_table.find(tokens[i]);
                if (it != g_command_hash_table.end()) {
                    write_builtin_output(it->second.path + L"\n");
                } else {
                    std::wstring path;
                    if (resolve_external_command_path(tokens[i], path)) {
                        write_builtin_output(path + L"\n");
                    } else {
                        std::wcerr << L"ksh: hash: " << tokens[i] << L": not found\n";
                        ok = false;
                    }
                }
            }
            ksh_env.variables[L"?"] = ok ? L"0" : L"1";
            return ok;
        }

        if (tokens.size() <= start) {
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        bool ok = true;
        for (size_t i = start; i < tokens.size(); ++i) {
            std::wstring path;
            if (resolve_external_command_path(tokens[i], path)) {
                if (verbose) {
                    write_builtin_output(tokens[i] + L"=" + path + L"\n");
                }
            } else {
                std::wcerr << L"ksh: hash: " << tokens[i] << L": not found\n";
                ok = false;
            }
        }

        ksh_env.variables[L"?"] = ok ? L"0" : L"1";
        return ok;
    }

    if (cmd == L"source" || cmd == L".") {
        if (tokens.size() < 2) {
            std::wcerr << L"ksh: source requires a file path\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        std::vector<std::wstring> source_args;
        for (size_t i = 2; i < tokens.size(); ++i) {
            source_args.push_back(tokens[i]);
        }
        return execute_script_file(tokens[1], source_args, should_exit_shell);
    }

    if (cmd == L"set") {
        if (tokens.size() >= 2 && tokens[1] == L"-A") {
            if (tokens.size() < 3) {
                std::wcerr << L"ksh: set: usage: set -A name [value ...]\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            std::wstring array_name = resolve_variable_name(tokens[2]);
            if (!is_valid_shell_identifier(array_name) || is_internal_shell_state_variable(array_name)) {
                std::wcerr << L"ksh: set: invalid array name: " << tokens[2] << L"\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            if (get_flag_value(ksh_env.readonly_flags, array_name)) {
                std::wcerr << L"ksh: set: variable is read-only: " << array_name << L"\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            std::vector<std::wstring> values;
            for (size_t i = 3; i < tokens.size(); ++i) {
                values.push_back(tokens[i]);
            }

            set_flag_value(ksh_env.associative_flags, array_name, false);
            assign_array_parameter(array_name, values);
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        if (tokens.size() == 1) {
            for (std::map<std::wstring, std::wstring>::const_iterator it = ksh_env.variables.begin(); it != ksh_env.variables.end(); ++it) {
                if (is_internal_shell_state_variable(it->first)) {
                    continue;
                }
                if (!write_builtin_output(it->first + L"=" + it->second + L"\n")) {
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
            }
            for (std::map<std::wstring, std::map<std::wstring, std::wstring>>::const_iterator arr_it = ksh_env.arrays.begin(); arr_it != ksh_env.arrays.end(); ++arr_it) {
                std::wstring line = arr_it->first + L"=(";
                bool is_assoc = get_flag_value(ksh_env.associative_flags, arr_it->first);
                if (is_assoc) {
                    size_t idx = 0;
                    for (const auto& pair : arr_it->second) {
                        line += L"[" + pair.first + L"]=" + quote_command_argument(pair.second);
                        if (idx + 1 < arr_it->second.size()) {
                            line += L" ";
                        }
                        idx++;
                    }
                } else {
                    std::vector<std::wstring> values = get_array_values_vector(arr_it->first);
                    for (size_t i = 0; i < values.size(); ++i) {
                        line += quote_command_argument(values[i]);
                        if (i + 1 < values.size()) {
                            line += L" ";
                        }
                    }
                }
                line += L")\n";
                if (!write_builtin_output(line)) {
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        bool ok = true;
        bool end_of_options = false;
        std::vector<std::wstring> new_positionals;
        bool set_positionals = false;

        for (size_t i = 1; i < tokens.size(); ++i) {
            std::wstring arg = tokens[i];

            // "set --" ends option processing; everything after becomes $1 $2 ...
            if (!end_of_options && arg == L"--") {
                end_of_options = true;
                set_positionals = true;
                for (size_t j = i + 1; j < tokens.size(); ++j) {
                    new_positionals.push_back(tokens[j]);
                }
                break;
            }

            // "set -" alone: turn off xtrace and replace positionals with remaining
            if (!end_of_options && arg == L"-") {
                g_xtrace_enabled = false;
                set_positionals = true;
                for (size_t j = i + 1; j < tokens.size(); ++j) {
                    new_positionals.push_back(tokens[j]);
                }
                break;
            }

            if (!end_of_options && arg.size() > 1 && (arg[0] == L'-' || arg[0] == L'+')) {
                bool enable = (arg[0] == L'-');
                for (size_t j = 1; j < arg.size(); ++j) {
                    wchar_t flag = arg[j];
                    if (flag == L'x') {
                        g_xtrace_enabled = enable;
                    } else if (flag == L'e') {
                        g_errexit_enabled = enable;
                    } else if (flag == L'u') {
                        g_nounset_enabled = enable;
                    } else if (flag == L'o') {
                        std::wstring opt_name;
                        if (j + 1 < arg.size()) {
                            opt_name = arg.substr(j + 1);
                            j = arg.size();
                        } else if (i + 1 < tokens.size()) {
                            opt_name = tokens[++i];
                        } else {
                            std::wcerr << L"ksh: set: " << arg << L" requires option name\n";
                            ok = false;
                            break;
                        }

                        if (opt_name == L"xtrace") {
                            g_xtrace_enabled = enable;
                        } else if (opt_name == L"errexit") {
                            g_errexit_enabled = enable;
                        } else if (opt_name == L"nounset") {
                            g_nounset_enabled = enable;
                        } else if (opt_name == L"vi") {
                            g_vi_mode_enabled = enable;
                        } else if (opt_name == L"pipefail") {
                            g_pipefail_enabled = enable;
                        } else if (opt_name == L"powershellbypass") {
                            g_powershell_bypass_enabled = enable;
                        } else {
                            std::wcerr << L"ksh: set: unsupported option: " << opt_name << L"\n";
                            ok = false;
                        }
                    } else {
                        std::wcerr << L"ksh: set: unsupported flag: " << flag << L"\n";
                        ok = false;
                    }
                }
            } else {
                // Bare word — treat as start of positional parameter list
                set_positionals = true;
                new_positionals.push_back(arg);
                for (size_t j = i + 1; j < tokens.size(); ++j) {
                    new_positionals.push_back(tokens[j]);
                }
                break;
            }
        }

        // Apply new positional parameters if requested
        if (set_positionals) {
            if (!g_script_context_stack.empty()) {
                g_script_context_stack.back().args = new_positionals;
            } else {
                // At top level with no script context, push one
                ScriptContext ctx;
                ctx.script_name = L"ksh";
                ctx.args = new_positionals;
                g_script_context_stack.push_back(ctx);
            }
            // Sync $#, $*, $@, and individual $1..$N into the variable table
            ksh_env.variables[L"#"] = std::to_wstring(new_positionals.size());
            std::wstring star;
            for (size_t k = 0; k < new_positionals.size(); ++k) {
                if (k > 0) star += L" ";
                star += new_positionals[k];
            }
            ksh_env.variables[L"*"] = star;
            ksh_env.variables[L"@"] = star;
            for (size_t k = 0; k < new_positionals.size(); ++k) {
                ksh_env.variables[std::to_wstring(k + 1)] = new_positionals[k];
            }
        }

        ksh_env.variables[L"?"] = ok ? L"0" : L"1";
        return ok;
    }

    if (cmd == L"unset") {
        bool unset_functions = false;
        size_t start = 1;
        if (tokens.size() > 1 && tokens[1] == L"-f") {
            unset_functions = true;
            start = 2;
        }

        if (tokens.size() <= start) {
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        bool ok = true;
        for (size_t i = start; i < tokens.size(); ++i) {
            if (unset_functions) {
                g_shell_functions.erase(tokens[i]);
                continue;
            }

            std::wstring base_name;
            std::wstring index_text;
            bool has_index = false;
            if (!parse_array_reference_expression(tokens[i], base_name, index_text, has_index)) {
                std::wcerr << L"ksh: unset: invalid name: " << tokens[i] << L"\n";
                ok = false;
                continue;
            }

            if (has_index) {
                std::wstring resolved = resolve_variable_name(tokens[i]);
                if (!parse_array_reference_expression(resolved, base_name, index_text, has_index)) {
                    std::wcerr << L"ksh: unset: invalid name: " << tokens[i] << L"\n";
                    ok = false;
                    continue;
                }
            }

            if (is_internal_shell_state_variable(base_name)) {
                std::wcerr << L"ksh: unset: cannot unset internal shell state: " << base_name << L"\n";
                ok = false;
                continue;
            }

            if (get_flag_value(ksh_env.readonly_flags, base_name)) {
                std::wcerr << L"ksh: unset: " << base_name << L": is read only\n";
                ok = false;
                continue;
            }

            if (has_index) {
                bool is_assoc = get_flag_value(ksh_env.associative_flags, base_name);
                if (is_assoc) {
                    std::map<std::wstring, std::map<std::wstring, std::wstring>>::iterator arr_it = ksh_env.arrays.find(base_name);
                    if (arr_it != ksh_env.arrays.end()) {
                        arr_it->second.erase(index_text);
                    }
                } else {
                    size_t index = 0;
                    if (!parse_non_negative_index(index_text, index)) {
                        std::wcerr << L"ksh: unset: array index must be numeric: " << tokens[i] << L"\n";
                        ok = false;
                        continue;
                    }

                    std::map<std::wstring, std::map<std::wstring, std::wstring>>::iterator arr_it = ksh_env.arrays.find(base_name);
                    if (arr_it != ksh_env.arrays.end()) {
                        arr_it->second.erase(std::to_wstring(index));
                    }
                }
                continue;
            }

            ksh_env.variables.erase(base_name);
            ksh_env.arrays.erase(base_name);
            ksh_env.associative_flags.erase(base_name);
            ksh_env.nameref_flags.erase(base_name);
            ksh_env.exported.erase(base_name);
            ksh_env.readonly_flags.erase(base_name);
            ksh_env.integer_flags.erase(base_name);
            ksh_env.uppercase_flags.erase(base_name);
            ksh_env.lowercase_flags.erase(base_name);
            SetEnvironmentVariableW(base_name.c_str(), nullptr);
        }

        ksh_env.variables[L"?"] = ok ? L"0" : L"1";
        return ok;
    }

    if (cmd == L"let") {
        if (tokens.size() < 2) {
            std::wcerr << L"ksh: let: requires an expression\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        std::wstring expression = join_tokens_with_spaces(tokens, 1);
        std::wstring value = evaluate_arithmetic(expression);
        bool nonzero = false;
        try {
            nonzero = std::stod(value) != 0.0;
        } catch (...) {
            nonzero = false;
        }

        ksh_env.variables[L"?"] = nonzero ? L"0" : L"1";
        return true;
    }

    if (cmd == L"export") {
        if (tokens.size() == 1 || (tokens.size() == 2 && tokens[1] == L"-p")) {
            for (std::map<std::wstring, bool>::const_iterator it = ksh_env.exported.begin(); it != ksh_env.exported.end(); ++it) {
                if (!it->second) {
                    continue;
                }
                std::wstring value;
                std::map<std::wstring, std::wstring>::const_iterator scalar_it = ksh_env.variables.find(it->first);
                if (scalar_it != ksh_env.variables.end()) {
                    value = scalar_it->second;
                }
                if (!write_builtin_output(L"export " + it->first + L"=" + quote_command_argument(value) + L"\n")) {
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        bool ok = true;
        for (size_t i = 1; i < tokens.size(); ++i) {
            const std::wstring& token = tokens[i];
            size_t eq = token.find(L'=');
            std::wstring name = token;
            if (eq != std::wstring::npos) {
                name = token.substr(0, eq);
                std::wstring rhs = token.substr(eq + 1);
                std::wstring assignment_error;
                if (!assign_parameter_value(name, rhs, false, false, assignment_error)) {
                    std::wcerr << L"ksh: export: " << assignment_error << L"\n";
                    ok = false;
                    continue;
                }
            }

            if (!is_valid_shell_identifier(name)) {
                std::wcerr << L"ksh: export: invalid name: " << name << L"\n";
                ok = false;
                continue;
            }

            if (is_internal_shell_state_variable(name)) {
                std::wcerr << L"ksh: export: cannot export internal shell state: " << name << L"\n";
                ok = false;
                continue;
            }

            set_flag_value(ksh_env.exported, name, true);
            sync_exported_environment_variable(name);
        }

        ksh_env.variables[L"?"] = ok ? L"0" : L"1";
        return ok;
    }

    if (cmd == L"disown") {
        update_background_jobs(false);

        if (tokens.size() == 1) {
            BackgroundJob* default_job = find_default_background_job_for_resume_or_foreground();
            if (default_job == nullptr) {
                std::wcerr << L"ksh: disown: no current job\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            remove_background_job(default_job->id);
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        if (tokens[1] == L"-a") {
            cleanup_all_background_jobs();
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        bool ok = true;
        for (size_t i = 1; i < tokens.size(); ++i) {
            int job_id = 0;
            if (!resolve_job_reference(tokens[i], job_id)) {
                std::wcerr << L"ksh: disown: invalid job id\n";
                ok = false;
                continue;
            }

            if (find_background_job(job_id) == nullptr) {
                std::wcerr << L"ksh: disown: job not found: " << job_id << L"\n";
                ok = false;
                continue;
            }

            remove_background_job(job_id);
        }

        ksh_env.variables[L"?"] = ok ? L"0" : L"1";
        return ok;
    }

    if (cmd == L"readonly") {
        if (tokens.size() == 1 || (tokens.size() == 2 && tokens[1] == L"-p")) {
            for (std::map<std::wstring, bool>::const_iterator it = ksh_env.readonly_flags.begin(); it != ksh_env.readonly_flags.end(); ++it) {
                if (!it->second) {
                    continue;
                }

                std::wstring value;
                std::map<std::wstring, std::wstring>::const_iterator scalar_it = ksh_env.variables.find(it->first);
                if (scalar_it != ksh_env.variables.end()) {
                    value = scalar_it->second;
                }

                if (!write_builtin_output(L"readonly " + it->first + L"=" + quote_command_argument(value) + L"\n")) {
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        bool ok = true;
        for (size_t i = 1; i < tokens.size(); ++i) {
            const std::wstring& token = tokens[i];
            size_t eq = token.find(L'=');
            std::wstring name = token;
            if (eq != std::wstring::npos) {
                name = token.substr(0, eq);
                std::wstring rhs = token.substr(eq + 1);
                std::wstring assignment_error;
                if (!assign_parameter_value(name, rhs, false, false, assignment_error)) {
                    std::wcerr << L"ksh: readonly: " << assignment_error << L"\n";
                    ok = false;
                    continue;
                }
            }

            if (!is_valid_shell_identifier(name)) {
                std::wcerr << L"ksh: readonly: invalid name: " << name << L"\n";
                ok = false;
                continue;
            }

            if (is_internal_shell_state_variable(name)) {
                std::wcerr << L"ksh: readonly: cannot mark internal shell state readonly: " << name << L"\n";
                ok = false;
                continue;
            }

            set_flag_value(ksh_env.readonly_flags, name, true);
        }

        ksh_env.variables[L"?"] = ok ? L"0" : L"1";
        return ok;
    }

    if (cmd == L"alias") {
        if (tokens.size() == 1) {
            for (std::map<std::wstring, std::wstring>::const_iterator it = g_aliases.begin(); it != g_aliases.end(); ++it) {
                if (!write_builtin_output(L"alias " + it->first + L"=" + quote_for_single_quoted_shell_literal(it->second) + L"\n")) {
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        bool ok = true;
        for (size_t i = 1; i < tokens.size(); ++i) {
            const std::wstring& token = tokens[i];
            size_t eq = token.find(L'=');
            if (eq == std::wstring::npos) {
                std::map<std::wstring, std::wstring>::const_iterator it = g_aliases.find(token);
                if (it == g_aliases.end()) {
                    std::wcerr << L"ksh: alias: not found: " << token << L"\n";
                    ok = false;
                    continue;
                }
                if (!write_builtin_output(L"alias " + it->first + L"=" + quote_for_single_quoted_shell_literal(it->second) + L"\n")) {
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
                continue;
            }

            std::wstring name = token.substr(0, eq);
            std::wstring value = token.substr(eq + 1);
            if (!is_valid_shell_identifier(name)) {
                std::wcerr << L"ksh: alias: invalid name: " << name << L"\n";
                ok = false;
                continue;
            }
            // Enforce per-session alias table cap (new entries only).
            if (g_aliases.find(name) == g_aliases.end() && g_aliases.size() >= kMaxShellAliases) {
                std::wcerr << L"ksh: alias table limit reached, cannot add: " << name << L"\n";
                ok = false;
                continue;
            }
            g_aliases[name] = value;
        }

        ksh_env.variables[L"?"] = ok ? L"0" : L"1";
        return ok;
    }

    if (cmd == L"unalias") {
        if (tokens.size() < 2) {
            std::wcerr << L"ksh: unalias requires an alias name or -a\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        if (tokens[1] == L"-a") {
            g_aliases.clear();
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        bool ok = true;
        for (size_t i = 1; i < tokens.size(); ++i) {
            if (g_aliases.erase(tokens[i]) == 0) {
                std::wcerr << L"ksh: unalias: not found: " << tokens[i] << L"\n";
                ok = false;
            }
        }
        ksh_env.variables[L"?"] = ok ? L"0" : L"1";
        return ok;
    }

    if (cmd == L"type" || cmd == L"whence" || cmd == L"command") {
        bool as_type = (cmd == L"type");
        bool as_whence = (cmd == L"whence");

        if (cmd == L"command" && tokens.size() > 1 && (tokens[1] == L"-v" || tokens[1] == L"-V")) {
            bool verbose = (tokens[1] == L"-V");
            if (tokens.size() < 3) {
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            bool all_found = true;
            for (size_t i = 2; i < tokens.size(); ++i) {
                std::wstring out = command_kind_description(tokens[i], verbose, false);
                if (out.empty() || out == L"not found" || out.find(L"\tnot found") != std::wstring::npos) {
                    all_found = false;
                    continue;
                }
                if (!write_builtin_output(out + L"\n")) {
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
            }

            ksh_env.variables[L"?"] = all_found ? L"0" : L"1";
            return all_found;
        }

        if (as_type || as_whence) {
            bool verbose = as_type;
            bool path_only = false;
            bool no_functions = false;
            bool all_matches = false;
            size_t start_index = 1;

            while (start_index < tokens.size()) {
                const std::wstring& arg = tokens[start_index];
                if (arg == L"--") {
                    start_index++;
                    break;
                }
                if (arg.size() > 1 && arg[0] == L'-') {
                    bool valid_flag_token = true;
                    for (size_t c = 1; c < arg.size(); ++c) {
                        if (arg[c] == L'v') {
                            verbose = true;
                        } else if (arg[c] == L'p') {
                            path_only = true;
                        } else if (arg[c] == L'f') {
                            no_functions = true;
                        } else if (arg[c] == L'a') {
                            all_matches = true;
                        } else {
                            valid_flag_token = false;
                            break;
                        }
                    }
                    if (valid_flag_token) {
                        start_index++;
                        continue;
                    }
                }
                break;
            }

            if (start_index >= tokens.size()) {
                std::wcerr << L"ksh: " << cmd << L": requires one or more command names\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            (void)all_matches;
            bool all_found = true;
            for (size_t i = start_index; i < tokens.size(); ++i) {
                const std::wstring& target = tokens[i];
                std::wstring detail;
                CommandResolutionKind kind = resolve_command_kind(target, detail);

                if (path_only && (kind == CommandResolutionKind::Alias || kind == CommandResolutionKind::Function || kind == CommandResolutionKind::Builtin)) {
                    std::wstring ext_path;
                    if (resolve_external_command_path(target, ext_path)) {
                        kind = CommandResolutionKind::External;
                        detail = ext_path;
                    } else {
                        kind = CommandResolutionKind::Missing;
                    }
                }
                if (no_functions && kind == CommandResolutionKind::Function) {
                    std::wstring ext_path;
                    if (resolve_external_command_path(target, ext_path)) {
                        kind = CommandResolutionKind::External;
                        detail = ext_path;
                    } else {
                        kind = CommandResolutionKind::Missing;
                    }
                }

                if (kind == CommandResolutionKind::Missing) {
                    all_found = false;
                    std::wcerr << L"ksh: " << cmd << L": " << target << L": not found\n";
                    continue;
                }

                std::wstring line;
                if (verbose) {
                    if (kind == CommandResolutionKind::Alias) {
                        line = target + L" is an alias for " + detail;
                    } else if (kind == CommandResolutionKind::Function) {
                        line = target + L" is a function";
                    } else if (kind == CommandResolutionKind::Builtin) {
                        line = target + L" is a shell builtin";
                    } else if (kind == CommandResolutionKind::External) {
                        line = target + L" is " + detail;
                    }
                } else {
                    if (kind == CommandResolutionKind::Alias) {
                        line = detail;
                    } else if (kind == CommandResolutionKind::Function || kind == CommandResolutionKind::Builtin) {
                        line = target;
                    } else if (kind == CommandResolutionKind::External) {
                        line = detail;
                    }
                }

                if (!write_builtin_output(line + L"\n")) {
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
            }

            ksh_env.variables[L"?"] = all_found ? L"0" : L"1";
            return all_found;
        }

        if (cmd == L"command") {
            size_t start = 1;
            if (tokens.size() > 1 && tokens[1] == L"--") {
                start = 2;
            }

            if (tokens.size() <= start) {
                ksh_env.variables[L"?"] = L"0";
                return true;
            }

            std::vector<std::wstring> command_tokens(tokens.begin() + static_cast<std::ptrdiff_t>(start), tokens.end());
            const std::wstring target_name = command_tokens.front();

            std::map<std::wstring, ShellFunctionDefinition>::iterator fn_it = g_shell_functions.find(target_name);
            if (fn_it == g_shell_functions.end()) {
                std::wstring eval_line = join_tokens_with_spaces(command_tokens);
                return execute_command_line(eval_line, should_exit_shell);
            }

            ShellFunctionDefinition saved_function = fn_it->second;
            g_shell_functions.erase(fn_it);

            std::wstring eval_line = join_tokens_with_spaces(command_tokens);
            bool invoke_ok = execute_command_line(eval_line, should_exit_shell);

            g_shell_functions[target_name] = saved_function;
            return invoke_ok;
        }
    }

    if (cmd == L"eval") {
        if (tokens.size() < 2) {
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        ScopedEvalDepth eval_depth;
        if (!eval_depth.active()) {
            std::wcerr << L"ksh: eval: maximum recursion depth exceeded\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        std::wstring eval_line = join_tokens_with_spaces(tokens, 1);
        return execute_command_line(eval_line, should_exit_shell);
    }

    if (cmd == L"getopts") {
        if (tokens.size() < 3) {
            std::wcerr << L"ksh: getopts: usage: getopts optstring name [arg ...]\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        const std::wstring optstring = tokens[1];
        const std::wstring name = tokens[2];
        if (!is_valid_shell_identifier(name)) {
            std::wcerr << L"ksh: getopts: invalid variable name: " << name << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        std::vector<std::wstring> args;
        if (tokens.size() > 3) {
            args.assign(tokens.begin() + 3, tokens.end());
        } else {
            args = current_script_args();
        }

        int optind = 1;
        std::map<std::wstring, std::wstring>::const_iterator optind_it = ksh_env.variables.find(L"OPTIND");
        if (optind_it != ksh_env.variables.end()) {
            int parsed = 0;
            if (parse_positive_int(optind_it->second, parsed)) {
                optind = parsed;
            }
        }

        int cursor = 1;
        std::map<std::wstring, std::wstring>::const_iterator mirror_it = ksh_env.variables.find(kGetoptsOptindMirrorVar);
        if (mirror_it != ksh_env.variables.end() && mirror_it->second == std::to_wstring(optind)) {
            std::map<std::wstring, std::wstring>::const_iterator cursor_it = ksh_env.variables.find(kGetoptsCursorVar);
            if (cursor_it != ksh_env.variables.end()) {
                int parsed_cursor = 0;
                if (parse_positive_int(cursor_it->second, parsed_cursor)) {
                    cursor = parsed_cursor;
                }
            }
        }

        auto finish_no_more_options = [&]() {
            ksh_env.variables[name] = L"?";
            ksh_env.variables[L"OPTIND"] = std::to_wstring(optind);
            ksh_env.variables[kGetoptsCursorVar] = L"1";
            ksh_env.variables[kGetoptsOptindMirrorVar] = std::to_wstring(optind);
            ksh_env.variables[L"OPTARG"] = L"";
            ksh_env.variables[L"?"] = L"1";
            return true;
        };

        while (optind >= 1 && static_cast<size_t>(optind) <= args.size()) {
            const std::wstring& arg = args[static_cast<size_t>(optind - 1)];
            if (arg == L"--" && cursor <= 1) {
                optind++;
                return finish_no_more_options();
            }

            if (arg.size() < 2 || arg[0] != L'-') {
                return finish_no_more_options();
            }

            if (cursor >= static_cast<int>(arg.size())) {
                optind++;
                cursor = 1;
                continue;
            }

            wchar_t optchar = arg[static_cast<size_t>(cursor)];
            cursor++;

            size_t pos = optstring.find(optchar);
            bool silent = !optstring.empty() && optstring[0] == L':';
            if (pos == std::wstring::npos || optchar == L':') {
                ksh_env.variables[name] = L"?";
                ksh_env.variables[L"OPTARG"] = std::wstring(1, optchar);
                if (!silent) {
                    std::wcerr << L"ksh: getopts: illegal option -- " << optchar << L"\n";
                }
                if (cursor >= static_cast<int>(arg.size())) {
                    optind++;
                    cursor = 1;
                }
                ksh_env.variables[L"OPTIND"] = std::to_wstring(optind);
                ksh_env.variables[kGetoptsCursorVar] = std::to_wstring(cursor);
                ksh_env.variables[kGetoptsOptindMirrorVar] = std::to_wstring(optind);
                ksh_env.variables[L"?"] = L"0";
                return true;
            }

            bool requires_argument = (pos + 1 < optstring.size() && optstring[pos + 1] == L':');
            if (requires_argument) {
                std::wstring optarg;
                if (cursor < static_cast<int>(arg.size())) {
                    optarg = arg.substr(static_cast<size_t>(cursor));
                    optind++;
                    cursor = 1;
                } else if (static_cast<size_t>(optind) < args.size()) {
                    optarg = args[static_cast<size_t>(optind)];
                    optind += 2;
                    cursor = 1;
                } else {
                    if (silent) {
                        ksh_env.variables[name] = L":";
                        ksh_env.variables[L"OPTARG"] = std::wstring(1, optchar);
                    } else {
                        ksh_env.variables[name] = L"?";
                        ksh_env.variables[L"OPTARG"] = L"";
                        std::wcerr << L"ksh: getopts: option requires an argument -- " << optchar << L"\n";
                    }
                    ksh_env.variables[L"OPTIND"] = std::to_wstring(optind);
                    ksh_env.variables[kGetoptsCursorVar] = L"1";
                    ksh_env.variables[kGetoptsOptindMirrorVar] = std::to_wstring(optind);
                    ksh_env.variables[L"?"] = L"0";
                    return true;
                }

                ksh_env.variables[name] = std::wstring(1, optchar);
                ksh_env.variables[L"OPTARG"] = optarg;
            } else {
                ksh_env.variables[name] = std::wstring(1, optchar);
                ksh_env.variables[L"OPTARG"] = L"";
                if (cursor >= static_cast<int>(arg.size())) {
                    optind++;
                    cursor = 1;
                }
            }

            ksh_env.variables[L"OPTIND"] = std::to_wstring(optind);
            ksh_env.variables[kGetoptsCursorVar] = std::to_wstring(cursor);
            ksh_env.variables[kGetoptsOptindMirrorVar] = std::to_wstring(optind);
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        return finish_no_more_options();
    }

    if (is_ksh_script_path(cmd)) {
        std::vector<std::wstring> script_args;
        for (size_t i = 1; i < tokens.size(); ++i) {
            script_args.push_back(tokens[i]);
        }

        if (!run_in_background) {
            return execute_script_file(cmd, script_args, should_exit_shell);
        }

        std::wstring script_command;
        if (!build_self_script_command(cmd, script_args, script_command)) {
            std::wcerr << L"ksh: failed to resolve shell executable path\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        HANDLE process_handle = nullptr;
        DWORD pid = 0;
        if (!launch_process(script_command, process_handle, pid)) {
            std::wcerr << L"ksh: failed to start background script\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        BackgroundJob job;
        job.id = g_next_job_id++;
        job.pid = pid;
        job.process_handle = process_handle;
        job.pids.push_back(pid);
        job.process_handles.push_back(process_handle);
        job.command = script_command;
        job.completed = false;
        job.exit_code = STILL_ACTIVE;
        job.completion_reported = false;
        g_background_jobs.push_back(job);

        std::wcout << L"[" << job.id << L"] " << job.pid << L"\n";
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    if (cmd == L"history") {
        for (size_t i = 0; i < g_command_history.size(); ++i) {
            std::wstring line = std::to_wstring(i + 1) + L"  ";
            if (history_timestamps_enabled() && !g_command_history[i].timestamp.empty()) {
                line += L"[" + g_command_history[i].timestamp + L"] ";
            }
            line += g_command_history[i].command + L"\n";
            if (!write_builtin_output(line)) {
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
        }
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    if (cmd == L"jobs") {
        bool long_format = false;
        bool pid_only = false;
        bool end_of_options = false;
        std::vector<int> requested_job_ids;

        for (size_t i = 1; i < tokens.size(); ++i) {
            const std::wstring& token = tokens[i];
            if (!end_of_options && token == L"--") {
                end_of_options = true;
                continue;
            }

            if (!end_of_options && token.size() > 1 && token[0] == L'-') {
                for (size_t flag_index = 1; flag_index < token.size(); ++flag_index) {
                    if (token[flag_index] == L'l') {
                        long_format = true;
                    } else if (token[flag_index] == L'p') {
                        pid_only = true;
                    } else {
                        std::wcerr << L"ksh: jobs: invalid option: " << token << L"\n";
                        ksh_env.variables[L"?"] = L"1";
                        return false;
                    }
                }
                continue;
            }

            int job_id = 0;
            if (!resolve_job_reference(token, job_id)) {
                std::wcerr << L"ksh: jobs: invalid job id: " << token << L"\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            requested_job_ids.push_back(job_id);
        }

        update_background_jobs(false);
        for (const BackgroundJob& job : g_background_jobs) {
            if (!requested_job_ids.empty() &&
                std::find(requested_job_ids.begin(), requested_job_ids.end(), job.id) == requested_job_ids.end()) {
                continue;
            }

            std::wstring line;
            if (pid_only) {
                line = std::to_wstring(job.pid);
            } else {
                line = format_background_job_line(job);
                if (long_format && job.completed) {
                    line = L"[" + std::to_wstring(job.id) + L"] Done (pid=" +
                        std::to_wstring(job.pid) + L", exit=" + std::to_wstring(job.exit_code) + L") " + job.command;
                }
            }
            line += L"\n";
            if (!write_builtin_output(line)) {
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
        }
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    if (cmd == L"fg") {
        int job_id = 0;
        if (tokens.size() < 2) {
            BackgroundJob* default_job = find_default_background_job_for_resume_or_foreground();
            if (default_job == nullptr) {
                std::wcerr << L"ksh: fg: no current job\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            job_id = default_job->id;
        } else if (!resolve_job_reference(tokens[1], job_id)) {
            std::wcerr << L"ksh: invalid job id\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        update_background_jobs(false);
        BackgroundJob* job = find_background_job(job_id);
        if (job == nullptr) {
            std::wcerr << L"ksh: job not found: " << job_id << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        bool wait_ok = true;
        if (!job->completed) {
            std::wstring line = L"[" + std::to_wstring(job->id) + L"] foreground " + job->command + L"\n";
            if (!write_builtin_output(line)) {
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            DWORD code = 1;
            wait_ok = wait_for_background_job(*job, code);
        }

        std::wstring exit_code = std::to_wstring(job->exit_code);
        remove_background_job(job_id);
        ksh_env.variables[L"?"] = exit_code;
        return wait_ok;
    }

    if (cmd == L"bg") {
        int job_id = 0;
        if (tokens.size() < 2) {
            BackgroundJob* default_job = find_default_background_job_for_resume_or_foreground();
            if (default_job == nullptr) {
                std::wcerr << L"ksh: bg: no current job\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            job_id = default_job->id;
        } else if (!resolve_job_reference(tokens[1], job_id)) {
            std::wcerr << L"ksh: invalid job id\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        update_background_jobs(false);
        BackgroundJob* job = find_background_job(job_id);
        if (job == nullptr) {
            std::wcerr << L"ksh: job not found: " << job_id << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        if (job->completed) {
            std::wcerr << L"ksh: bg: job already completed: " << job_id << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        std::wstring line = L"[" + std::to_wstring(job->id) + L"] " + job->command + L"\n";
        if (!write_builtin_output(line)) {
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    if (cmd == L"kill") {
        if (tokens.size() < 2) {
            std::wcerr << L"ksh: kill: usage: kill [-l] [-s signal] [-p | -j] pid|%job...\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        
        if (tokens[1] == L"-l") {
            write_builtin_output(L"1) SIGHUP   2) SIGINT   3) SIGQUIT   8) SIGFPE   9) SIGKILL  10) SIGUSR1  11) SIGSEGV  12) SIGUSR2  14) SIGALRM  15) SIGTERM  17) SIGCHLD\n");
            ksh_env.variables[L"?"] = L"0";
            return true;
        }
        
        std::wstring signal_spec = L"TERM";

        enum class KillTargetMode {
            Auto,
            Process,
            Job
        };

        KillTargetMode target_mode = KillTargetMode::Auto;
        size_t target_start = 1;
        while (target_start < tokens.size()) {
            const std::wstring& option = tokens[target_start];
            if (option == L"-s") {
                if (target_start + 1 >= tokens.size()) {
                    std::wcerr << L"ksh: kill: -s requires an argument\n";
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
                signal_spec = tokens[target_start + 1];
                target_start += 2;
                continue;
            }

            if (option == L"-p") {
                target_mode = KillTargetMode::Process;
                target_start++;
                continue;
            }

            if (option == L"-j") {
                target_mode = KillTargetMode::Job;
                target_start++;
                continue;
            }

            if (option.size() > 1 && option[0] == L'-') {
                signal_spec = option.substr(1);
                target_start++;
                continue;
            }

            break;
        }
        
        if (target_start >= tokens.size()) {
            std::wcerr << L"ksh: kill: missing destination pid or job id\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        KillBuiltinSignal selected_signal = KillBuiltinSignal::Term;
        if (!parse_kill_builtin_signal(signal_spec, selected_signal)) {
            std::wcerr << L"ksh: kill: unsupported signal: " << signal_spec << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        
        bool all_ok = true;
        const DWORD self_pid = GetCurrentProcessId();
        ScopedDebugPrivilege scoped_debug_privilege;
        for (size_t i = target_start; i < tokens.size(); ++i) {
            const std::wstring& target = tokens[i];

            int job_id = 0;
            const bool explicit_job_target = !target.empty() && target[0] == L'%';
            const bool treat_as_job =
                explicit_job_target ||
                (target_mode == KillTargetMode::Job && resolve_job_reference(target, job_id));

            if (treat_as_job) {
                if (!resolve_job_reference(target, job_id)) {
                    std::wcerr << L"ksh: kill: invalid job target: " << target << L"\n";
                    all_ok = false;
                    continue;
                }
                BackgroundJob* job = find_background_job(job_id);
                if (job == nullptr) {
                    std::wcerr << L"ksh: kill: job not found: " << target << L"\n";
                    all_ok = false;
                    continue;
                }
                
                if (!job->completed) {
                    if (job->process_handles.empty() && job->process_handle != nullptr) {
                        job->process_handles.push_back(job->process_handle);
                    }
                    for (HANDLE handle : job->process_handles) {
                        if (handle != nullptr && handle != INVALID_HANDLE_VALUE) {
                            TerminateProcess(handle, 1);
                        }
                    }
                    DWORD ignored_code = 1;
                    wait_for_background_job(*job, ignored_code);
                }
                remove_background_job(job_id);
            } else {
                unsigned long parsed_pid = 0;
                if (!try_parse_unsigned_long_strict(target, parsed_pid)) {
                    std::wcerr << L"ksh: kill: invalid target: " << target << L"\n";
                    all_ok = false;
                    continue;
                }
                DWORD pid = static_cast<DWORD>(parsed_pid);
                std::wstring normalized_signal = signal_spec_to_trap_event(signal_spec);
                if (pid == self_pid && !normalized_signal.empty()) {
                    if (!queue_pending_trap_event(normalized_signal)) {
                        std::wcerr << L"ksh: kill: unsupported signal: " << signal_spec << L"\n";
                        all_ok = false;
                    }
                    continue;
                }

                bool success = false;
                switch (selected_signal) {
                case KillBuiltinSignal::Hup:
                case KillBuiltinSignal::Term:
                    success = graceful_kill_process_by_pid(pid);
                    break;
                case KillBuiltinSignal::Int:
                    success = send_console_event_to_pid(pid, CTRL_C_EVENT);
                    break;
                case KillBuiltinSignal::Quit:
                    success = send_console_event_to_pid(pid, CTRL_BREAK_EVENT);
                    break;
                case KillBuiltinSignal::Kill:
                    success = force_kill_process_by_pid(pid);
                    break;
                }

                if (!success) {
                    DWORD failure_error = GetLastError();
                    std::wcerr << L"ksh: kill: " << pid << L": ";
                    if (failure_error == ERROR_INVALID_PARAMETER || failure_error == ERROR_PROC_NOT_FOUND) {
                        std::wcerr << L"no such process\n";
                    } else if (failure_error == ERROR_ACCESS_DENIED) {
                        std::wcerr << L"operation not permitted\n";
                    } else {
                        std::wcerr << L"error " << failure_error << L"\n";
                    }
                    all_ok = false;
                }
            }
        }
        
        ksh_env.variables[L"?"] = all_ok ? L"0" : L"1";
        return all_ok;
    }

    if (cmd == L"find") {
        std::vector<std::wstring> paths;
        std::wstring name_pattern;
        bool iname = false;
        std::wstring type_filter;
        int maxdepth = -1;
        int mindepth = -1;
        bool check_empty = false;
        bool has_size = false;
        wchar_t size_op = L'=';
        uint64_t size_bytes = 0;
        
        size_t idx = 1;
        while (idx < tokens.size() && (tokens[idx].empty() || (tokens[idx][0] != L'-' && tokens[idx] != L"!"))) {
            paths.push_back(tokens[idx]);
            idx++;
        }
        
        if (paths.empty()) {
            paths.push_back(L".");
        }
        
        bool has_error = false;
        bool next_not = false;
        while (idx < tokens.size()) {
            const std::wstring& opt = tokens[idx];
            if (opt == L"!" || opt == L"-not") {
                next_not = !next_not;
                idx++;
                continue;
            }
            if (opt == L"-name" || opt == L"-iname") {
                if (idx + 1 >= tokens.size()) {
                    std::wcerr << L"ksh: find: " << opt << L" requires an argument\n";
                    has_error = true;
                    break;
                }
                name_pattern = tokens[idx + 1];
                iname = (opt == L"-iname");
                idx += 2;
            } else if (opt == L"-type") {
                if (idx + 1 >= tokens.size()) {
                    std::wcerr << L"ksh: find: -type requires an argument\n";
                    has_error = true;
                    break;
                }
                type_filter = tokens[idx + 1];
                if (type_filter != L"f" && type_filter != L"d") {
                    std::wcerr << L"ksh: find: invalid type: " << type_filter << L"\n";
                    has_error = true;
                    break;
                }
                idx += 2;
            } else if (opt == L"-maxdepth") {
                if (idx + 1 >= tokens.size()) {
                    std::wcerr << L"ksh: find: -maxdepth requires an argument\n";
                    has_error = true;
                    break;
                }
                if (!try_parse_int_strict(tokens[idx + 1], maxdepth) || maxdepth < 0) {
                    std::wcerr << L"ksh: find: invalid -maxdepth: " << tokens[idx + 1] << L"\n";
                    has_error = true;
                    break;
                }
                idx += 2;
            } else if (opt == L"-mindepth") {
                if (idx + 1 >= tokens.size()) {
                    std::wcerr << L"ksh: find: -mindepth requires an argument\n";
                    has_error = true;
                    break;
                }
                if (!try_parse_int_strict(tokens[idx + 1], mindepth) || mindepth < 0) {
                    std::wcerr << L"ksh: find: invalid -mindepth: " << tokens[idx + 1] << L"\n";
                    has_error = true;
                    break;
                }
                idx += 2;
            } else if (opt == L"-empty") {
                check_empty = true;
                idx++;
            } else if (opt == L"-size") {
                if (idx + 1 >= tokens.size()) {
                    std::wcerr << L"ksh: find: -size requires an argument\n";
                    has_error = true;
                    break;
                }
                std::wstring s_arg = tokens[idx + 1];
                if (s_arg.empty()) {
                    std::wcerr << L"ksh: find: invalid -size argument\n";
                    has_error = true;
                    break;
                }
                size_op = L'=';
                if (s_arg[0] == L'+' || s_arg[0] == L'-') {
                    size_op = s_arg[0];
                    s_arg = s_arg.substr(1);
                }
                uint64_t multiplier = 512; // default blocks (512-byte)
                if (!s_arg.empty()) {
                    wchar_t suffix = s_arg.back();
                    if (suffix == L'c') { multiplier = 1; s_arg.pop_back(); }
                    else if (suffix == L'k' || suffix == L'K') { multiplier = 1024; s_arg.pop_back(); }
                    else if (suffix == L'M') { multiplier = 1024 * 1024; s_arg.pop_back(); }
                    else if (suffix == L'G') { multiplier = 1024ULL * 1024 * 1024; s_arg.pop_back(); }
                    else if (suffix == L'b') { multiplier = 512; s_arg.pop_back(); }
                }
                int parsed_num = 0;
                if (!try_parse_int_strict(s_arg, parsed_num) || parsed_num < 0) {
                    std::wcerr << L"ksh: find: invalid -size: " << tokens[idx + 1] << L"\n";
                    has_error = true;
                    break;
                }
                size_bytes = static_cast<uint64_t>(parsed_num) * multiplier;
                has_size = true;
                idx += 2;
            } else if (opt == L"-print") {
                idx++;
            } else {
                std::wcerr << L"ksh: find: unknown option: " << opt << L"\n";
                has_error = true;
                break;
            }
            next_not = false;
        }
        
        if (has_error) {
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        
        std::vector<std::wstring> results;

        auto is_dir_empty = [](const std::wstring& dir_path) -> bool {
            std::wstring search = dir_path + L"\\*";
            WIN32_FIND_DATAW test_fd;
            HANDLE test_find = FindFirstFileW(search.c_str(), &test_fd);
            if (test_find == INVALID_HANDLE_VALUE) return true;
            bool empty = true;
            do {
                if (wcscmp(test_fd.cFileName, L".") != 0 && wcscmp(test_fd.cFileName, L"..") != 0) {
                    empty = false;
                    break;
                }
            } while (FindNextFileW(test_find, &test_fd));
            FindClose(test_find);
            return empty;
        };

        auto matches_entry = [&](const std::wstring& name, bool is_dir, uint64_t file_sz, int depth) -> bool {
            if (mindepth >= 0 && depth < mindepth) {
                return false;
            }
            if (maxdepth >= 0 && depth > maxdepth) {
                return false;
            }
            if (!name_pattern.empty()) {
                if (iname) {
                    if (!match_glob_pattern(to_lower_copy(name_pattern), to_lower_copy(name))) return false;
                } else {
                    if (!match_glob_pattern(name_pattern, name)) return false;
                }
            }
            if (!type_filter.empty()) {
                if (type_filter == L"f" && is_dir) return false;
                if (type_filter == L"d" && !is_dir) return false;
            }
            if (check_empty) {
                if (is_dir) {
                    if (!is_dir_empty(name)) return false;
                } else {
                    if (file_sz != 0) return false;
                }
            }
            if (has_size && !is_dir) {
                if (size_op == L'+' && !(file_sz > size_bytes)) return false;
                if (size_op == L'-' && !(file_sz < size_bytes)) return false;
                if (size_op == L'=' && !(file_sz == size_bytes)) return false;
            }
            return true;
        };
        
        std::function<void(const std::wstring&, int)> traverse;
        traverse = [&](const std::wstring& dir, int depth) {
            if (maxdepth >= 0 && depth >= maxdepth) {
                return;
            }
            std::wstring search_path = dir + L"\\*";
            WIN32_FIND_DATAW fd;
            HANDLE hFind = FindFirstFileW(search_path.c_str(), &fd);
            if (hFind == INVALID_HANDLE_VALUE) {
                return;
            }
            
            do {
                std::wstring name = fd.cFileName;
                if (name == L"." || name == L"..") {
                    continue;
                }
                
                std::wstring full_path = dir;
                if (full_path.back() != L'\\' && full_path.back() != L'/') {
                    full_path += L"/";
                }
                full_path += name;
                
                bool is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                uint64_t file_sz = (static_cast<uint64_t>(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow;
                
                if (matches_entry(name, is_dir, file_sz, depth + 1)) {
                    results.push_back(full_path);
                }
                
                if (is_dir) {
                    traverse(full_path, depth + 1);
                }
            } while (FindNextFileW(hFind, &fd));
            
            FindClose(hFind);
        };
        
        auto evaluate_path = [&](const std::wstring& path) {
            DWORD attrs = GetFileAttributesW(path.c_str());
            if (attrs == INVALID_FILE_ATTRIBUTES) {
                std::wcerr << L"ksh: find: " << path << L": No such file or directory\n";
                return;
            }
            
            bool is_dir = (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
            std::wstring name = path;
            size_t last_slash = path.find_last_of(L"\\/");
            if (last_slash != std::wstring::npos) {
                name = path.substr(last_slash + 1);
            }
            
            uint64_t file_sz = 0;
            if (!is_dir) {
                WIN32_FILE_ATTRIBUTE_DATA fad;
                if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad)) {
                    file_sz = (static_cast<uint64_t>(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
                }
            }

            if (matches_entry(name, is_dir, file_sz, 0)) {
                results.push_back(path);
            }
            
            if (is_dir) {
                traverse(path, 0);
            }
        };
        
        for (const auto& path : paths) {
            evaluate_path(path);
        }
        
        bool print_ok = true;
        for (const auto& r : results) {
            std::wstring unix_path = r;
            for (auto& ch : unix_path) {
                if (ch == L'\\') ch = L'/';
            }
            if (!write_builtin_output(unix_path + L"\n")) {
                print_ok = false;
                break;
            }
        }
        
        ksh_env.variables[L"?"] = print_ok ? L"0" : L"1";
        return print_ok;
    }

    if (cmd == L"wait") {
        if (tokens.size() == 2 && tokens[1] == L"-p") {
            if (!g_coprocess.active) {
                ksh_env.variables[L"?"] = L"127";
                return false;
            }
            if (g_coprocess.input_write != INVALID_HANDLE_VALUE) {
                CloseHandle(g_coprocess.input_write);
                g_coprocess.input_write = INVALID_HANDLE_VALUE;
            }
            WaitForSingleObject(g_coprocess.process, INFINITE);
            DWORD exit_code = 1;
            GetExitCodeProcess(g_coprocess.process, &exit_code);
            close_coprocess(false);
            ksh_env.variables[L"COPROC_ACTIVE"] = L"0";
            ksh_env.variables[L"?"] = std::to_wstring(exit_code);
            return exit_code == 0;
        }

        if (tokens.size() == 1) {
            update_background_jobs(false);
            std::vector<int> ids;
            for (const BackgroundJob& job : g_background_jobs) {
                ids.push_back(job.id);
            }

            DWORD last_status = 0;
            bool waited_any = false;
            for (int id : ids) {
                BackgroundJob* job = find_background_job(id);
                if (job != nullptr) {
                    DWORD code = 0;
                    if (!job->completed) {
                        wait_for_background_job(*job, code);
                    } else {
                        code = job->exit_code;
                    }
                    last_status = code;
                    waited_any = true;
                }
                remove_background_job(id);
            }

            ksh_env.variables[L"?"] = waited_any ? std::to_wstring(last_status) : L"0";
            return true;
        }

        DWORD last_status = 0;
        bool waited_any = false;
        bool ok = true;

        for (size_t i = 1; i < tokens.size(); ++i) {
            int job_id = 0;
            if (!resolve_job_reference(tokens[i], job_id)) {
                std::wcerr << L"ksh: invalid job id\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            BackgroundJob* job = find_background_job(job_id);
            if (job == nullptr) {
                std::wcerr << L"ksh: job not found: " << job_id << L"\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            DWORD code = 0;
            if (!job->completed) {
                if (!wait_for_background_job(*job, code)) {
                    ok = false;
                }
            } else {
                code = job->exit_code;
            }

            remove_background_job(job_id);
            last_status = code;
            waited_any = true;
        }

        ksh_env.variables[L"?"] = waited_any ? std::to_wstring(last_status) : L"0";
        return ok;
    }

    if (cmd == L"complete") {
        std::wstring prefix;
        for (size_t i = 1; i < tokens.size(); ++i) {
            if (!prefix.empty()) {
                prefix += L" ";
            }
            prefix += tokens[i];
        }
        std::vector<std::wstring> matches = collect_completion_candidates(prefix);
        for (const std::wstring& match : matches) {
            if (!write_builtin_output(match + L"\n")) {
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
        }
        ksh_env.variables[L"?"] = matches.empty() ? L"1" : L"0";
        return !matches.empty();
    }

    if (cmd == L"math") {
        if (tokens.size() < 2) {
            std::wcerr << L"ksh: math requires an expression\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        std::wstring expression;
        for (size_t i = 1; i < tokens.size(); ++i) {
            expression += tokens[i];
            if (i + 1 < tokens.size()) {
                expression += L" ";
            }
        }

        std::wstring value = evaluate_arithmetic(expression);
        if (!write_builtin_output(value + L"\n")) {
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    if (cmd == L"trap") {
        auto print_traps = [&](const std::vector<std::wstring>& event_filter) -> bool {
            if (event_filter.empty()) {
                for (std::map<std::wstring, std::wstring>::const_iterator it = g_trap_handlers.begin(); it != g_trap_handlers.end(); ++it) {
                    if (!write_builtin_output(L"trap -- " + quote_for_single_quoted_shell_literal(it->second) + L" " + it->first + L"\n")) {
                        return false;
                    }
                }
                return true;
            }

            for (const std::wstring& raw_event : event_filter) {
                const std::wstring event_name = normalize_trap_event_name(raw_event);
                if (event_name.empty()) {
                    std::wcerr << L"ksh: trap: unsupported event: " << raw_event << L"\n";
                    return false;
                }

                std::map<std::wstring, std::wstring>::const_iterator it = g_trap_handlers.find(event_name);
                if (it != g_trap_handlers.end()) {
                    if (!write_builtin_output(L"trap -- " + quote_for_single_quoted_shell_literal(it->second) + L" " + it->first + L"\n")) {
                        return false;
                    }
                }
            }

            return true;
        };

        if (tokens.size() == 1) {
            if (!print_traps(std::vector<std::wstring>())) {
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        if (tokens[1] == L"-p") {
            std::vector<std::wstring> filter;
            if (tokens.size() > 2) {
                filter.assign(tokens.begin() + 2, tokens.end());
            }
            if (!print_traps(filter)) {
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        bool clear_mode = false;
        std::wstring action = tokens[1];
        size_t event_start = 2;
        if (tokens[1] == L"-") {
            clear_mode = true;
            event_start = 2;
        }

        if (event_start >= tokens.size()) {
            std::wcerr << L"ksh: trap: usage: trap [command|-] EVENT [EVENT ...]\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        bool ok = true;
        for (size_t i = event_start; i < tokens.size(); ++i) {
            const std::wstring event_name = normalize_trap_event_name(tokens[i]);
            if (event_name.empty()) {
                std::wcerr << L"ksh: trap: unsupported event: " << tokens[i] << L"\n";
                ok = false;
                continue;
            }

            if (clear_mode) {
                g_trap_handlers.erase(event_name);
            } else {
                // Enforce per-session trap-handler table cap (new entries only).
                if (g_trap_handlers.find(event_name) == g_trap_handlers.end() &&
                    g_trap_handlers.size() >= kMaxTrapHandlers) {
                    std::wcerr << L"ksh: trap handler table limit reached\n";
                    ok = false;
                    continue;
                }
                g_trap_handlers[event_name] = action;
            }

            if (event_name == L"INT") {
                InterlockedExchange(&g_int_trap_active, clear_mode ? 0 : 1);
            } else if (event_name == L"BREAK") {
                InterlockedExchange(&g_break_trap_active, clear_mode ? 0 : 1);
            } else if (event_name == L"HUP") {
                InterlockedExchange(&g_hup_trap_active, clear_mode ? 0 : 1);
            } else if (event_name == L"ALRM") {
                InterlockedExchange(&g_alrm_trap_active, clear_mode ? 0 : 1);
            } else if (event_name == L"USR1") {
                InterlockedExchange(&g_usr1_trap_active, clear_mode ? 0 : 1);
            } else if (event_name == L"USR2") {
                InterlockedExchange(&g_usr2_trap_active, clear_mode ? 0 : 1);
            } else if (event_name == L"SEGV") {
                InterlockedExchange(&g_segv_trap_active, clear_mode ? 0 : 1);
            } else if (event_name == L"FPE") {
                InterlockedExchange(&g_fpe_trap_active, clear_mode ? 0 : 1);
            } else if (event_name == L"TERM") {
                InterlockedExchange(&g_term_trap_active, clear_mode ? 0 : 1);
            }
        }

        ksh_env.variables[L"?"] = ok ? L"0" : L"1";
        return ok;
    }

    // ksh93 Built-in: typeset (supports -a, -A, -i, -r, -x, -L, -R, -Z)
    if (cmd == L"typeset") {
        struct TypesetOptions {
            bool array_mode = false;
            bool associative_mode = false;
            bool nameref_mode = false;
            bool integer_mode = false;
            bool readonly_mode = false;
            bool export_mode = false;
            bool uppercase_mode = false;
            bool lowercase_mode = false;
            bool left_justify_mode = false;
            bool right_justify_mode = false;
            bool zero_pad_mode = false;
            bool clear_integer_mode = false;
            bool clear_export_mode = false;
            bool clear_uppercase_mode = false;
            bool clear_lowercase_mode = false;
            bool clear_left_justify_mode = false;
            bool clear_right_justify_mode = false;
            bool clear_zero_pad_mode = false;
            bool list_mode = false;
            bool type_mode = false;
            int justify_width = -1;
            bool justify_width_set = false;
        };

        TypesetOptions options;
        std::vector<std::wstring> operands;
        bool end_of_options = false;

        auto parse_option_token = [&](const std::wstring& token, bool enable_mode, size_t& option_index) -> bool {
            for (size_t j = 1; j < token.size(); ++j) {
                if (token[j] == L'a') {
                    if (!enable_mode) {
                        std::wcerr << L"ksh: typeset: option removal (+a) is not supported\n";
                        return false;
                    }
                    options.array_mode = true;
                } else if (token[j] == L'A') {
                    if (!enable_mode) {
                        std::wcerr << L"ksh: typeset: option removal (+A) is not supported\n";
                        return false;
                    }
                    options.associative_mode = true;
                } else if (token[j] == L'n') {
                    if (!enable_mode) {
                        std::wcerr << L"ksh: typeset: option removal (+n) is not supported\n";
                        return false;
                    }
                    options.nameref_mode = true;
                } else if (token[j] == L'i') {
                    if (enable_mode) {
                        options.integer_mode = true;
                        options.clear_integer_mode = false;
                    } else {
                        options.clear_integer_mode = true;
                        options.integer_mode = false;
                    }
                } else if (token[j] == L'r') {
                    if (!enable_mode) {
                        std::wcerr << L"ksh: typeset: option removal (+r) is not supported\n";
                        return false;
                    }
                    options.readonly_mode = true;
                } else if (token[j] == L'x') {
                    if (enable_mode) {
                        options.export_mode = true;
                        options.clear_export_mode = false;
                    } else {
                        options.clear_export_mode = true;
                        options.export_mode = false;
                    }
                } else if (token[j] == L'u') {
                    if (enable_mode) {
                        options.uppercase_mode = true;
                        options.lowercase_mode = false;
                        options.clear_uppercase_mode = false;
                    } else {
                        options.clear_uppercase_mode = true;
                        options.uppercase_mode = false;
                    }
                } else if (token[j] == L'l') {
                    if (enable_mode) {
                        options.lowercase_mode = true;
                        options.uppercase_mode = false;
                        options.clear_lowercase_mode = false;
                    } else {
                        options.clear_lowercase_mode = true;
                        options.lowercase_mode = false;
                    }
                } else if (token[j] == L'L' || token[j] == L'R' || token[j] == L'Z') {
                    if (enable_mode) {
                        if (options.left_justify_mode || options.right_justify_mode || options.zero_pad_mode) {
                            std::wcerr << L"ksh: typeset: justification attributes cannot be combined\n";
                            return false;
                        }

                        std::wstring width_text;
                        if (j + 1 < token.size()) {
                            width_text = token.substr(j + 1);
                            j = token.size();
                        } else if (option_index + 1 < tokens.size() && !tokens[option_index + 1].empty() && tokens[option_index + 1][0] != L'-' && tokens[option_index + 1][0] != L'+') {
                            width_text = tokens[++option_index];
                            j = token.size();
                        } else {
                            std::wcerr << L"ksh: typeset: justification width required for -" << token[j] << L"\n";
                            return false;
                        }

                        int width = 0;
                        if (!try_parse_int_strict(width_text, width) || width < 0) {
                            std::wcerr << L"ksh: typeset: invalid justification width: " << width_text << L"\n";
                            return false;
                        }

                        options.justify_width = width;
                        options.justify_width_set = true;
                        if (token[j] == L'L') {
                            options.left_justify_mode = true;
                            options.right_justify_mode = false;
                            options.zero_pad_mode = false;
                        } else if (token[j] == L'R') {
                            options.right_justify_mode = true;
                            options.left_justify_mode = false;
                            options.zero_pad_mode = false;
                        } else {
                            options.zero_pad_mode = true;
                            options.left_justify_mode = false;
                            options.right_justify_mode = false;
                        }
                    } else {
                        if (token[j] == L'L') {
                            options.clear_left_justify_mode = true;
                        } else if (token[j] == L'R') {
                            options.clear_right_justify_mode = true;
                        } else {
                            options.clear_zero_pad_mode = true;
                        }
                    }
                } else if (token[j] == L'p') {
                    if (!enable_mode) {
                        std::wcerr << L"ksh: typeset: option removal (+p) is not supported\n";
                        return false;
                    }
                    options.list_mode = true;
                } else if (token[j] == L'T') {
                    if (!enable_mode) {
                        std::wcerr << L"ksh: typeset: option removal (+T) is not supported\n";
                        return false;
                    }
                    options.type_mode = true;
                } else {
                    std::wcerr << L"ksh: typeset: unsupported option: -" << token[j] << L"\n";
                    return false;
                }
            }

            return true;
        };

        for (size_t i = 1; i < tokens.size(); ++i) {
            const std::wstring& token = tokens[i];
            if (!end_of_options && token == L"--") {
                end_of_options = true;
                continue;
            }

            if (!end_of_options && !token.empty() && token.size() > 1 && (token[0] == L'-' || token[0] == L'+')) {
                if (!parse_option_token(token, token[0] == L'-', i)) {
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
                continue;
            }

            operands.push_back(token);
        }

        if (options.nameref_mode && (options.array_mode || options.associative_mode || options.integer_mode ||
            options.uppercase_mode || options.lowercase_mode || options.clear_integer_mode ||
            options.clear_uppercase_mode || options.clear_lowercase_mode)) {
            std::wcerr << L"ksh: typeset: nameref cannot be combined with other attributes\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        if ((options.array_mode || options.associative_mode) && options.integer_mode) {
            std::wcerr << L"ksh: typeset: array and -i cannot be combined\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        if ((options.array_mode || options.associative_mode) &&
            (options.left_justify_mode || options.right_justify_mode || options.zero_pad_mode ||
             options.clear_left_justify_mode || options.clear_right_justify_mode || options.clear_zero_pad_mode)) {
            std::wcerr << L"ksh: typeset: justification attributes cannot be applied to arrays\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        if (options.justify_width_set && !(options.left_justify_mode || options.right_justify_mode || options.zero_pad_mode)) {
            std::wcerr << L"ksh: typeset: justification width requires -L, -R, or -Z\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        if (options.array_mode && options.associative_mode) {
            std::wcerr << L"ksh: typeset: -a and -A cannot be combined\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        if ((options.array_mode || options.associative_mode) && (options.uppercase_mode || options.lowercase_mode || options.clear_uppercase_mode || options.clear_lowercase_mode)) {
            std::wcerr << L"ksh: typeset: case attributes cannot be applied to arrays\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        if (options.type_mode) {
            std::wstring full_expr;
            for (size_t k = 0; k < operands.size(); ++k) {
                if (k > 0) full_expr += L" ";
                full_expr += operands[k];
            }
            
            size_t eq_pos = full_expr.find(L'=');
            if (eq_pos == std::wstring::npos) {
                std::wstring type_to_print = trim_copy(full_expr);
                auto t_it = g_custom_types.find(type_to_print);
                if (t_it != g_custom_types.end()) {
                    std::wstring out = L"typeset -T " + type_to_print + L"=(\n";
                    for (const auto& member : t_it->second.members) {
                        out += L"    ";
                        if (!member.type_name.empty()) {
                            out += member.type_name;
                        } else if (member.is_integer) {
                            out += L"integer";
                        } else {
                            out += L"typeset";
                        }
                        out += L" " + member.name;
                        if (!member.default_value.empty()) {
                            out += L"=" + member.default_value;
                        }
                        out += L"\n";
                    }
                    out += L")\n";
                    write_builtin_output(out);
                    ksh_env.variables[L"?"] = L"0";
                    return true;
                } else {
                    std::wcerr << L"ksh: typeset -T: type not found: " << type_to_print << L"\n";
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
            }
            
            std::wstring type_name = trim_copy(full_expr.substr(0, eq_pos));
            std::wstring body_part = trim_copy(full_expr.substr(eq_pos + 1));
            
            if (body_part.front() != L'(' || body_part.back() != L')') {
                std::wcerr << L"ksh: typeset -T: type definition must be enclosed in (...)\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            
            std::wstring body = body_part.substr(1, body_part.size() - 2);
            CustomTypeDefinition def;
            if (parse_custom_type_definition(type_name, body, def)) {
                g_custom_types[type_name] = def;
                ksh_env.variables[L"?"] = L"0";
                return true;
            } else {
                std::wcerr << L"ksh: typeset -T: failed to parse type definition\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
        }

        if (!operands.empty() && g_custom_types.find(operands[0]) != g_custom_types.end()) {
            std::wstring t_name = operands[0];
            for (size_t i = 1; i < operands.size(); ++i) {
                std::wstring token = operands[i];
                std::wstring var_name = token;
                std::wstring assign_part;
                size_t eq = token.find(L'=');
                if (eq != std::wstring::npos) {
                    var_name = token.substr(0, eq);
                    assign_part = token.substr(eq + 1);
                }
                
                instantiate_custom_type(t_name, var_name);
                
                if (!assign_part.empty()) {
                    if (assign_part.front() == L'(' && assign_part.back() == L')') {
                        std::wstring inner = assign_part.substr(1, assign_part.size() - 2);
                        std::vector<std::wstring> sub_tokens = ksh_tokenize_preserve_quotes(inner);
                        for (const auto& sub_t : sub_tokens) {
                            size_t sub_eq = sub_t.find(L'=');
                            if (sub_eq != std::wstring::npos) {
                                std::wstring member = sub_t.substr(0, sub_eq);
                                std::wstring val = sub_t.substr(sub_eq + 1);
                                std::wstring full_name = var_name + L"." + member;
                                ksh_env.variables[full_name] = val;
                                SetEnvironmentVariableW(full_name.c_str(), val.c_str());
                            }
                        }
                    }
                }
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        }

        const bool local_scope = is_executing_function_scope();

        auto matches_list_filters = [&](const std::wstring& name, bool is_array) -> bool {
            if (options.nameref_mode && !get_flag_value(ksh_env.nameref_flags, name)) {
                return false;
            }
            if (options.array_mode && (!is_array || get_flag_value(ksh_env.associative_flags, name))) {
                return false;
            }
            if (options.associative_mode && (!is_array || !get_flag_value(ksh_env.associative_flags, name))) {
                return false;
            }
            if (options.integer_mode && !get_flag_value(ksh_env.integer_flags, name)) {
                return false;
            }
            if (options.readonly_mode && !get_flag_value(ksh_env.readonly_flags, name)) {
                return false;
            }
            if (options.export_mode && !get_flag_value(ksh_env.exported, name)) {
                return false;
            }
            if (options.uppercase_mode && !get_flag_value(ksh_env.uppercase_flags, name)) {
                return false;
            }
            if (options.lowercase_mode && !get_flag_value(ksh_env.lowercase_flags, name)) {
                return false;
            }
            std::map<std::wstring, wchar_t>::const_iterator justify_mode_it = ksh_env.justify_modes.find(name);
            wchar_t justify_mode = (justify_mode_it != ksh_env.justify_modes.end()) ? justify_mode_it->second : 0;
            if (options.left_justify_mode && justify_mode != L'L') {
                return false;
            }
            if (options.right_justify_mode && justify_mode != L'R') {
                return false;
            }
            if (options.zero_pad_mode && justify_mode != L'Z') {
                return false;
            }
            return true;
        };

        auto write_typeset_entry = [&](const std::wstring& name, bool require_existing) -> bool {
            const bool has_scalar = ksh_env.variables.find(name) != ksh_env.variables.end();
            const bool has_array = ksh_env.arrays.find(name) != ksh_env.arrays.end();
            const bool is_nameref = get_flag_value(ksh_env.nameref_flags, name);
            const bool has_any_state = has_scalar || has_array || is_nameref ||
                get_flag_value(ksh_env.exported, name) ||
                get_flag_value(ksh_env.readonly_flags, name) ||
                get_flag_value(ksh_env.integer_flags, name) ||
                get_flag_value(ksh_env.uppercase_flags, name) ||
                get_flag_value(ksh_env.lowercase_flags, name);

            if (!has_any_state) {
                if (require_existing) {
                    std::wcerr << L"ksh: typeset: not found: " << name << L"\n";
                }
                return !require_existing;
            }

            const bool is_array = has_array;
            if (!matches_list_filters(name, is_array)) {
                return true;
            }

            std::wstring line = L"typeset";
            if (is_nameref) {
                line += L" -n";
            }
            if (is_array) {
                if (get_flag_value(ksh_env.associative_flags, name)) {
                    line += L" -A";
                } else {
                    line += L" -a";
                }
            }
            if (get_flag_value(ksh_env.integer_flags, name)) {
                line += L" -i";
            }
            if (get_flag_value(ksh_env.readonly_flags, name)) {
                line += L" -r";
            }
            if (get_flag_value(ksh_env.exported, name)) {
                line += L" -x";
            }
            if (get_flag_value(ksh_env.uppercase_flags, name)) {
                line += L" -u";
            }
            if (get_flag_value(ksh_env.lowercase_flags, name)) {
                line += L" -l";
            }

            std::map<std::wstring, wchar_t>::const_iterator justify_mode_it = ksh_env.justify_modes.find(name);
            if (justify_mode_it != ksh_env.justify_modes.end()) {
                if (justify_mode_it->second == L'L') {
                    line += L" -L";
                } else if (justify_mode_it->second == L'R') {
                    line += L" -R";
                } else if (justify_mode_it->second == L'Z') {
                    line += L" -Z";
                }

                std::map<std::wstring, int>::const_iterator justify_width_it = ksh_env.justify_widths.find(name);
                if (justify_width_it != ksh_env.justify_widths.end() && justify_width_it->second >= 0) {
                    line += L" ";
                    line += std::to_wstring(justify_width_it->second);
                }
            }

            line += L" ";
            line += name;

            if (has_array) {
                line += L"=(";
                const std::map<std::wstring, std::wstring>& m = ksh_env.arrays[name];
                bool is_assoc = get_flag_value(ksh_env.associative_flags, name);
                if (is_assoc) {
                    size_t idx = 0;
                    for (const auto& pair : m) {
                        line += L"[" + pair.first + L"]=" + quote_command_argument(pair.second);
                        if (idx + 1 < m.size()) {
                            line += L" ";
                        }
                        idx++;
                    }
                } else {
                    std::vector<std::wstring> values = get_array_values_vector(name);
                    for (size_t index = 0; index < values.size(); ++index) {
                        line += quote_command_argument(values[index]);
                        if (index + 1 < values.size()) {
                            line += L" ";
                        }
                    }
                }
                line += L")";
            } else if (has_scalar) {
                line += L"=";
                line += quote_command_argument(ksh_env.variables[name]);
            }

            return write_builtin_output(line + L"\n");
        };

        const bool has_mutating_flags = options.array_mode || options.associative_mode || options.integer_mode || options.readonly_mode ||
            options.export_mode || options.uppercase_mode || options.lowercase_mode ||
            options.left_justify_mode || options.right_justify_mode || options.zero_pad_mode ||
            options.clear_integer_mode || options.clear_export_mode || options.clear_uppercase_mode ||
            options.clear_lowercase_mode || options.clear_left_justify_mode || options.clear_right_justify_mode ||
            options.clear_zero_pad_mode;

        if (options.list_mode || operands.empty()) {
            if (!operands.empty()) {
                bool ok = true;
                for (const std::wstring& name : operands) {
                    if (!is_valid_shell_identifier(name)) {
                        std::wcerr << L"ksh: typeset: invalid name: " << name << L"\n";
                        ok = false;
                        continue;
                    }
                    if (!write_typeset_entry(name, true)) {
                        ok = false;
                    }
                }
                ksh_env.variables[L"?"] = ok ? L"0" : L"1";
                return ok;
            }

            if (!options.list_mode && !has_mutating_flags) {
                // Fall through to print all variables.
            } else if (!options.list_mode && has_mutating_flags) {
                // ksh93-style option-only invocations print matching declarations.
            }

            bool ok = true;
            for (std::map<std::wstring, std::wstring>::const_iterator it = ksh_env.variables.begin(); it != ksh_env.variables.end(); ++it) {
                if (is_internal_shell_state_variable(it->first)) {
                    continue;
                }
                if (!write_typeset_entry(it->first, false)) {
                    ok = false;
                    break;
                }
            }
            if (ok) {
                ksh_env.variables[L"?"] = L"0";
                return true;
            }
        }

        for (const std::wstring& token : operands) {
            size_t assignment_eq = token.find(L'=');
            if (assignment_eq != std::wstring::npos) {
                std::wstring lhs = token.substr(0, assignment_eq);
                std::wstring rhs = token.substr(assignment_eq + 1);

                std::wstring base_name;
                std::wstring index_text;
                bool has_index = false;
                if (!parse_array_reference_expression(lhs, base_name, index_text, has_index)) {
                    std::wcerr << L"ksh: typeset: invalid name: " << lhs << L"\n";
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }

                if (options.readonly_mode && (options.clear_integer_mode || options.clear_export_mode || options.clear_uppercase_mode || options.clear_lowercase_mode)) {
                    // no-op, combined set/clear remains meaningful for non-readonly flags
                }

                if ((options.array_mode || options.associative_mode || options.nameref_mode) && has_index) {
                    std::wcerr << L"ksh: typeset: indexed assignment is not allowed with -a/-A/-n: " << lhs << L"\n";
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }

                if (local_scope) {
                    snapshot_local_variable_if_needed(base_name);
                }

                if (options.clear_integer_mode) {
                    set_flag_value(ksh_env.integer_flags, base_name, false);
                }
                if (options.clear_export_mode) {
                    set_flag_value(ksh_env.exported, base_name, false);
                    SetEnvironmentVariableW(base_name.c_str(), nullptr);
                }
                if (options.clear_uppercase_mode) {
                    set_flag_value(ksh_env.uppercase_flags, base_name, false);
                }
                if (options.clear_lowercase_mode) {
                    set_flag_value(ksh_env.lowercase_flags, base_name, false);
                }

                if (options.clear_left_justify_mode || options.clear_right_justify_mode || options.clear_zero_pad_mode) {
                    ksh_env.justify_modes.erase(base_name);
                    ksh_env.justify_widths.erase(base_name);
                }

                if (options.array_mode || options.associative_mode) {
                    set_flag_value(ksh_env.integer_flags, base_name, false);
                }
                if (options.integer_mode) {
                    set_flag_value(ksh_env.integer_flags, base_name, true);
                }
                if (options.uppercase_mode) {
                    set_flag_value(ksh_env.uppercase_flags, base_name, true);
                    set_flag_value(ksh_env.lowercase_flags, base_name, false);
                }
                if (options.lowercase_mode) {
                    set_flag_value(ksh_env.lowercase_flags, base_name, true);
                    set_flag_value(ksh_env.uppercase_flags, base_name, false);
                }

                if (options.left_justify_mode || options.right_justify_mode || options.zero_pad_mode) {
                    wchar_t justify_mode = options.left_justify_mode ? L'L' : (options.right_justify_mode ? L'R' : L'Z');
                    ksh_env.justify_modes[base_name] = justify_mode;
                    ksh_env.justify_widths[base_name] = options.justify_width;
                }

                if (options.associative_mode) {
                    set_flag_value(ksh_env.associative_flags, base_name, true);
                } else if (options.array_mode) {
                    set_flag_value(ksh_env.associative_flags, base_name, false);
                }

                std::wstring assignment_error;
                if (!assign_parameter_value(lhs, rhs, options.array_mode || options.associative_mode, local_scope, assignment_error, options.nameref_mode)) {
                    std::wcerr << L"ksh: typeset: " << assignment_error << L"\n";
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }

                if (options.export_mode) {
                    set_flag_value(ksh_env.exported, base_name, true);
                    sync_exported_environment_variable(base_name);
                }

                if (options.readonly_mode) {
                    set_flag_value(ksh_env.readonly_flags, base_name, true);
                }

                continue;
            }

            if (!is_valid_shell_identifier(token)) {
                std::wcerr << L"ksh: typeset: invalid name: " << token << L"\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            if (options.clear_integer_mode) {
                set_flag_value(ksh_env.integer_flags, token, false);
            }
            if (options.clear_export_mode) {
                set_flag_value(ksh_env.exported, token, false);
                SetEnvironmentVariableW(token.c_str(), nullptr);
            }
            if (options.clear_uppercase_mode) {
                set_flag_value(ksh_env.uppercase_flags, token, false);
            }
            if (options.clear_lowercase_mode) {
                set_flag_value(ksh_env.lowercase_flags, token, false);
            }

            if (options.clear_left_justify_mode || options.clear_right_justify_mode || options.clear_zero_pad_mode) {
                ksh_env.justify_modes.erase(token);
                ksh_env.justify_widths.erase(token);
            }

            if (options.nameref_mode) {
                if (local_scope) {
                    snapshot_local_variable_if_needed(token);
                }
                if (get_flag_value(ksh_env.readonly_flags, token)) {
                    std::wcerr << L"ksh: typeset: variable is read-only: " << token << L"\n";
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
                set_flag_value(ksh_env.nameref_flags, token, true);
                ksh_env.variables.erase(token);
                ksh_env.arrays.erase(token);
                ksh_env.associative_flags.erase(token);
                set_flag_value(ksh_env.integer_flags, token, false);
                set_flag_value(ksh_env.uppercase_flags, token, false);
                set_flag_value(ksh_env.lowercase_flags, token, false);
            } else if (options.array_mode || options.associative_mode) {
                if (local_scope) {
                    snapshot_local_variable_if_needed(token);
                }
                if (get_flag_value(ksh_env.readonly_flags, token)) {
                    std::wcerr << L"ksh: typeset: variable is read-only: " << token << L"\n";
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
                ksh_env.arrays.try_emplace(token);
                if (options.associative_mode) {
                    set_flag_value(ksh_env.associative_flags, token, true);
                } else {
                    set_flag_value(ksh_env.associative_flags, token, false);
                }
                ksh_env.variables.erase(token);
                SetEnvironmentVariableW(token.c_str(), nullptr);
                set_flag_value(ksh_env.integer_flags, token, false);
                set_flag_value(ksh_env.uppercase_flags, token, false);
                set_flag_value(ksh_env.lowercase_flags, token, false);
            } else if (ksh_env.variables.find(token) == ksh_env.variables.end() && ksh_env.arrays.find(token) == ksh_env.arrays.end()) {
                if (local_scope) {
                    snapshot_local_variable_if_needed(token);
                }
                if (options.integer_mode) {
                    assign_scalar_parameter(token, L"0");
                } else {
                    assign_scalar_parameter(token, L"");
                }
            } else if (local_scope) {
                snapshot_local_variable_if_needed(token);
            }

            if (options.integer_mode) {
                if (ksh_env.arrays.find(token) != ksh_env.arrays.end()) {
                    std::wcerr << L"ksh: typeset: cannot apply -i to array: " << token << L"\n";
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }

                std::map<std::wstring, std::wstring>::const_iterator scalar_it = ksh_env.variables.find(token);
                if (scalar_it != ksh_env.variables.end() && !scalar_it->second.empty()) {
                    assign_scalar_parameter(token, evaluate_arithmetic(scalar_it->second));
                }
                set_flag_value(ksh_env.integer_flags, token, true);
            }

            if (options.left_justify_mode || options.right_justify_mode || options.zero_pad_mode) {
                if (ksh_env.arrays.find(token) != ksh_env.arrays.end()) {
                    std::wcerr << L"ksh: typeset: cannot apply justification attributes to array: " << token << L"\n";
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
                wchar_t justify_mode = options.left_justify_mode ? L'L' : (options.right_justify_mode ? L'R' : L'Z');
                ksh_env.justify_modes[token] = justify_mode;
                ksh_env.justify_widths[token] = options.justify_width;
                std::map<std::wstring, std::wstring>::const_iterator scalar_it = ksh_env.variables.find(token);
                if (scalar_it != ksh_env.variables.end()) {
                    assign_scalar_parameter(token, scalar_it->second);
                }
            }

            if (options.uppercase_mode) {
                if (ksh_env.arrays.find(token) != ksh_env.arrays.end()) {
                    std::wcerr << L"ksh: typeset: cannot apply -u to array: " << token << L"\n";
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
                set_flag_value(ksh_env.uppercase_flags, token, true);
                set_flag_value(ksh_env.lowercase_flags, token, false);
                std::map<std::wstring, std::wstring>::const_iterator scalar_it = ksh_env.variables.find(token);
                if (scalar_it != ksh_env.variables.end()) {
                    assign_scalar_parameter(token, scalar_it->second);
                }
            }

            if (options.lowercase_mode) {
                if (ksh_env.arrays.find(token) != ksh_env.arrays.end()) {
                    std::wcerr << L"ksh: typeset: cannot apply -l to array: " << token << L"\n";
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
                set_flag_value(ksh_env.lowercase_flags, token, true);
                set_flag_value(ksh_env.uppercase_flags, token, false);
                std::map<std::wstring, std::wstring>::const_iterator scalar_it = ksh_env.variables.find(token);
                if (scalar_it != ksh_env.variables.end()) {
                    assign_scalar_parameter(token, scalar_it->second);
                }
            }

            if (options.export_mode) {
                set_flag_value(ksh_env.exported, token, true);
                sync_exported_environment_variable(token);
            }

            if (options.readonly_mode) {
                set_flag_value(ksh_env.readonly_flags, token, true);
            }
        }

        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    // ksh93 Built-in: [[ conditional test ]]
    if (cmd == L"[[") {
        std::wstring cond_block;
        std::wstring parse_error;
        if (!extract_ksh_conditional_block(expanded, cond_block, parse_error)) {
            std::wcerr << L"ksh: [[ ... ]]: " << parse_error << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        bool test_result = false;
        std::wstring eval_error;
        if (!evaluate_ksh_conditional(cond_block, test_result, eval_error)) {
            std::wcerr << L"ksh: [[ ... ]]: " << eval_error << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        ksh_env.variables[L"?"] = test_result ? L"0" : L"1";
        return true;
    }

    if (cmd == L"enum") {
        if (tokens.size() < 2) {
            std::wcerr << L"ksh: enum: usage: enum Name=(value ...)\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        std::wstring declaration;
        for (size_t index = 1; index < tokens.size(); ++index) {
            if (index > 1) declaration += L" ";
            declaration += tokens[index];
        }
        const size_t equals = declaration.find(L'=');
        if (equals == std::wstring::npos) {
            std::wcerr << L"ksh: enum: expected Name=(value ...)\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        const std::wstring type_name = trim_copy(declaration.substr(0, equals));
        std::wstring values = trim_copy(declaration.substr(equals + 1));
        if (type_name.empty() || values.size() < 2 || values.front() != L'(' || values.back() != L')') {
            std::wcerr << L"ksh: enum: expected Name=(value ...)\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        CustomTypeDefinition definition;
        definition.name = type_name;
        const std::vector<std::wstring> names = ksh_tokenize_preserve_quotes(values.substr(1, values.size() - 2));
        if (names.empty()) {
            std::wcerr << L"ksh: enum: enumeration must contain at least one value\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        const auto is_identifier = [](const std::wstring& name) {
            if (name.empty() || !(std::iswalpha(name.front()) || name.front() == L'_')) return false;
            return std::all_of(name.begin() + 1, name.end(), [](wchar_t character) {
                return std::iswalnum(character) || character == L'_';
            });
        };
        for (size_t index = 0; index < names.size(); ++index) {
            if (!is_identifier(names[index])) {
                std::wcerr << L"ksh: enum: invalid value: " << names[index] << L"\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            CustomTypeMember member;
            member.name = names[index];
            member.is_integer = true;
            member.default_value = std::to_wstring(index);
            definition.members.push_back(std::move(member));
        }
        g_custom_types[type_name] = std::move(definition);
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    if (cmd == L"coproc") {
        return execute_builtin_coproc(tokens);
    }

    if (cmd == L"sleep") {
        if (tokens.size() != 2) {
            std::wcerr << L"ksh: sleep: usage: sleep seconds\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        try {
            size_t consumed = 0;
            const double seconds = std::stod(tokens[1], &consumed);
            if (consumed != tokens[1].size() || !std::isfinite(seconds) || seconds < 0.0 || seconds > 4294967.295) {
                throw std::invalid_argument("seconds");
            }
            const ULONGLONG duration_ms = static_cast<ULONGLONG>(std::llround(seconds * 1000.0));
            const ULONGLONG deadline = GetTickCount64() + duration_ms;
            while (GetTickCount64() < deadline) {
                const bool interrupted =
                    InterlockedCompareExchange(&g_pending_int_trap, 0, 0) != 0 ||
                    InterlockedCompareExchange(&g_pending_break_trap, 0, 0) != 0 ||
                    InterlockedCompareExchange(&g_pending_hup_trap, 0, 0) != 0 ||
                    InterlockedCompareExchange(&g_pending_term_trap, 0, 0) != 0;
                if (interrupted) {
                    bool interrupted_should_exit = false;
                    process_pending_traps(interrupted_should_exit);
                    return false;
                }
                const ULONGLONG remaining_ms = deadline - GetTickCount64();
                Sleep(static_cast<DWORD>(remaining_ms < 20 ? remaining_ms : 20));
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        } catch (...) {
            std::wcerr << L"ksh: sleep: invalid time interval: " << tokens[1] << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
    }

    // ksh93 Built-in: test / [ ]
    if (cmd == L"test" || cmd == L"[") {
        std::vector<std::wstring> condition_tokens;
        if (cmd == L"[") {
            if (tokens.size() < 2 || tokens.back() != L"]") {
                std::wcerr << L"ksh: [: missing closing ]\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            condition_tokens.assign(tokens.begin() + 1, tokens.end() - 1);
        } else {
            condition_tokens.assign(tokens.begin() + 1, tokens.end());
        }

        if (condition_tokens.empty()) {
            ksh_env.variables[L"?"] = L"1";
            return true;
        }

        bool test_result = false;
        std::wstring eval_error;
        if (!evaluate_test_condition_expression(condition_tokens, test_result, eval_error)) {
            std::wcerr << L"ksh: " << (cmd == L"[" ? L"[: " : L"test: ") << eval_error << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }

        ksh_env.variables[L"?"] = test_result ? L"0" : L"1";
        return true;
    }

    std::map<std::wstring, ShellFunctionDefinition>::const_iterator function_it = g_shell_functions.find(cmd);
    if (!bypass_function_lookup && function_it != g_shell_functions.end()) {
        std::vector<std::wstring> function_args;
        for (size_t i = 1; i < tokens.size(); ++i) {
            function_args.push_back(tokens[i]);
        }
        return execute_defined_function(cmd, function_args, should_exit_shell);
    }

    // Variable assignment standard ksh: var=value
    size_t eq = cmd.find(L'=');
    if (eq != std::wstring::npos && eq > 0) {
        std::wstring k = cmd.substr(0, eq);
        std::wstring v = cmd.substr(eq + 1);
        std::wstring assignment_error;
        if (!assign_parameter_value(k, v, false, false, assignment_error)) {
            std::wcerr << L"ksh: " << assignment_error << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    // Builtin group: process control and shell state.
    // Built-in: exec
    if (cmd == L"exec") {
        if (tokens.size() == 1) {
            if (!redir.custom_fd_actions.empty()) {
                std::wstring custom_fd_error;
                if (!apply_persistent_custom_fd_actions(redir, custom_fd_error)) {
                    std::wcerr << custom_fd_error << L"\n";
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
            }

            HANDLE std_in = nullptr;
            HANDLE std_out = nullptr;
            HANDLE std_err = nullptr;
            bool close_in = false;
            bool close_out = false;
            bool close_err = false;
            
            if (setup_redirection_handles(&redir, std_in, std_out, std_err, close_in, close_out, close_err)) {
                if (redir.has_stdin) {
                    SetStdHandle(STD_INPUT_HANDLE, std_in);
                }
                if (redir.has_stdout) {
                    SetStdHandle(STD_OUTPUT_HANDLE, std_out);
                }
                if (redir.has_stderr) {
                    SetStdHandle(STD_ERROR_HANDLE, std_err);
                }
                ksh_env.variables[L"?"] = L"0";
                return true;
            } else {
                std::wcerr << L"ksh: exec: redirection failed\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
        } else {
            if (!redir.custom_fd_actions.empty()) {
                std::wcerr << L"ksh: exec: custom fd redirection (3-9) with command replacement is not supported\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            std::vector<std::wstring> exec_tokens(tokens.begin() + 1, tokens.end());
            std::wstring cmd_line = join_tokens_as_command_line(exec_tokens);
            
            STARTUPINFOW si;
            PROCESS_INFORMATION pi;
            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            ZeroMemory(&pi, sizeof(pi));
            
            HANDLE std_in = nullptr;
            HANDLE std_out = nullptr;
            HANDLE std_err = nullptr;
            bool close_in = false;
            bool close_out = false;
            bool close_err = false;
            if (!setup_redirection_handles(&redir, std_in, std_out, std_err, close_in, close_out, close_err)) {
                std::wcerr << L"ksh: exec: redirection failed\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdInput = std_in;
            si.hStdOutput = std_out;
            si.hStdError = std_err;
            std::vector<HANDLE> inherited_handles = { std_in, std_out, std_err };

            if (create_process_with_handle_list(cmd_line, si, inherited_handles, 0, pi)) {
                WaitForSingleObject(pi.hProcess, INFINITE);
                DWORD exit_code = 0;
                GetExitCodeProcess(pi.hProcess, &exit_code);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                close_redirection_handles(std_in, std_out, std_err, close_in, close_out, close_err);
                exit(static_cast<int>(exit_code));
            } else {
                close_redirection_handles(std_in, std_out, std_err, close_in, close_out, close_err);
                std::wcerr << L"ksh: exec: failed to execute " << exec_tokens[0] << L"\n";
                ksh_env.variables[L"?"] = L"127";
                return false;
            }
        }
    }

    // Builtin group: permissions and terminal state.
    // Built-in: umask
    if (cmd == L"umask") {
        bool symbolic = false;
        std::wstring mask_arg;
        for (size_t i = 1; i < tokens.size(); ++i) {
            if (tokens[i] == L"-S") {
                symbolic = true;
            } else {
                mask_arg = tokens[i];
            }
        }
        
        if (mask_arg.empty()) {
            if (symbolic) {
                int u = (g_umask >> 6) & 7;
                int g = (g_umask >> 3) & 7;
                int o = g_umask & 7;
                auto to_sym = [](int bits) -> std::wstring {
                    std::wstring res;
                    if (!(bits & 4)) res += L"r";
                    if (!(bits & 2)) res += L"w";
                    if (!(bits & 1)) res += L"x";
                    return res;
                };
                std::wstring symbolic_out = L"u=" + to_sym(u) + L",g=" + to_sym(g) + L",o=" + to_sym(o) + L"\n";
                if (!write_builtin_output(symbolic_out)) {
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
            } else {
                wchar_t buf[32];
                swprintf(buf, 32, L"%04o\n", g_umask);
                if (!write_builtin_output(buf)) {
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        } else {
            wchar_t* end_ptr = nullptr;
            errno = 0;
            long new_mask = std::wcstol(mask_arg.c_str(), &end_ptr, 8);
            if (errno != 0 || end_ptr == mask_arg.c_str() || (end_ptr != nullptr && *end_ptr != L'\0') || new_mask < 0 || new_mask > 0777) {
                std::wcerr << L"ksh: umask: invalid mask: " << mask_arg << L"\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            g_umask = static_cast<int>(new_mask);
            ksh_env.variables[L"?"] = L"0";
            return true;
        }
    }

    // Builtin group: terminal and timing utilities.
    // Built-in: stty
    if (cmd == L"stty") {
        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
        DWORD mode = 0;
        bool has_mode = GetConsoleMode(hIn, &mode) != FALSE;
        
        if (tokens.size() == 1) {
            if (has_mode) {
                std::wstring settings;
                settings += (mode & ENABLE_ECHO_INPUT) ? L"echo " : L"-echo ";
                settings += (mode & ENABLE_LINE_INPUT) ? L"icanon " : L"-icanon ";
                settings += (mode & ENABLE_PROCESSED_INPUT) ? L"isig " : L"-isig ";
                
                CONSOLE_SCREEN_BUFFER_INFO csbi;
                if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
                    int rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
                    int cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
                    settings += L"speed 38400 baud; rows " + std::to_wstring(rows) + L"; columns " + std::to_wstring(cols) + L";";
                }
                if (!write_builtin_output(settings + L"\n")) {
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
                ksh_env.variables[L"?"] = L"0";
                return true;
            }
            std::wcerr << L"ksh: stty: standard input is not a console\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        
        bool ok = true;
        for (size_t i = 1; i < tokens.size(); ++i) {
            std::wstring arg = tokens[i];
            if (arg == L"size") {
                CONSOLE_SCREEN_BUFFER_INFO csbi;
                if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
                    int rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
                    int cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
                    std::wstring sz = std::to_wstring(rows) + L" " + std::to_wstring(cols) + L"\n";
                    if (!write_builtin_output(sz)) {
                        ok = false;
                    }
                } else {
                    std::wcerr << L"ksh: stty: failed to get console size\n";
                    ok = false;
                }
            } else if (has_mode) {
                if (arg == L"echo") {
                    SetConsoleMode(hIn, mode | ENABLE_ECHO_INPUT);
                    mode |= ENABLE_ECHO_INPUT;
                } else if (arg == L"-echo") {
                    SetConsoleMode(hIn, mode & ~ENABLE_ECHO_INPUT);
                    mode &= ~ENABLE_ECHO_INPUT;
                } else if (arg == L"icanon") {
                    SetConsoleMode(hIn, mode | ENABLE_LINE_INPUT);
                    mode |= ENABLE_LINE_INPUT;
                } else if (arg == L"-icanon") {
                    SetConsoleMode(hIn, mode & ~ENABLE_LINE_INPUT);
                    mode &= ~ENABLE_LINE_INPUT;
                } else if (arg == L"isig") {
                    SetConsoleMode(hIn, mode | ENABLE_PROCESSED_INPUT);
                    mode |= ENABLE_PROCESSED_INPUT;
                } else if (arg == L"-isig") {
                    SetConsoleMode(hIn, mode & ~ENABLE_PROCESSED_INPUT);
                    mode &= ~ENABLE_PROCESSED_INPUT;
                } else if (arg == L"raw") {
                    SetConsoleMode(hIn, mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT));
                    mode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
                } else if (arg == L"cooked" || arg == L"-raw") {
                    SetConsoleMode(hIn, mode | (ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT));
                    mode |= (ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
                } else {
                    std::wcerr << L"ksh: stty: unknown mode: " << arg << L"\n";
                    ok = false;
                }
            } else {
                std::wcerr << L"ksh: stty: standard input is not a console\n";
                ok = false;
            }
        }
        ksh_env.variables[L"?"] = ok ? L"0" : L"1";
        return ok;
    }

    // Built-in: times
    if (cmd == L"times") {
        FILETIME creationTime, exitTime, kernelTime, userTime;
        if (GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime)) {
            ULARGE_INTEGER uUser, uKernel;
            uUser.LowPart = userTime.dwLowDateTime;
            uUser.HighPart = userTime.dwHighDateTime;
            uKernel.LowPart = kernelTime.dwLowDateTime;
            uKernel.HighPart = kernelTime.dwHighDateTime;
            
            double sh_user = uUser.QuadPart / 10000000.0;
            double sh_kernel = uKernel.QuadPart / 10000000.0;
            double ch_user = g_child_user_time / 10000000.0;
            double ch_kernel = g_child_kernel_time / 10000000.0;
            
            wchar_t buf1[256], buf2[256];
            swprintf(buf1, 256, L"%dm%.2fs %dm%.2fs\n", static_cast<int>(sh_user / 60), sh_user - (static_cast<int>(sh_user / 60)) * 60, static_cast<int>(sh_kernel / 60), sh_kernel - (static_cast<int>(sh_kernel / 60)) * 60);
            swprintf(buf2, 256, L"%dm%.2fs %dm%.2fs\n", static_cast<int>(ch_user / 60), ch_user - (static_cast<int>(ch_user / 60)) * 60, static_cast<int>(ch_kernel / 60), ch_kernel - (static_cast<int>(ch_kernel / 60)) * 60);
            std::wstring out = std::wstring(buf1) + std::wstring(buf2);
            if (!write_builtin_output(out)) {
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        }
        std::wcerr << L"ksh: times failed\n";
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    // Builtin group: history and editing helpers.
    // Built-in: fc
    if (cmd == L"fc") {
        if (g_command_history.empty()) {
            load_command_history_from_file();
        }
        bool list_mode = false;
        bool suppress_numbers = false;
        bool reverse_order = false;
        std::wstring editor;
        std::wstring env_ed = get_environment_value(L"FCEDIT");
        if (env_ed.empty()) env_ed = get_environment_value(L"VISUAL");
        if (env_ed.empty()) env_ed = get_environment_value(L"EDITOR");
        if (!env_ed.empty()) editor = env_ed;
        else editor = L"notepad";

        std::wstring substitute_old;
        std::wstring substitute_new;
        bool has_substitution = false;
        std::vector<std::wstring> positional_args;
        
        for (size_t i = 1; i < tokens.size(); ++i) {
            std::wstring arg = tokens[i];
            if (arg == L"-s") {
                editor = L"-";
            } else if (!arg.empty() && arg[0] == L'-' && arg.size() > 1 && arg[1] != L'-') {
                for (size_t k = 1; k < arg.size(); ++k) {
                    wchar_t c = arg[k];
                    if (c == L'l') list_mode = true;
                    else if (c == L'n') suppress_numbers = true;
                    else if (c == L'r') reverse_order = true;
                    else if (c == L's') {
                        editor = L"-";
                    } else if (c == L'e' && i + 1 < tokens.size()) {
                        editor = tokens[++i];
                        break;
                    }
                }
            } else {
                size_t equal_pos = arg.find(L'=');
                if (editor == L"-" && equal_pos != std::wstring::npos && !has_substitution) {
                    substitute_old = arg.substr(0, equal_pos);
                    substitute_new = arg.substr(equal_pos + 1);
                    has_substitution = true;
                } else {
                    positional_args.push_back(arg);
                }
            }
        }
        
        int history_count = static_cast<int>(g_command_history.size());
        int first_idx = -1;
        int last_idx = -1;
        
        if (list_mode) {
            first_idx = (history_count > 16) ? history_count - 16 : 0;
            last_idx = (history_count > 0) ? history_count - 1 : 0;
        } else {
            first_idx = (history_count > 0) ? history_count - 1 : 0;
            last_idx = first_idx;
        }
        
        auto parse_positional = [&](const std::wstring& arg, int& idx) -> bool {
            int num = 0;
            if (!try_parse_int_strict(arg, num)) {
                for (int k = history_count - 1; k >= 0; --k) {
                    if (g_command_history[k].command.rfind(arg, 0) == 0) {
                        idx = k;
                        return true;
                    }
                }
                return false;
            }

            if (num < 0) {
                idx = history_count + num;
            } else {
                idx = num - 1;
            }
            if (idx < 0) idx = 0;
            if (idx >= history_count) idx = history_count - 1;
            return true;
        };
        
        if (positional_args.size() >= 1) {
            if (!parse_positional(positional_args[0], first_idx)) {
                std::wcerr << L"ksh: fc: history specification not found: " << positional_args[0] << L"\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            if (positional_args.size() >= 2) {
                if (!parse_positional(positional_args[1], last_idx)) {
                    std::wcerr << L"ksh: fc: history specification not found: " << positional_args[1] << L"\n";
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
            } else {
                last_idx = list_mode ? (history_count - 1) : first_idx;
            }
        }
        
        if (list_mode) {
            int start = first_idx;
            int end = last_idx;
            int step = (start <= end) ? 1 : -1;
            if (reverse_order) {
                std::swap(start, end);
                step = -step;
            }
            
            bool print_ok = true;
            for (int k = start; ; k += step) {
                if (k < 0 || k >= history_count) break;
                
                std::wstring line;
                if (!suppress_numbers) {
                    line += std::to_wstring(k + 1) + L"\t";
                }
                line += g_command_history[k].command + L"\n";
                if (!write_builtin_output(line)) {
                    print_ok = false;
                    break;
                }
                if (k == end) break;
            }
            ksh_env.variables[L"?"] = print_ok ? L"0" : L"1";
            return print_ok;
        } else {
            bool re_execute = (editor == L"-");
            wchar_t temp_path[MAX_PATH];
            wchar_t temp_file[MAX_PATH];
            if (GetTempPathW(MAX_PATH, temp_path) == 0 || GetTempFileNameW(temp_path, L"ksh_fc", 0, temp_file) == 0) {
                std::wcerr << L"ksh: fc: failed to create temporary file\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            
            {
                std::wofstream out(temp_file);
                if (!out) {
                    std::wcerr << L"ksh: fc: failed to open temporary file\n";
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
                int start = first_idx;
                int end = last_idx;
                int step = (start <= end) ? 1 : -1;
                if (reverse_order) {
                    std::swap(start, end);
                    step = -step;
                }
                for (int k = start; ; k += step) {
                    if (k >= 0 && k < history_count) {
                        std::wstring cmd_text = g_command_history[k].command;
                        if (has_substitution && !substitute_old.empty()) {
                            size_t sub_pos = cmd_text.find(substitute_old);
                            if (sub_pos != std::wstring::npos) {
                                cmd_text.replace(sub_pos, substitute_old.size(), substitute_new);
                            }
                        }
                        out << cmd_text << L"\n";
                    }
                    if (k == end) break;
                }
            }
            
            if (re_execute) {
                std::wifstream in(temp_file);
                std::wstring line;
                bool ok = true;
                while (std::getline(in, line)) {
                    std::wstring trimmed = trim_copy(line);
                    if (!trimmed.empty()) {
                        write_builtin_output(trimmed + L"\n");
                        bool should_exit = false;
                        ok = execute_command_line_impl(trimmed, should_exit);
                        if (should_exit) exit(0);
                    }
                }
                DeleteFileW(temp_file);
                ksh_env.variables[L"?"] = ok ? L"0" : L"1";
                return ok;
            } else {
                std::wstring cmd_line = quote_command_argument(editor) + L" " + quote_command_argument(temp_file);
                STARTUPINFOW si;
                PROCESS_INFORMATION pi;
                ZeroMemory(&si, sizeof(si));
                si.cb = sizeof(si);
                ZeroMemory(&pi, sizeof(pi));
                
                si.dwFlags = STARTF_USESTDHANDLES;
                si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
                si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
                si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
                std::vector<HANDLE> inherited_handles = { si.hStdInput, si.hStdOutput, si.hStdError };

                if (create_process_with_handle_list(cmd_line, si, inherited_handles, 0, pi)) {
                    WaitForSingleObject(pi.hProcess, INFINITE);
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                    
                    std::wifstream in(temp_file);
                    std::wstring line;
                    bool ok = true;
                    while (std::getline(in, line)) {
                        std::wstring trimmed = trim_copy(line);
                        if (!trimmed.empty()) {
                            write_builtin_output(trimmed + L"\n");
                            bool should_exit = false;
                            ok = execute_command_line_impl(trimmed, should_exit);
                            if (should_exit) exit(0);
                        }
                    }
                    DeleteFileW(temp_file);
                    ksh_env.variables[L"?"] = ok ? L"0" : L"1";
                    return ok;
                } else {
                    std::wcerr << L"ksh: fc: failed to launch editor: " << editor << L"\n";
                    DeleteFileW(temp_file);
                    ksh_env.variables[L"?"] = L"1";
                    return false;
                }
            }
        }
    }

    // Builtin group: system metadata and path helpers.
    // Built-in: getconf
    if (cmd == L"getconf") {
        return execute_builtin_getconf(tokens, write_builtin_output);
    }

    // Built-in: pathchk
    if (cmd == L"pathchk") {
        return execute_builtin_pathchk(tokens);
    }

    // Builtin group: positional and parameter utilities.
    // Built-in: :
    if (cmd == L":") {
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    // Built-in: shift
    if (cmd == L"shift") {
        size_t n = 1;
        if (tokens.size() > 1) {
            int parsed = 0;
            if (!try_parse_positive_int_strict(tokens[1], parsed)) {
                std::wcerr << L"ksh: shift: invalid shift count: " << tokens[1] << L"\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            n = static_cast<size_t>(parsed);
        }
        
        if (g_script_context_stack.empty()) {
            ksh_env.variables[L"?"] = L"0";
            return true;
        }
        
        std::vector<std::wstring>& args = g_script_context_stack.back().args;
        if (n > args.size()) {
            std::wcerr << L"ksh: shift: shift count out of range\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        
        size_t old_size = args.size();
        args.erase(args.begin(), args.begin() + n);
        
        // Sync $#, $*, $@, and individual $1..$N
        ksh_env.variables[L"#"] = std::to_wstring(args.size());
        std::wstring star;
        for (size_t k = 0; k < args.size(); ++k) {
            if (k > 0) star += L" ";
            star += args[k];
        }
        ksh_env.variables[L"*"] = star;
        ksh_env.variables[L"@"] = star;
        for (size_t k = 0; k < args.size(); ++k) {
            ksh_env.variables[std::to_wstring(k + 1)] = args[k];
        }
        for (size_t k = args.size(); k < old_size; ++k) {
            ksh_env.variables.erase(std::to_wstring(k + 1));
        }

        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    // Builtin group: directory and prompt-oriented utilities.
    // Built-in: cd
    if (cmd == L"cd") {
        wchar_t prev_cwd[MAX_PATH];
        std::wstring old_cwd_str;
        if (GetCurrentDirectoryW(MAX_PATH, prev_cwd) > 0) {
            old_cwd_str = prev_cwd;
        }

        std::wstring target;
        bool print_after = false;

        if (tokens.size() <= 1) {
            std::wstring home = get_environment_value(L"HOME");
            if (home.empty()) {
                home = get_environment_value(L"USERPROFILE");
            }
            if (home.empty()) {
                home = L"C:\\";
            }
            target = home;
        } else if (tokens[1] == L"-") {
            std::wstring oldpwd = get_environment_value(L"OLDPWD");
            if (oldpwd.empty()) {
                std::wcerr << L"ksh: cd: OLDPWD not set\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            target = oldpwd;
            print_after = true;
        } else {
            target = tokens[1];
        }

        std::wstring p = normalize_cd_path(target);
        if (SetCurrentDirectoryW(p.c_str())) {
            wchar_t new_cwd[MAX_PATH];
            if (GetCurrentDirectoryW(MAX_PATH, new_cwd) > 0) {
                if (!old_cwd_str.empty()) {
                    ksh_env.variables[L"OLDPWD"] = old_cwd_str;
                    SetEnvironmentVariableW(L"OLDPWD", old_cwd_str.c_str());
                }
                std::wstring new_cwd_str = new_cwd;
                ksh_env.variables[L"PWD"] = new_cwd_str;
                SetEnvironmentVariableW(L"PWD", new_cwd_str.c_str());
                if (print_after) {
                    write_builtin_output(new_cwd_str + L"\n");
                }
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        }
        std::wcerr << L"ksh: cd failed: " << p << L"\n";
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    // Built-in: pwd
    if (cmd == L"pwd") {
        wchar_t currentDir[MAX_PATH];
        if (GetCurrentDirectoryW(MAX_PATH, currentDir) > 0) {
            std::wstring dir_str = currentDir;
            dir_str += L"\n";
            if (!write_builtin_output(dir_str)) {
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
            ksh_env.variables[L"?"] = L"0";
            return true;
        }
        std::wcerr << L"ksh: pwd failed\n";
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    // Builtin group: formatted output.
    // Built-in: printf
    if (cmd == L"printf") {
        std::wstring output_str;
        std::wstring error_msg;
        if (!evaluate_builtin_printf(tokens, output_str, error_msg)) {
            std::wcerr << error_msg << L"\n";
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        if (!write_builtin_output(output_str)) {
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    // Built-in: print / echo
    if (cmd == L"print" || cmd == L"echo") {
        bool omit_newline = false;
        bool raw_mode = false;
        bool use_coprocess_output = false;
        size_t start_idx = 1;

        if (cmd == L"print") {
            while (start_idx < tokens.size()) {
                const std::wstring& arg = tokens[start_idx];
                if (arg.size() > 1 && arg[0] == L'-') {
                    bool recognized = false;
                    bool opt_n = false;
                    bool opt_r = false;
                    for (size_t j = 1; j < arg.size(); ++j) {
                        if (arg[j] == L'n') {
                            opt_n = true;
                            recognized = true;
                        } else if (arg[j] == L'r') {
                            opt_r = true;
                            recognized = true;
                        } else if (arg[j] == L'p' && g_coprocess.active) {
                            use_coprocess_output = true;
                            recognized = true;
                        } else {
                            recognized = false;
                            break;
                        }
                    }
                    if (recognized) {
                        if (opt_n) omit_newline = true;
                        if (opt_r) raw_mode = true;
                        start_idx++;
                    } else {
                        break;
                    }
                } else {
                    break;
                }
            }

            if (start_idx < tokens.size() && tokens[start_idx].size() > 1 &&
                tokens[start_idx][0] == L'-' && tokens[start_idx].find(L'p') != std::wstring::npos &&
                !g_coprocess.active) {
                std::wcerr << L"ksh: print -p: no active co-process\n";
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
        } else if (cmd == L"echo") {
            if (tokens.size() > 1 && tokens[1] == L"-n") {
                omit_newline = true;
                start_idx = 2;
            }
        }

        std::wstring line;
        for (size_t i = start_idx; i < tokens.size(); ++i) {
            line += tokens[i];
            if (i + 1 < tokens.size()) {
                line += L" ";
            }
        }

        if (cmd == L"print" && !raw_mode) {
            std::wstring expanded_line;
            for (size_t i = 0; i < line.size(); ++i) {
                if (line[i] == L'\\' && i + 1 < line.size()) {
                    wchar_t next = line[i + 1];
                    if (next == L'n') { expanded_line += L'\n'; i++; }
                    else if (next == L't') { expanded_line += L'\t'; i++; }
                    else if (next == L'v') { expanded_line += L'\v'; i++; }
                    else if (next == L'b') { expanded_line += L'\b'; i++; }
                    else if (next == L'r') { expanded_line += L'\r'; i++; }
                    else if (next == L'f') { expanded_line += L'\f'; i++; }
                    else if (next == L'a') { expanded_line += L'\a'; i++; }
                    else if (next == L'\\') { expanded_line += L'\\'; i++; }
                    else { expanded_line += line[i]; }
                } else {
                    expanded_line += line[i];
                }
            }
            line = expanded_line;
        }

        if (!omit_newline) {
            line += L"\n";
        }

        if (use_coprocess_output) {
            if (line.empty()) {
                ksh_env.variables[L"?"] = L"0";
                return true;
            }

            const int utf8_length = WideCharToMultiByte(
                CP_UTF8,
                0,
                line.c_str(),
                static_cast<int>(line.size()),
                nullptr,
                0,
                nullptr,
                nullptr);
            if (utf8_length <= 0) {
                ksh_env.variables[L"?"] = L"1";
                return false;
            }

            std::string utf8(static_cast<size_t>(utf8_length), '\0');
            WideCharToMultiByte(
                CP_UTF8,
                0,
                line.c_str(),
                static_cast<int>(line.size()),
                &utf8[0],
                utf8_length,
                nullptr,
                nullptr);
            DWORD written = 0;
            if (!WriteFile(g_coprocess.input_write, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr) ||
                written != utf8.size()) {
                ksh_env.variables[L"?"] = L"1";
                return false;
            }
        } else if (!write_builtin_output(line)) {
            ksh_env.variables[L"?"] = L"1";
            return false;
        }
        ksh_env.variables[L"?"] = L"0";
        return true;
    }

    // Execute external processes or system fallback
    if (!run_in_background) {
        const ULONGLONG launch_start = perf_trace ? GetTickCount64() : 0;
        DWORD exit_code = 1;
        bool launched = execute_native_or_fallback(external_command_to_run, &redir, &exit_code);
        ksh_env.variables[L"?"] = std::to_wstring(exit_code);
        if (perf_trace) {
            log_performance_trace(L"execute.command_launch", GetTickCount64() - launch_start, L"background=0 exit=" + std::to_wstring(exit_code));
            log_performance_trace(L"execute.total", GetTickCount64() - command_start);
        }
        return launched;
    }

    const ULONGLONG launch_start = perf_trace ? GetTickCount64() : 0;
    HANDLE process_handle = nullptr;
    DWORD pid = 0;
    if (!launch_process(external_command_to_run, process_handle, pid, &redir)) {
        if (perf_trace) {
            log_performance_trace(L"execute.command_launch", GetTickCount64() - launch_start, L"background=1 failed");
        }
        std::wcerr << L"ksh: failed to start background command\n";
        ksh_env.variables[L"?"] = L"1";
        return false;
    }

    BackgroundJob job;
    job.id = g_next_job_id++;
    job.pid = pid;
    job.process_handle = process_handle;
    job.pids.push_back(pid);
    job.process_handles.push_back(process_handle);
    job.command = command_to_run;
    job.completed = false;
    job.exit_code = STILL_ACTIVE;
    job.completion_reported = false;
    g_background_jobs.push_back(job);

    std::wcout << L"[" << job.id << L"] " << job.pid << L"\n";
    ksh_env.variables[L"?"] = L"0";
    if (perf_trace) {
        log_performance_trace(L"execute.command_launch", GetTickCount64() - launch_start, L"background=1 pid=" + std::to_wstring(pid));
        log_performance_trace(L"execute.total", GetTickCount64() - command_start);
    }
    return true;
}

