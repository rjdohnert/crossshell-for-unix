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

#ifndef CROSSSHELL_KSH_BUILTINS_H
#define CROSSSHELL_KSH_BUILTINS_H

#include "ksh_types.h"

CommandResolutionKind resolve_command_kind(const std::wstring& name, std::wstring& detail);
std::wstring command_kind_description(const std::wstring& name, bool verbose = false, bool ksh_style = false);
bool resolve_external_command_path(const std::wstring& command_name, std::wstring& resolved_path);
const std::vector<std::wstring>& builtin_commands();
std::wstring get_command_name(const std::wstring& full_command);
bool is_ksh_builtin_command(const std::wstring& cmd);
bool is_valid_builtin_name(const std::wstring& name);
bool is_alias_defined(const std::wstring& name);
bool initialize_default_shell_aliases();

void print_help_text();
void write_full_help_text(const std::function<bool(const std::wstring&)>& write_output);
bool write_help_topic(const std::wstring& raw_topic, const std::function<bool(const std::wstring&)>& write_output);
bool execute_builtin_help(const std::vector<std::wstring>& tokens, const std::function<bool(const std::wstring&)>& write_output);

bool extract_ksh_conditional_block(const std::wstring& expanded_input, std::wstring& condition, std::wstring& error_message);
bool tokenize_ksh_conditional(const std::wstring& condition, std::vector<std::wstring>& tokens, std::wstring& error_message);
bool evaluate_test_condition_expression(const std::vector<std::wstring>& tokens, bool& result, std::wstring& error_message);
bool evaluate_ksh_conditional(const std::wstring& condition_str, bool& value, std::wstring& error_message);

bool format_printf_time_argument(const std::wstring& arg_value, std::wstring& formatted_time);
bool evaluate_builtin_printf(const std::vector<std::wstring>& tokens, std::wstring& output_str, std::wstring& error_msg);

std::wstring decode_raw_bytes(const std::string& bytes);
bool execute_builtin_read(const std::vector<std::wstring>& tokens);
bool execute_builtin_getconf(const std::vector<std::wstring>& tokens, const std::function<bool(const std::wstring&)>& write_output);
bool execute_builtin_pathchk(const std::vector<std::wstring>& tokens);

#endif // CROSSSHELL_KSH_BUILTINS_H
