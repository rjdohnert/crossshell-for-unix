/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "../include/ksh_internal.h"

double evaluate_math_function(const std::wstring& function_name, const std::vector<double>& args) {
    std::wstring name = to_lower_copy(function_name);

    if (name == L"abs" && args.size() == 1) return std::fabs(args[0]);
    if (name == L"sqrt" && args.size() == 1) return std::sqrt(args[0]);
    if (name == L"pow" && args.size() == 2) return std::pow(args[0], args[1]);
    if (name == L"min" && args.size() == 2) return (args[0] < args[1]) ? args[0] : args[1];
    if (name == L"max" && args.size() == 2) return (args[0] > args[1]) ? args[0] : args[1];
    if (name == L"sin" && args.size() == 1) return std::sin(args[0]);
    if (name == L"cos" && args.size() == 1) return std::cos(args[0]);
    if (name == L"tan" && args.size() == 1) return std::tan(args[0]);
    if (name == L"asin" && args.size() == 1) return std::asin(args[0]);
    if (name == L"acos" && args.size() == 1) return std::acos(args[0]);
    if (name == L"atan" && args.size() == 1) return std::atan(args[0]);
    if (name == L"log" && args.size() == 1) return std::log(args[0]);
    if (name == L"log10" && args.size() == 1) return std::log10(args[0]);
    if (name == L"exp" && args.size() == 1) return std::exp(args[0]);
    if (name == L"floor" && args.size() == 1) return std::floor(args[0]);
    if (name == L"ceil" && args.size() == 1) return std::ceil(args[0]);
    if (name == L"round" && args.size() == 1) return std::round(args[0]);

    throw std::runtime_error("unsupported function");
}

static std::wstring get_variable_value_simple(const std::wstring& name) {
    std::wstring resolved = resolve_variable_name(name);
    std::wstring base_name;
    std::wstring index_expr;
    bool has_index = false;
    if (parse_array_reference_expression(resolved, base_name, index_expr, has_index)) {
        auto arr_it = ksh_env.arrays.find(base_name);
        if (arr_it != ksh_env.arrays.end()) {
            if (!has_index) {
                auto val_it = arr_it->second.find(L"0");
                return (val_it != arr_it->second.end()) ? val_it->second : L"0";
            }
            bool is_assoc = get_flag_value(ksh_env.associative_flags, base_name);
            std::wstring key = index_expr;
            if (!is_assoc) {
                size_t idx = 0;
                if (parse_non_negative_index(index_expr, idx)) {
                    key = std::to_wstring(idx);
                }
            }
            auto val_it = arr_it->second.find(key);
            return (val_it != arr_it->second.end()) ? val_it->second : L"0";
        }
    }
    auto it = ksh_env.variables.find(resolved);
    if (it != ksh_env.variables.end()) {
        return it->second;
    }
    return L"0";
}

class ArithmeticParser {
public:
    explicit ArithmeticParser(const std::wstring& expression) : expr(expression), pos(0), depth(0), has_float_operands(false) {}

    bool is_float() const { return has_float_operands; }

    static long long to_safe_int64(double v) {
        if (!std::isfinite(v)) return 0;
        if (v >= static_cast<double>(LLONG_MAX)) return LLONG_MAX;
        if (v <= static_cast<double>(LLONG_MIN)) return LLONG_MIN;
        return static_cast<long long>(v);
    }

    double parse() {
        double value = parse_expression();
        skip_spaces();
        if (pos != expr.size()) {
            throw std::runtime_error("unexpected trailing characters");
        }
        return value;
    }

private:
    const std::wstring& expr;
    size_t pos;
    int depth;
    bool has_float_operands;

    void check_depth() {
        if (depth >= kMaxArithmeticParseDepth) {
            throw std::runtime_error("arithmetic expression nesting too deep");
        }
    }

    void skip_spaces() {
        while (pos < expr.size() && std::iswspace(expr[pos])) {
            pos++;
        }
    }

    bool consume(wchar_t token) {
        skip_spaces();
        if (pos < expr.size() && expr[pos] == token) {
            pos++;
            return true;
        }
        return false;
    }

    bool consume(const wchar_t* token) {
        skip_spaces();
        const size_t length = wcslen(token);
        if (expr.compare(pos, length, token) == 0) {
            pos += length;
            return true;
        }
        return false;
    }

    double parse_expression() {
        check_depth();
        depth++;

        size_t saved_pos = pos;
        skip_spaces();
        if (pos < expr.size() && (std::iswalpha(expr[pos]) || expr[pos] == L'_')) {
            std::wstring var = parse_identifier();
            skip_spaces();
            bool parsed_subscript = true;
            std::wstring subscript_expr;
            if (pos < expr.size() && expr[pos] == L'[') {
                pos++;
                int bracket_depth = 1;
                while (pos < expr.size() && bracket_depth > 0) {
                    if (expr[pos] == L'[') {
                        bracket_depth++;
                    } else if (expr[pos] == L']') {
                        bracket_depth--;
                        if (bracket_depth == 0) {
                            pos++;
                            break;
                        }
                    }
                    subscript_expr.push_back(expr[pos]);
                    pos++;
                }
                if (bracket_depth > 0) {
                    parsed_subscript = false;
                }
            }

            if (parsed_subscript) {
                skip_spaces();
                bool is_plus_eq = (pos + 1 < expr.size() && expr[pos] == L'+' && expr[pos + 1] == L'=');
                bool is_minus_eq = (pos + 1 < expr.size() && expr[pos] == L'-' && expr[pos + 1] == L'=');
                bool is_eq = (pos < expr.size() && expr[pos] == L'=' && (pos + 1 >= expr.size() || expr[pos + 1] != L'='));

                if (is_eq || is_plus_eq || is_minus_eq) {
                    if (is_eq) pos++;
                    else pos += 2;

                    double rhs = parse_expression();

                    std::wstring lhs_target = var;
                    if (!subscript_expr.empty()) {
                        ArithmeticParser sub_parser(subscript_expr);
                        double idx_val = sub_parser.parse();
                        std::wstring idx_str;
                        if (idx_val == std::floor(idx_val)) {
                            idx_str = std::to_wstring(static_cast<long long>(idx_val));
                        } else {
                            idx_str = std::to_wstring(idx_val);
                            size_t dot_or_comma = idx_str.find_first_of(L".,");
                            if (dot_or_comma != std::wstring::npos) {
                                while (!idx_str.empty() && idx_str.back() == L'0') {
                                    idx_str.pop_back();
                                }
                                if (!idx_str.empty() && (idx_str.back() == L'.' || idx_str.back() == L',')) {
                                    idx_str.pop_back();
                                }
                            }
                        }
                        lhs_target += L"[" + idx_str + L"]";
                    }

                    double lhs_val = 0.0;
                    if (is_plus_eq || is_minus_eq) {
                        std::wstring current_str = get_variable_value_simple(lhs_target);
                        try {
                            lhs_val = std::stod(current_str);
                        } catch (...) {
                            lhs_val = 0.0;
                        }
                    }

                    double final_val = rhs;
                    if (is_plus_eq) final_val = lhs_val + rhs;
                    else if (is_minus_eq) final_val = lhs_val - rhs;

                    std::wstring val_str;
                    if (final_val == std::floor(final_val)) {
                        val_str = std::to_wstring(static_cast<long long>(final_val));
                    } else {
                        val_str = std::to_wstring(final_val);
                        size_t dot_or_comma = val_str.find_first_of(L".,");
                        if (dot_or_comma != std::wstring::npos) {
                            while (!val_str.empty() && val_str.back() == L'0') {
                                val_str.pop_back();
                            }
                            if (!val_str.empty() && (val_str.back() == L'.' || val_str.back() == L',')) {
                                val_str.pop_back();
                            }
                        }
                    }

                    std::wstring err;
                    assign_parameter_value(lhs_target, val_str, false, false, err, false);
                    depth--;
                    return final_val;
                }
            }
        }

        pos = saved_pos;
        double value = parse_ternary();
        depth--;
        return value;
    }

    double parse_ternary() {
        check_depth();
        depth++;
        double condition = parse_logical_or();
        if (!consume(L'?')) {
            depth--;
            return condition;
        }
        const double when_true = parse_expression();
        if (!consume(L':')) {
            depth--;
            throw std::runtime_error("missing ':' in ternary expression");
        }
        const double when_false = parse_ternary();
        depth--;
        return condition != 0.0 ? when_true : when_false;
    }

    double parse_logical_or() {
        double value = parse_logical_and();
        while (consume(L"||")) {
            const double right = parse_logical_and();
            value = (value != 0.0 || right != 0.0) ? 1.0 : 0.0;
        }
        return value;
    }

    double parse_logical_and() {
        double value = parse_bitwise_or();
        while (consume(L"&&")) {
            const double right = parse_bitwise_or();
            value = (value != 0.0 && right != 0.0) ? 1.0 : 0.0;
        }
        return value;
    }

    double parse_bitwise_or() {
        double value = parse_bitwise_xor();
        while (true) {
            skip_spaces();
            if (expr.compare(pos, 2, L"||") == 0 || !consume(L'|')) break;
            value = static_cast<double>(to_safe_int64(value) | to_safe_int64(parse_bitwise_xor()));
        }
        return value;
    }

    double parse_bitwise_xor() {
        double value = parse_bitwise_and();
        while (consume(L'^')) value = static_cast<double>(to_safe_int64(value) ^ to_safe_int64(parse_bitwise_and()));
        return value;
    }

