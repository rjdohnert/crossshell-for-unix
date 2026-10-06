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

#ifndef CROSSSHELL_KSH_EXPANSION_H
#define CROSSSHELL_KSH_EXPANSION_H

#include "ksh_common.h"
#include "ksh_types.h"
#include "ksh_state.h"

class ScopedExpansionDepth {
public:
    ScopedExpansionDepth() : active_(false) {
        if (g_expansion_nesting_depth >= kMaxExpansionNestingDepth) {
            return;
        }
        g_expansion_nesting_depth++;
        active_ = true;
    }

    ~ScopedExpansionDepth() {
        if (active_ && g_expansion_nesting_depth > 0) {
            g_expansion_nesting_depth--;
        }
    }

    bool active() const {
        return active_;
    }

private:
    bool active_;
};

double evaluate_math_function(const std::wstring& function_name, const std::vector<double>& args);
std::wstring evaluate_arithmetic(const std::wstring& expr);
double evaluate_arithmetic_double(const std::wstring& expr);
std::wstring trim_trailing_line_endings(const std::wstring& value);

bool parse_command_substitution_content(const std::wstring& input, size_t start_index, std::wstring& content, size_t& next_index);
bool parse_backtick_command_substitution_content(const std::wstring& input, size_t start_index, std::wstring& content, size_t& next_index);
bool parse_arithmetic_substitution_content(const std::wstring& input, size_t start_index, std::wstring& content, size_t& next_index);
std::wstring expand_variable_reference(const std::wstring& input, size_t dollar_index, size_t& next_index, bool in_double_quotes);
std::wstring expand_substitutions_left_to_right(const std::wstring& input);
std::wstring evaluate_command_substitutions(const std::wstring& input);

bool can_capture_builtin_command_substitution(const std::vector<std::wstring>& tokens);
bool is_snapshot_safe_external_segment(const std::wstring& segment_text);
bool command_requires_environment_snapshot(const std::wstring& command);
std::wstring execute_command_substitution(const std::wstring& command, bool use_capture_sink);

std::wstring join_fields_with_internal_separator(const std::vector<std::wstring>& fields);
std::wstring effective_ifs_value();
std::vector<std::wstring> split_fields_by_ifs(const std::wstring& input);
std::wstring remove_quotes_and_escapes_from_token(const std::wstring& token);
std::vector<std::wstring> remove_quotes_and_escapes_from_tokens(const std::vector<std::wstring>& tokens);
std::wstring ksh_expand(const std::wstring& input);
std::wstring escape_for_double_quotes(const std::wstring& str);

std::wstring expand_tilde(const std::wstring& input);
std::vector<std::wstring> expand_tilde_for_tokens(const std::vector<std::wstring>& tokens);
std::wstring join_path_for_glob(const std::wstring& base, const std::wstring& name);
std::wstring normalize_relative_glob_result(const std::wstring& value);
void expand_glob_token(const std::wstring& pattern, std::vector<std::wstring>& matches);
std::vector<std::wstring> expand_globs_for_tokens(const std::vector<std::wstring>& tokens, const std::vector<std::wstring>& preserved_tokens);

std::wstring run_command_and_capture_stdout(const std::wstring& command, DWORD& exit_code);

#endif // CROSSSHELL_KSH_EXPANSION_H