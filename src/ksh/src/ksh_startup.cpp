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

std::wstring get_history_control_value(const std::wstring& name) {
    std::map<std::wstring, std::wstring>::const_iterator it = ksh_env.variables.find(name);
    if (it != ksh_env.variables.end()) {
        return it->second;
    }

    return get_system_env_var(name);
}

bool history_timestamps_enabled() {
    return !trim_copy(get_history_control_value(L"HISTTIMEFORMAT")).empty();
}

bool history_dedupe_enabled() {
    std::wstring value = trim_copy(get_history_control_value(L"HISTCONTROL"));
    if (value.empty()) {
        std::wstring legacy = trim_copy(get_history_control_value(L"HISTDEDUPE"));
        if (legacy.empty()) {
            legacy = trim_copy(get_history_control_value(L"HISTORY_DEDUPE"));
        }
        if (legacy.empty()) {
            return false;
        }

        for (wchar_t ch : legacy) {
            if (ch == L'1' || ch == L'y' || ch == L'Y' || ch == L't' || ch == L'T') {
                return true;
            }
        }
        return false;
    }

    return value.find(L"ignoredups") != std::wstring::npos ||
        value.find(L"erasedups") != std::wstring::npos ||
        value.find(L"dedupe") != std::wstring::npos;
}

size_t history_size_limit() {
    std::wstring value = trim_copy(get_history_control_value(L"HISTSIZE"));
    if (value.empty()) {
        return kDefaultHistoryLimit;
    }

    try {
        size_t index = 0;
        unsigned long long parsed = std::stoull(value, &index);
        if (index != value.size()) {
            return kDefaultHistoryLimit;
        }
        if (parsed == 0) {
            return 0;
        }
        return static_cast<size_t>(parsed);
    } catch (...) {
        return kDefaultHistoryLimit;
    }
}

std::wstring format_history_timestamp() {
    SYSTEMTIME local_time;
    GetLocalTime(&local_time);

    wchar_t buffer[32];
    _snwprintf_s(
        buffer,
        _countof(buffer),
        _TRUNCATE,
        L"%04u-%02u-%02u %02u:%02u:%02u",
        static_cast<unsigned>(local_time.wYear),
        static_cast<unsigned>(local_time.wMonth),
        static_cast<unsigned>(local_time.wDay),
        static_cast<unsigned>(local_time.wHour),
        static_cast<unsigned>(local_time.wMinute),
        static_cast<unsigned>(local_time.wSecond));
    return buffer;
}

std::wstring serialize_history_entry(const HistoryEntry& entry) {
    std::wstring timestamp = entry.timestamp;
    if (timestamp.empty()) {
        timestamp = format_history_timestamp();
    }

    if (timestamp.empty()) {
        return entry.command;
    }

    return timestamp + L"\t" + entry.command;
}

void rewrite_history_file();


std::wstring resolve_history_file_path() {
    auto read_env_var = [](const wchar_t* name) -> std::wstring {
        if (name == nullptr || *name == L'\0') {
            return L"";
        }
        return get_system_env_var(name);
    };

    // Prefer HOME first for POSIX-style workflows, then USERPROFILE and HOMEDRIVE+HOMEPATH.
    std::wstring home_path = trim_copy(read_env_var(L"HOME"));
    if (home_path.empty()) {
        home_path = trim_copy(read_env_var(L"USERPROFILE"));
    }
    if (home_path.empty()) {
        std::wstring home_drive = trim_copy(read_env_var(L"HOMEDRIVE"));
        std::wstring home_path_part = trim_copy(read_env_var(L"HOMEPATH"));
        if (!home_drive.empty() && !home_path_part.empty()) {
            home_path = home_drive + home_path_part;
        }
    }

    if (!home_path.empty()) {
        while (!home_path.empty() && (home_path.back() == L'\\' || home_path.back() == L'/')) {
            home_path.pop_back();
        }
        if (!home_path.empty()) {
            return home_path + L"\\.ksh_history";
        }
    }

    return L"";
}
void enforce_history_size_limit(bool rewrite_file) {
    size_t limit = history_size_limit();
    if (limit == 0 || g_command_history.size() <= limit) {
        return;
    }

    size_t remove_count = g_command_history.size() - limit;
    std::vector<HistoryEntry>::difference_type offset = static_cast<std::vector<HistoryEntry>::difference_type>(remove_count);
    g_command_history.erase(g_command_history.begin(), g_command_history.begin() + offset);
    if (rewrite_file && g_is_interactive_session) {
        rewrite_history_file();
    }
}