    double parse_bitwise_and() {
        double value = parse_equality();
        while (true) {
            skip_spaces();
            if (expr.compare(pos, 2, L"&&") == 0 || !consume(L'&')) break;
            value = static_cast<double>(to_safe_int64(value) & to_safe_int64(parse_equality()));
        }
        return value;
    }

    double parse_equality() {
        double value = parse_relational();
        while (true) {
            if (consume(L"==")) value = value == parse_relational() ? 1.0 : 0.0;
            else if (consume(L"!=")) value = value != parse_relational() ? 1.0 : 0.0;
            else break;
        }
        return value;
    }

    double parse_relational() {
        double value = parse_shift();
        while (true) {
            if (consume(L"<=")) value = value <= parse_shift() ? 1.0 : 0.0;
            else if (consume(L">=")) value = value >= parse_shift() ? 1.0 : 0.0;
            else if (consume(L'<')) value = value < parse_shift() ? 1.0 : 0.0;
            else if (consume(L'>')) value = value > parse_shift() ? 1.0 : 0.0;
            else break;
        }
        return value;
    }

    double parse_shift() {
        double value = parse_additive();
        while (true) {
            if (consume(L"<<")) {
                double rhs = parse_additive();
                long long lhs_i = to_safe_int64(value);
                long long shift_cnt = to_safe_int64(rhs);
                if (shift_cnt < 0 || shift_cnt >= 64) {
                    value = 0.0;
                } else {
                    value = static_cast<double>(static_cast<unsigned long long>(lhs_i) << shift_cnt);
                }
            } else if (consume(L">>")) {
                double rhs = parse_additive();
                long long lhs_i = to_safe_int64(value);
                long long shift_cnt = to_safe_int64(rhs);
                if (shift_cnt < 0) {
                    value = 0.0;
                } else if (shift_cnt >= 64) {
                    value = (lhs_i < 0) ? -1.0 : 0.0;
                } else {
                    value = static_cast<double>(lhs_i >> shift_cnt);
                }
            } else {
                break;
            }
        }
        return value;
    }

    double parse_additive() {
        double value = parse_term();
        while (true) {
            if (consume(L'+')) value += parse_term();
            else if (consume(L'-')) value -= parse_term();
            else break;
        }
        return value;
    }

    double parse_term() {
        double value = parse_power();
        while (true) {
            if (consume(L'*')) {
                value *= parse_power();
            } else if (consume(L'/')) {
                double right = parse_power();
                if (right == 0.0) {
                    throw std::runtime_error("division by zero");
                }
                if (!has_float_operands && std::floor(value) == value && std::floor(right) == right) {
                    long long i_val = to_safe_int64(value);
                    long long i_right = to_safe_int64(right);
                    value = static_cast<double>(i_val / i_right);
                } else {
                    has_float_operands = true;
                    value /= right;
                }
            } else if (consume(L'%')) {
                double right = parse_power();
                if (right == 0.0) {
                    throw std::runtime_error("modulo by zero");
                }
                value = std::fmod(value, right);
            } else {
                break;
            }
        }
        return value;
    }

    double parse_power() {
        double left = parse_unary();
        if (consume(L"**")) {
            check_depth();
            depth++;
            double right = parse_power();
            depth--;
            return std::pow(left, right);
        }
        return left;
    }

    double parse_unary() {
        check_depth();
        depth++;
        double result;
        if (consume(L'+')) {
            result = parse_unary();
        } else if (consume(L'-')) {
            result = -parse_unary();
        } else {
            result = parse_primary();
        }
        depth--;
        return result;
    }

    std::wstring parse_identifier() {
        skip_spaces();
        std::wstring identifier;
        while (pos < expr.size() && (std::iswalnum(expr[pos]) || expr[pos] == L'_')) {
            identifier.push_back(expr[pos]);
            pos++;
        }
        return identifier;
    }

    double parse_number() {
        skip_spaces();
        size_t start = pos;
        size_t digit_start = pos;
        while (pos < expr.size() && std::iswdigit(expr[pos])) {
            pos++;
        }

        if (pos < expr.size() && expr[pos] == L'#') {
            std::wstring base_str = expr.substr(digit_start, pos - digit_start);
            if (base_str.empty()) {
                throw std::runtime_error("missing base for arithmetic conversion");
            }
            int base = 0;
            if (!try_parse_int_strict(base_str, base)) {
                throw std::runtime_error("invalid arithmetic base");
            }
            if (base < 2 || base > 64) {
                throw std::runtime_error("invalid arithmetic base (must be 2-64)");
            }

            pos++;
            double value = 0;
            bool read_any = false;
            while (pos < expr.size()) {
                wchar_t ch = expr[pos];
                int digit_val = -1;
                if (ch >= L'0' && ch <= L'9') {
                    digit_val = ch - L'0';
                } else if (ch >= L'a' && ch <= L'z') {
                    digit_val = (base <= 36) ? (ch - L'a' + 10) : (ch - L'a' + 10);
                } else if (ch >= L'A' && ch <= L'Z') {
                    digit_val = (base <= 36) ? (ch - L'A' + 10) : (ch - L'A' + 36);
                } else if (ch == L'_') {
                    digit_val = 62;
                } else if (ch == L'@') {
                    digit_val = 63;
                }

                if (digit_val >= 0 && digit_val < base) {
                    value = value * base + digit_val;
                    pos++;
                    read_any = true;
                } else {
                    break;
                }
            }

            if (!read_any) {
                throw std::runtime_error("invalid character for base arithmetic");
            }
            return value;
        }

        pos = start;
        bool seen_dot = false;
        while (pos < expr.size()) {
            wchar_t ch = expr[pos];
            if (std::iswdigit(ch)) {
                pos++;
                continue;
            }
            if (ch == L'.' && !seen_dot) {
                seen_dot = true;
                pos++;
                continue;
            }
            break;
        }
        if (start == pos) {
            throw std::runtime_error("number expected");
        }
        if (seen_dot) {
            has_float_operands = true;
        }
        return std::stod(expr.substr(start, pos - start));
    }

    double parse_primary() {
        skip_spaces();
        if (consume(L'(')) {
            double inner = parse_expression();
            if (!consume(L')')) {
                throw std::runtime_error("missing closing parenthesis");
            }
            return inner;
        }

        if (pos < expr.size() && (std::iswalpha(expr[pos]) || expr[pos] == L'_')) {
            std::wstring identifier = parse_identifier();
            std::wstring lowered = to_lower_copy(identifier);
            if (lowered == L"pi") {
                has_float_operands = true;
                return std::acos(-1.0);
            }
            if (lowered == L"e") {
                has_float_operands = true;
                return std::exp(1.0);
            }

            if (consume(L'(')) {
                std::vector<double> args;
                skip_spaces();
                if (!consume(L')')) {
                    while (true) {
                        args.push_back(parse_expression());
                        if (consume(L')')) {
                            break;
                        }
                        if (!consume(L',')) {
                            throw std::runtime_error("comma expected");
                        }
                    }
                }
                has_float_operands = true;
                return evaluate_math_function(identifier, args);
            }

            bool is_set = false;
            std::wstring val = get_variable_value_with_hooks(identifier, is_set);
            if (is_set) {
                if (val.find(L'.') != std::wstring::npos) {
                    has_float_operands = true;
                }
                return std::stod(val);
            }
            return 0.0;
        }

        return parse_number();
    }
};

std::wstring evaluate_arithmetic(const std::wstring& expr) {
    try {
        ArithmeticParser parser(expr);
        double value = parser.parse();
        if (!std::isfinite(value)) {
            return L"0";
        }

        if (!parser.is_float()) {
            return std::to_wstring(ArithmeticParser::to_safe_int64(value));
        }

        double nearest = std::round(value);
        if (std::fabs(value - nearest) < 1e-9) {
            return std::to_wstring(static_cast<long long>(nearest));
        }

        wchar_t buffer[64];
        _snwprintf_s(buffer, _countof(buffer), _TRUNCATE, L"%.10f", value);
        std::wstring result(buffer);
        while (!result.empty() && result.back() == L'0') {
            result.pop_back();
        }
        if (!result.empty() && (result.back() == L'.' || result.back() == L',')) {
            result.pop_back();
        }
        return result.empty() ? L"0" : result;
    } catch (...) {
        return L"0";
    }
}

double evaluate_arithmetic_double(const std::wstring& expr) {
    ArithmeticParser parser(expr);
    return parser.parse();
}

std::wstring trim_trailing_line_endings(const std::wstring& value) {
    size_t end = value.size();
    while (end > 0 && (value[end - 1] == L'\n' || value[end - 1] == L'\r')) {
        end--;
    }
    return value.substr(0, end);
}

