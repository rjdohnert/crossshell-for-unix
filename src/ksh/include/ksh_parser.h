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

#ifndef CROSSSHELL_KSH_PARSER_H
#define CROSSSHELL_KSH_PARSER_H

#include "ksh_types.h"

std::wstring to_upper_copy(const std::wstring& value);
std::vector<std::wstring> ksh_tokenize_preserve_quotes(const std::wstring& input);
bool has_unquoted_shell_metacharacters(const std::wstring& input);
bool is_valid_shell_identifier(const std::wstring& name);
bool validate_shell_lexical_state(const std::wstring& input, std::wstring& error_message);
std::wstring remove_quotes_and_escapes_from_token(const std::wstring& token);

inline bool is_path_separator(wchar_t ch) { return ch == L'/' || ch == L'\\'; }
bool has_glob_metacharacters(const std::wstring& value);
bool has_unquoted_glob_metacharacters(const std::wstring& token);
bool contains_unquoted_wildcards(const std::wstring& token);
bool match_glob_character_class(const std::wstring& pattern, size_t class_start, wchar_t candidate, size_t& consumed_length, bool case_sensitive);
bool parse_extended_glob(const std::wstring& pattern, size_t ext_start, size_t& ext_end, std::vector<std::wstring>& sub_patterns);
bool match_glob_pattern_recursive(
    const std::wstring& pattern,
    size_t pattern_index,
    const std::wstring& candidate,
    size_t candidate_index,
    bool case_sensitive,
    unsigned long long& step_budget);
bool match_glob_pattern(const std::wstring& pattern, const std::wstring& candidate, bool case_sensitive = false);

std::wstring to_lower_copy(const std::wstring& value);
std::wstring to_lower_copy(std::wstring_view value);
bool starts_with_case_insensitive(std::wstring_view value, std::wstring_view prefix);
bool ends_with_case_insensitive(const std::wstring& value, const std::wstring& suffix);
std::wstring trim_copy(const std::wstring& value);
std::wstring trim_copy(std::wstring_view value);
std::wstring decode_multibyte(const std::string& input, UINT code_page);

bool try_parse_int_strict(const std::wstring& text, int& out_value);
bool try_parse_positive_int_strict(const std::wstring& text, int& out_value);
bool parse_positive_int(const std::wstring& text, int& value);
bool try_parse_unsigned_long_strict(const std::wstring& text, unsigned long& out_value);
bool try_parse_strict_double(const std::wstring& text, double& out_value);

std::vector<std::wstring> ksh_tokenize(const std::wstring& line);
std::wstring expand_ansi_c_quoting(const std::wstring& input, size_t& i);
std::wstring quote_for_single_quoted_shell_literal(const std::wstring& value);
std::wstring join_tokens_as_command_line(const std::vector<std::wstring>& tokens);
std::wstring join_tokens_with_spaces(const std::vector<std::wstring>& tokens, size_t start_index = 0);
std::vector<std::wstring> remove_quotes_and_escapes_from_tokens(const std::vector<std::wstring>& tokens);

#endif // CROSSSHELL_KSH_PARSER_H
