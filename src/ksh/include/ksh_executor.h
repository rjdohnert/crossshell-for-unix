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

#ifndef CROSSSHELL_KSH_EXECUTOR_H
#define CROSSSHELL_KSH_EXECUTOR_H

#include "ksh_types.h"

bool execute_command_line(const std::wstring& input_line, bool& should_exit_shell, bool bypass_aliases = false, bool bypass_function_lookup = false);
bool execute_command_line_impl(const std::wstring& input_line, bool& should_exit_shell, bool bypass_aliases = false, bool bypass_function_lookup = false);
bool execute_script_file(const std::wstring& script_path, const std::vector<std::wstring>& script_args, bool& should_exit_shell);
bool execute_defined_function(const std::wstring& function_name, const std::vector<std::wstring>& function_args, bool& should_exit_shell);

bool is_script_empty_or_comment_line(const std::wstring& trimmed);
bool starts_with_if_control_keyword(const std::wstring& trimmed);
bool starts_with_elif_control_keyword(const std::wstring& trimmed);
bool starts_with_while_control_keyword(const std::wstring& trimmed);
bool starts_with_until_control_keyword(const std::wstring& trimmed);
bool starts_with_for_control_keyword(const std::wstring& trimmed);
bool starts_with_case_control_keyword(const std::wstring& trimmed);
bool starts_with_select_control_keyword(const std::wstring& trimmed);
bool starts_with_loop_control_keyword(const std::wstring& trimmed);

int count_unquoted_brace_delta(const std::wstring& line);
bool parse_function_header(const std::wstring& trimmed_line, std::wstring& function_name, bool& has_open_brace);
bool capture_function_definition(const std::vector<std::wstring>& lines, size_t& line_index, size_t end_index, std::wstring& error_message, size_t& error_line_index);

void parse_and_replace_process_substitutions(std::wstring& line);
std::wstring make_process_substitution_pipe_name(LONG id);

const std::vector<std::wstring>& current_script_args();
std::wstring current_script_name();
std::wstring join_script_args(const std::vector<std::wstring>& args);
std::wstring quote_command_argument(const std::wstring& arg);

std::wstring decode_script_text(const std::vector<char>& bytes);
std::vector<std::wstring> split_script_lines(const std::wstring& text);
std::vector<std::wstring> split_top_level_script_commands(const std::wstring& line);
std::vector<std::wstring> prepare_script_lines(const std::wstring& text);

class ScopedEvalDepth {
public:
    ScopedEvalDepth() : active_(false) {
        if (g_eval_nesting_depth >= kMaxEvalNestingDepth) {
            return;
        }
        g_eval_nesting_depth++;
        active_ = true;
    }

    ~ScopedEvalDepth() {
        if (active_ && g_eval_nesting_depth > 0) {
            g_eval_nesting_depth--;
        }
    }

    bool active() const {
        return active_;
    }

private:
    bool active_;
};

#endif // CROSSSHELL_KSH_EXECUTOR_H