std::wstring run_command_and_capture_stdout(const std::wstring& command, DWORD& exit_code) {
    exit_code = 1;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = nullptr;
    sa.bInheritHandle = TRUE;

    HANDLE read_pipe = nullptr;
    HANDLE write_pipe = nullptr;
    if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
        return L"";
    }

    if (!SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(read_pipe);
        CloseHandle(write_pipe);
        return L"";
    }

    std::wstring command_line;
    if (!build_cmd_shell_command_line(command, command_line)) {
        CloseHandle(read_pipe);
        CloseHandle(write_pipe);
        return L"";
    }
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    HANDLE inherited_stdin = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE inherited_stderr = GetStdHandle(STD_ERROR_HANDLE);
    HANDLE dup_stdin = nullptr;
    HANDLE dup_stderr = nullptr;

    if (inherited_stdin != nullptr && inherited_stdin != INVALID_HANDLE_VALUE) {
        if (!DuplicateHandle(GetCurrentProcess(), inherited_stdin, GetCurrentProcess(), &dup_stdin, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
            CloseHandle(read_pipe);
            CloseHandle(write_pipe);
            return L"";
        }
        inherited_stdin = dup_stdin;
    }

    if (inherited_stderr != nullptr && inherited_stderr != INVALID_HANDLE_VALUE) {
        if (!DuplicateHandle(GetCurrentProcess(), inherited_stderr, GetCurrentProcess(), &dup_stderr, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
            if (dup_stdin != nullptr) {
                CloseHandle(dup_stdin);
            }
            CloseHandle(read_pipe);
            CloseHandle(write_pipe);
            return L"";
        }
        inherited_stderr = dup_stderr;
    }

    si.hStdInput = inherited_stdin;
    si.hStdOutput = write_pipe;
    si.hStdError = inherited_stderr;

    std::vector<HANDLE> inherited_handles = { si.hStdInput, si.hStdOutput, si.hStdError };
    BOOL created = create_process_with_handle_list(command_line, si, inherited_handles, CREATE_NO_WINDOW, pi) ? TRUE : FALSE;

    CloseHandle(write_pipe);
    if (dup_stdin != nullptr) {
        CloseHandle(dup_stdin);
    }
    if (dup_stderr != nullptr) {
        CloseHandle(dup_stderr);
    }

    if (!created) {
        CloseHandle(read_pipe);
        return L"";
    }

    std::string stdout_bytes;
    char buffer[4096];
    DWORD bytes_read = 0;
    bool output_limit_hit = false;
    while (ReadFile(read_pipe, buffer, static_cast<DWORD>(sizeof(buffer)), &bytes_read, nullptr) && bytes_read > 0) {
        if (stdout_bytes.size() + bytes_read > kMaxCommandSubstitutionOutputBytes) {
            output_limit_hit = true;
            TerminateProcess(pi.hProcess, 1);
            break;
        }
        stdout_bytes.append(buffer, buffer + bytes_read);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    register_child_process_times(pi.hProcess);
    GetExitCodeProcess(pi.hProcess, &exit_code);

    if (output_limit_hit) {
        std::wcerr << L"ksh: command substitution output exceeded limit\n";
        exit_code = 1;
        g_expansion_error = true;
        ksh_env.variables[L"?"] = L"1";
    }

    CloseHandle(read_pipe);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    UINT cp = GetConsoleOutputCP();
    if (cp == 0) {
        cp = CP_OEMCP;
    }

    std::wstring text = decode_multibyte(stdout_bytes, CP_UTF8);
    if (text.empty() && !stdout_bytes.empty()) {
        text = decode_multibyte(stdout_bytes, cp);
    }
    if (text.empty() && !stdout_bytes.empty()) {
        text = decode_multibyte(stdout_bytes, CP_ACP);
    }

    return trim_trailing_line_endings(text);
}

bool parse_command_substitution_content(const std::wstring& input, size_t start_index, std::wstring& content, size_t& next_index) {
    content.clear();
    next_index = start_index;

    int depth = 1;
    bool in_single_quotes = false;
    bool in_double_quotes = false;
    bool escaped = false;

    size_t i = start_index;
    while (i < input.size()) {
        wchar_t ch = input[i];

        if (escaped) {
            content += ch;
            escaped = false;
            i++;
            continue;
        }

        if (ch == L'\\' && !in_single_quotes) {
            content += ch;
            escaped = true;
            i++;
            continue;
        }

        if (ch == L'\'' && !in_double_quotes) {
            in_single_quotes = !in_single_quotes;
            content += ch;
            i++;
            continue;
        }

        if (ch == L'"' && !in_single_quotes) {
            in_double_quotes = !in_double_quotes;
            content += ch;
            i++;
            continue;
        }

        if (!in_single_quotes && !in_double_quotes) {
            if (ch == L'$' && (i + 1) < input.size() && input[i + 1] == L'(' && !((i + 2) < input.size() && input[i + 2] == L'(')) {
                depth++;
                if (depth > kMaxSubstitutionParseDepth) {
                    return false;
                }
                content += L"$(";
                i += 2;
                continue;
            }

            if (ch == L'(') {
                depth++;
                if (depth > kMaxSubstitutionParseDepth) {
                    return false;
                }
                content += ch;
                i++;
                continue;
            }

            if (ch == L')') {
                depth--;
                if (depth == 0) {
                    next_index = i + 1;
                    return true;
                }
                content += ch;
                i++;
                continue;
            }
        }

        content += ch;
        i++;
    }

    return false;
}

bool parse_arithmetic_substitution_content(const std::wstring& input, size_t start_index, std::wstring& content, size_t& next_index) {
    content.clear();
    next_index = start_index;

    int paren_depth = 0;
    bool in_single_quotes = false;
    bool in_double_quotes = false;
    bool escaped = false;

    size_t i = start_index;
    while (i < input.size()) {
        wchar_t ch = input[i];

        if (escaped) {
            content += ch;
            escaped = false;
            i++;
            continue;
        }

        if (ch == L'\\' && !in_single_quotes) {
            content += ch;
            escaped = true;
            i++;
            continue;
        }

        if (ch == L'\'' && !in_double_quotes) {
            in_single_quotes = !in_single_quotes;
            content += ch;
            i++;
            continue;
        }

        if (ch == L'"' && !in_single_quotes) {
            in_double_quotes = !in_double_quotes;
            content += ch;
            i++;
            continue;
        }

        if (!in_single_quotes && !in_double_quotes) {
            if (ch == L'(') {
                paren_depth++;
                if (paren_depth > kMaxSubstitutionParseDepth) {
                    return false;
                }
                content += ch;
                i++;
                continue;
            }

            if (ch == L')') {
                if (paren_depth > 0) {
                    paren_depth--;
                    content += ch;
                    i++;
                    continue;
                }

                if ((i + 1) < input.size() && input[i + 1] == L')') {
                    next_index = i + 2;
                    return true;
                }
            }
        }

        content += ch;
        i++;
    }

    return false;
}

bool can_capture_builtin_command_substitution(const std::vector<std::wstring>& tokens) {
    if (tokens.empty()) {
        return false;
    }

    static const std::vector<std::wstring> capture_safe_builtins = {
        L":",
        L"echo",
        L"getconf",
        L"help",
        L"math",
        L"pathchk",
        L"print",
        L"printf",
        L"pwd",
        L"times",
    };

    if (std::find(capture_safe_builtins.begin(), capture_safe_builtins.end(), tokens[0]) == capture_safe_builtins.end()) {
        return false;
    }

    if (g_aliases.find(tokens[0]) != g_aliases.end()) {
        return false;
    }
    if (g_shell_functions.find(tokens[0]) != g_shell_functions.end()) {
        return false;
    }

    return true;
}

bool is_snapshot_safe_external_segment(const std::wstring& segment_text) {
    const std::wstring trimmed = trim_copy(segment_text);
    if (trimmed.empty()) {
        return false;
    }

    std::vector<std::wstring> tokens = ksh_tokenize_preserve_quotes(trimmed);
    if (tokens.empty()) {
        return false;
    }

    RedirectionSpec redir;
    std::wstring redirection_error;
    if (!parse_redirections(tokens, redir, redirection_error)) {
        return false;
    }

    if (tokens.empty()) {
        return false;
    }

    const std::wstring residual = join_tokens_with_spaces(tokens);
    if (has_unquoted_shell_metacharacters(residual)) {
        return false;
    }

    const std::wstring& first = tokens[0];
    size_t eq_pos = first.find(L'=');
    if (eq_pos != std::wstring::npos && eq_pos > 0) {
        std::wstring lhs = first.substr(0, eq_pos);
        if (is_valid_shell_identifier(lhs)) {
            return false;
        }
    }

    std::wstring detail;
    CommandResolutionKind kind = resolve_command_kind(first, detail);
    return kind == CommandResolutionKind::External;
}

bool command_requires_environment_snapshot(const std::wstring& command) {
    const std::wstring trimmed = trim_copy(command);
    if (trimmed.empty()) {
        return true;
    }

    if (!has_unquoted_shell_metacharacters(trimmed)) {
        return !is_snapshot_safe_external_segment(trimmed);
    }

    std::vector<std::wstring> pipeline_segments = split_pipeline_segments(trimmed);
    if (pipeline_segments.size() <= 1) {
        return true;
    }

    for (const std::wstring& segment : pipeline_segments) {
        if (!is_snapshot_safe_external_segment(segment)) {
            return true;
        }
    }

    return false;
}

std::wstring execute_command_substitution(const std::wstring& command, bool use_capture_sink) {
    const bool perf_trace = is_performance_telemetry_enabled();
    const ULONGLONG substitution_start = GetTickCount64();

    const bool require_state_snapshot = !use_capture_sink && command_requires_environment_snapshot(command);
    ScopedSubshellDepthGuard subshell_depth_guard;
    std::optional<ShellStateSnapshot> saved_snapshot;
    ScopedSnapshotRestoreGuard snapshot_guard(saved_snapshot, L"0");

    std::wstring sub_output;

    if (use_capture_sink) {
        struct CaptureSinkGuard {
            std::wstring*& slot;
            std::wstring* previous;
            CaptureSinkGuard(std::wstring*& slot_ref, std::wstring* current) : slot(slot_ref), previous(slot_ref) {
                slot = current;
            }
            ~CaptureSinkGuard() {
                slot = previous;
            }
        } capture_guard(g_builtin_capture_output, &sub_output);

        bool should_exit = false;
        execute_command_line(command, should_exit);
    } else {
        HANDLE read_pipe = INVALID_HANDLE_VALUE;
        HANDLE write_pipe = INVALID_HANDLE_VALUE;
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = nullptr;
        sa.bInheritHandle = TRUE;

        if (CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
            if (!SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0)) {
                CloseHandle(read_pipe);
                CloseHandle(write_pipe);
            } else {
                HANDLE old_subshell_stdout = g_subshell_stdout;
                g_subshell_stdout = write_pipe;
                std::string bytes;
                bool output_limit_hit = false;

                bool reader_started = false;
                std::thread reader_thread;
                try {
                    reader_thread = std::thread([&]() {
                        char buffer[4096];
                        DWORD bytesRead = 0;
                        while (ReadFile(read_pipe, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
                            if (output_limit_hit) {
                                continue;
                            }

                            const size_t current_size = bytes.size();
                            if (current_size + bytesRead > kMaxCommandSubstitutionOutputBytes) {
                                const size_t remaining = kMaxCommandSubstitutionOutputBytes - current_size;
                                if (remaining > 0) {
                                    bytes.append(buffer, remaining);
                                }
                                output_limit_hit = true;
                                continue;
                            }

                            bytes.append(buffer, bytesRead);
                        }
                    });
                    reader_started = true;
                } catch (...) {
                    g_subshell_stdout = old_subshell_stdout;
                    CloseHandle(write_pipe);
                    CloseHandle(read_pipe);
                }

                if (reader_started) {
                    try {
                        if (require_state_snapshot) {
                            saved_snapshot.emplace(capture_shell_state_snapshot());
                        }

                        bool should_exit = false;
                        execute_command_line(command, should_exit);
                    } catch (...) {
                        if (write_pipe != INVALID_HANDLE_VALUE) {
                            CloseHandle(write_pipe);
                            write_pipe = INVALID_HANDLE_VALUE;
                        }
                        g_subshell_stdout = old_subshell_stdout;
                        if (reader_thread.joinable()) {
                            reader_thread.join();
                        }
                        if (read_pipe != INVALID_HANDLE_VALUE) {
                            CloseHandle(read_pipe);
                            read_pipe = INVALID_HANDLE_VALUE;
                        }
                        throw;
                    }

                    if (write_pipe != INVALID_HANDLE_VALUE) {
                        CloseHandle(write_pipe);
                        write_pipe = INVALID_HANDLE_VALUE;
                    }
                    g_subshell_stdout = old_subshell_stdout;

                    if (reader_thread.joinable()) {
                        reader_thread.join();
                    }
                    if (read_pipe != INVALID_HANDLE_VALUE) {
                        CloseHandle(read_pipe);
                        read_pipe = INVALID_HANDLE_VALUE;
                    }
                }

                if (reader_started && output_limit_hit) {
                    std::wcerr << L"ksh: command substitution output exceeded limit\n";
                    g_expansion_error = true;
                    ksh_env.variables[L"?"] = L"1";
                } else if (reader_started && !bytes.empty()) {
                    int wlen = MultiByteToWideChar(CP_UTF8, 0, bytes.data(), (int)bytes.size(), NULL, 0);
                    if (wlen > 0) {
                        std::vector<wchar_t> wbuf(wlen);
                        MultiByteToWideChar(CP_UTF8, 0, bytes.data(), (int)bytes.size(), wbuf.data(), wlen);
                        sub_output = std::wstring(wbuf.data(), wlen);
                    } else {
                        wlen = MultiByteToWideChar(CP_ACP, 0, bytes.data(), (int)bytes.size(), NULL, 0);
                        if (wlen > 0) {
                            std::vector<wchar_t> wbuf(wlen);
                            MultiByteToWideChar(CP_ACP, 0, bytes.data(), (int)bytes.size(), wbuf.data(), wlen);
                            sub_output = std::wstring(wbuf.data(), wlen);
                        }
                    }
                }
            }
        }
    }

    const bool used_snapshot_restore = saved_snapshot.has_value();
    restore_shell_state_snapshot_if_present(saved_snapshot, L"0");
    snapshot_guard.dismiss();

    while (!sub_output.empty() && (sub_output.back() == L'\n' || sub_output.back() == L'\r')) {
        sub_output.pop_back();
    }

    const ULONGLONG elapsed_ms = GetTickCount64() - substitution_start;
    record_command_substitution_path_perf(use_capture_sink, used_snapshot_restore, elapsed_ms);
    if (perf_trace) {
        std::wstring detail;
        if (use_capture_sink) {
            detail = L"path=fast_capture count=" + std::to_wstring(g_subshell_path_perf_counters.command_subst_fast_count) +
                L" total_ms=" + std::to_wstring(g_subshell_path_perf_counters.command_subst_fast_ms);
        } else if (used_snapshot_restore) {
            detail = L"path=snapshot_restore count=" + std::to_wstring(g_subshell_path_perf_counters.command_subst_snapshot_count) +
                L" total_ms=" + std::to_wstring(g_subshell_path_perf_counters.command_subst_snapshot_ms);
        } else {
            detail = L"path=direct_no_snapshot count=" + std::to_wstring(g_subshell_path_perf_counters.command_subst_direct_count) +
                L" total_ms=" + std::to_wstring(g_subshell_path_perf_counters.command_subst_direct_ms);
        }
        log_performance_trace(L"substitution.command", elapsed_ms, detail);
    }

    return sub_output;
}

std::wstring expand_variable_reference(const std::wstring& input, size_t dollar_index, size_t& next_index, bool in_double_quotes) {
    auto is_assignable_parameter_name = [](const std::wstring& name) -> bool {
        if (name.empty()) {
            return false;
        }
        if (!(std::iswalpha(name[0]) || name[0] == L'_')) {
            return false;
        }
        return std::all_of(name.begin(), name.end(), [](wchar_t ch) {
            return std::iswalnum(ch) != 0 || ch == L'_' || ch == L'.';
        });
    };

    auto is_valid_variable_name = [](const std::wstring& name) -> bool {
        if (name.empty()) {
            return false;
        }
        if (name[0] == L'.') {
            if (name.size() < 2) return false;
            return std::all_of(name.begin() + 1, name.end(), [](wchar_t ch) {
                return std::iswalnum(ch) != 0 || ch == L'_' || ch == L'.' || ch == L'/';
            });
        }
        if (!(std::iswalpha(name[0]) || name[0] == L'_')) {
            return false;
        }
        return std::all_of(name.begin(), name.end(), [](wchar_t ch) {
            return std::iswalnum(ch) != 0 || ch == L'_' || ch == L'.' || ch == L'/';
        });
    };

    auto parse_array_reference = [&](const std::wstring& text, std::wstring& array_name, std::wstring& index_expr, bool& has_index) -> bool {
        array_name.clear();
        index_expr.clear();
        has_index = false;

        size_t open = text.find(L'[');
        if (open == std::wstring::npos) {
            array_name = text;
            return is_valid_variable_name(array_name);
        }

        if (open == 0 || text.back() != L']') {
            return false;
        }

        array_name = text.substr(0, open);
        if (!is_valid_variable_name(array_name)) {
            return false;
        }

        index_expr = text.substr(open + 1, text.size() - open - 2);
        if (index_expr.empty()) {
            return false;
        }

        has_index = true;
        return true;
    };

    auto parse_non_negative_index_local = [](const std::wstring& text, size_t& out_index) -> bool {
        if (text.empty()) {
            return false;
        }

        try {
            size_t consumed = 0;
            unsigned long long parsed = std::stoull(text, &consumed);
            if (consumed != text.size()) {
                return false;
            }
            out_index = static_cast<size_t>(parsed);
            return true;
        } catch (...) {
            return false;
        }
    };

    auto join_array_values = [](const std::vector<std::wstring>& values, wchar_t separator) -> std::wstring {
        std::wstring joined;
        for (size_t i = 0; i < values.size(); ++i) {
            joined += values[i];
            if (i + 1 < values.size()) {
                joined.push_back(separator);
            }
        }
        return joined;
    };

    auto try_get_parameter_value = [&](const std::wstring& name, std::wstring& out_value, bool& is_set) {
        out_value.clear();
        is_set = false;

        const std::vector<std::wstring>& script_args = current_script_args();
        const std::wstring script_name = current_script_name();

        if (name == L"#") {
            out_value = std::to_wstring(script_args.size());
            is_set = true;
            return;
        }

        if (name == L"@") {
            if (in_double_quotes) {
                out_value = join_array_values(script_args, k_array_at_quoted_separator);
            } else {
                out_value = join_script_args(script_args);
            }
            is_set = true;
            return;
        }

        if (name == L"*") {
            wchar_t separator = L' ';
            std::map<std::wstring, std::wstring>::const_iterator ifs_it = ksh_env.variables.find(L"IFS");
            if (ifs_it != ksh_env.variables.end()) {
                if (ifs_it->second.empty()) {
                    separator = L'\0';
                } else {
                    separator = ifs_it->second[0];
                }
            }

            if (separator == L'\0') {
                out_value.clear();
                for (const std::wstring& arg : script_args) {
                    out_value += arg;
                }
            } else {
                out_value = join_array_values(script_args, separator);
            }
            is_set = true;
            return;
        }

        if (name == L"$") {
            out_value = std::to_wstring(GetCurrentProcessId());
            is_set = true;
            return;
        }

        if (!name.empty() && std::all_of(name.begin(), name.end(), [](wchar_t ch) { return std::iswdigit(ch) != 0; })) {
            unsigned long arg_index = 0;
            if (!try_parse_unsigned_long_strict(name, arg_index)) {
                return;
            }
            if (arg_index == 0) {
                out_value = script_name;
                is_set = true;
                return;
            }
            if (arg_index <= script_args.size()) {
                out_value = script_args[static_cast<size_t>(arg_index - 1)];
                is_set = true;
                return;
            }
            return;
        }

        std::wstring array_name;
        std::wstring index_expr;
        bool has_index = false;
        bool is_key_expansion = false;
        std::wstring name_to_use = name;
        if (name.size() > 1 && name.front() == L'!') {
            is_key_expansion = true;
            name_to_use = name.substr(1);
        }

        bool handled_nameref_key = false;
        std::wstring array_name_tmp;
        std::wstring index_expr_tmp;
        bool has_index_tmp = false;
        if (parse_array_reference(name_to_use, array_name_tmp, index_expr_tmp, has_index_tmp)) {
            if (is_key_expansion && !has_index_tmp) {
                if (get_flag_value(ksh_env.nameref_flags, array_name_tmp)) {
                    std::map<std::wstring, std::wstring>::const_iterator val_it = ksh_env.variables.find(array_name_tmp);
                    if (val_it != ksh_env.variables.end()) {
                        out_value = val_it->second;
                        is_set = true;
                        handled_nameref_key = true;
                    }
                }
            }
        }

        if (!handled_nameref_key) {
            name_to_use = resolve_variable_name(name_to_use);
            if (parse_array_reference(name_to_use, array_name, index_expr, has_index)) {
                std::map<std::wstring, std::map<std::wstring, std::wstring>>::const_iterator arr_it = ksh_env.arrays.find(array_name);
                if (arr_it != ksh_env.arrays.end()) {
                    is_set = true;

                    if (is_key_expansion) {
                        if (has_index && (index_expr == L"@" || index_expr == L"*")) {
                            std::vector<std::wstring> keys;
                            for (const auto& pair : arr_it->second) {
                                keys.push_back(pair.first);
                            }
                            if (index_expr == L"@") {
                                out_value = in_double_quotes
                                    ? join_array_values(keys, k_array_at_quoted_separator)
                                    : join_array_values(keys, L' ');
                            } else {
                                out_value = join_array_values(keys, L' ');
                            }
                        } else {
                            out_value = array_name;
                        }
                        return;
                    }

                    if (!has_index) {
                        std::map<std::wstring, std::wstring>::const_iterator val_it = arr_it->second.find(L"0");
                        out_value = (val_it != arr_it->second.end()) ? val_it->second : L"";
                        return;
                    }

                    if (index_expr == L"@") {
                        std::vector<std::wstring> values = get_array_values_vector(array_name);
                        out_value = in_double_quotes
                            ? join_array_values(values, k_array_at_quoted_separator)
                            : join_array_values(values, L' ');
                        return;
                    }

                    if (index_expr == L"*") {
                        std::vector<std::wstring> values = get_array_values_vector(array_name);
                        out_value = join_array_values(values, L' ');
                        return;
                    }

                    bool is_assoc = false;
                    std::map<std::wstring, bool>::const_iterator assoc_it = ksh_env.associative_flags.find(array_name);
                    if (assoc_it != ksh_env.associative_flags.end()) {
                        is_assoc = assoc_it->second;
                    }

                    if (is_assoc) {
                        std::map<std::wstring, std::wstring>::const_iterator val_it = arr_it->second.find(index_expr);
                        if (val_it != arr_it->second.end()) {
                            out_value = val_it->second;
                        } else {
                            out_value.clear();
                        }
                    } else {
                        size_t idx = 0;
                        if (parse_non_negative_index_local(index_expr, idx)) {
                            std::wstring key = std::to_wstring(idx);
                            std::map<std::wstring, std::wstring>::const_iterator val_it = arr_it->second.find(key);
                            if (val_it != arr_it->second.end()) {
                                out_value = val_it->second;
                            } else {
                                out_value.clear();
                            }
                        } else {
                            is_set = false;
                            out_value.clear();
                        }
                    }
                    return;
                }
            }
        }

        std::wstring hook_val = get_variable_value_with_hooks(name_to_use, is_set);
        if (is_set) {
            out_value = hook_val;
            return;
        }

        wchar_t small_buf[256];
        DWORD ret = GetEnvironmentVariableW(name_to_use.c_str(), small_buf, _countof(small_buf));
        if (ret > 0 && ret < _countof(small_buf)) {
            out_value.assign(small_buf, ret);
            is_set = true;
        } else if (ret >= _countof(small_buf)) {
            std::vector<wchar_t> dyn_buf(ret + 1);
            DWORD dyn_ret = GetEnvironmentVariableW(name_to_use.c_str(), dyn_buf.data(), static_cast<DWORD>(dyn_buf.size()));
            if (dyn_ret > 0 && dyn_ret < dyn_buf.size()) {
                out_value.assign(dyn_buf.data(), dyn_ret);
                is_set = true;
            }
        } else if (GetLastError() == ERROR_SUCCESS) {
            out_value.clear();
            is_set = true;
        }
    };

    auto parse_braced_parameter_word = [](const std::wstring& text, size_t start_index, std::wstring& word, size_t& after_closing_brace) -> bool {
        word.clear();
        after_closing_brace = start_index;

        int brace_depth = 0;
        bool in_single_quotes = false;
        bool in_double_quotes = false;
        bool escaped = false;

        size_t i = start_index;
        while (i < text.size()) {
            wchar_t ch = text[i];

            if (escaped) {
                word += ch;
                escaped = false;
                i++;
                continue;
            }

            if (ch == L'\\' && !in_single_quotes) {
                word += ch;
                escaped = true;
                i++;
                continue;
            }

            if (ch == L'\'' && !in_double_quotes) {
                in_single_quotes = !in_single_quotes;
                word += ch;
                i++;
                continue;
            }

            if (ch == L'"' && !in_single_quotes) {
                in_double_quotes = !in_double_quotes;
                word += ch;
                i++;
                continue;
            }

            if (!in_single_quotes && !in_double_quotes) {
                if (ch == L'{') {
                    brace_depth++;
                    word += ch;
                    i++;
                    continue;
                }

                if (ch == L'}') {
                    if (brace_depth == 0) {
                        after_closing_brace = i + 1;
                        return true;
                    }
                    brace_depth--;
                    word += ch;
                    i++;
                    continue;
                }
            }

            word += ch;
            i++;
        }

        return false;
    };

    auto match_parameter_pattern = [](const std::wstring& pattern, const std::wstring& candidate) -> bool {
        const bool case_sensitive = pattern_match_case_sensitive();
        unsigned long long step_budget = kMaxPatternMatchSteps;
        return match_glob_pattern_recursive(pattern, 0, candidate, 0, case_sensitive, step_budget);
    };

    auto apply_parameter_pattern_removal = [&](const std::wstring& value, const std::wstring& raw_pattern, bool prefix_mode, bool longest_match) -> std::wstring {
        std::wstring expanded_pattern = expand_substitutions_left_to_right(raw_pattern);
        if (g_expansion_error) {
            return L"";
        }

        if (expanded_pattern.empty()) {
            return value;
        }

        if (prefix_mode) {
            if (longest_match) {
                for (size_t len = value.size() + 1; len > 0; --len) {
                    size_t prefix_len = len - 1;
                    if (match_parameter_pattern(expanded_pattern, value.substr(0, prefix_len))) {
                        return value.substr(prefix_len);
                    }
                }
            } else {
                for (size_t prefix_len = 0; prefix_len <= value.size(); ++prefix_len) {
                    if (match_parameter_pattern(expanded_pattern, value.substr(0, prefix_len))) {
                        return value.substr(prefix_len);
                    }
                }
            }
            return value;
        }

        if (longest_match) {
            for (size_t start = 0; start <= value.size(); ++start) {
                if (match_parameter_pattern(expanded_pattern, value.substr(start))) {
                    return value.substr(0, start);
                }
            }
        } else {
            for (size_t start = value.size() + 1; start > 0; --start) {
                size_t suffix_start = start - 1;
                if (match_parameter_pattern(expanded_pattern, value.substr(suffix_start))) {
                    return value.substr(0, suffix_start);
                }
            }
        }

        return value;
    };

    auto apply_parameter_substitution = [&](const std::wstring& value, const std::wstring& raw_pattern, const std::wstring& raw_replacement, bool replace_all) -> std::wstring {
        std::wstring expanded_pattern = expand_substitutions_left_to_right(raw_pattern);
        if (g_expansion_error) {
            return L"";
        }

        std::wstring replacement = expand_substitutions_left_to_right(raw_replacement);
        if (g_expansion_error) {
            return L"";
        }

        if (expanded_pattern.empty()) {
            return value;
        }

        auto find_match = [&](size_t start_index, size_t& match_start, size_t& match_length) -> bool {
            for (size_t begin = start_index; begin <= value.size(); ++begin) {
                for (size_t end = begin; end <= value.size(); ++end) {
                    if (match_parameter_pattern(expanded_pattern, value.substr(begin, end - begin))) {
                        match_start = begin;
                        match_length = end - begin;
                        return true;
                    }
                }
            }
            return false;
        };

        std::wstring result;
        size_t cursor = 0;
        bool replaced = false;
        while (cursor <= value.size()) {
            size_t match_start = 0;
            size_t match_length = 0;
            if (!find_match(cursor, match_start, match_length)) {
                result += value.substr(cursor);
                break;
            }

            replaced = true;
            result += value.substr(cursor, match_start - cursor);
            result += replacement;

            if (match_length == 0) {
                if (match_start < value.size()) {
                    result.push_back(value[match_start]);
                }
                cursor = match_start + 1;
            } else {
                cursor = match_start + match_length;
            }

            if (!replace_all) {
                result += value.substr(cursor);
                break;
            }
        }

        return replaced ? result : value;
    };

    next_index = dollar_index + 1;

    auto fail_bad_substitution = [&]() -> std::wstring {
        std::wcerr << L"ksh: bad substitution: missing }\n";
        ksh_env.variables[L"?"] = L"1";
        g_expansion_error = true;
        return L"";
    };

    if (next_index >= input.length()) {
        return L"$";
    }

    wchar_t marker = input[next_index];
    if (marker == L'#') {
        next_index++;
        std::wstring value;
        bool is_set = false;
        try_get_parameter_value(L"#", value, is_set);
        return value;
    }
    if (marker == L'@') {
        next_index++;
        std::wstring value;
        bool is_set = false;
        try_get_parameter_value(L"@", value, is_set);
        return value;
    }
    if (marker == L'*') {
        next_index++;
        std::wstring value;
        bool is_set = false;
        try_get_parameter_value(L"*", value, is_set);
        return value;
    }
    if (marker == L'$') {
        next_index++;
        std::wstring value;
        bool is_set = false;
        try_get_parameter_value(L"$", value, is_set);
        return value;
    }
    if (marker == L'?') {
        next_index++;
        std::wstring value;
        bool is_set = false;
        try_get_parameter_value(L"?", value, is_set);
        if (is_set) {
            return value;
        }
        return L"0";
    }
    if (std::iswdigit(marker)) {
        std::wstring index_text;
        while (next_index < input.length() && std::iswdigit(input[next_index])) {
            index_text += input[next_index];
            next_index++;
        }
        std::wstring value;
        bool is_set = false;
        try_get_parameter_value(index_text, value, is_set);
        if (is_set) {
            return value;
        }
        return L"";
    }

    bool braced = false;
    bool closing_brace_consumed = false;
    bool length_operator = false;
    std::wstring var_name;
    if (marker == L'{') {
        braced = true;
        next_index++;

        if (next_index < input.length() && input[next_index] == L'#') {
            length_operator = true;
            next_index++;
        }

        if (next_index < input.length() && input[next_index] == L'!') {
            var_name += L'!';
            next_index++;
        }
    }

    if (next_index < input.length() && (input[next_index] == L'@' || input[next_index] == L'#' || input[next_index] == L'*')) {
        var_name += input[next_index];
        next_index++;
    } else if (next_index < input.length() && input[next_index] == L'.') {
        var_name += input[next_index];
        next_index++;
    }

    while (next_index < input.length() && (std::iswalnum(input[next_index]) || input[next_index] == L'_' || input[next_index] == L'.')) {
        var_name += input[next_index];
        next_index++;
    }

    if (braced && next_index < input.length() && input[next_index] == L'[') {
        size_t bracket_end = input.find(L']', next_index + 1);
        if (bracket_end == std::wstring::npos) {
            return fail_bad_substitution();
        }

        var_name += input.substr(next_index, bracket_end - next_index + 1);
        next_index = bracket_end + 1;
    }

    if (length_operator) {
        std::wstring resolved_value;
        bool resolved_is_set = false;

        if (var_name.empty()) {
            return fail_bad_substitution();
        }

        std::wstring resolved_var_name = resolve_variable_name(var_name);

        std::wstring array_name;
        std::wstring index_expr;
        bool has_index = false;
        if (parse_array_reference(resolved_var_name, array_name, index_expr, has_index)) {
            std::map<std::wstring, std::map<std::wstring, std::wstring>>::const_iterator arr_it = ksh_env.arrays.find(array_name);
            if (arr_it != ksh_env.arrays.end()) {
                if (has_index && (index_expr == L"@" || index_expr == L"*")) {
                    resolved_value = std::to_wstring(arr_it->second.size());
                    resolved_is_set = true;
                } else if (has_index) {
                    bool is_assoc = false;
                    std::map<std::wstring, bool>::const_iterator assoc_it = ksh_env.associative_flags.find(array_name);
                    if (assoc_it != ksh_env.associative_flags.end()) {
                        is_assoc = assoc_it->second;
                    }

                    if (is_assoc) {
                        std::map<std::wstring, std::wstring>::const_iterator val_it = arr_it->second.find(index_expr);
                        if (val_it != arr_it->second.end()) {
                            resolved_value = std::to_wstring(val_it->second.size());
                        } else {
                            resolved_value = L"0";
                        }
                        resolved_is_set = true;
                    } else {
                        size_t idx = 0;
                        if (parse_non_negative_index_local(index_expr, idx)) {
                            std::wstring key = std::to_wstring(idx);
                            std::map<std::wstring, std::wstring>::const_iterator val_it = arr_it->second.find(key);
                            if (val_it != arr_it->second.end()) {
                                resolved_value = std::to_wstring(val_it->second.size());
                            } else {
                                resolved_value = L"0";
                            }
                            resolved_is_set = true;
                        } else {
                            resolved_value = L"0";
                            resolved_is_set = true;
                        }
                    }
                } else {
                    std::map<std::wstring, std::wstring>::const_iterator val_it = arr_it->second.find(L"0");
                    resolved_value = (val_it != arr_it->second.end()) ? std::to_wstring(val_it->second.size()) : L"0";
                    resolved_is_set = true;
                }
            }
        }

        if (!resolved_is_set) {
            try_get_parameter_value(resolved_var_name, resolved_value, resolved_is_set);
            if (resolved_is_set) {
                resolved_value = std::to_wstring(resolved_value.size());
            }
        }

        if (!resolved_is_set) {
            resolved_value = L"0";
        }

        if (braced) {
            if (next_index >= input.length() || input[next_index] != L'}') {
                return fail_bad_substitution();
            }
            next_index++;
            closing_brace_consumed = true;
        }

        return resolved_value;
    }

    if (braced && next_index < input.length() && input[next_index] == L':' && (next_index + 1) < input.length()) {
        wchar_t op = input[next_index + 1];
        if (op != L'-' && op != L'=' && op != L'+' && op != L'?') {
            size_t after_offset = next_index + 1;
            size_t colon_or_brace = after_offset;
            int inner_brace_depth = 0;
            while (colon_or_brace < input.length()) {
                if (input[colon_or_brace] == L'{') inner_brace_depth++;
                if (input[colon_or_brace] == L'}') {
                    if (inner_brace_depth == 0) break;
                    inner_brace_depth--;
                }
                if (input[colon_or_brace] == L':' && inner_brace_depth == 0) {
                    break;
                }
                colon_or_brace++;
            }

            if (colon_or_brace == input.length()) {
                return fail_bad_substitution();
            }

            std::wstring offset_str = input.substr(after_offset, colon_or_brace - after_offset);
            std::wstring length_str;
            bool has_length = false;

            if (input[colon_or_brace] == L':') {
                has_length = true;
                size_t length_start = colon_or_brace + 1;
                size_t closing_brace = length_start;
                int inner_brace_depth2 = 0;
                while (closing_brace < input.length()) {
                    if (input[closing_brace] == L'{') inner_brace_depth2++;
                    if (input[closing_brace] == L'}') {
                        if (inner_brace_depth2 == 0) break;
                        inner_brace_depth2--;
                    }
                    closing_brace++;
                }
                if (closing_brace == input.length()) {
                    return fail_bad_substitution();
                }
                length_str = input.substr(length_start, closing_brace - length_start);
                next_index = closing_brace + 1;
            } else {
                next_index = colon_or_brace + 1;
            }

            closing_brace_consumed = true;

            std::wstring expanded_offset = expand_substitutions_left_to_right(offset_str);
            if (g_expansion_error) return L"";
            std::wstring evaluated_offset = evaluate_arithmetic(expanded_offset);
            int offset = 0;
            if (!try_parse_int_strict(evaluated_offset, offset)) {
                std::wcerr << L"ksh: bad substitution: invalid offset\n";
                ksh_env.variables[L"?"] = L"1";
                g_expansion_error = true;
                return L"";
            }

            int length = -1;
            if (has_length) {
                std::wstring expanded_length = expand_substitutions_left_to_right(length_str);
                if (g_expansion_error) return L"";
                std::wstring evaluated_length = evaluate_arithmetic(expanded_length);
                if (!try_parse_int_strict(evaluated_length, length)) {
                    std::wcerr << L"ksh: bad substitution: invalid length\n";
                    ksh_env.variables[L"?"] = L"1";
                    g_expansion_error = true;
                    return L"";
                }
            }

            std::wstring existing_value;
            bool is_set = false;
            try_get_parameter_value(var_name, existing_value, is_set);
            if (!is_set) {
                if (g_nounset_enabled) {
                    std::wcerr << L"ksh: " << var_name << L": parameter not set\n";
                    ksh_env.variables[L"?"] = L"1";
                    g_expansion_error = true;
                }
                return L"";
            }

            int val_len = static_cast<int>(existing_value.length());
            if (offset < 0) {
                offset = val_len + offset;
                if (offset < 0) offset = 0;
            }
            if (offset > val_len) {
                return L"";
            }

            if (length < 0) {
                return existing_value.substr(offset);
            } else {
                if (offset + length > val_len) {
                    length = val_len - offset;
                }
                return existing_value.substr(offset, length);
            }
        }
        if (op == L'-' || op == L'=' || op == L'+' || op == L'?') {
            std::wstring parameter_word;
            size_t after_word = next_index + 2;
            if (!parse_braced_parameter_word(input, next_index + 2, parameter_word, after_word)) {
                return fail_bad_substitution();
            }

            next_index = after_word;
            closing_brace_consumed = true;

            std::wstring existing_value;
            bool is_set = false;
            try_get_parameter_value(var_name, existing_value, is_set);
            bool is_null_or_unset = (!is_set || existing_value.empty());

            if (op == L'-') {
                if (is_null_or_unset) {
                    return expand_substitutions_left_to_right(parameter_word);
                }
                return existing_value;
            }

            if (op == L'=') {
                if (!is_null_or_unset) {
                    return existing_value;
                }

                std::wstring assigned_value = expand_substitutions_left_to_right(parameter_word);
                if (g_expansion_error) {
                    return L"";
                }

                if (!is_assignable_parameter_name(var_name)) {
                    std::wcerr << L"ksh: bad substitution for := operator\n";
                    ksh_env.variables[L"?"] = L"1";
                    g_expansion_error = true;
                    return L"";
                }

                ksh_env.variables[var_name] = assigned_value;
                SetEnvironmentVariableW(var_name.c_str(), assigned_value.c_str());
                return assigned_value;
            }

            if (op == L'+') {
                if (is_null_or_unset) {
                    return L"";
                }
                return expand_substitutions_left_to_right(parameter_word);
            }

            if (is_null_or_unset) {
                std::wstring message = expand_substitutions_left_to_right(parameter_word);
                if (message.empty()) {
                    message = L"parameter '" + var_name + L"' is unset or empty";
                }
                std::wcerr << L"ksh: " << message << L"\n";
                ksh_env.variables[L"?"] = L"1";
                g_expansion_error = true;
                return L"";
            }

            return existing_value;
        }
    }

    if (braced && next_index < input.length() && (input[next_index] == L'#' || input[next_index] == L'%')) {
        const bool prefix_mode = input[next_index] == L'#';
        bool longest_match = false;
        size_t pattern_start = next_index + 1;
        if (pattern_start < input.length() && input[pattern_start] == input[next_index]) {
            longest_match = true;
            pattern_start++;
        }

        std::wstring pattern_word;
        size_t after_word = pattern_start;
        if (!parse_braced_parameter_word(input, pattern_start, pattern_word, after_word)) {
            return fail_bad_substitution();
        }

        next_index = after_word;
        closing_brace_consumed = true;

        std::wstring existing_value;
        bool is_set = false;
        try_get_parameter_value(var_name, existing_value, is_set);
        if (!is_set) {
            return L"";
        }

        return apply_parameter_pattern_removal(existing_value, pattern_word, prefix_mode, longest_match);
    }

    if (braced && next_index < input.length() && input[next_index] == L'/') {
        bool replace_all = false;
        size_t pattern_start = next_index + 1;
        if (pattern_start < input.length() && input[pattern_start] == L'/') {
            replace_all = true;
            pattern_start++;
        }

        std::wstring pattern_word;
        std::wstring replacement_word;
        size_t i = pattern_start;
        int brace_depth = 0;
        bool in_single_quotes = false;
        bool inner_in_double_quotes = false;
        bool escaped = false;
        bool saw_separator = false;

        while (i < input.length()) {
            wchar_t ch = input[i];

            if (escaped) {
                (saw_separator ? replacement_word : pattern_word) += ch;
                escaped = false;
                i++;
                continue;
            }

            if (ch == L'\\' && !in_single_quotes) {
                (saw_separator ? replacement_word : pattern_word) += ch;
                escaped = true;
                i++;
                continue;
            }

            if (ch == L'\'' && !inner_in_double_quotes) {
                in_single_quotes = !in_single_quotes;
                (saw_separator ? replacement_word : pattern_word) += ch;
                i++;
                continue;
            }

            if (ch == L'"' && !in_single_quotes) {
                inner_in_double_quotes = !inner_in_double_quotes;
                (saw_separator ? replacement_word : pattern_word) += ch;
                i++;
                continue;
            }

            if (!in_single_quotes && !inner_in_double_quotes) {
                if (ch == L'{') {
                    brace_depth++;
                    (saw_separator ? replacement_word : pattern_word) += ch;
                    i++;
                    continue;
                }

                if (ch == L'}') {
                    if (brace_depth == 0) {
                        next_index = i + 1;
                        closing_brace_consumed = true;

                        std::wstring existing_value;
                        bool is_set = false;
                        try_get_parameter_value(var_name, existing_value, is_set);
                        if (!is_set) {
                            return L"";
                        }

                        return apply_parameter_substitution(existing_value, pattern_word, replacement_word, replace_all);
                    }

                    brace_depth--;
                    (saw_separator ? replacement_word : pattern_word) += ch;
                    i++;
                    continue;
                }

                if (ch == L'/' && brace_depth == 0 && !saw_separator) {
                    saw_separator = true;
                    i++;
                    continue;
                }
            }

            (saw_separator ? replacement_word : pattern_word) += ch;
            i++;
        }

        return fail_bad_substitution();
    }

    if (braced && !closing_brace_consumed) {
        if (next_index >= input.length() || input[next_index] != L'}') {
            return fail_bad_substitution();
        }
        next_index++;
        closing_brace_consumed = true;
    }

    if (var_name.empty()) {
        if (braced) {
            std::wcerr << L"ksh: bad substitution\n";
            ksh_env.variables[L"?"] = L"1";
            g_expansion_error = true;
            return L"";
        }
        return L"$";
    }

    std::wstring resolved_value;
    bool resolved_is_set = false;
    try_get_parameter_value(var_name, resolved_value, resolved_is_set);
    if (resolved_is_set) {
        return resolved_value;
    }
    if (g_nounset_enabled) {
        std::wcerr << L"ksh: " << var_name << L": parameter not set\n";
        ksh_env.variables[L"?"] = L"1";
        g_expansion_error = true;
    }
    return L"";
}

std::wstring expand_substitutions_left_to_right(const std::wstring& input) {
    ScopedExpansionDepth depth_guard;
    if (!depth_guard.active()) {
        std::wcerr << L"ksh: expansion nesting depth exceeded limit\n";
        ksh_env.variables[L"?"] = L"1";
        g_expansion_error = true;
        return L"";
    }

    std::wstring result;
    size_t i = 0;
    bool in_single_quotes = false;
    bool in_double_quotes = false;
    bool escaped = false;

    while (i < input.length()) {
        wchar_t ch = input[i];

        if (escaped) {
            result += ch;
            escaped = false;
            i++;
            continue;
        }

        if (ch == L'\\' && !in_single_quotes) {
            result += ch;
            escaped = true;
            i++;
            continue;
        }

        if (ch == L'\'' && !in_double_quotes) {
            in_single_quotes = !in_single_quotes;
            result += ch;
            i++;
            continue;
        }

        if (ch == L'"' && !in_single_quotes) {
            in_double_quotes = !in_double_quotes;
            result += ch;
            i++;
            continue;
        }

        if (ch != L'$' || in_single_quotes) {
            result += ch;
            i++;
            continue;
        }

        if ((i + 1) < input.length() && input[i + 1] == L'(') {
            if ((i + 2) < input.length() && input[i + 2] == L'(') {
                std::wstring arith_content;
                size_t next_index = i + 3;
                if (!parse_arithmetic_substitution_content(input, i + 3, arith_content, next_index)) {
                    result += L"$((";
                    i += 3;
                    continue;
                }

                std::wstring expanded_arith = expand_substitutions_left_to_right(arith_content);
                result += evaluate_arithmetic(expanded_arith);
                i = next_index;
                continue;
            }

            std::wstring cmd_content;
            size_t next_index = i + 2;
            if (!parse_command_substitution_content(input, i + 2, cmd_content, next_index)) {
                result += L"$(";
                i += 2;
                continue;
            }

            std::wstring expanded_command = expand_substitutions_left_to_right(cmd_content);
            std::vector<std::wstring> substitution_tokens = ksh_tokenize_preserve_quotes(expanded_command);
            const bool use_capture_sink = can_capture_builtin_command_substitution(substitution_tokens);
            std::wstring sub_output = execute_command_substitution(expanded_command, use_capture_sink);

            result += sub_output;
            i = next_index;
            continue;
        }

        size_t next_index = i + 1;
        result += expand_variable_reference(input, i, next_index, in_double_quotes);
        if (next_index <= i) {
            next_index = i + 1;
        }
        i = next_index;
    }

    return result;
}

std::wstring evaluate_command_substitutions(const std::wstring& input) {
    ScopedExpansionDepth depth_guard;
    if (!depth_guard.active()) {
        g_expansion_error = true;
        std::wcerr << L"ksh: maximum expansion nesting depth exceeded\n";
        return L"";
    }

    std::wstring output;
    size_t i = 0;

    while (i < input.size()) {
        if (input[i] == L'$' && (i + 1) < input.size() && input[i + 1] == L'(' && !((i + 2) < input.size() && input[i + 2] == L'(')) {
            std::wstring cmd_content;
            size_t next_index = i + 2;
            if (!parse_command_substitution_content(input, i + 2, cmd_content, next_index)) {
                output += L"$(";
                i += 2;
                continue;
            }

            std::wstring nested = evaluate_command_substitutions(cmd_content);
            if (g_expansion_error) {
                return L"";
            }
            std::vector<std::wstring> substitution_tokens = ksh_tokenize_preserve_quotes(nested);
            const bool use_capture_sink = can_capture_builtin_command_substitution(substitution_tokens);
            std::wstring sub_output = execute_command_substitution(nested, use_capture_sink);

            output += sub_output;
            i = next_index;
            continue;
        }

        output += input[i];
        i++;
    }

    return output;
}

std::wstring join_fields_with_internal_separator(const std::vector<std::wstring>& fields) {
    std::wstring joined;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) {
            joined.push_back(k_array_at_quoted_separator);
        }
        joined += fields[i];
    }
    return joined;
}

