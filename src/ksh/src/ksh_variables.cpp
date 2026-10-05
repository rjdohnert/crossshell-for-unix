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

#include "ksh_internal.h"

bool is_executing_function_scope() {
    return !g_function_scope_stack.empty();
}

bool is_function_return_requested() {
    return !g_function_scope_stack.empty() && g_function_scope_stack.back().return_requested;
}

void request_function_return(int status_code) {
    if (g_function_scope_stack.empty()) {
        return;
    }

    g_function_scope_stack.back().return_requested = true;
    g_function_scope_stack.back().return_status = status_code;
}

void snapshot_local_variable_if_needed(const std::wstring& name) {
    if (!is_executing_function_scope()) {
        return;
    }

    FunctionScopeContext& context = g_function_scope_stack.back();
    if (context.locals.find(name) != context.locals.end()) {
        return;
    }

    VariableScopeSnapshot snapshot;

    std::map<std::wstring, std::wstring>::const_iterator scalar_it = ksh_env.variables.find(name);
    if (scalar_it != ksh_env.variables.end()) {
        snapshot.had_scalar = true;
        snapshot.scalar_value = scalar_it->second;
    }

    std::map<std::wstring, std::map<std::wstring, std::wstring>>::const_iterator array_it = ksh_env.arrays.find(name);
    if (array_it != ksh_env.arrays.end()) {
        snapshot.had_array = true;
        snapshot.array_value = array_it->second;
    }

    std::map<std::wstring, bool>::const_iterator assoc_it = ksh_env.associative_flags.find(name);
    if (assoc_it != ksh_env.associative_flags.end()) {
        snapshot.had_associative = true;
        snapshot.associative_value = assoc_it->second;
    }

    std::map<std::wstring, wchar_t>::const_iterator justify_mode_it = ksh_env.justify_modes.find(name);
    if (justify_mode_it != ksh_env.justify_modes.end()) {
        snapshot.had_justify_mode = true;
        snapshot.justify_mode_value = justify_mode_it->second;
    }

    std::map<std::wstring, int>::const_iterator justify_width_it = ksh_env.justify_widths.find(name);
    if (justify_width_it != ksh_env.justify_widths.end()) {
        snapshot.had_justify_width = true;
        snapshot.justify_width_value = justify_width_it->second;
    }

    DWORD env_len = GetEnvironmentVariableW(name.c_str(), nullptr, 0);
    if (env_len > 0) {
        snapshot.had_environment = true;
        std::wstring environment_value;
        environment_value.resize(env_len - 1);
        if (env_len > 1) {
            GetEnvironmentVariableW(name.c_str(), &environment_value[0], env_len);
        }
        snapshot.environment_value = std::move(environment_value);
    }

    snapshot.had_exported = get_flag_value(ksh_env.exported, name);
    snapshot.exported_value = snapshot.had_exported;
    snapshot.had_readonly = get_flag_value(ksh_env.readonly_flags, name);
    snapshot.readonly_value = snapshot.had_readonly;
    snapshot.had_integer = get_flag_value(ksh_env.integer_flags, name);
    snapshot.integer_value = snapshot.had_integer;
    snapshot.had_uppercase = get_flag_value(ksh_env.uppercase_flags, name);
    snapshot.uppercase_value = snapshot.had_uppercase;
    snapshot.had_lowercase = get_flag_value(ksh_env.lowercase_flags, name);
    snapshot.lowercase_value = snapshot.had_lowercase;
    snapshot.had_nameref = get_flag_value(ksh_env.nameref_flags, name);
    snapshot.nameref_value = snapshot.had_nameref;

    context.locals.insert(std::make_pair(name, std::move(snapshot)));
}

