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
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
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

#include "ksh_internal.h"

std::wstring to_lower_copy(const std::wstring& value) {
    return to_lower_copy(std::wstring_view(value));
}

std::wstring to_lower_copy(std::wstring_view value) {
    std::wstring lower;
    lower.reserve(value.size());
    for (wchar_t ch : value) {
        lower.push_back(static_cast<wchar_t>(std::towlower(ch)));
    }
    return lower;
}

std::wstring to_upper_copy(const std::wstring& value) {
    std::wstring upper;
    upper.reserve(value.size());
    for (wchar_t ch : value) {
        upper.push_back(static_cast<wchar_t>(std::towupper(ch)));
    }
    return upper;
}

bool starts_with_case_insensitive(std::wstring_view value, std::wstring_view prefix) {
    if (prefix.size() > value.size()) {
        return false;
    }
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (std::towlower(value[i]) != std::towlower(prefix[i])) {
            return false;
        }
    }
    return true;
}

bool ends_with_case_insensitive(const std::wstring& value, const std::wstring& suffix) {
    if (suffix.size() > value.size()) {
        return false;
    }

    const size_t offset = value.size() - suffix.size();
    for (size_t i = 0; i < suffix.size(); ++i) {
        if (std::towlower(value[offset + i]) != std::towlower(suffix[i])) {
            return false;
        }
    }
    return true;
}

std::wstring trim_copy(const std::wstring& value) {
    return trim_copy(std::wstring_view(value));
}

std::wstring trim_copy(std::wstring_view value) {
    const std::wstring_view whitespace = L" \t\r\n";
    const size_t start = value.find_first_not_of(whitespace);
    if (start == std::wstring::npos) {
        return L"";
    }
    const size_t end = value.find_last_not_of(whitespace);
    return std::wstring(value.substr(start, end - start + 1));
}

std::wstring decode_multibyte(const std::string& input, UINT code_page) {
    if (input.empty()) {
        return L"";
    }

    if (input.size() > static_cast<size_t>(INT_MAX)) {
        return L"";
    }

    const int input_len = static_cast<int>(input.size());
    DWORD flags = (code_page == CP_UTF8) ? MB_ERR_INVALID_CHARS : 0;

    int wide_len = MultiByteToWideChar(code_page, flags, input.data(), input_len, nullptr, 0);
    if (wide_len <= 0 && flags != 0) {
        wide_len = MultiByteToWideChar(code_page, 0, input.data(), input_len, nullptr, 0);
    }
    if (wide_len <= 0) {
        return L"";
    }

    std::wstring output(static_cast<size_t>(wide_len), L'\0');
    if (!MultiByteToWideChar(code_page, 0, input.data(), input_len, &output[0], wide_len)) {
        return L"";
    }
    return output;
}

bool try_parse_int_strict(const std::wstring& raw, int& parsed_value) {
    if (raw.empty()) {
        return false;
    }

    wchar_t* end_ptr = nullptr;
    errno = 0;
    long parsed = std::wcstol(raw.c_str(), &end_ptr, 10);
    if (errno != 0 || end_ptr == raw.c_str() || (end_ptr != nullptr && *end_ptr != L'\0')) {
        return false;
    }
    if (parsed < static_cast<long>(INT_MIN) || parsed > static_cast<long>(INT_MAX)) {
        return false;
    }

    parsed_value = static_cast<int>(parsed);
    return true;
}

bool try_parse_unsigned_long_strict(const std::wstring& raw, unsigned long& parsed_value) {
    if (raw.empty()) {
        return false;
    }

    wchar_t* end_ptr = nullptr;
    errno = 0;
    unsigned long parsed = std::wcstoul(raw.c_str(), &end_ptr, 10);
    if (errno != 0 || end_ptr == raw.c_str() || (end_ptr != nullptr && *end_ptr != L'\0')) {
        return false;
    }

    parsed_value = parsed;
    return true;
}

bool try_parse_positive_int_strict(const std::wstring& raw, int& parsed_value) {
    if (!try_parse_int_strict(raw, parsed_value)) {
        return false;
    }
    return parsed_value > 0;
}