std::wstring effective_ifs_value() {
    std::map<std::wstring, std::wstring>::const_iterator it = ksh_env.variables.find(L"IFS");
    if (it != ksh_env.variables.end()) {
        return it->second;
    }
    return L" \t\n";
}

std::vector<std::wstring> split_fields_by_ifs(const std::wstring& input) {
    std::vector<std::wstring> fields;
    std::wstring current;
    const std::wstring ifs = effective_ifs_value();

    bool in_quotes = false;
    bool in_single_quotes = false;
    bool in_array_def = false;
    bool escaped = false;
    bool ended_with_internal_separator = false;

    auto is_ifs_delimiter = [&](wchar_t ch) {
        return ifs.find(ch) != std::wstring::npos;
    };

    for (size_t i = 0; i < input.length(); ++i) {
        wchar_t c = input[i];

        if (c == k_array_at_quoted_separator) {
            fields.push_back(current);
            current.clear();
            ended_with_internal_separator = true;
            continue;
        }

        ended_with_internal_separator = false;

        if (escaped) {
            current += c;
            escaped = false;
            continue;
        }

        if (c == L'\\' && !in_single_quotes) {
            current += c;
            escaped = true;
            continue;
        }

        if (c == L'"' && !in_array_def && !in_single_quotes) {
            in_quotes = !in_quotes;
            current += c;
            continue;
        }

        if (c == L'\'' && !in_array_def && !in_quotes) {
            in_single_quotes = !in_single_quotes;
            current += c;
            continue;
        }

        if (c == L'(' && i > 0 && input[i - 1] == L'=') {
            in_array_def = true;
            current += c;
            continue;
        }

        if (c == L')' && in_array_def) {
            in_array_def = false;
            current += c;
            continue;
        }

        if (!in_quotes && !in_single_quotes && !in_array_def && is_ifs_delimiter(c)) {
            if (!current.empty()) {
                fields.push_back(current);
                current.clear();
            }
            continue;
        }

        current += c;
    }

    if (!current.empty() || ended_with_internal_separator) {
        fields.push_back(current);
    }

    return fields;
}