bool append_history_entry(const std::wstring& command_line) {
    if (command_line.empty()) {
        return false;
    }

    if (history_dedupe_enabled() && !g_command_history.empty() && g_command_history.back().command == command_line) {
        return false;
    }

    HistoryEntry entry;
    entry.command = command_line;
    entry.timestamp = format_history_timestamp();
    g_command_history.push_back(entry);

    bool rewrote_history = false;
    size_t limit = history_size_limit();
    if (limit > 0 && g_command_history.size() > limit) {
        enforce_history_size_limit(false);
        rewrote_history = true;
    }

    if (g_is_interactive_session) {
        if (rewrote_history) {
            rewrite_history_file();
        } else {
            if (g_history_file_path.empty()) {
                g_history_file_path = resolve_history_file_path();
            }

            std::wofstream history_file(g_history_file_path.c_str(), std::ios::app);
            if (history_file.is_open()) {
                history_file << serialize_history_entry(entry) << L"\n";
            }
        }
    }

    return true;
}

bool resolve_history_recall(const std::wstring& input, std::wstring& resolved) {
    resolved = input;
    if (input.empty() || input[0] != L'!') {
        return true;
    }

    if (input == L"!!") {
        if (g_command_history.empty()) {
            std::wcerr << L"ksh: history empty\n";
            return false;
        }
        resolved = g_command_history.back().command;
        return true;
    }

    if (input.size() > 1 && input[1] == L'-') {
        unsigned long rel = 0;
        if (!try_parse_unsigned_long_strict(input.substr(2), rel)) {
            std::wcerr << L"ksh: invalid history reference\n";
            return false;
        }
        if (rel == 0 || rel > g_command_history.size()) {
            std::wcerr << L"ksh: history event not found\n";
            return false;
        }
        resolved = g_command_history[g_command_history.size() - static_cast<size_t>(rel)].command;
        return true;
    }

    bool numeric = input.size() > 1;
    for (size_t i = 1; i < input.size(); ++i) {
        if (!std::iswdigit(input[i])) {
            numeric = false;
            break;
        }
    }

    if (numeric) {
        unsigned long index = 0;
        if (!try_parse_unsigned_long_strict(input.substr(1), index)) {
            std::wcerr << L"ksh: invalid history reference\n";
            return false;
        }
        if (index == 0 || index > g_command_history.size()) {
            std::wcerr << L"ksh: history event not found\n";
            return false;
        }
        resolved = g_command_history[static_cast<size_t>(index - 1)].command;
        return true;
    }

    std::wstring prefix = input.substr(1);
    for (size_t i = g_command_history.size(); i > 0; --i) {
        if (starts_with_case_insensitive(g_command_history[i - 1].command, prefix)) {
            resolved = g_command_history[i - 1].command;
            return true;
        }
    }

    std::wcerr << L"ksh: history event not found\n";
    return false;
}

void load_command_history_from_file() {
    g_history_file_path = resolve_history_file_path();

    if (g_history_file_path.empty()) {
        return;
    }

    std::wifstream history_file(g_history_file_path.c_str());
    if (!history_file.is_open()) {
        return;
    }

    const size_t configured_limit = history_size_limit();
    size_t history_load_cap = configured_limit;
    if (history_load_cap == 0) {
        history_load_cap = kMaxHistoryLoadEntries;
    } else {
        if (history_load_cap > kMaxHistoryLoadEntries) {
            history_load_cap = kMaxHistoryLoadEntries;
        }
    }

    std::deque<HistoryEntry> loaded_history;
    if (history_load_cap > 0) {
        loaded_history.clear();
    }

    std::wstring line;
    bool saw_legacy_untimestamped_entry = false;
    while (std::getline(history_file, line)) {
        // Skip lines that exceed the per-line limit to avoid memory exhaustion from
        // a malformed or adversarially crafted history file.
        if (line.size() > kMaxHistoryLineBytes) {
            continue;
        }
        std::wstring trimmed = trim_copy(line);
        if (!trimmed.empty()) {
            HistoryEntry entry;
            size_t separator = trimmed.find(L'\t');
            if (separator != std::wstring::npos) {
                entry.timestamp = trim_copy(trimmed.substr(0, separator));
                entry.command = trim_copy(trimmed.substr(separator + 1));
            } else {
                entry.timestamp = format_history_timestamp();
                entry.command = trimmed;
                saw_legacy_untimestamped_entry = true;
            }

            if (!entry.command.empty()) {
                if (history_load_cap > 0 && loaded_history.size() == history_load_cap) {
                    loaded_history.pop_front();
                }
                loaded_history.push_back(std::move(entry));
            }
        }
    }

    g_command_history.assign(loaded_history.begin(), loaded_history.end());

    // Normalize pre-timestamp history files once loaded so all persisted entries use
    // the timestamped on-disk format.
    if (saw_legacy_untimestamped_entry && g_is_interactive_session) {
        rewrite_history_file();
    }
}

