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

void enable_ansi_support() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE && hOut != nullptr) {
        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode)) {
            SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
    HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
    if (hErr != INVALID_HANDLE_VALUE && hErr != nullptr) {
        DWORD mode = 0;
        if (GetConsoleMode(hErr, &mode)) {
            SetConsoleMode(hErr, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
}

class RedStderrBuffer : public std::wstreambuf {
public:
    explicit RedStderrBuffer(std::wstreambuf* target)
        : target_(target), stderr_handle_(GetStdHandle(STD_ERROR_HANDLE)), is_console_(false) {
        if (stderr_handle_ != nullptr && stderr_handle_ != INVALID_HANDLE_VALUE) {
            DWORD mode = 0;
            if (GetConsoleMode(stderr_handle_, &mode)) {
                is_console_ = true;
            }
        }
    }

protected:
    int_type overflow(int_type ch) override {
        if (traits_type::eq_int_type(ch, traits_type::eof())) {
            return traits_type::not_eof(ch);
        }

        const wchar_t wide_ch = traits_type::to_char_type(ch);
        if (!write_with_color(&wide_ch, 1)) {
            return traits_type::eof();
        }
        return ch;
    }

    std::streamsize xsputn(const wchar_t* s, std::streamsize n) override {
        if (n <= 0) {
            return 0;
        }

        if (!write_with_color(s, n)) {
            return 0;
        }
        return n;
    }

    int sync() override {
        return target_->pubsync();
    }

private:
    bool write_with_color(const wchar_t* text, std::streamsize count) {
        WORD previous_attributes = 0;
        const bool changed_color = try_set_red(previous_attributes);
        const std::streamsize written = target_->sputn(text, count);
        if (changed_color) {
            SetConsoleTextAttribute(stderr_handle_, previous_attributes);
        }
        return written == count;
    }

    bool try_set_red(WORD& previous_attributes) {
        if (!is_console_) {
            return false;
        }

        if (stderr_handle_ == nullptr || stderr_handle_ == INVALID_HANDLE_VALUE) {
            return false;
        }

        CONSOLE_SCREEN_BUFFER_INFO info;
        if (!GetConsoleScreenBufferInfo(stderr_handle_, &info)) {
            return false;
        }

        previous_attributes = info.wAttributes;
        if (!SetConsoleTextAttribute(stderr_handle_, FOREGROUND_RED | FOREGROUND_INTENSITY)) {
            return false;
        }

        return true;
    }

    std::wstreambuf* target_;
    HANDLE stderr_handle_;
    bool is_console_;
};

static std::wstreambuf* g_original_wcerr_buffer = nullptr;
static RedStderrBuffer* g_red_stderr_buffer = nullptr;

void enable_red_error_output() {
    if (g_red_stderr_buffer != nullptr) {
        return;
    }

    g_original_wcerr_buffer = std::wcerr.rdbuf();
    static RedStderrBuffer red_buffer(g_original_wcerr_buffer);
    g_red_stderr_buffer = &red_buffer;
    std::wcerr.rdbuf(g_red_stderr_buffer);
}

static std::wstring get_system_env_val(const std::wstring& name) {
    wchar_t buffer[256];
    DWORD ret = GetEnvironmentVariableW(name.c_str(), buffer, _countof(buffer));
    if (ret > 0 && ret < _countof(buffer)) {
        return std::wstring(buffer, ret);
    }
    if (ret >= _countof(buffer)) {
        std::vector<wchar_t> dyn(ret + 1);
        DWORD dyn_ret = GetEnvironmentVariableW(name.c_str(), dyn.data(), static_cast<DWORD>(dyn.size()));
        if (dyn_ret > 0 && dyn_ret < dyn.size()) {
            return std::wstring(dyn.data(), dyn_ret);
        }
    }
    return L"";
}

static std::wstring get_current_working_directory_for_prompt() {
    DWORD needed = GetCurrentDirectoryW(0, nullptr);
    if (needed == 0) {
        return L".";
    }

    std::vector<wchar_t> buffer(needed);
    DWORD written = GetCurrentDirectoryW(needed, buffer.data());
    if (written == 0 || written >= needed) {
        return L".";
    }

    return std::wstring(buffer.data(), written);
}

static std::wstring get_username_for_prompt() {
    wchar_t username[256] = { 0 };
    DWORD username_len = ARRAYSIZE(username);
    if (GetUserNameW(username, &username_len) && username_len > 0) {
        size_t actual_len = std::wcslen(username);
        return std::wstring(username, actual_len);
    }
    return L"";
}

static std::wstring get_domain_for_prompt() {
    std::wstring domain = get_system_env_val(L"USERDNSDOMAIN");
    if (!domain.empty()) {
        return domain;
    }
    domain = get_system_env_val(L"USERDOMAIN");
    if (!domain.empty()) {
        return domain;
    }
    return L"";
}

static std::wstring get_hostname_for_prompt() {
    wchar_t hostname[256] = { 0 };
    DWORD host_len = ARRAYSIZE(hostname);
    if (GetComputerNameW(hostname, &host_len) && host_len > 0) {
        return std::wstring(hostname, host_len);
    }
    return L"";
}

bool IsRunningAsAdmin() {
    SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
    PSID admin_group = nullptr;

    if (!AllocateAndInitializeSid(
            &nt_authority,
            2,
            SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS,
            0, 0, 0, 0, 0, 0,
            &admin_group)) {
        return false;
    }

    BOOL is_admin = FALSE;
    CheckTokenMembership(NULL, admin_group, &is_admin);
    FreeSid(admin_group);

    return is_admin == TRUE;
}

std::wstring format_prompt_template(const std::wstring& prompt_template) {
    const std::wstring username = get_username_for_prompt();
    const std::wstring domain = get_domain_for_prompt();
    const std::wstring cwd = get_current_working_directory_for_prompt();
    const std::wstring hostname = get_hostname_for_prompt();
    const wchar_t prompt_char = IsRunningAsAdmin() ? L'#' : L'$';

    std::wstring formatted;
    formatted.reserve(prompt_template.size() + 32);

    for (size_t i = 0; i < prompt_template.size(); ++i) {
        wchar_t ch = prompt_template[i];
        if (ch == L'%' && i + 1 < prompt_template.size()) {
            wchar_t token = prompt_template[i + 1];
            bool consumed = true;
            if (token == L'u') {
                formatted += username;
            } else if (token == L'd') {
                formatted += domain;
            } else if (token == L'w') {
                formatted += cwd;
            } else if (token == L'm') {
                formatted += hostname;
            } else if (token == L'#') {
                formatted.push_back(prompt_char);
            } else if (token == L'%') {
                formatted.push_back(L'%');
            } else {
                consumed = false;
            }

            if (consumed) {
                ++i;
                continue;
            }
        }

        formatted.push_back(ch);
    }

    return formatted;
}

bool get_current_directory_path(std::wstring& current_directory) {
    DWORD needed = GetCurrentDirectoryW(0, nullptr);
    if (needed == 0) {
        return false;
    }

    std::vector<wchar_t> buffer(needed);
    DWORD written = GetCurrentDirectoryW(needed, buffer.data());
    if (written == 0 || written >= needed) {
        return false;
    }

    current_directory.assign(buffer.data(), written);
    return true;
}

std::wstring get_interactive_prompt() {
    std::wstring prompt_template;

    auto scalar_it = ksh_env.variables.find(L"PS1");
    if (scalar_it != ksh_env.variables.end()) {
        prompt_template = scalar_it->second;
    } else {
        prompt_template = get_system_env_val(L"PS1");
    }

    if (!prompt_template.empty()) {
        return format_prompt_template(prompt_template);
    }

    return IsRunningAsAdmin() ? L"# " : L"$ ";
}

bool clear_console_screen() {
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    if (console == nullptr || console == INVALID_HANDLE_VALUE) {
        return false;
    }

    CONSOLE_SCREEN_BUFFER_INFO screen_info;
    if (!GetConsoleScreenBufferInfo(console, &screen_info)) {
        return false;
    }

    const DWORD cell_count =
        static_cast<DWORD>(screen_info.dwSize.X) * static_cast<DWORD>(screen_info.dwSize.Y);
    COORD top_left = { 0, 0 };
    DWORD written = 0;

    if (!FillConsoleOutputCharacterW(console, L' ', cell_count, top_left, &written)) {
        return false;
    }

    if (!FillConsoleOutputAttribute(console, screen_info.wAttributes, cell_count, top_left, &written)) {
        return false;
    }

    if (!SetConsoleCursorPosition(console, top_left)) {
        return false;
    }

    return true;
}

static bool contains_whitespace(const std::wstring& value) {
    for (wchar_t ch : value) {
        if (std::iswspace(ch)) {
            return true;
        }
    }
    return false;
}

static std::wstring longest_common_prefix_case_insensitive(const std::vector<std::wstring>& values) {
    if (values.empty()) {
        return L"";
    }

    std::wstring prefix = values[0];
    for (size_t i = 1; i < values.size() && !prefix.empty(); ++i) {
        size_t common = 0;
        size_t limit = (prefix.size() < values[i].size()) ? prefix.size() : values[i].size();
        while (common < limit && std::towlower(prefix[common]) == std::towlower(values[i][common])) {
            common++;
        }
        prefix = prefix.substr(0, common);
    }
    return prefix;
}

void add_completion_candidate(std::map<std::wstring, CompletionCandidate>& candidates, const std::wstring& candidate, int rank) {
    if (candidate.empty()) {
        return;
    }

    std::wstring key = to_lower_copy(candidate);
    auto it = candidates.find(key);
    if (it == candidates.end() || rank < it->second.rank || (rank == it->second.rank && candidate < it->second.value)) {
        candidates[key] = CompletionCandidate{candidate, rank};
    }
}

CompletionQuery analyze_completion_query(const std::wstring& raw_prefix) {
    CompletionQuery query;
    query.normalized_prefix = raw_prefix;

    if (!query.normalized_prefix.empty() && (query.normalized_prefix.front() == L'"' || query.normalized_prefix.front() == L'\'')) {
        query.quoted = true;
        query.quote_char = query.normalized_prefix.front();
        query.normalized_prefix.erase(0, 1);
    }

    size_t separator = query.normalized_prefix.find_last_of(L"\\/");
    if (separator != std::wstring::npos) {
        query.path_completion = true;
        query.path_directory = query.normalized_prefix.substr(0, separator + 1);
    }

    return query;
}

static std::vector<CompletionCandidate> filter_completion_candidates(const std::vector<CompletionCandidate>& candidates, const std::wstring& prefix) {
    std::vector<CompletionCandidate> matches;
    for (const CompletionCandidate& candidate : candidates) {
        if (prefix.empty() || starts_with_case_insensitive(candidate.value, prefix)) {
            matches.push_back(candidate);
        }
    }

    std::sort(matches.begin(), matches.end(), [](const CompletionCandidate& left, const CompletionCandidate& right) {
        if (left.rank != right.rank) {
            return left.rank < right.rank;
        }

        std::wstring left_lower = to_lower_copy(left.value);
        std::wstring right_lower = to_lower_copy(right.value);
        if (left_lower != right_lower) {
            return left_lower < right_lower;
        }
        return left.value < right.value;
    });

    return matches;
}

static const std::vector<CompletionCandidate>& collect_cached_command_candidates() {
    static CompletionCache cache;
    const ULONGLONG now_ms = GetTickCount64();

    wchar_t cwd_buffer[MAX_PATH];
    std::wstring cwd_value;
    if (GetCurrentDirectoryW(MAX_PATH, cwd_buffer) > 0) {
        cwd_value = cwd_buffer;
    }

    std::wstring path_value = get_system_env_val(L"PATH");

    if (cache.cwd == cwd_value && cache.path_value == path_value && !cache.candidates.empty() && (now_ms - cache.cached_at_ms) < kCompletionCacheTtlMs) {
        return cache.candidates;
    }

    std::map<std::wstring, CompletionCandidate> candidates;

    for (const std::wstring& builtin : builtin_commands()) {
        add_completion_candidate(candidates, builtin, 0);
    }

    if (!cwd_value.empty()) {
        std::wstring pattern = cwd_value;
        if (!pattern.empty() && pattern.back() != L'\\' && pattern.back() != L'/') {
            pattern += L"\\";
        }
        pattern += L"*";

        WIN32_FIND_DATAW find_data;
        HANDLE handle = FindFirstFileW(pattern.c_str(), &find_data);
        if (handle != INVALID_HANDLE_VALUE) {
            do {
                std::wstring name = find_data.cFileName;
                if (name == L"." || name == L"..") {
                    continue;
                }
                add_completion_candidate(candidates, name, 1);

                if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                    continue;
                }

                size_t dot = name.find_last_of(L'.');
                if (dot != std::wstring::npos) {
                    std::wstring extension = to_lower_copy(name.substr(dot));
                    if (extension == L".exe" || extension == L".cmd" || extension == L".bat" || extension == L".com") {
                        add_completion_candidate(candidates, name.substr(0, dot), 1);
                    }
                }
            } while (FindNextFileW(handle, &find_data));

            FindClose(handle);
        }
    }

    if (!path_value.empty()) {
        size_t start = 0;
        while (start <= path_value.size()) {
            size_t separator = path_value.find(L';', start);
            std::wstring entry = (separator == std::wstring::npos)
                ? path_value.substr(start)
                : path_value.substr(start, separator - start);
            entry = trim_copy(entry);
            if (!entry.empty()) {
                std::wstring pattern = entry;
                if (!pattern.empty() && pattern.back() != L'\\' && pattern.back() != L'/') {
                    pattern += L"\\";
                }
                pattern += L"*";

                WIN32_FIND_DATAW find_data;
                HANDLE handle = FindFirstFileW(pattern.c_str(), &find_data);
                if (handle != INVALID_HANDLE_VALUE) {
                    do {
                        std::wstring name = find_data.cFileName;
                        if (name == L"." || name == L"..") {
                            continue;
                        }
                        add_completion_candidate(candidates, name, 2);

                        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                            continue;
                        }

                        size_t dot = name.find_last_of(L'.');
                        if (dot != std::wstring::npos) {
                            std::wstring extension = to_lower_copy(name.substr(dot));
                            if (extension == L".exe" || extension == L".cmd" || extension == L".bat" || extension == L".com") {
                                add_completion_candidate(candidates, name.substr(0, dot), 2);
                            }
                        }
                    } while (FindNextFileW(handle, &find_data));

                    FindClose(handle);
                }
            }

            if (separator == std::wstring::npos) {
                break;
            }
            start = separator + 1;
        }
    }

    cache.cwd = cwd_value;
    cache.path_value = path_value;
    cache.cached_at_ms = now_ms;
    cache.candidates.clear();
    cache.candidates.reserve(candidates.size());
    for (const auto& kv : candidates) {
        cache.candidates.push_back(kv.second);
    }
    return cache.candidates;
}

static std::vector<CompletionCandidate> collect_path_completion_candidates(const CompletionQuery& query) {
    std::vector<CompletionCandidate> candidates;
    std::wstring directory = query.path_directory;
    if (directory.empty()) {
        return candidates;
    }

    std::wstring pattern = directory;
    if (!pattern.empty() && pattern.back() != L'\\' && pattern.back() != L'/') {
        pattern += L"\\";
    }
    pattern += L"*";

    WIN32_FIND_DATAW find_data;
    HANDLE handle = FindFirstFileW(pattern.c_str(), &find_data);
    if (handle == INVALID_HANDLE_VALUE) {
        return candidates;
    }

    do {
        std::wstring name = find_data.cFileName;
        if (name == L"." || name == L"..") {
            continue;
        }
        std::wstring full_name = directory + name;
        candidates.push_back(CompletionCandidate{full_name, 0});

        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }

        size_t dot = name.find_last_of(L'.');
        if (dot != std::wstring::npos) {
            std::wstring extension = to_lower_copy(name.substr(dot));
            if (extension == L".exe" || extension == L".cmd" || extension == L".bat" || extension == L".com") {
                candidates.push_back(CompletionCandidate{directory + name.substr(0, dot), 0});
            }
        }
    } while (FindNextFileW(handle, &find_data));

    FindClose(handle);

    return filter_completion_candidates(candidates, query.normalized_prefix);
}