std::wstring expand_tilde(const std::wstring& input) {
    if (input.empty() || input[0] != L'~') {
        return input;
    }
    
    size_t slash_pos = input.find_first_of(L"/\\");
    std::wstring username = (slash_pos == std::wstring::npos) ? input.substr(1) : input.substr(1, slash_pos - 1);
    std::wstring rest = (slash_pos == std::wstring::npos) ? L"" : input.substr(slash_pos);
    
    std::wstring home_dir;
    if (username.empty()) {
        wchar_t user_profile[MAX_PATH];
        if (GetEnvironmentVariableW(L"USERPROFILE", user_profile, MAX_PATH) > 0) {
            home_dir = user_profile;
        } else if (GetEnvironmentVariableW(L"HOME", user_profile, MAX_PATH) > 0) {
            home_dir = user_profile;
        } else {
            home_dir = L"C:\\";
        }
    } else {
        std::wstring target = L"C:\\Users\\" + username;
        DWORD attr = GetFileAttributesW(target.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
            home_dir = target;
        } else {
            return input;
        }
    }
    return home_dir + rest;
}

std::vector<std::wstring> expand_tilde_for_tokens(const std::vector<std::wstring>& tokens) {
    std::vector<std::wstring> res;
    res.reserve(tokens.size());
    for (const std::wstring& t : tokens) {
        res.push_back(expand_tilde(t));
    }
    return res;
}