void append_history_entry_to_file(const std::wstring& command_line) {
    (void)append_history_entry(command_line);
}

void rewrite_history_file() {
    if (g_history_file_path.empty()) {
        g_history_file_path = resolve_history_file_path();
    }

    std::wofstream history_file(g_history_file_path.c_str(), std::ios::trunc);
    if (!history_file.is_open()) {
        return;
    }

    for (const HistoryEntry& entry : g_command_history) {
        history_file << serialize_history_entry(entry) << L"\n";
    }
}

std::wstring format_bytes_iec(ULONGLONG bytes) {
    static const wchar_t* units[] = { L"B", L"KiB", L"MiB", L"GiB", L"TiB" };
    double value = static_cast<double>(bytes);
    size_t unit_index = 0;
    while (value >= 1024.0 && unit_index + 1 < _countof(units)) {
        value /= 1024.0;
        unit_index++;
    }

    wchar_t buffer[64];
    if (unit_index == 0) {
        _snwprintf_s(buffer, _countof(buffer), _TRUNCATE, L"%llu %s", bytes, units[unit_index]);
    } else {
        _snwprintf_s(buffer, _countof(buffer), _TRUNCATE, L"%.2f %s", value, units[unit_index]);
    }
    return buffer;
}

std::wstring get_registry_string(HKEY root, const wchar_t* subkey, const wchar_t* value_name) {
    wchar_t buffer[256];
    DWORD size = static_cast<DWORD>(sizeof(buffer));
    LONG status = RegGetValueW(root, subkey, value_name, RRF_RT_REG_SZ, nullptr, buffer, &size);
    if (status != ERROR_SUCCESS) {
        return L"";
    }
    return std::wstring(buffer);
}