void restore_function_scope_variables(FunctionScopeContext& context) {
    for (std::unordered_map<std::wstring, VariableScopeSnapshot>::const_iterator it = context.locals.begin(); it != context.locals.end(); ++it) {
        const std::wstring& name = it->first;
        const VariableScopeSnapshot& snapshot = it->second;

        if (snapshot.had_scalar) {
            ksh_env.variables[name] = snapshot.scalar_value;
        } else {
            ksh_env.variables.erase(name);
        }

        if (snapshot.had_array) {
            ksh_env.arrays[name] = *snapshot.array_value;
        } else {
            ksh_env.arrays.erase(name);
        }

        if (snapshot.had_associative) {
            ksh_env.associative_flags.insert_or_assign(name, snapshot.associative_value);
        } else {
            ksh_env.associative_flags.erase(name);
        }

        if (snapshot.had_justify_mode) {
            ksh_env.justify_modes.insert_or_assign(name, snapshot.justify_mode_value);
        } else {
            ksh_env.justify_modes.erase(name);
        }

        if (snapshot.had_justify_width) {
            ksh_env.justify_widths.insert_or_assign(name, snapshot.justify_width_value);
        } else {
            ksh_env.justify_widths.erase(name);
        }

        if (snapshot.had_exported) {
            ksh_env.exported.insert_or_assign(name, snapshot.exported_value);
        } else {
            ksh_env.exported.erase(name);
        }

        if (snapshot.had_readonly) {
            ksh_env.readonly_flags.insert_or_assign(name, snapshot.readonly_value);
        } else {
            ksh_env.readonly_flags.erase(name);
        }

        if (snapshot.had_integer) {
            ksh_env.integer_flags.insert_or_assign(name, snapshot.integer_value);
        } else {
            ksh_env.integer_flags.erase(name);
        }

        if (snapshot.had_uppercase) {
            ksh_env.uppercase_flags.insert_or_assign(name, snapshot.uppercase_value);
        } else {
            ksh_env.uppercase_flags.erase(name);
        }

        if (snapshot.had_lowercase) {
            ksh_env.lowercase_flags.insert_or_assign(name, snapshot.lowercase_value);
        } else {
            ksh_env.lowercase_flags.erase(name);
        }

        if (snapshot.had_nameref) {
            ksh_env.nameref_flags.insert_or_assign(name, snapshot.nameref_value);
        } else {
            ksh_env.nameref_flags.erase(name);
        }

        if (snapshot.had_environment) {
            SetEnvironmentVariableW(name.c_str(), snapshot.environment_value.c_str());
        } else {
            SetEnvironmentVariableW(name.c_str(), nullptr);
        }
    }
}

void sync_exported_environment_variable(const std::wstring& name) {
    std::map<std::wstring, std::wstring>::const_iterator value_it = ksh_env.variables.find(name);
    if (value_it != ksh_env.variables.end() && get_flag_value(ksh_env.exported, name)) {
           SetEnvironmentVariableW(name.c_str(), value_it->second.c_str());
    } else {
        SetEnvironmentVariableW(name.c_str(), nullptr);
    }
}

bool read_registry_property(const std::wstring& property_path, std::wstring& value, RegistryValueMetadata& metadata) {
    const size_t separator = property_path.find(L'.');
    if (separator == std::wstring::npos || separator == 0 || separator + 1 >= property_path.size()) {
        return false;
    }

    std::wstring root_name = property_path.substr(0, separator);
    std::wstring remainder = property_path.substr(separator + 1);
    HKEY root = nullptr;
    if (root_name == L"HKLM" || root_name == L"HKEY_LOCAL_MACHINE") {
        root = HKEY_LOCAL_MACHINE;
    } else if (root_name == L"HKCU" || root_name == L"HKEY_CURRENT_USER") {
        root = HKEY_CURRENT_USER;
    } else {
        return false;
    }

    const size_t value_separator = remainder.rfind(L'.');
    if (value_separator == std::wstring::npos || value_separator == 0 || value_separator + 1 >= remainder.size()) {
        return false;
    }

    std::wstring key_path = remainder.substr(0, value_separator);
    std::wstring value_name = remainder.substr(value_separator + 1);
    if (key_path.find(L'/') != std::wstring::npos) {
        std::replace(key_path.begin(), key_path.end(), L'/', L'\\');
    } else {
        std::replace(key_path.begin(), key_path.end(), L'.', L'\\');
    }
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, key_path.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }

    DWORD type = REG_NONE;
    DWORD size = 0;
    LONG status = RegQueryValueExW(key, value_name.c_str(), nullptr, &type, nullptr, &size);
    if (status != ERROR_SUCCESS) {
        RegCloseKey(key);
        return false;
    }

    std::vector<BYTE> data(size);
    if (size > 0) {
        status = RegQueryValueExW(key, value_name.c_str(), nullptr, &type, data.data(), &size);
    }
    RegCloseKey(key);
    if (status != ERROR_SUCCESS) {
        return false;
    }
    data.resize(size);

    metadata.type = type;
    metadata.data = data;
    value.clear();
    if (type == REG_DWORD && data.size() >= sizeof(DWORD)) {
        value = std::to_wstring(*reinterpret_cast<const DWORD*>(data.data()));
    } else if (type == REG_QWORD && data.size() >= sizeof(ULONGLONG)) {
        value = std::to_wstring(*reinterpret_cast<const ULONGLONG*>(data.data()));
    } else if (type == REG_BINARY) {
        static const wchar_t hex[] = L"0123456789ABCDEF";
        for (BYTE byte : data) {
            value.push_back(hex[(byte >> 4) & 0x0F]);
            value.push_back(hex[byte & 0x0F]);
        }
    } else if (type == REG_MULTI_SZ) {
        const size_t wchar_count = data.size() / sizeof(wchar_t);
        const wchar_t* text = reinterpret_cast<const wchar_t*>(data.data());
        size_t offset = 0;
        while (offset < wchar_count && text[offset] != L'\0') {
            const size_t value_start = offset;
            while (offset < wchar_count && text[offset] != L'\0') {
                ++offset;
            }
            if (offset >= wchar_count) {
                return false;
            }
            if (!value.empty()) value.push_back(L'\n');
            value.append(text + value_start, offset - value_start);
            ++offset;
        }
    } else if (type == REG_SZ || type == REG_EXPAND_SZ) {
        const wchar_t* text = reinterpret_cast<const wchar_t*>(data.data());
        size_t length = data.size() / sizeof(wchar_t);
        while (length > 0 && text[length - 1] == L'\0') --length;
        value.assign(text, length);
    } else if (type != REG_SZ && type != REG_EXPAND_SZ) {
        return false;
    }
    return true;
}