std::wstring join_path_for_glob(const std::wstring& base, const std::wstring& name) {
    if (base.empty()) {
        return name;
    }
    if (base == L".") {
        return L".\\" + name;
    }
    if (base.back() == L'\\' || base.back() == L'/') {
        return base + name;
    }
    return base + L"\\" + name;
}

std::wstring normalize_relative_glob_result(const std::wstring& value) {
    if (value.rfind(L".\\", 0) == 0) {
        return value.substr(2);
    }
    return value;
}

void expand_glob_token(const std::wstring& pattern, std::vector<std::wstring>& matches) {
    matches.clear();

    std::wstring normalized = pattern;
    std::replace(normalized.begin(), normalized.end(), L'/', L'\\');

    std::vector<std::wstring> segments;
    std::wstring current_segment;
    for (wchar_t ch : normalized) {
        if (is_path_separator(ch)) {
            if (!current_segment.empty()) {
                segments.push_back(current_segment);
                current_segment.clear();
            }
            continue;
        }
        current_segment += ch;
    }
    if (!current_segment.empty()) {
        segments.push_back(current_segment);
    }

    if (segments.empty()) {
        return;
    }

    bool absolute_drive = normalized.size() >= 2 && std::iswalpha(normalized[0]) && normalized[1] == L':';
    bool rooted = !normalized.empty() && is_path_separator(normalized[0]);

    std::vector<std::wstring> candidates;
    if (absolute_drive) {
        std::wstring base;
        base += normalized[0];
        base += L':';
        base += L'\\';
        candidates.push_back(base);
    } else if (rooted) {
        candidates.push_back(L"\\");
    } else {
        candidates.push_back(L".");
    }

    for (size_t segment_index = 0; segment_index < segments.size(); ++segment_index) {
        const std::wstring& segment = segments[segment_index];
        const bool needs_glob = has_glob_metacharacters(segment);
        const bool is_last = segment_index + 1 == segments.size();

        std::vector<std::wstring> next_candidates;
        for (const std::wstring& base : candidates) {
            if (!needs_glob) {
                std::wstring next_path = join_path_for_glob(base, segment);
                DWORD attrs = GetFileAttributesW(next_path.c_str());
                if (attrs == INVALID_FILE_ATTRIBUTES) {
                    continue;
                }
                if (!is_last && ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0)) {
                    continue;
                }
                next_candidates.push_back(next_path);
                continue;
            }

            std::wstring probe = join_path_for_glob(base, L"*");
            WIN32_FIND_DATAW find_data;
            HANDLE handle = FindFirstFileW(probe.c_str(), &find_data);
            if (handle == INVALID_HANDLE_VALUE) {
                continue;
            }

            do {
                std::wstring name = find_data.cFileName;
                if (name == L"." || name == L"..") {
                    continue;
                }
                if (!match_glob_pattern(segment, name)) {
                    continue;
                }
                if (!is_last && ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)) {
                    continue;
                }

                next_candidates.push_back(join_path_for_glob(base, name));
            } while (FindNextFileW(handle, &find_data));

            FindClose(handle);
        }

        candidates.swap(next_candidates);
        if (candidates.empty()) {
            return;
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const std::wstring& left, const std::wstring& right) {
        return _wcsicmp(left.c_str(), right.c_str()) < 0;
    });

    for (const std::wstring& candidate : candidates) {
        matches.push_back(normalize_relative_glob_result(candidate));
    }
}