bool parse_positive_int(const std::wstring& text, int& value) {
    if (!try_parse_positive_int_strict(text, value)) {
        return false;
    }
    return value > 0;
}

bool try_parse_strict_double(const std::wstring& token, double& value) {
    if (token.empty()) {
        return false;
    }

    wchar_t* end_ptr = nullptr;
    value = std::wcstod(token.c_str(), &end_ptr);
    return end_ptr != token.c_str() && end_ptr != nullptr && *end_ptr == L'\0';
}

bool is_valid_shell_identifier(const std::wstring& name) {
    if (name.empty()) {
        return false;
    }
    if (name[0] == L'.') {
        if (name.size() < 2) return false;
        return std::all_of(name.begin() + 1, name.end(), [](wchar_t ch) {
            return std::iswalnum(ch) != 0 || ch == L'_' || ch == L'.';
        });
    }
    if (!(std::iswalpha(name[0]) || name[0] == L'_')) {
        return false;
    }
    return std::all_of(name.begin(), name.end(), [](wchar_t ch) {
        return std::iswalnum(ch) != 0 || ch == L'_' || ch == L'.';
    });
}

bool validate_shell_lexical_state(const std::wstring& input, std::wstring& error_message) {
    bool in_single_quotes = false;
    bool in_double_quotes = false;
    bool in_backticks = false;
    bool escaped = false;

    for (wchar_t ch : input) {
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
        } else if (ch == L'"' && !in_single_quotes) {
            in_double_quotes = !in_double_quotes;
        } else if (ch == L'`' && !in_single_quotes) {
            in_backticks = !in_backticks;
        }
    }

    if (in_single_quotes || in_double_quotes || in_backticks) {
        error_message = in_backticks ? L"ksh: syntax error: unterminated backtick" : L"ksh: syntax error: unterminated quote";
        return false;
    }
    if (escaped) {
        const std::wstring trimmed = trim_copy(input);
        const size_t command_end = trimmed.find_first_of(L" \t");
        const std::wstring command = trimmed.substr(0, command_end);
        if ((command == L"cd" || command == L"cp" || command == L"mv" || command == L"ls") &&
            trimmed.size() > command.size() && trimmed.back() == L'\\') {
            return true;
        }
        error_message = L"ksh: syntax error: trailing escape";
        return false;
    }
    return true;
}

bool has_unquoted_shell_metacharacters(const std::wstring& input) {
    bool in_single_quotes = false;
    bool in_double_quotes = false;
    bool escaped = false;
    int paren_depth = 0;

    for (wchar_t ch : input) {
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

        if (!in_single_quotes && !in_double_quotes) {
            if (ch == L'(') {
                paren_depth++;
            } else if (ch == L')') {
                if (paren_depth > 0) paren_depth--;
            } else if (paren_depth == 0 && (ch == L'>' || ch == L'<' || ch == L'&' || ch == L'|' || ch == L';')) {
                return true;
            }
        }
    }

    return false;
}

std::wstring remove_quotes_and_escapes_from_token(const std::wstring& token) {
    std::wstring result;
    bool in_double_quotes = false;
    bool in_single_quotes = false;

    for (size_t i = 0; i < token.size(); ++i) {
        wchar_t ch = token[i];

        if (ch == L'"' && !in_single_quotes) {
            in_double_quotes = !in_double_quotes;
            continue;
        }

        if (ch == L'\'' && !in_double_quotes) {
            in_single_quotes = !in_single_quotes;
            continue;
        }

        if (ch == L'\\' && !in_single_quotes) {
            if (i + 1 >= token.size()) {
                result += ch;
                continue;
            }

            wchar_t next = token[i + 1];
            if (!in_double_quotes) {
                if (std::iswspace(next) || next == L'\\' || next == L'"' || next == L'\'' || next == L'$' ||
                    next == L'`' || next == L'|' || next == L'&' || next == L';' || next == L'<' ||
                    next == L'>' || next == L'(' || next == L')') {
                    result += next;
                    i++;
                    continue;
                }

                result += ch;
                continue;
            }

            if (next == L'\\' || next == L'"' || next == L'$' || next == L'`' || next == L'\n') {
                result += next;
                i++;
                continue;
            }

            result += ch;
            continue;
        }

        result += ch;
    }

    return result;
}