bool write_registry_property(const std::wstring& property_path, const std::wstring& value) {
    const size_t separator = property_path.find(L'.');
    if (separator == std::wstring::npos || separator == 0 || separator + 1 >= property_path.size()) {
        return false;
    }

    const std::wstring root_name = property_path.substr(0, separator);
    const std::wstring remainder = property_path.substr(separator + 1);
    HKEY root = nullptr;
    if (root_name == L"HKLM" || root_name == L"HKEY_LOCAL_MACHINE") {
        root = HKEY_LOCAL_MACHINE;
    } else if (root_name == L"HKCU" || root_name == L"HKEY_CURRENT_USER") {
        root = HKEY_CURRENT_USER;
    } else {
        return false;
    }

    const size_t value_separator = remainder.rfind(L'.');
    if (value_separator == std::wstring::npos || value_separator == 0 || value_separator + 1 >= remainder.size()) {
        return false;
    }

    std::wstring key_path = remainder.substr(0, value_separator);
    const std::wstring value_name = remainder.substr(value_separator + 1);
    if (key_path.find(L'/') != std::wstring::npos) {
        std::replace(key_path.begin(), key_path.end(), L'/', L'\\');
    } else {
        std::replace(key_path.begin(), key_path.end(), L'.', L'\\');
    }

    HKEY key = nullptr;
    if (RegOpenKeyExW(root, key_path.c_str(), 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &key) != ERROR_SUCCESS) {
        return false;
    }

    DWORD type = REG_SZ;
    DWORD existing_size = 0;
    if (RegQueryValueExW(key, value_name.c_str(), nullptr, &type, nullptr, &existing_size) != ERROR_SUCCESS) {
        type = REG_SZ;
    }

    std::vector<BYTE> data;
    if (type == REG_DWORD) {
        wchar_t* end = nullptr;
        unsigned long parsed = std::wcstoul(value.c_str(), &end, 0);
        if (end == value.c_str() || (end != nullptr && *end != L'\0') || parsed > MAXDWORD) {
            RegCloseKey(key);
            return false;
        }
        data.resize(sizeof(DWORD));
        const DWORD number = static_cast<DWORD>(parsed);
        std::memcpy(data.data(), &number, sizeof(number));
    } else if (type == REG_QWORD) {
        wchar_t* end = nullptr;
        unsigned long long parsed = std::wcstoull(value.c_str(), &end, 0);
        if (end == value.c_str() || (end != nullptr && *end != L'\0')) {
            RegCloseKey(key);
            return false;
        }
        data.resize(sizeof(ULONGLONG));
        const ULONGLONG number = static_cast<ULONGLONG>(parsed);
        std::memcpy(data.data(), &number, sizeof(number));
    } else if (type == REG_SZ || type == REG_EXPAND_SZ) {
        data.resize((value.size() + 1) * sizeof(wchar_t));
        std::memcpy(data.data(), value.c_str(), data.size());
        type = (type == REG_EXPAND_SZ) ? REG_EXPAND_SZ : REG_SZ;
    } else {
        RegCloseKey(key);
        return false;
    }

    const LONG status = RegSetValueExW(key, value_name.c_str(), 0, type, data.data(), static_cast<DWORD>(data.size()));
    RegCloseKey(key);
    return status == ERROR_SUCCESS;
}

static void register_registry_namespaces() {
    for (const wchar_t* name : {L"HKLM", L"HKCU"}) {
        CustomTypeDefinition& definition = g_custom_types[name];
        definition.name = name;
        definition.is_dynamic_namespace = true;
        definition.property_getter = [](const std::wstring& property_path, std::wstring& value, RegistryValueMetadata& metadata) {
            return read_registry_property(property_path, value, metadata);
        };
        definition.property_setter = [](const std::wstring& property_path, const std::wstring& value) {
            return write_registry_property(property_path, value);
        };
    }
}