static std::wstring format_completion_replacement(const CompletionQuery& query, const std::wstring& match) {
    bool should_quote = query.quoted || contains_whitespace(match);
    if (!should_quote) {
        return match;
    }

    wchar_t quote_char = query.quote_char != 0 ? query.quote_char : L'"';
    return std::wstring(1, quote_char) + match + std::wstring(1, quote_char);
}

static size_t find_completion_token_start(const std::wstring& buffer, size_t cursor) {
    bool in_single_quotes = false;
    bool in_double_quotes = false;
    bool escaping = false;
    size_t token_start = 0;

    for (size_t i = 0; i < cursor; ++i) {
        wchar_t ch = buffer[i];
        if (escaping) {
            escaping = false;
            continue;
        }

        if (ch == L'\\' && !in_single_quotes) {
            escaping = true;
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

        if (!in_single_quotes && !in_double_quotes && std::iswspace(ch)) {
            token_start = i + 1;
        }
    }

    return token_start;
}

std::vector<std::wstring> collect_completion_candidates(const std::wstring& raw_prefix) {
    CompletionQuery query = analyze_completion_query(raw_prefix);
    std::vector<CompletionCandidate> candidates;

    if (query.path_completion) {
        candidates = collect_path_completion_candidates(query);
    } else {
        const std::vector<CompletionCandidate>& cached = collect_cached_command_candidates();
        candidates = filter_completion_candidates(cached, query.normalized_prefix);
    }

    std::vector<std::wstring> matches;
    matches.reserve(candidates.size());
    for (const CompletionCandidate& candidate : candidates) {
        matches.push_back(candidate.value);
    }
    return matches;
}

std::vector<std::wstring> collect_tab_completions(const std::wstring& current_buffer, size_t cursor_pos, size_t& replace_start, size_t& replace_len) {
    replace_start = find_completion_token_start(current_buffer, cursor_pos);
    replace_len = cursor_pos - replace_start;
    std::wstring prefix = current_buffer.substr(replace_start, replace_len);
    return collect_completion_candidates(prefix);
}

struct InputLineRenderState {
    bool initialized = false;
    COORD line_start = { 0, 0 };
    size_t rendered_width = 0;
};

static InputLineRenderState& input_line_render_state() {
    static InputLineRenderState state;
    return state;
}

static void reset_input_line_render_state() {
    input_line_render_state() = InputLineRenderState();
}

static void initialize_input_line_render_state() {
    HANDLE output_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (output_handle == nullptr || output_handle == INVALID_HANDLE_VALUE) {
        reset_input_line_render_state();
        return;
    }

    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!GetConsoleScreenBufferInfo(output_handle, &info)) {
        reset_input_line_render_state();
        return;
    }

    InputLineRenderState& state = input_line_render_state();
    state.initialized = true;
    state.line_start = info.dwCursorPosition;
    state.line_start.Y = info.dwCursorPosition.Y;
    state.rendered_width = 0;
}