std::vector<std::wstring> remove_quotes_and_escapes_from_tokens(const std::vector<std::wstring>& tokens) {
    std::vector<std::wstring> stripped;
    stripped.reserve(tokens.size());
    for (const std::wstring& token : tokens) {
        stripped.push_back(remove_quotes_and_escapes_from_token(token));
    }
    return stripped;
}

bool has_glob_metacharacters(const std::wstring& value) {
    if (value.find_first_of(L"*?[") != std::wstring::npos) {
        return true;
    }
    return value.find(L"+(") != std::wstring::npos ||
           value.find(L"@(") != std::wstring::npos ||
           value.find(L"!(") != std::wstring::npos;
}

bool has_unquoted_glob_metacharacters(const std::wstring& token) {
    bool in_double_quotes = false;
    bool in_single_quotes = false;
    bool escaped = false;

    for (size_t i = 0; i < token.size(); ++i) {
        wchar_t ch = token[i];
        if (escaped) {
            escaped = false;
            continue;
        }

        if (ch == L'\\' && !in_single_quotes) {
            escaped = true;
            continue;
        }

        if (ch == L'"' && !in_single_quotes) {
            in_double_quotes = !in_double_quotes;
            continue;
        }

        if (ch == L'\'' && !in_double_quotes) {
            in_single_quotes = !in_single_quotes;
            continue;
        }

        if (!in_double_quotes && !in_single_quotes) {
            if (ch == L'*' || ch == L'?' || ch == L'[') {
                return true;
            }
            if (ch == L'+' || ch == L'@' || ch == L'!') {
                if (i + 1 < token.size() && token[i + 1] == L'(') {
                    return true;
                }
            }
        }
    }

    return false;
}

bool contains_unquoted_wildcards(const std::wstring& token) {
    return has_unquoted_glob_metacharacters(token);
}

bool match_glob_character_class(const std::wstring& pattern, size_t class_start, wchar_t candidate, size_t& consumed_length, bool case_sensitive) {
    consumed_length = 0;
    if (class_start >= pattern.size() || pattern[class_start] != L'[') {
        return false;
    }

    size_t i = class_start + 1;
    bool negate = false;
    if (i < pattern.size() && (pattern[i] == L'!' || pattern[i] == L'^')) {
        negate = true;
        i++;
    }

    bool matched = false;
    bool saw_item = false;
    while (i < pattern.size() && pattern[i] != L']') {
        wchar_t left = pattern[i];
        if ((i + 2) < pattern.size() && pattern[i + 1] == L'-' && pattern[i + 2] != L']') {
            wchar_t right = pattern[i + 2];
            if (case_sensitive) {
                if (left <= candidate && candidate <= right) {
                    matched = true;
                }
            } else {
                wchar_t lower_candidate = static_cast<wchar_t>(std::towlower(candidate));
                wchar_t lower_left = static_cast<wchar_t>(std::towlower(left));
                wchar_t lower_right = static_cast<wchar_t>(std::towlower(right));
                if (lower_left <= lower_candidate && lower_candidate <= lower_right) {
                    matched = true;
                }
            }
            i += 3;
            saw_item = true;
            continue;
        }

        if (case_sensitive ? (left == candidate) : (std::towlower(left) == std::towlower(candidate))) {
            matched = true;
        }
        i++;
        saw_item = true;
    }

    if (i >= pattern.size() || pattern[i] != L']' || !saw_item) {
        return false;
    }

    consumed_length = (i - class_start) + 1;
    return negate ? !matched : matched;
}