void ensure_registry_namespaces_registered() {
    static std::once_flag registry_namespace_once;
    std::call_once(registry_namespace_once, register_registry_namespaces);
}

bool parse_non_negative_index(const std::wstring& text, size_t& out_index) {
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
}

bool parse_array_reference_expression(const std::wstring& expression, std::wstring& base_name, std::wstring& index_text, bool& has_index) {
    base_name.clear();
    index_text.clear();
    has_index = false;

    if (expression.empty()) {
        return false;
    }

    size_t open = expression.find(L'[');
    if (open == std::wstring::npos) {
        if (!is_valid_shell_identifier(expression)) {
            return false;
        }
        base_name = expression;
        return true;
    }

    int bracket_depth = 0;
    size_t match_pos = std::wstring::npos;
    for (size_t i = open; i < expression.size(); ++i) {
        if (expression[i] == L'[') {
            bracket_depth++;
        } else if (expression[i] == L']') {
            bracket_depth--;
            if (bracket_depth == 0) {
                match_pos = i;
                break;
            }
        }
    }
    if (match_pos == std::wstring::npos || match_pos != expression.size() - 1) {
        return false;
    }

    base_name = expression.substr(0, open);
    if (!is_valid_shell_identifier(base_name)) {
        return false;
    }

    index_text = expression.substr(open + 1, expression.size() - open - 2);
    if (index_text.empty()) {
        return false;
    }

    has_index = true;
    return true;
}

static bool parse_array_literal(const std::wstring& value, std::vector<std::wstring>& elements) {
    elements.clear();
    std::wstring trimmed = trim_copy(value);
    if (trimmed.size() < 2 || trimmed.front() != L'(' || trimmed.back() != L')') {
        return false;
    }

    std::wstring inner = trim_copy(trimmed.substr(1, trimmed.size() - 2));
    if (inner.empty()) {
        return true;
    }

    std::vector<std::wstring> tokens = ksh_tokenize_preserve_quotes(inner);
    size_t next_positional_index = 0;

    for (const std::wstring& token : tokens) {
        if (token.empty()) {
            continue;
        }

        size_t close_bracket = token.find(L']');
        if (token[0] == L'[' && close_bracket != std::wstring::npos && (close_bracket + 1) < token.size() && token[close_bracket + 1] == L'=') {
            std::wstring index_text = token.substr(1, close_bracket - 1);
            size_t explicit_index = 0;
            if (!parse_non_negative_index(index_text, explicit_index)) {
                return false;
            }
            if (explicit_index >= kMaxArrayIndex) {
                return false;
            }

            std::wstring explicit_value = token.substr(close_bracket + 2);
            if (elements.size() <= explicit_index) {
                elements.resize(explicit_index + 1);
            }
            elements[explicit_index] = explicit_value;
            if (next_positional_index <= explicit_index) {
                next_positional_index = explicit_index + 1;
            }
            continue;
        }

        if (elements.size() <= next_positional_index) {
            elements.resize(next_positional_index + 1);
        }
        elements[next_positional_index] = token;
        next_positional_index++;
    }

    return true;
}

static bool parse_associative_array_literal(const std::wstring& value, std::map<std::wstring, std::wstring>& elements) {
    elements.clear();
    std::wstring trimmed = trim_copy(value);
    if (trimmed.size() < 2 || trimmed.front() != L'(' || trimmed.back() != L')') {
        return false;
    }

    std::wstring inner = trim_copy(trimmed.substr(1, trimmed.size() - 2));
    if (inner.empty()) {
        return true;
    }

    std::vector<std::wstring> tokens = ksh_tokenize_preserve_quotes(inner);

    for (const std::wstring& token : tokens) {
        if (token.empty()) {
            continue;
        }

        size_t close_bracket = token.find(L']');
        if (token[0] == L'[' && close_bracket != std::wstring::npos && (close_bracket + 1) < token.size() && token[close_bracket + 1] == L'=') {
            std::wstring key = token.substr(1, close_bracket - 1);
            std::wstring explicit_value = token.substr(close_bracket + 2);
            elements[key] = explicit_value;
        } else {
            return false;
        }
    }
    return true;
}