void render_input_line(const std::wstring& prompt, const std::wstring& buffer, size_t cursor_pos) {
    InputLineRenderState& state = input_line_render_state();
    HANDLE output_handle = GetStdHandle(STD_OUTPUT_HANDLE);

    if (state.initialized && output_handle != nullptr && output_handle != INVALID_HANDLE_VALUE) {
        const size_t rendered_width = prompt.size() + buffer.size();
        const size_t clear_width = (state.rendered_width > rendered_width) ? (state.rendered_width - rendered_width) : 0;
        DWORD written = 0;
        CONSOLE_CURSOR_INFO cursor_info;
        const bool have_cursor_info = GetConsoleCursorInfo(output_handle, &cursor_info) != FALSE;

        if (have_cursor_info && cursor_info.bVisible) {
            CONSOLE_CURSOR_INFO hidden_cursor_info = cursor_info;
            hidden_cursor_info.bVisible = FALSE;
            SetConsoleCursorInfo(output_handle, &hidden_cursor_info);
        }

        SetConsoleCursorPosition(output_handle, state.line_start);
        std::wcout << prompt << buffer;
        if (clear_width > 0) {
            COORD clear_position = state.line_start;
            clear_position.X = static_cast<SHORT>(clear_position.X + static_cast<SHORT>(prompt.size() + buffer.size()));
            FillConsoleOutputCharacterW(output_handle, L' ', static_cast<DWORD>(clear_width), clear_position, &written);
        }

        COORD cursor_position = state.line_start;
        cursor_position.X = static_cast<SHORT>(cursor_position.X + static_cast<SHORT>(prompt.size() + cursor_pos));
        SetConsoleCursorPosition(output_handle, cursor_position);

        if (have_cursor_info && cursor_info.bVisible) {
            SetConsoleCursorInfo(output_handle, &cursor_info);
        }

        std::wcout.flush();
        state.rendered_width = rendered_width;
        return;
    }

    std::wcout << L"\r" << prompt << buffer;
    if (state.rendered_width > prompt.size() + buffer.size()) {
        std::wcout << std::wstring(state.rendered_width - (prompt.size() + buffer.size()), L' ');
    }
    std::wcout << L"\r" << prompt << buffer.substr(0, cursor_pos);
    std::wcout.flush();
    state.rendered_width = prompt.size() + buffer.size();
}