bool parse_extended_glob(const std::wstring& pattern, size_t ext_start, size_t& ext_end, std::vector<std::wstring>& sub_patterns) {
    if (ext_start + 2 >= pattern.size() || pattern[ext_start + 1] != L'(') {
        return false;
    }

    size_t i = ext_start + 2;
    int paren_level = 1;
    std::wstring current;
    std::vector<std::wstring> parts;

    bool in_double_quotes = false;
    bool in_single_quotes = false;
    bool escaped = false;

    while (i < pattern.size()) {
        wchar_t ch = pattern[i];

        if (escaped) {
            current += L'\\';
            current += ch;
            escaped = false;
            i++;
            continue;
        }

        if (ch == L'\\' && !in_single_quotes) {
            escaped = true;
            i++;
            continue;
        }

        if (ch == L'"' && !in_single_quotes) {
            in_double_quotes = !in_double_quotes;
            current += ch;
            i++;
            continue;
        }

        if (ch == L'\'' && !in_double_quotes) {
            in_single_quotes = !in_single_quotes;
            current += ch;
            i++;
            continue;
        }

        if (!in_double_quotes && !in_single_quotes) {
            if (ch == L'(') {
                paren_level++;
            } else if (ch == L')') {
                paren_level--;
                if (paren_level == 0) {
                    parts.push_back(current);
                    ext_end = i;
                    sub_patterns = parts;
                    return true;
                }
            } else if (ch == L'|' && paren_level == 1) {
                parts.push_back(current);
                current.clear();
                i++;
                continue;
            }
        }

        current += ch;
        i++;
    }

    return false;
}

bool match_glob_pattern_recursive(
    const std::wstring& pattern,
    size_t pattern_index,
    const std::wstring& candidate,
    size_t candidate_index,
    bool case_sensitive,
    unsigned long long& step_budget) {
    if (step_budget == 0) {
        return false;
    }
    step_budget--;

    while (pattern_index < pattern.size()) {
        wchar_t p = pattern[pattern_index];

        bool is_ext_glob = false;
        if (pattern_index + 1 < pattern.size() && pattern[pattern_index + 1] == L'(') {
            if (p == L'+' || p == L'*' || p == L'?' || p == L'@' || p == L'!') {
                is_ext_glob = true;
            }
        }

        if (is_ext_glob) {
            size_t ext_end = 0;
            std::vector<std::wstring> sub_patterns;
            if (parse_extended_glob(pattern, pattern_index, ext_end, sub_patterns)) {
                std::wstring remaining_pattern = pattern.substr(ext_end + 1);
                wchar_t op = p;

                if (op == L'?') {
                    if (match_glob_pattern_recursive(remaining_pattern, 0, candidate, candidate_index, case_sensitive, step_budget)) {
                        return true;
                    }
                    for (const auto& sub : sub_patterns) {
                        std::wstring temp = sub + remaining_pattern;
                        if (match_glob_pattern_recursive(temp, 0, candidate, candidate_index, case_sensitive, step_budget)) {
                            return true;
                        }
                    }
                    return false;
                }

                if (op == L'@') {
                    for (const auto& sub : sub_patterns) {
                        std::wstring temp = sub + remaining_pattern;
                        if (match_glob_pattern_recursive(temp, 0, candidate, candidate_index, case_sensitive, step_budget)) {
                            return true;
                        }
                    }
                    return false;
                }

                if (op == L'*') {
                    if (match_glob_pattern_recursive(remaining_pattern, 0, candidate, candidate_index, case_sensitive, step_budget)) {
                        return true;
                    }
                    std::wstring ext_glob_part = pattern.substr(pattern_index, ext_end - pattern_index + 1);
                    for (const auto& sub : sub_patterns) {
                        std::wstring temp = sub + ext_glob_part + remaining_pattern;
                        if (match_glob_pattern_recursive(temp, 0, candidate, candidate_index, case_sensitive, step_budget)) {
                            return true;
                        }
                    }
                    return false;
                }

                if (op == L'+') {
                    std::wstring star_glob_part = L"*" + pattern.substr(pattern_index + 1, ext_end - pattern_index);
                    for (const auto& sub : sub_patterns) {
                        std::wstring temp = sub + star_glob_part + remaining_pattern;
                        if (match_glob_pattern_recursive(temp, 0, candidate, candidate_index, case_sensitive, step_budget)) {
                            return true;
                        }
                    }
                    return false;
                }

                if (op == L'!') {
                    size_t max_L = candidate.size() - candidate_index;
                    for (size_t L = 0; L <= max_L; ++L) {
                        std::wstring candidate_prefix = candidate.substr(candidate_index, L);
                        bool matches_any = false;
                        for (const auto& sub : sub_patterns) {
                            unsigned long long local_budget = step_budget;
                            if (match_glob_pattern_recursive(sub, 0, candidate_prefix, 0, case_sensitive, local_budget)) {
                                matches_any = true;
                                break;
                            }
                        }

                        if (!matches_any) {
                            if (match_glob_pattern_recursive(remaining_pattern, 0, candidate, candidate_index + L, case_sensitive, step_budget)) {
                                return true;
                            }
                        }
                    }
                    return false;
                }
            }
        }

        if (p == L'*') {
            while (pattern_index < pattern.size() && pattern[pattern_index] == L'*') {
                pattern_index++;
            }
            if (pattern_index == pattern.size()) {
                return true;
            }
            while (candidate_index <= candidate.size()) {
                if (match_glob_pattern_recursive(pattern, pattern_index, candidate, candidate_index, case_sensitive, step_budget)) {
                    return true;
                }
                if (candidate_index == candidate.size()) {
                    break;
                }
                candidate_index++;
            }
            return false;
        }

        if (candidate_index >= candidate.size()) {
            return false;
        }

        if (p == L'?') {
            pattern_index++;
            candidate_index++;
            continue;
        }

        if (p == L'[') {
            size_t consumed = 0;
            if (!match_glob_character_class(pattern, pattern_index, candidate[candidate_index], consumed, case_sensitive)) {
                return false;
            }
            pattern_index += consumed;
            candidate_index++;
            continue;
        }

        if (case_sensitive ? (p != candidate[candidate_index]) : (std::towlower(p) != std::towlower(candidate[candidate_index]))) {
            return false;
        }
        pattern_index++;
        candidate_index++;
    }

    return candidate_index == candidate.size();
}