std::vector<std::wstring> get_array_values_vector(const std::wstring& name) {
    std::vector<std::wstring> result;
    std::map<std::wstring, std::map<std::wstring, std::wstring>>::const_iterator it = ksh_env.arrays.find(name);
    if (it == ksh_env.arrays.end()) {
        return result;
    }

    const std::map<std::wstring, std::wstring>& m = it->second;
    bool is_assoc = false;
    std::map<std::wstring, bool>::const_iterator assoc_it = ksh_env.associative_flags.find(name);
    if (assoc_it != ksh_env.associative_flags.end()) {
        is_assoc = assoc_it->second;
    }

    if (is_assoc) {
        for (const auto& pair : m) {
            result.push_back(pair.second);
        }
    } else {
        size_t max_idx = 0;
        bool has_any = false;
        std::vector<std::pair<size_t, std::wstring>> parsed;
        for (const auto& pair : m) {
            size_t idx = 0;
            if (parse_non_negative_index(pair.first, idx)) {
                parsed.push_back({idx, pair.second});
                if (idx > max_idx) {
                    max_idx = idx;
                }
                has_any = true;
            }
        }
        if (has_any) {
            result.resize(max_idx + 1);
            for (const auto& p : parsed) {
                result[p.first] = p.second;
            }
        }
    }
    return result;
}

void assign_scalar_parameter(const std::wstring& name, const std::wstring& value) {
    ensure_registry_namespaces_registered();
    const size_t namespace_separator = name.find(L'.');
    if (namespace_separator != std::wstring::npos) {
        const std::wstring namespace_name = name.substr(0, namespace_separator);
        std::map<std::wstring, CustomTypeDefinition>::const_iterator namespace_it = g_custom_types.find(namespace_name);
        if (namespace_it != g_custom_types.end() && namespace_it->second.is_dynamic_namespace && namespace_it->second.property_setter) {
            if (!namespace_it->second.property_setter(name, value)) {
                ksh_env.variables[L"?"] = L"1";
            } else {
                ksh_env.variables[L"?"] = L"0";
            }
            return;
        }
    }

    bool exists = ksh_env.variables.find(name) != ksh_env.variables.end();
    if (!exists && g_shell_functions.find(name + L".init") != g_shell_functions.end() &&
        g_active_init_hooks.find(name) == g_active_init_hooks.end()) {
        
        g_active_init_hooks.insert(name);
        bool dummy = false;
        execute_defined_function(name + L".init", {}, dummy);
        g_active_init_hooks.erase(name);
    }

    std::wstring assigned_value = value;
    if (g_shell_functions.find(name + L".set") != g_shell_functions.end() &&
        g_active_set_hooks.find(name) == g_active_set_hooks.end()) {
        
        g_active_set_hooks.insert(name);
        
        std::wstring old_sh_val;
        bool had_sh_val = false;
        auto sh_it = ksh_env.variables.find(L".sh.value");
        if (sh_it != ksh_env.variables.end()) {
            old_sh_val = sh_it->second;
            had_sh_val = true;
        }
        
        ksh_env.variables[L".sh.value"] = value;
        
        bool dummy = false;
        execute_defined_function(name + L".set", {}, dummy);
        
        assigned_value = ksh_env.variables[L".sh.value"];
        
        if (had_sh_val) {
            ksh_env.variables[L".sh.value"] = old_sh_val;
        } else {
            ksh_env.variables.erase(L".sh.value");
        }
        
        g_active_set_hooks.erase(name);
    }

    const bool uppercase_mode = get_flag_value(ksh_env.uppercase_flags, name);
    const bool lowercase_mode = get_flag_value(ksh_env.lowercase_flags, name);
    if (uppercase_mode) {
        assigned_value = to_upper_copy(assigned_value);
    } else if (lowercase_mode) {
        assigned_value = to_lower_copy(assigned_value);
    }

    std::map<std::wstring, wchar_t>::const_iterator justify_mode_it = ksh_env.justify_modes.find(name);
    if (justify_mode_it != ksh_env.justify_modes.end()) {
        std::map<std::wstring, int>::const_iterator justify_width_it = ksh_env.justify_widths.find(name);
        int justify_width = (justify_width_it != ksh_env.justify_widths.end()) ? justify_width_it->second : 0;
        if (justify_width > 0) {
            const size_t width = static_cast<size_t>(justify_width);
            if (justify_mode_it->second == L'L') {
                if (assigned_value.size() > width) {
                    assigned_value = assigned_value.substr(0, width);
                } else if (assigned_value.size() < width) {
                    assigned_value.append(width - assigned_value.size(), L' ');
                }
            } else if (justify_mode_it->second == L'R') {
                if (assigned_value.size() > width) {
                    assigned_value = assigned_value.substr(assigned_value.size() - width);
                } else if (assigned_value.size() < width) {
                    assigned_value.insert(0, width - assigned_value.size(), L' ');
                }
            } else if (justify_mode_it->second == L'Z') {
                if (assigned_value.size() > width) {
                    assigned_value = assigned_value.substr(assigned_value.size() - width);
                } else if (assigned_value.size() < width) {
                    size_t pad_count = width - assigned_value.size();
                    if (!assigned_value.empty() && (assigned_value[0] == L'+' || assigned_value[0] == L'-')) {
                        assigned_value.insert(1, pad_count, L'0');
                    } else {
                        assigned_value.insert(0, pad_count, L'0');
                    }
                }
            }
        }
    }

    ksh_env.variables[name] = assigned_value;
    ksh_env.registry_metadata.erase(name);
    ksh_env.arrays.erase(name);
    ksh_env.associative_flags.erase(name);
    SetEnvironmentVariableW(name.c_str(), assigned_value.c_str());
}