bool read_interactive_line(const std::wstring& prompt, std::wstring& output) {
    struct InteractiveConsoleModeGuard {
        HANDLE input = INVALID_HANDLE_VALUE;
        DWORD original_mode = 0;
        bool changed = false;

        InteractiveConsoleModeGuard() {
            input = GetStdHandle(STD_INPUT_HANDLE);
            if (input != nullptr && input != INVALID_HANDLE_VALUE && GetConsoleMode(input, &original_mode)) {
                const DWORD editor_mode = original_mode &
                    ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
                changed = SetConsoleMode(input, editor_mode) != FALSE;
            }
        }

        ~InteractiveConsoleModeGuard() {
            if (changed) {
                SetConsoleMode(input, original_mode);
            }
        }
    } console_mode_guard;

    enum class ViState {
        Insert,
        Command
    };

    std::wstring buffer;
    size_t cursor = 0;
    size_t history_index = g_command_history.size();
    std::wstring saved_current_line;
    ViState vi_state = ViState::Insert;
    wint_t last_find_motion = 0;
    wint_t last_find_target = 0;
    int vi_pending_count = 0;
    bool vi_has_pending_count = false;

    auto invert_find_motion = [&](wint_t motion) -> wint_t {
        switch (motion) {
            case L'f': return L'F';
            case L'F': return L'f';
            case L't': return L'T';
            case L'T': return L't';
            default: return 0;
        }
    };

    auto apply_find_motion = [&](wint_t motion, wint_t target) -> bool {
        if (buffer.empty()) {
            return false;
        }
        if (target == 0 || target == 224) {
            return false;
        }

        const wchar_t needle = static_cast<wchar_t>(target);

        if (motion == L'f' || motion == L't') {
            const size_t start = (cursor < buffer.size()) ? (cursor + 1) : buffer.size();
            for (size_t i = start; i < buffer.size(); ++i) {
                if (buffer[i] == needle) {
                    cursor = (motion == L't' && i > 0) ? (i - 1) : i;
                    return true;
                }
            }
            return false;
        }

        if (motion == L'F' || motion == L'T') {
            if (cursor == 0) {
                return false;
            }

            const size_t start = cursor - 1;
            for (size_t i = start + 1; i > 0; --i) {
                const size_t index = i - 1;
                if (buffer[index] == needle) {
                    cursor = (motion == L'T' && (index + 1) < buffer.size()) ? (index + 1) : index;
                    return true;
                }
            }
            return false;
        }

        return false;
    };

    auto clamp_insert_cursor = [&]() {
        if (cursor > buffer.size()) {
            cursor = buffer.size();
        }
    };

    auto clamp_command_cursor = [&]() {
        if (buffer.empty()) {
            cursor = 0;
            return;
        }
        if (cursor >= buffer.size()) {
            cursor = buffer.size() - 1;
        }
    };

    auto move_to_first_non_blank = [&]() {
        size_t pos = 0;
        while (pos < buffer.size() && std::iswspace(buffer[pos])) {
            pos++;
        }
        cursor = (pos < buffer.size()) ? pos : 0;
    };

    auto history_up = [&]() {
        if (g_command_history.empty()) {
            return;
        }
        if (history_index == g_command_history.size()) {
            saved_current_line = buffer;
        }
        if (history_index > 0) {
            history_index--;
            buffer = g_command_history[history_index].command;
            cursor = buffer.size();
            if (g_vi_mode_enabled && vi_state == ViState::Command) {
                clamp_command_cursor();
            }
            render_input_line(prompt, buffer, cursor);
        }
    };

    auto history_down = [&]() {
        if (history_index < g_command_history.size()) {
            history_index++;
            if (history_index == g_command_history.size()) {
                buffer = saved_current_line;
            } else {
                buffer = g_command_history[history_index].command;
            }
            cursor = buffer.size();
            if (g_vi_mode_enabled && vi_state == ViState::Command) {
                clamp_command_cursor();
            }
            render_input_line(prompt, buffer, cursor);
        }
    };

    initialize_input_line_render_state();
    std::wcout << prompt;
    std::wcout.flush();

    while (true) {
        wint_t ch = _getwch();

        if (ch == 26) {
            if (buffer.empty()) {
                std::wcout << L"\n";
                return false;
            }
            continue;
        }

        if (ch == 13) {
            std::wcout << L"\n";
            output = buffer;
            return true;
        }

        if (ch == 3) {
            std::wcout << L"^C\n";
            output.clear();
            return true;
        }

        if (g_vi_mode_enabled) {
            if (ch == 27) {
                if (vi_state == ViState::Insert) {
                    vi_state = ViState::Command;
                    vi_pending_count = 0;
                    vi_has_pending_count = false;
                    if (cursor > 0) {
                        cursor--;
                    }
                    clamp_command_cursor();
                    render_input_line(prompt, buffer, cursor);
                }
                continue;
            }

            if (vi_state == ViState::Command && ch != 0 && ch != 224) {
                if (ch >= L'1' && ch <= L'9') {
                    const int digit = static_cast<int>(ch - L'0');
                    if (!vi_has_pending_count) {
                        vi_has_pending_count = true;
                        vi_pending_count = 0;
                    }
                    if (vi_pending_count <= 100000000) {
                        vi_pending_count = (vi_pending_count * 10) + digit;
                    }
                    continue;
                }
                if (ch == L'0' && vi_has_pending_count) {
                    if (vi_pending_count <= 100000000) {
                        vi_pending_count *= 10;
                    }
                    continue;
                }

                const int command_count = vi_has_pending_count ? vi_pending_count : 0;
                vi_pending_count = 0;
                vi_has_pending_count = false;
                const int repeat_count = (command_count > 0) ? command_count : 1;

                switch (ch) {
                    case L'i':
                        vi_state = ViState::Insert;
                        clamp_insert_cursor();
                        continue;
                    case L'a':
                        if (cursor < buffer.size()) {
                            cursor++;
                        }
                        vi_state = ViState::Insert;
                        clamp_insert_cursor();
                        render_input_line(prompt, buffer, cursor);
                        continue;
                    case L'A':
                        cursor = buffer.size();
                        vi_state = ViState::Insert;
                        render_input_line(prompt, buffer, cursor);
                        continue;
                    case L'I':
                        move_to_first_non_blank();
                        vi_state = ViState::Insert;
                        render_input_line(prompt, buffer, cursor);
                        continue;
                    case L'^':
                    case L'_':
                        move_to_first_non_blank();
                        render_input_line(prompt, buffer, cursor);
                        continue;
                    case L'0':
                        cursor = 0;
                        render_input_line(prompt, buffer, cursor);
                        continue;
                    case L'$':
                        cursor = buffer.empty() ? 0 : (buffer.size() - 1);
                        render_input_line(prompt, buffer, cursor);
                        continue;
                    case L'|': {
                        if (buffer.empty()) {
                            cursor = 0;
                        } else {
                            size_t target_column = 0;
                            if (command_count > 0) {
                                target_column = static_cast<size_t>(command_count - 1);
                            }
                            cursor = (target_column >= buffer.size()) ? (buffer.size() - 1) : target_column;
                        }
                        render_input_line(prompt, buffer, cursor);
                        continue;
                    }
                    case L'h':
                        for (int i = 0; i < repeat_count; ++i) {
                            if (cursor > 0) {
                                cursor--;
                            }
                        }
                        if (!buffer.empty()) {
                            render_input_line(prompt, buffer, cursor);
                        }
                        continue;
                    case L'l':
                        for (int i = 0; i < repeat_count; ++i) {
                            if (!buffer.empty() && cursor + 1 < buffer.size()) {
                                cursor++;
                            }
                        }
                        if (!buffer.empty()) {
                            render_input_line(prompt, buffer, cursor);
                        }
                        continue;
                    case L'w':
                    case L'W': {
                        for (int i = 0; i < repeat_count; ++i) {
                            size_t pos = cursor;
                            while (pos < buffer.size() && !std::iswspace(buffer[pos])) {
                                pos++;
                            }
                            while (pos < buffer.size() && std::iswspace(buffer[pos])) {
                                pos++;
                            }
                            if (!buffer.empty()) {
                                cursor = (pos >= buffer.size()) ? (buffer.size() - 1) : pos;
                            } else {
                                cursor = 0;
                            }
                        }
                        render_input_line(prompt, buffer, cursor);
                        continue;
                    }
                    case L'e':
                    case L'E': {
                        if (!buffer.empty()) {
                            for (int i = 0; i < repeat_count; ++i) {
                                size_t pos = (cursor >= buffer.size()) ? (buffer.size() - 1) : cursor;

                                while (pos < buffer.size() && std::iswspace(buffer[pos])) {
                                    pos++;
                                }

                                if (pos >= buffer.size()) {
                                    cursor = buffer.size() - 1;
                                } else {
                                    while ((pos + 1) < buffer.size() && !std::iswspace(buffer[pos + 1])) {
                                        pos++;
                                    }
                                    cursor = pos;
                                }
                            }

                            render_input_line(prompt, buffer, cursor);
                        }
                        continue;
                    }
                    case L'g': {
                        wint_t next = _getwch();
                        if (next == L'e' || next == L'E') {
                            if (!buffer.empty()) {
                                size_t pos = (cursor >= buffer.size()) ? (buffer.size() - 1) : cursor;

                                if (std::iswspace(buffer[pos])) {
                                    while (pos > 0 && std::iswspace(buffer[pos])) {
                                        pos--;
                                    }
                                } else {
                                    while (pos > 0 && !std::iswspace(buffer[pos - 1])) {
                                        pos--;
                                    }
                                    if (pos > 0) {
                                        pos--;
                                        while (pos > 0 && std::iswspace(buffer[pos])) {
                                            pos--;
                                        }
                                    }
                                }

                                while (pos > 0 && std::iswspace(buffer[pos])) {
                                    pos--;
                                }
                                while (pos > 0 && !std::iswspace(buffer[pos - 1])) {
                                    pos--;
                                }
                                size_t end_pos = pos;
                                while ((end_pos + 1) < buffer.size() && !std::iswspace(buffer[end_pos + 1])) {
                                    end_pos++;
                                }

                                cursor = end_pos;
                                render_input_line(prompt, buffer, cursor);
                            }
                            continue;
                        }
                        continue;
                    }
                    case L'f':
                    case L'F':
                    case L't':
                    case L'T': {
                        wint_t target = _getwch();
                        bool moved = false;
                        for (int i = 0; i < repeat_count; ++i) {
                            if (!apply_find_motion(ch, target)) {
                                break;
                            }
                            moved = true;
                        }
                        if (moved) {
                            last_find_motion = ch;
                            last_find_target = target;
                            render_input_line(prompt, buffer, cursor);
                        }
                        continue;
                    }
                    case L';': {
                        bool moved = false;
                        for (int i = 0; i < repeat_count; ++i) {
                            if (!apply_find_motion(last_find_motion, last_find_target)) {
                                break;
                            }
                            moved = true;
                        }
                        if (moved) {
                            render_input_line(prompt, buffer, cursor);
                        }
                        continue;
                    }
                    case L',': {
                        wint_t reverse_motion = invert_find_motion(last_find_motion);
                        bool moved = false;
                        for (int i = 0; i < repeat_count; ++i) {
                            if (!apply_find_motion(reverse_motion, last_find_target)) {
                                break;
                            }
                            moved = true;
                        }
                        if (moved) {
                            render_input_line(prompt, buffer, cursor);
                        }
                        continue;
                    }
                    case L'b':
                    case L'B': {
                        if (!buffer.empty()) {
                            for (int i = 0; i < repeat_count; ++i) {
                                size_t pos = cursor;
                                if (pos > 0) {
                                    pos--;
                                }
                                while (pos > 0 && std::iswspace(buffer[pos])) {
                                    pos--;
                                }
                                while (pos > 0 && !std::iswspace(buffer[pos - 1])) {
                                    pos--;
                                }
                                cursor = pos;
                            }
                            render_input_line(prompt, buffer, cursor);
                        }
                        continue;
                    }
                    case L'k':
                        for (int i = 0; i < repeat_count; ++i) {
                            history_up();
                        }
                        continue;
                    case L'j':
                        for (int i = 0; i < repeat_count; ++i) {
                            history_down();
                        }
                        continue;
                    case L'x':
                        if (cursor < buffer.size()) {
                            buffer.erase(cursor, 1);
                            clamp_command_cursor();
                            render_input_line(prompt, buffer, cursor);
                        }
                        continue;
                    case L'D':
                        if (cursor < buffer.size()) {
                            buffer.erase(cursor);
                            clamp_command_cursor();
                            render_input_line(prompt, buffer, cursor);
                        }
                        continue;
                    default:
                        continue;
                }
            }
        }

        if (ch == 9 && (!g_vi_mode_enabled || vi_state == ViState::Insert)) {
            size_t token_start = find_completion_token_start(buffer, cursor);
            std::wstring prefix = buffer.substr(token_start, cursor - token_start);
            CompletionQuery query = analyze_completion_query(prefix);
            std::vector<std::wstring> matches = collect_completion_candidates(prefix);

            if (matches.empty()) {
                std::wcout << L'\a';
                std::wcout.flush();
                continue;
            }

            std::wstring replacement = format_completion_replacement(query, matches[0]);
            if (matches.size() > 1) {
                std::wstring common_prefix = longest_common_prefix_case_insensitive(matches);
                if (common_prefix.size() > query.normalized_prefix.size()) {
                    replacement = format_completion_replacement(query, common_prefix);
                } else {
                    std::wcout << L"\n";
                    for (const std::wstring& match : matches) {
                        std::wcout << match << L"\n";
                    }
                    std::wcout.flush();
                    initialize_input_line_render_state();
                    render_input_line(prompt, buffer, cursor);
                    continue;
                }
            }

            buffer.replace(token_start, cursor - token_start, replacement);
            cursor = token_start + replacement.size();
            if (matches.size() == 1 && (cursor == buffer.size() || !std::iswspace(buffer[cursor]))) {
                buffer.insert(cursor, 1, L' ');
                cursor++;
            }
            render_input_line(prompt, buffer, cursor);
            continue;
        }

        if (ch == 8 && (!g_vi_mode_enabled || vi_state == ViState::Insert)) {
            if (cursor > 0) {
                buffer.erase(cursor - 1, 1);
                cursor--;
                render_input_line(prompt, buffer, cursor);
            }
            continue;
        }

        if (ch == 0 || ch == 224) {
            wint_t key = _getwch();
            if (key == 72) {
                history_up();
            } else if (key == 80) {
                history_down();
            } else if (key == 75) {
                if (cursor > 0) {
                    cursor--;
                    render_input_line(prompt, buffer, cursor);
                }
            } else if (key == 77) {
                if (!g_vi_mode_enabled || vi_state == ViState::Insert) {
                    if (cursor < buffer.size()) {
                        cursor++;
                        render_input_line(prompt, buffer, cursor);
                    }
                } else if (!buffer.empty() && cursor + 1 < buffer.size()) {
                    cursor++;
                    render_input_line(prompt, buffer, cursor);
                }
            } else if (key == 71) {
                cursor = 0;
                render_input_line(prompt, buffer, cursor);
            } else if (key == 79) {
                cursor = (!g_vi_mode_enabled || vi_state == ViState::Insert)
                    ? buffer.size()
                    : (buffer.empty() ? 0 : (buffer.size() - 1));
                render_input_line(prompt, buffer, cursor);
            } else if (key == 83) {
                if (cursor < buffer.size()) {
                    buffer.erase(cursor, 1);
                    if (g_vi_mode_enabled && vi_state == ViState::Command) {
                        clamp_command_cursor();
                    }
                    render_input_line(prompt, buffer, cursor);
                }
            }
            continue;
        }

        if (ch >= 32 && (!g_vi_mode_enabled || vi_state == ViState::Insert)) {
            buffer.insert(cursor, 1, static_cast<wchar_t>(ch));
            cursor++;
            render_input_line(prompt, buffer, cursor);
        }
    }
}