bool match_glob_pattern(const std::wstring& pattern, const std::wstring& candidate, bool case_sensitive) {
    if (!candidate.empty() && candidate[0] == L'.' && (pattern.empty() || pattern[0] != L'.')) {
        return false;
    }

    unsigned long long step_budget = kMaxPatternMatchSteps;
    return match_glob_pattern_recursive(pattern, 0, candidate, 0, case_sensitive, step_budget);
}

std::wstring expand_ansi_c_quoting(const std::wstring& input, size_t& i) {
    std::wstring result;
    i += 2;
    while (i < input.length()) {
        wchar_t c = input[i];
        if (c == L'\'') {
            i++;
            break;
        }
        if (c == L'\\') {
            i++;
            if (i >= input.length()) {
                result += L'\\';
                break;
            }
            wchar_t escape_char = input[i];
            if (escape_char == L'a') { result += L'\a'; i++; }
            else if (escape_char == L'b') { result += L'\b'; i++; }
            else if (escape_char == L'e' || escape_char == L'E') { result += L'\x1B'; i++; }
            else if (escape_char == L'f') { result += L'\f'; i++; }
            else if (escape_char == L'n') { result += L'\n'; i++; }
            else if (escape_char == L'r') { result += L'\r'; i++; }
            else if (escape_char == L't') { result += L'\t'; i++; }
            else if (escape_char == L'v') { result += L'\v'; i++; }
            else if (escape_char == L'\\') { result += L'\\'; i++; }
            else if (escape_char == L'\'') { result += L'\''; i++; }
            else if (escape_char == L'"') { result += L'"'; i++; }
            else if (escape_char == L'?') { result += L'?'; i++; }
            else if (escape_char == L'x') {
                i++;
                std::wstring hex_str;
                for (int count = 0; count < 2 && i < input.length(); ++count) {
                    if (std::iswxdigit(input[i])) {
                        hex_str.push_back(input[i]);
                        i++;
                    } else {
                        break;
                    }
                }
                if (!hex_str.empty()) {
                    unsigned long val = 0;
                    for (wchar_t digit : hex_str) {
                        val *= 16;
                        if (digit >= L'0' && digit <= L'9') val += (digit - L'0');
                        else if (digit >= L'a' && digit <= L'f') val += (digit - L'a' + 10);
                        else if (digit >= L'A' && digit <= L'F') val += (digit - L'A' + 10);
                    }
                    result += static_cast<wchar_t>(val);
                } else {
                    result += L'x';
                }
            } else if (escape_char == L'u') {
                i++;
                std::wstring hex_str;
                for (int count = 0; count < 4 && i < input.length(); ++count) {
                    if (std::iswxdigit(input[i])) {
                        hex_str.push_back(input[i]);
                        i++;
                    } else {
                        break;
                    }
                }
                if (!hex_str.empty()) {
                    unsigned long val = 0;
                    for (wchar_t digit : hex_str) {
                        val *= 16;
                        if (digit >= L'0' && digit <= L'9') val += (digit - L'0');
                        else if (digit >= L'a' && digit <= L'f') val += (digit - L'a' + 10);
                        else if (digit >= L'A' && digit <= L'F') val += (digit - L'A' + 10);
                    }
                    result += static_cast<wchar_t>(val);
                } else {
                    result += L'u';
                }
            } else if (escape_char == L'U') {
                i++;
                std::wstring hex_str;
                for (int count = 0; count < 8 && i < input.length(); ++count) {
                    if (std::iswxdigit(input[i])) {
                        hex_str.push_back(input[i]);
                        i++;
                    } else {
                        break;
                    }
                }
                if (!hex_str.empty()) {
                    unsigned long val = 0;
                    for (wchar_t digit : hex_str) {
                        val *= 16;
                        if (digit >= L'0' && digit <= L'9') val += (digit - L'0');
                        else if (digit >= L'a' && digit <= L'f') val += (digit - L'a' + 10);
                        else if (digit >= L'A' && digit <= L'F') val += (digit - L'A' + 10);
                    }
                    result += static_cast<wchar_t>(val);
                } else {
                    result += L'U';
                }
            } else if (escape_char == L'c') {
                i++;
                if (i < input.length()) {
                    wchar_t ctrl_char = input[i];
                    result += static_cast<wchar_t>(ctrl_char & 0x1F);
                    i++;
                } else {
                    result += L'c';
                }
            } else if (escape_char >= L'0' && escape_char <= L'7') {
                std::wstring oct_str;
                for (int count = 0; count < 3 && i < input.length(); ++count) {
                    if (input[i] >= L'0' && input[i] <= L'7') {
                        oct_str.push_back(input[i]);
                        i++;
                    } else {
                        break;
                    }
                }
                unsigned long val = 0;
                for (wchar_t digit : oct_str) {
                    val = val * 8 + (digit - L'0');
                }
                result += static_cast<wchar_t>(val);
            } else {
                result += escape_char;
                i++;
            }
        } else {
            result += c;
            i++;
        }
    }
    return result;
}