void assign_array_parameter(const std::wstring& name, const std::vector<std::wstring>& values) {
    std::map<std::wstring, std::wstring> map_values;
    for (size_t i = 0; i < values.size(); ++i) {
        if (!values[i].empty()) {
            map_values[std::to_wstring(i)] = values[i];
        }
    }
    ksh_env.arrays[name] = std::move(map_values);
    ksh_env.variables.erase(name);
    ksh_env.registry_metadata.erase(name);
    ksh_env.justify_modes.erase(name);
    ksh_env.justify_widths.erase(name);
    SetEnvironmentVariableW(name.c_str(), nullptr);
}

std::wstring resolve_variable_name(const std::wstring& name, int depth) {
    if (name.empty() || depth > 10) {
        return name;
    }

    std::wstring base_name;
    std::wstring index_text;
    bool has_index = false;
    if (parse_array_reference_expression(name, base_name, index_text, has_index)) {
        if (has_index) {
            std::wstring resolved_base = resolve_variable_name(base_name, depth + 1);
            return resolved_base + L"[" + index_text + L"]";
        }
    }

    std::map<std::wstring, bool>::const_iterator nameref_it = ksh_env.nameref_flags.find(name);
    if (nameref_it != ksh_env.nameref_flags.end() && nameref_it->second) {
        std::map<std::wstring, std::wstring>::const_iterator val_it = ksh_env.variables.find(name);
        if (val_it != ksh_env.variables.end()) {
            return resolve_variable_name(val_it->second, depth + 1);
        }
    }

    return name;
}