std::vector<std::wstring> expand_globs_for_tokens(const std::vector<std::wstring>& tokens, const std::vector<std::wstring>& preserved_tokens) {
    std::vector<std::wstring> expanded;
    expanded.reserve(tokens.size());

    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::wstring& token = tokens[i];
        const std::wstring& preserved = (i < preserved_tokens.size()) ? preserved_tokens[i] : token;

        if (!has_unquoted_glob_metacharacters(preserved) || !has_glob_metacharacters(token)) {
            expanded.push_back(token);
            continue;
        }

        std::vector<std::wstring> matches;
        expand_glob_token(token, matches);
        if (matches.empty()) {
            // Keep unmatched wildcard tokens literal for compatibility.
            expanded.push_back(token);
            continue;
        }

        expanded.insert(expanded.end(), std::make_move_iterator(matches.begin()), std::make_move_iterator(matches.end()));
    }

    return expanded;
}

std::wstring ksh_expand(const std::wstring& input) {
    g_expansion_error = false;
    std::wstring tilde_expanded = expand_tilde(input);
    std::wstring expanded = expand_substitutions_left_to_right(tilde_expanded);
    if (g_expansion_error) {
        return L"";
    }

    std::vector<std::wstring> fields = split_fields_by_ifs(expanded);
    fields = expand_globs_for_tokens(fields, fields);
    std::vector<std::wstring> stripped = remove_quotes_and_escapes_from_tokens(fields);
    return join_fields_with_internal_separator(stripped);
}