std::wstring escape_for_double_quotes(const std::wstring& str) {
    std::wstring result;
    result += L'"';
    for (wchar_t ch : str) {
        if (ch == L'\\' || ch == L'"' || ch == L'$' || ch == L'`' || ch == L'\n') {
            result += L'\\';
        }
        result += ch;
    }
    result += L'"';
    return result;
}

std::vector<std::wstring> ksh_tokenize(const std::wstring& input) {
    std::vector<std::wstring> tokens;
    std::wstring current;
    bool in_quotes = false;
    bool in_single_quotes = false;
    bool in_backticks = false;
    bool in_array_def = false;
    bool escaped = false;

    for (size_t i = 0; i < input.length(); ) {
        wchar_t c = input[i];

        if (c == k_array_at_quoted_separator) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            i++;
            continue;
        }

        if (escaped) {
            current += c;
            escaped = false;
            i++;
            continue;
        }

        if (c == L'\\' && !in_single_quotes) {
            current += c;
            escaped = true;
            i++;
            continue;
        }

        if (c == L'$' && i + 1 < input.length() && input[i + 1] == L'\'' && !in_quotes && !in_single_quotes && !in_array_def) {
            current += expand_ansi_c_quoting(input, i);
            continue;
        }

        if (c == L'`' && !in_single_quotes) {
            in_backticks = !in_backticks;
            current += c;
            i++;
            continue;
        }

        if (c == L'"' && !in_array_def && !in_single_quotes) {
            in_quotes = !in_quotes;
            i++;
        } else if (c == L'\'' && !in_array_def && !in_quotes) {
            in_single_quotes = !in_single_quotes;
            i++;
        } else if (c == L'(' && i > 0 && input[i-1] == L'=') {
            in_array_def = true;
            current += c;
            i++;
        } else if (c == L')' && in_array_def) {
            in_array_def = false;
            current += c;
            i++;
        } else if ((c == L' ' || c == L'\t') && !in_quotes && !in_single_quotes && !in_backticks && !in_array_def) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            i++;
        } else {
            current += c;
            i++;
        }
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

