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

#ifndef CROSSSHELL_KSH_VARIABLES_H
#define CROSSSHELL_KSH_VARIABLES_H

#include "ksh_types.h"

bool is_executing_function_scope();
bool is_function_return_requested();
void request_function_return(int status_code);
void snapshot_local_variable_if_needed(const std::wstring& name);
void restore_function_scope_variables(FunctionScopeContext& context);
void sync_exported_environment_variable(const std::wstring& name);

bool is_valid_shell_identifier(const std::wstring& name);
bool is_internal_shell_state_variable(const std::wstring& name);
std::wstring get_variable_value_with_hooks(const std::wstring& name, bool& is_set);
std::wstring get_environment_value(const std::wstring& name);

std::wstring resolve_variable_name(const std::wstring& name, int depth = 0);
bool assign_parameter_value(const std::wstring& lhs, const std::wstring& rhs, bool array_hint, bool local_scope, std::wstring& error_message, bool is_nameref = false);
bool parse_array_reference_expression(const std::wstring& expression, std::wstring& base_name, std::wstring& index_text, bool& has_index);
bool parse_non_negative_index(const std::wstring& text, size_t& out_index);
std::vector<std::wstring> get_array_values_vector(const std::wstring& name);
void assign_scalar_parameter(const std::wstring& name, const std::wstring& value);
void assign_array_parameter(const std::wstring& name, const std::vector<std::wstring>& values);

bool read_registry_property(const std::wstring& property_path, std::wstring& value, RegistryValueMetadata& metadata);
bool write_registry_property(const std::wstring& property_path, const std::wstring& value);
void ensure_registry_namespaces_registered();

bool parse_custom_type_definition(const std::wstring& type_name, const std::wstring& body, CustomTypeDefinition& def);
void instantiate_custom_type(const std::wstring& type_name, const std::wstring& var_prefix);

#endif // CROSSSHELL_KSH_VARIABLES_H