bool assign_parameter_value(const std::wstring& lhs, const std::wstring& rhs, bool array_hint, bool local_scope, std::wstring& error_message, bool is_nameref) {
    error_message.clear();

    if (lhs.empty()) {
        error_message = L"missing variable name for assignment";
        return false;
    }

    if (is_nameref) {
        std::wstring base_name;
        std::wstring index_text;
        bool has_index = false;
        if (!parse_array_reference_expression(lhs, base_name, index_text, has_index)) {
            error_message = L"invalid variable name for nameref: " + lhs;
            return false;
        }

        if (has_index) {
            error_message = L"cannot define array element as a nameref: " + lhs;
            return false;
        }

        if (local_scope) {
            snapshot_local_variable_if_needed(base_name);
        }

        if (get_flag_value(ksh_env.readonly_flags, base_name)) {
            error_message = L"variable is read-only: " + base_name;
            return false;
        }

        ksh_env.variables[base_name] = rhs;
        ksh_env.arrays.erase(base_name);
        ksh_env.associative_flags.erase(base_name);
        set_flag_value(ksh_env.nameref_flags, base_name, true);
        return true;
    }

    std::wstring resolved_lhs = resolve_variable_name(lhs);

    std::wstring base_name;
    std::wstring index_text;
    bool has_index = false;
    if (!parse_array_reference_expression(resolved_lhs, base_name, index_text, has_index)) {
        error_message = L"invalid assignment target: " + lhs;
        return false;
    }

    if (!has_index) {
        ensure_registry_namespaces_registered();
        const size_t namespace_separator = base_name.find(L'.');
        if (namespace_separator != std::wstring::npos) {
            const std::wstring namespace_name = base_name.substr(0, namespace_separator);
            std::map<std::wstring, CustomTypeDefinition>::const_iterator namespace_it = g_custom_types.find(namespace_name);
            if (namespace_it != g_custom_types.end() && namespace_it->second.is_dynamic_namespace && namespace_it->second.property_setter) {
                if (!namespace_it->second.property_setter(base_name, rhs)) {
                    error_message = L"failed to set Registry property: " + base_name;
                    return false;
                }
                ksh_env.variables[L"?"] = L"0";
                return true;
            }
        }
    }

    if (local_scope) {
        snapshot_local_variable_if_needed(base_name);
    }

    if (get_flag_value(ksh_env.readonly_flags, base_name)) {
        error_message = L"variable is read-only: " + base_name;
        return false;
    }

    const bool integer_mode = get_flag_value(ksh_env.integer_flags, base_name);

    if (integer_mode && (has_index || array_hint)) {
        error_message = L"integer variable cannot be assigned as an array: " + base_name;
        return false;
    }

    const bool is_assoc = get_flag_value(ksh_env.associative_flags, base_name);

    if (is_assoc) {
        if (integer_mode) {
            error_message = L"integer variable cannot be assigned as an array: " + base_name;
            return false;
        }

        if (has_index) {
            if (index_text.empty()) {
                error_message = L"associative array key cannot be empty";
                return false;
            }
            ksh_env.arrays[base_name][index_text] = rhs;
            ksh_env.variables.erase(base_name);
            return true;
        }

        std::wstring trimmed_rhs = trim_copy(rhs);
        if (!trimmed_rhs.empty() && trimmed_rhs.front() == L'(' && trimmed_rhs.back() == L')') {
            std::map<std::wstring, std::wstring> assoc_elements;
            if (parse_associative_array_literal(rhs, assoc_elements)) {
                ksh_env.arrays[base_name] = std::move(assoc_elements);
                ksh_env.variables.erase(base_name);
                return true;
            } else {
                error_message = L"invalid associative array literal assignment";
                return false;
            }
        }

        ksh_env.arrays[base_name][L"0"] = rhs;
        ksh_env.variables.erase(base_name);
        return true;
    }

    std::vector<std::wstring> literal_values;
    const bool rhs_is_array_literal = parse_array_literal(rhs, literal_values);

    if (integer_mode && rhs_is_array_literal) {
        error_message = L"integer variable cannot be assigned an array literal: " + base_name;
        return false;
    }

    if (has_index) {
        size_t index = 0;
        if (!parse_non_negative_index(index_text, index)) {
            error_message = L"array index must be a non-negative integer";
            return false;
        }
        if (index >= kMaxArrayIndex) {
            error_message = L"array index exceeds maximum allowed value";
            return false;
        }

        std::map<std::wstring, std::wstring>& target_array = ksh_env.arrays[base_name];
        target_array[std::to_wstring(index)] = rhs;
        ksh_env.variables.erase(base_name);
        return true;
    }

    if (array_hint || rhs_is_array_literal) {
        if (!rhs_is_array_literal && array_hint) {
            literal_values.push_back(rhs);
        }
        assign_array_parameter(base_name, literal_values);
        sync_exported_environment_variable(base_name);
        return true;
    }

    std::wstring scalar_value = rhs;
    if (integer_mode) {
        scalar_value = evaluate_arithmetic(rhs);
    }

    // Enforce per-session variable table cap (new entries only).
    const bool scalar_exists = ksh_env.variables.find(base_name) != ksh_env.variables.end();
    const bool array_exists  = ksh_env.arrays.find(base_name)    != ksh_env.arrays.end();
    if (!scalar_exists && !array_exists) {
        const size_t total = ksh_env.variables.size() + ksh_env.arrays.size();
        if (total >= kMaxShellVariables) {
            error_message = L"shell variable table limit reached, cannot create: " + base_name;
            return false;
        }
    }

    assign_scalar_parameter(base_name, scalar_value);
    sync_exported_environment_variable(base_name);
    return true;
}

bool parse_custom_type_definition(const std::wstring& type_name, const std::wstring& body, CustomTypeDefinition& def) {
    def.name = type_name;
    std::vector<std::wstring> tokens = ksh_tokenize_preserve_quotes(body);
    
    size_t i = 0;
    while (i < tokens.size()) {
        if (tokens[i] == L";" || tokens[i] == L"\n") {
            i++;
            continue;
        }
        
        std::wstring type_kw = tokens[i];
        bool is_integer = false;
        bool is_array = false;
        bool is_associative = false;
        std::wstring member_type_name = L"";
        
        if (type_kw == L"integer") {
            is_integer = true;
            i++;
        } else if (type_kw == L"typeset") {
            i++;
            while (i < tokens.size() && tokens[i][0] == L'-') {
                std::wstring opt = tokens[i];
                for (size_t j = 1; j < opt.size(); ++j) {
                    if (opt[j] == L'i') is_integer = true;
                    else if (opt[j] == L'a') is_array = true;
                    else if (opt[j] == L'A') is_associative = true;
                }
                i++;
            }
        } else if (g_custom_types.find(type_kw) != g_custom_types.end()) {
            member_type_name = type_kw;
            i++;
        } else {
            // Default variable type
        }
        
        while (i < tokens.size() && tokens[i] != L";" && tokens[i] != L"\n" &&
               tokens[i] != L"integer" && tokens[i] != L"typeset" &&
               g_custom_types.find(tokens[i]) == g_custom_types.end()) {
            
            std::wstring token = tokens[i];
            i++;
            
            std::wstring name = token;
            std::wstring def_val = L"";
            size_t eq = token.find(L'=');
            if (eq != std::wstring::npos) {
                name = token.substr(0, eq);
                def_val = remove_quotes_and_escapes_from_token(token.substr(eq + 1));
            }
            
            if (name.empty()) continue;
            
            CustomTypeMember m;
            m.name = name;
            m.is_integer = is_integer;
            m.is_array = is_array;
            m.is_associative = is_associative;
            m.type_name = member_type_name;
            m.default_value = def_val;
            
            def.members.push_back(m);
        }
    }
    return true;
}