std::wstring get_windows_release_text() {
    std::wstring product_name = get_registry_string(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"ProductName");
    std::wstring display_version = get_registry_string(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"DisplayVersion");

    typedef LONG(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    RtlGetVersionPtr rtl_get_version = (ntdll != nullptr)
        ? reinterpret_cast<RtlGetVersionPtr>(GetProcAddress(ntdll, "RtlGetVersion"))
        : nullptr;

    RTL_OSVERSIONINFOEXW os = {};
    os.dwOSVersionInfoSize = sizeof(os);

    wchar_t version_text[96] = L"unknown";
    if (rtl_get_version != nullptr && rtl_get_version(reinterpret_cast<PRTL_OSVERSIONINFOW>(&os)) == 0) {
        _snwprintf_s(version_text, _countof(version_text), _TRUNCATE, L"%lu.%lu.%lu", os.dwMajorVersion, os.dwMinorVersion, os.dwBuildNumber);
    }

    std::wstring release = product_name.empty() ? L"Windows" : product_name;
    release += L" (";
    release += version_text;
    if (!display_version.empty()) {
        release += L", ";
        release += display_version;
    }
    release += L")";
    return release;
}

void print_version_text(const std::function<bool(const std::wstring&)>& write_output) {
    std::wstring out =
        L"\n"
        L"  CrossShellKSH 3.1.16-2026 Copyright (C) 2026, Roberto J Dohnert\n"
        L"         All Rights Reserved. License: BSD-3-Clause        \n"
        L"\n"
        L"Microsoft Windows is a registered trademark of Microsoft Corporation.\n"
        L"KornShell is a registered trademark of AT&T Research released under the\n"
        L"Eclipse Public License.\n"
        L"\n"
        L"This program is licensed under the BSD-3 Clause License.\n"
        L"\n"
        L"OS Release: Microsoft " + get_windows_release_text() + L"\n\n";
    write_output(out);
}


void print_startup_system_info() {
    std::wcout << L"CrossShellKSH v3.1.16-2026" << std::endl;
}

void append_command_to_history_file(const std::wstring& command) {
    append_history_entry_to_file(command);
}

bool file_exists_regular(const std::wstring& path) {
    DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool ensure_default_home_kshrc_exists(const std::wstring& profile_path) {
    if (profile_path.empty()) {
        return false;
    }

    DWORD attributes = GetFileAttributesW(profile_path.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES) {
        return (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    std::ofstream profile_file(profile_path.c_str(), std::ios::binary);
    if (!profile_file.is_open()) {
        std::wcerr << L"ksh: warning: could not create startup profile: " << profile_path << L"\n";
        return false;
    }

    const char* default_profile =
        "# ksh startup profile (.kshrc)\n"
        "# This file is loaded automatically when ksh starts.\n"
        "# Add aliases, variables, and shell functions here.\n"
        "\n"
        "# Prompt customization tokens for PS1:\n"
        "#   %u user, %d domain, %w cwd, %m host, %# role-char, %% literal-percent\n"
        "# Example:\n"
        "# PS1='%u@%d %w %# '\n"
        "# alias ll='dir'\n";
    profile_file.write(default_profile, static_cast<std::streamsize>(std::strlen(default_profile)));
    if (!profile_file.good()) {
        std::wcerr << L"ksh: warning: failed writing startup profile: " << profile_path << L"\n";
        return false;
    }

    return true;
}

std::wstring get_default_startup_profile_path() {
    wchar_t userprofile_path[MAX_PATH];
    DWORD userprofile_len = GetEnvironmentVariableW(L"USERPROFILE", userprofile_path, MAX_PATH);
    if (userprofile_len > 0 && userprofile_len < MAX_PATH) {
        std::wstring candidate = std::wstring(userprofile_path) + L"\\.kshrc";
        if (file_exists_regular(candidate) || ensure_default_home_kshrc_exists(candidate)) {
            return candidate;
        }
    }

    // Legacy fallback for existing installs that use the AppData profile path.
    wchar_t appdata_path[MAX_PATH];
    DWORD appdata_len = GetEnvironmentVariableW(L"APPDATA", appdata_path, MAX_PATH);
    if (appdata_len > 0 && appdata_len < MAX_PATH) {
        std::wstring candidate = std::wstring(appdata_path) + L"\\ksh_profile.ksh";
        if (file_exists_regular(candidate)) {
            return candidate;
        }
    }

    return L"";
}

bool load_startup_profile(const StartupOptions& options, bool& should_exit_shell) {
    if (options.disable_profile) {
        return true;
    }

    std::wstring profile_path;
    if (options.profile_override_set) {
        profile_path = trim_copy(options.profile_override_path);
        if (profile_path.empty()) {
            return true;
        }
    } else {
        profile_path = get_default_startup_profile_path();
        if (profile_path.empty()) {
            return true;
        }
    }

    if (!file_exists_regular(profile_path)) {
        if (options.profile_override_set) {
            std::wcerr << L"ksh: cannot open startup profile: " << profile_path << L"\n";
        }
        return true;
    }

    std::vector<std::wstring> empty_args;
    if (!execute_script_file(profile_path, empty_args, should_exit_shell)) {
        std::wcerr << L"ksh: startup profile reported an error: " << profile_path << L"\n";
    }

    return true;
}

// Startup section: command-line option parsing and shell bootstrap.
bool parse_startup_arguments(int argc, wchar_t* argv[], StartupOptions& options, int& first_positional_index, bool& show_help, bool& show_version) {
    first_positional_index = argc;

    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];

        if (arg == L"--") {
            first_positional_index = i + 1;
            return true;
        }

        if (arg == L"-c") {
            if (i + 1 >= argc) {
                std::wcerr << L"ksh: -c requires a command string\n";
                return false;
            }
            options.command_string_set = true;
            options.command_string = argv[++i];
            first_positional_index = i + 1;
            return true;
        }

        if (arg == L"--script-test") {
            if (i + 1 >= argc) {
                std::wcerr << L"ksh: --script-test requires a script path\n";
                return false;
            }
            options.script_test_set = true;
            options.script_test_path = argv[++i];
            options.disable_profile = true;
            first_positional_index = i + 1;
            return true;
        }

        if (arg == L"-h" || arg == L"--help") {
            show_help = true;
            continue;
        }

        if (arg == L"--version") {
            show_version = true;
            continue;
        }

        if (arg == L"--no-profile") {
            options.disable_profile = true;
            continue;
        }

        if (arg == L"--profile") {
            if (i + 1 >= argc) {
                std::wcerr << L"ksh: --profile requires a file path\n";
                return false;
            }
            options.profile_override_set = true;
            options.profile_override_path = argv[++i];
            continue;
        }

        if (arg.rfind(L"--profile=", 0) == 0) {
            options.profile_override_set = true;
            options.profile_override_path = arg.substr(10);
            continue;
        }

        first_positional_index = i;
        return true;
    }

    return true;
}