std::vector<std::wstring> ksh_tokenize_preserve_quotes(const std::wstring& input) {
    std::vector<std::wstring> tokens;
    std::wstring current;
    bool in_quotes = false;
    bool in_single_quotes = false;
    bool in_backticks = false;
    bool in_array_def = false;
    bool escaped = false;

    for (size_t i = 0; i < input.length(); ) {
        wchar_t c = input[i];

        if (c == k_array_at_quoted_separator) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            i++;
            continue;
        }

        if (escaped) {
            current += c;
            escaped = false;
            i++;
            continue;
        }

        if (c == L'\\' && !in_single_quotes) {
            current += c;
            escaped = true;
            i++;
            continue;
        }

        if (c == L'$' && i + 1 < input.length() && input[i + 1] == L'\'' && !in_quotes && !in_single_quotes && !in_array_def) {
            current += escape_for_double_quotes(expand_ansi_c_quoting(input, i));
            continue;
        }

        if (c == L'`' && !in_single_quotes) {
            in_backticks = !in_backticks;
            current += c;
            i++;
            continue;
        }

        if (c == L'"' && !in_array_def && !in_single_quotes) {
            in_quotes = !in_quotes;
            current += c;
            i++;
            continue;
        }

        if (c == L'\'' && !in_array_def && !in_quotes) {
            in_single_quotes = !in_single_quotes;
            current += c;
            i++;
            continue;
        }

        if (c == L'(' && i > 0 && input[i - 1] == L'=') {
            in_array_def = true;
            current += c;
            i++;
            continue;
        }

        if (c == L')' && in_array_def) {
            in_array_def = false;
            current += c;
            i++;
            continue;
        }

        if ((c == L' ' || c == L'\t') && !in_quotes && !in_single_quotes && !in_backticks && !in_array_def) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            i++;
            continue;
        }

        current += c;
        i++;
    }

    if (!current.empty()) {
        tokens.push_back(current);
    }

    return tokens;
}

std::wstring join_tokens_as_command_line(const std::vector<std::wstring>& tokens) {
    std::wstring command;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (!command.empty()) {
            command += L" ";
        }
        command += quote_command_argument(tokens[i]);
    }
    return command;
}

std::wstring join_tokens_with_spaces(const std::vector<std::wstring>& tokens, size_t start_index) {
    size_t estimated_size = 0;
    if (start_index < tokens.size()) {
        for (size_t i = start_index; i < tokens.size(); ++i) {
            estimated_size += tokens[i].size();
            if (i + 1 < tokens.size()) {
                estimated_size += 1;
            }
        }
    }

    std::wstring joined;
    joined.reserve(estimated_size);
    for (size_t i = start_index; i < tokens.size(); ++i) {
        if (!joined.empty()) {
            joined += L" ";
        }
        joined += tokens[i];
    }
    return joined;
}

std::wstring quote_for_single_quoted_shell_literal(const std::wstring& value) {
    std::wstring quoted = L"'";
    for (wchar_t ch : value) {
        if (ch == L'\'') {
            quoted += L"'\\''";
            continue;
        }
        quoted.push_back(ch);
    }
    quoted += L"'";
    return quoted;
}