void instantiate_custom_type(const std::wstring& type_name, const std::wstring& var_prefix) {
    auto it = g_custom_types.find(type_name);
    if (it == g_custom_types.end()) return;
    
    for (const auto& member : it->second.members) {
        std::wstring full_member_name = var_prefix + L"." + member.name;
        if (!member.type_name.empty()) {
            instantiate_custom_type(member.type_name, full_member_name);
        } else {
            std::wstring val = member.default_value;
            if (val.empty() && member.is_integer) {
                val = L"0";
            }
            
            if (member.is_integer) {
                set_flag_value(ksh_env.integer_flags, full_member_name, true);
            }
            if (member.is_array) {
                ksh_env.arrays[full_member_name].clear();
            } else if (member.is_associative) {
                ksh_env.arrays[full_member_name].clear();
                ksh_env.associative_flags[full_member_name] = true;
            } else {
                ksh_env.variables[full_member_name] = val;
                SetEnvironmentVariableW(full_member_name.c_str(), val.c_str());
            }
        }
    }
}

bool is_internal_shell_state_variable(const std::wstring& name) {
    return name == kGetoptsCursorVar || name == kGetoptsOptindMirrorVar;
}

std::wstring get_variable_value_with_hooks(const std::wstring& name, bool& is_set) {
    is_set = false;
    ensure_registry_namespaces_registered();
    std::wstring resolved_name = resolve_variable_name(name);

    const size_t namespace_separator = resolved_name.find(L'.');
    if (namespace_separator != std::wstring::npos) {
        const std::wstring namespace_name = resolved_name.substr(0, namespace_separator);
        std::map<std::wstring, CustomTypeDefinition>::const_iterator namespace_it = g_custom_types.find(namespace_name);
        if (namespace_it != g_custom_types.end() && namespace_it->second.is_dynamic_namespace && namespace_it->second.property_getter) {
            std::wstring registry_value;
            RegistryValueMetadata metadata;
            if (namespace_it->second.property_getter(resolved_name, registry_value, metadata)) {
                ksh_env.registry_metadata[resolved_name] = std::move(metadata);
                is_set = true;
                return registry_value;
            }
        }
    }
    
    if (g_shell_functions.find(resolved_name + L".get") != g_shell_functions.end() &&
        g_active_get_hooks.find(resolved_name) == g_active_get_hooks.end()) {
        
        g_active_get_hooks.insert(resolved_name);
        
        std::wstring old_sh_val;
        bool had_sh_val = false;
        auto sh_it = ksh_env.variables.find(L".sh.value");
        if (sh_it != ksh_env.variables.end()) {
            old_sh_val = sh_it->second;
            had_sh_val = true;
        }
        
        auto val_it = ksh_env.variables.find(resolved_name);
        std::wstring current_val = (val_it != ksh_env.variables.end()) ? val_it->second : L"";
        ksh_env.variables[L".sh.value"] = current_val;
        
        bool dummy = false;
        execute_defined_function(resolved_name + L".get", {}, dummy);
        
        std::wstring returned_val = ksh_env.variables[L".sh.value"];
        
        if (had_sh_val) {
            ksh_env.variables[L".sh.value"] = old_sh_val;
        } else {
            ksh_env.variables.erase(L".sh.value");
        }
        
        g_active_get_hooks.erase(resolved_name);
        is_set = true;
        return returned_val;
    }
    
    auto it = ksh_env.variables.find(resolved_name);
    if (it != ksh_env.variables.end()) {
        is_set = true;
        return it->second;
    }
    
    return L"";
}

std::wstring get_environment_value(const std::wstring& name) {
    bool is_set = false;
    std::wstring val = get_variable_value_with_hooks(name, is_set);
    if (is_set) {
        return val;
    }

    return get_system_env_var(name);
}

