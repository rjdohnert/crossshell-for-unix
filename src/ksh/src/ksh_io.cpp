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

bool is_custom_fd_in_range(int fd) {
    return fd >= kMinCustomFd && fd <= kMaxCustomFd;
}

bool is_valid_handle_value(HANDLE handle) {
    return handle != nullptr && handle != INVALID_HANDLE_VALUE;
}

std::array<HANDLE, kCustomFdTableSize> make_invalid_custom_fd_table() {
    std::array<HANDLE, kCustomFdTableSize> table = {};
    table.fill(INVALID_HANDLE_VALUE);
    return table;
}

HANDLE get_persistent_custom_fd(int fd) {
    if (!is_custom_fd_in_range(fd)) {
        return INVALID_HANDLE_VALUE;
    }
    return g_custom_fd_table[static_cast<size_t>(fd)];
}

bool open_redirection_file(const std::wstring& path, DWORD access, DWORD creation, HANDLE& handle) {
    std::wstring normalized_path = translate_device_path(path);
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = nullptr;
    sa.bInheritHandle = TRUE;

    handle = CreateFileW(
        normalized_path.c_str(),
        access,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        &sa,
        creation,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    return handle != INVALID_HANDLE_VALUE;
}

bool set_persistent_custom_fd(int fd, HANDLE new_handle, std::wstring& error_message) {
    if (!is_custom_fd_in_range(fd)) {
        error_message = L"ksh: custom fd out of range (expected 3-9)";
        return false;
    }

    HANDLE& slot = g_custom_fd_table[static_cast<size_t>(fd)];
    if (is_valid_handle_value(slot)) {
        CloseHandle(slot);
    }

    if (is_valid_handle_value(new_handle)) {
        slot = new_handle;
    } else {
        slot = INVALID_HANDLE_VALUE;
    }
    return true;
}

static bool duplicate_handle_for_custom_fd_source(int source_fd, HANDLE& duplicated_handle, std::wstring& error_message) {
    duplicated_handle = INVALID_HANDLE_VALUE;

    HANDLE source_handle = INVALID_HANDLE_VALUE;
    if (source_fd == 0) {
        source_handle = GetStdHandle(STD_INPUT_HANDLE);
    } else if (source_fd == 1) {
        source_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    } else if (source_fd == 2) {
        source_handle = GetStdHandle(STD_ERROR_HANDLE);
    } else if (is_custom_fd_in_range(source_fd)) {
        HANDLE persistent = get_persistent_custom_fd(source_fd);
        if (!is_valid_handle_value(persistent)) {
            error_message = L"ksh: source custom fd is not open: " + std::to_wstring(source_fd);
            return false;
        }
        source_handle = persistent;
    } else {
        error_message = L"ksh: unsupported source fd: " + std::to_wstring(source_fd);
        return false;
    }

    if (!is_valid_handle_value(source_handle)) {
        error_message = L"ksh: source fd is not available: " + std::to_wstring(source_fd);
        return false;
    }

    if (!DuplicateHandle(
            GetCurrentProcess(),
            source_handle,
            GetCurrentProcess(),
            &duplicated_handle,
            0,
            TRUE,
            DUPLICATE_SAME_ACCESS)) {
        error_message = L"ksh: failed to duplicate fd: " + std::to_wstring(source_fd);
        return false;
    }

    return true;
}

bool close_persistent_custom_fd(int fd, std::wstring& error_message) {
    return set_persistent_custom_fd(fd, INVALID_HANDLE_VALUE, error_message);
}

bool duplicate_persistent_custom_fd(int source_fd, int target_fd, std::wstring& error_message) {
    HANDLE duplicated = INVALID_HANDLE_VALUE;
    if (!duplicate_handle_for_custom_fd_source(source_fd, duplicated, error_message)) {
        return false;
    }
    return set_persistent_custom_fd(target_fd, duplicated, error_message);
}

bool has_any_redirection(const RedirectionSpec& redir) {
    return redir.has_stdin || redir.has_stdout || redir.stdout_to_stderr || redir.has_stderr || redir.stderr_to_stdout || !redir.custom_fd_actions.empty();
}

bool try_parse_custom_fd_redirection_token(
    const std::wstring& token,
    const std::vector<std::wstring>& tokens,
    size_t& token_index,
    RedirectionSpec& redir,
    std::wstring& error_message,
    bool& handled) {
    handled = false;
    if (token.size() < 2 || !std::iswdigit(token[0])) {
        return true;
    }

    const int fd = token[0] - L'0';
    if (!is_custom_fd_in_range(fd)) {
        return true;
    }

    std::wstring suffix = token.substr(1);
    if (suffix.empty() || (suffix[0] != L'>' && suffix[0] != L'<')) {
        return true;
    }
    handled = true;

    auto consume_target = [&](size_t op_len, std::wstring& target_out) -> bool {
        if (suffix.size() > op_len) {
            target_out = suffix.substr(op_len);
            return true;
        }
        if (token_index + 1 >= tokens.size()) {
            return false;
        }
        token_index++;
        target_out = tokens[token_index];
        return true;
    };

    if (suffix == L">&-" || suffix == L"<&-") {
        RedirectionSpec::CustomFdAction action;
        action.fd = fd;
        action.kind = RedirectionSpec::CustomFdAction::Kind::Close;
        redir.custom_fd_actions.push_back(action);
        return true;
    }

    if (suffix.rfind(L">&", 0) == 0) {
        std::wstring source_text;
        if (!consume_target(2, source_text) || source_text.size() != 1 || !std::iswdigit(source_text[0])) {
            error_message = L"ksh: invalid custom fd duplication: " + token;
            return false;
        }

        RedirectionSpec::CustomFdAction action;
        action.fd = fd;
        action.kind = RedirectionSpec::CustomFdAction::Kind::Duplicate;
        action.duplicate_source_fd = source_text[0] - L'0';
        redir.custom_fd_actions.push_back(action);
        return true;
    }

    std::wstring target_path;
    if (suffix.rfind(L">>", 0) == 0) {
        if (!consume_target(2, target_path) || target_path.empty()) {
            error_message = L"ksh: missing file for custom fd append redirection";
            return false;
        }
        RedirectionSpec::CustomFdAction action;
        action.fd = fd;
        action.kind = RedirectionSpec::CustomFdAction::Kind::OpenWriteAppend;
        action.path = target_path;
        redir.custom_fd_actions.push_back(action);
        return true;
    }

    if (suffix.rfind(L">", 0) == 0) {
        if (!consume_target(1, target_path) || target_path.empty()) {
            error_message = L"ksh: missing file for custom fd output redirection";
            return false;
        }
        RedirectionSpec::CustomFdAction action;
        action.fd = fd;
        action.kind = RedirectionSpec::CustomFdAction::Kind::OpenWriteTruncate;
        action.path = target_path;
        redir.custom_fd_actions.push_back(action);
        return true;
    }

    if (suffix.rfind(L"<", 0) == 0) {
        if (!consume_target(1, target_path) || target_path.empty()) {
            error_message = L"ksh: missing file for custom fd input redirection";
            return false;
        }
        RedirectionSpec::CustomFdAction action;
        action.fd = fd;
        action.kind = RedirectionSpec::CustomFdAction::Kind::OpenRead;
        action.path = target_path;
        redir.custom_fd_actions.push_back(action);
        return true;
    }

    error_message = L"ksh: unsupported custom fd redirection construct: " + token;
    return false;
}

bool parse_redirections(std::vector<std::wstring>& tokens, RedirectionSpec& redir, std::wstring& error_message) {
    std::vector<std::wstring> filtered;
    filtered.reserve(tokens.size());

    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::wstring& token = tokens[i];

        bool handled_custom_fd = false;
        if (!try_parse_custom_fd_redirection_token(token, tokens, i, redir, error_message, handled_custom_fd)) {
            return false;
        }
        if (handled_custom_fd) {
            continue;
        }

        enum class RedirKind {
            None,
            In,
            OutTrunc,
            OutAppend,
            OutToErr,
            ErrTrunc,
            ErrAppend,
            ErrToOut
        };

        auto classify_token = [&](const std::wstring& value, size_t& operator_len, RedirKind& kind) -> bool {
            operator_len = 0;
            kind = RedirKind::None;

            if (value == L"2>&1") {
                operator_len = value.size();
                kind = RedirKind::ErrToOut;
                return true;
            }
            if (value == L"1>&2" || value == L">&2") {
                operator_len = value.size();
                kind = RedirKind::OutToErr;
                return true;
            }

            size_t dup_pos = value.find(L">&");
            if (dup_pos != std::wstring::npos) {
                int source_fd = 1;
                if (dup_pos > 0) {
                    if (dup_pos > 1 || !std::iswdigit(value[0])) {
                        return false;
                    }
                    source_fd = value[0] - L'0';
                }

                if (dup_pos + 2 >= value.size() || (dup_pos + 3) != value.size() || !std::iswdigit(value[dup_pos + 2])) {
                    return false;
                }

                int target_fd = value[dup_pos + 2] - L'0';
                if (source_fd == 1 && target_fd == 2) {
                    operator_len = value.size();
                    kind = RedirKind::OutToErr;
                    return true;
                }
                if (source_fd == 2 && target_fd == 1) {
                    operator_len = value.size();
                    kind = RedirKind::ErrToOut;
                    return true;
                }

                if (source_fd == target_fd && (source_fd == 1 || source_fd == 2)) {
                    operator_len = value.size();
                    kind = RedirKind::None;
                    return true;
                }

                return false;
            }

            int fd = -1;
            size_t pos = 0;
            if (!value.empty() && std::iswdigit(value[0])) {
                fd = value[0] - L'0';
                pos = 1;
            }

            if (pos >= value.size()) {
                return false;
            }

            if (value.compare(pos, 2, L">>") == 0) {
                if (fd == -1 || fd == 1) {
                    operator_len = pos + 2;
                    kind = RedirKind::OutAppend;
                    return true;
                }
                if (fd == 2) {
                    operator_len = pos + 2;
                    kind = RedirKind::ErrAppend;
                    return true;
                }
                return false;
            }

            if (value[pos] == L'>') {
                if (fd == -1 || fd == 1) {
                    operator_len = pos + 1;
                    kind = RedirKind::OutTrunc;
                    return true;
                }
                if (fd == 2) {
                    operator_len = pos + 1;
                    kind = RedirKind::ErrTrunc;
                    return true;
                }
                return false;
            }

            if (value[pos] == L'<') {
                if (fd == -1 || fd == 0 || fd == 1) {
                    operator_len = pos + 1;
                    kind = RedirKind::In;
                    return true;
                }
                return false;
            }

            return false;
        };

        auto consume_target = [&](size_t operator_len, std::wstring& target_out) -> bool {
            if (token.size() > operator_len) {
                target_out = token.substr(operator_len);
                return true;
            }
            if (i + 1 >= tokens.size()) {
                return false;
            }
            i++;
            target_out = tokens[i];
            return true;
        };

        size_t op_len = 0;
        RedirKind kind = RedirKind::None;
        if (!classify_token(token, op_len, kind)) {
            size_t redir_pos = 0;
            if (!token.empty() && std::iswdigit(token[0])) {
                redir_pos = 1;
            }

            if (redir_pos < token.size() && (token[redir_pos] == L'>' || token[redir_pos] == L'<')) {
                error_message = L"ksh: unsupported or invalid redirection construct: " + token;
                return false;
            }

            filtered.push_back(token);
            continue;
        }

        std::wstring target;
        if (kind == RedirKind::None) {
            continue;
        }

        if (kind == RedirKind::ErrToOut) {
            redir.stderr_to_stdout = true;
            redir.has_stderr = false;
            redir.append_stderr = false;
            redir.stderr_path.clear();
            continue;
        }

        if (kind == RedirKind::OutToErr) {
            redir.stdout_to_stderr = true;
            redir.has_stdout = false;
            redir.append_stdout = false;
            redir.stdout_path.clear();
            continue;
        }

        if (kind == RedirKind::In) {
            if (!consume_target(op_len, target)) {
                error_message = L"ksh: missing file for input redirection";
                return false;
            }
            redir.has_stdin = true;
            redir.stdin_path = target;
            continue;
        }

        if (kind == RedirKind::OutAppend) {
            if (!consume_target(op_len, target)) {
                error_message = L"ksh: missing file for output redirection";
                return false;
            }
            redir.has_stdout = true;
            redir.stdout_to_stderr = false;
            redir.append_stdout = true;
            redir.stdout_path = target;
            continue;
        }

        if (kind == RedirKind::OutTrunc) {
            if (!consume_target(op_len, target)) {
                error_message = L"ksh: missing file for output redirection";
                return false;
            }
            redir.has_stdout = true;
            redir.stdout_to_stderr = false;
            redir.append_stdout = false;
            redir.stdout_path = target;
            continue;
        }

        if (kind == RedirKind::ErrAppend) {
            if (!consume_target(op_len, target)) {
                error_message = L"ksh: missing file for stderr redirection";
                return false;
            }
            redir.has_stderr = true;
            redir.stderr_to_stdout = false;
            redir.append_stderr = true;
            redir.stderr_path = target;
            continue;
        }

        if (kind == RedirKind::ErrTrunc) {
            if (!consume_target(op_len, target)) {
                error_message = L"ksh: missing file for stderr redirection";
                return false;
            }
            redir.has_stderr = true;
            redir.stderr_to_stdout = false;
            redir.append_stderr = false;
            redir.stderr_path = target;
            continue;
        }
    }

    if (redir.stdout_to_stderr && (redir.has_stderr || redir.stderr_to_stdout)) {
        error_message = L"ksh: unsupported mixed fd redirection requiring ordered duplication semantics";
        return false;
    }
    if (redir.stderr_to_stdout && (redir.has_stdout || redir.stdout_to_stderr)) {
        error_message = L"ksh: unsupported mixed fd redirection requiring ordered duplication semantics";
        return false;
    }

    tokens = filtered;
    return true;
}

class ScopedCustomFdTransaction {
public:
    ScopedCustomFdTransaction(
        const std::vector<RedirectionSpec::CustomFdAction>& actions,
        std::wstring& error_message)
        : committed_(false), ready_(true) {
        for (const RedirectionSpec::CustomFdAction& action : actions) {
            if (!is_custom_fd_in_range(action.fd)) {
                error_message = L"ksh: custom fd out of range (expected 3-9)";
                ready_ = false;
                cleanup_saved_handles();
                return;
            }

            const size_t slot = static_cast<size_t>(action.fd);
            if (has_snapshot_[slot]) {
                continue;
            }
            has_snapshot_[slot] = true;

            Snapshot snapshot;
            snapshot.had_entry = false;
            snapshot.saved_handle = INVALID_HANDLE_VALUE;

            HANDLE current = g_custom_fd_table[slot];
            if (is_valid_handle_value(current)) {
                HANDLE duplicated = INVALID_HANDLE_VALUE;
                if (!DuplicateHandle(
                        GetCurrentProcess(),
                        current,
                        GetCurrentProcess(),
                        &duplicated,
                        0,
                        TRUE,
                        DUPLICATE_SAME_ACCESS)) {
                    error_message = L"ksh: failed to snapshot custom fd: " + std::to_wstring(action.fd);
                    ready_ = false;
                    cleanup_saved_handles();
                    return;
                }

                snapshot.had_entry = true;
                snapshot.saved_handle = duplicated;
            }

            snapshots_[slot] = snapshot;
        }
    }

    ~ScopedCustomFdTransaction() {
        if (!ready_) {
            return;
        }

        if (!committed_) {
            rollback();
            return;
        }

        cleanup_saved_handles();
    }

    bool ready() const {
        return ready_;
    }

    void commit() {
        committed_ = true;
    }

private:
    struct Snapshot {
        bool had_entry;
        HANDLE saved_handle;
    };

    void rollback() {
        for (int fd = kMinCustomFd; fd <= kMaxCustomFd; ++fd) {
            const size_t slot = static_cast<size_t>(fd);
            if (!has_snapshot_[slot]) {
                continue;
            }

            Snapshot& snapshot = snapshots_[slot];
            if (is_valid_handle_value(g_custom_fd_table[slot])) {
                CloseHandle(g_custom_fd_table[slot]);
            }

            if (snapshot.had_entry && is_valid_handle_value(snapshot.saved_handle)) {
                g_custom_fd_table[slot] = snapshot.saved_handle;
                snapshot.saved_handle = INVALID_HANDLE_VALUE;
            } else {
                g_custom_fd_table[slot] = INVALID_HANDLE_VALUE;
            }
        }
    }

    void cleanup_saved_handles() {
        for (int fd = kMinCustomFd; fd <= kMaxCustomFd; ++fd) {
            const size_t slot = static_cast<size_t>(fd);
            if (!has_snapshot_[slot]) {
                continue;
            }

            HANDLE& handle = snapshots_[slot].saved_handle;
            if (is_valid_handle_value(handle)) {
                CloseHandle(handle);
                handle = INVALID_HANDLE_VALUE;
            }
        }
    }

    std::array<Snapshot, kCustomFdTableSize> snapshots_ = {};
    std::array<bool, kCustomFdTableSize> has_snapshot_ = {};
    bool committed_;
    bool ready_;
};

static bool is_custom_fd_source_available_preflight(int source_fd, const std::array<bool, kCustomFdTableSize>& simulated_custom_open) {
    if (is_custom_fd_in_range(source_fd)) {
        return simulated_custom_open[static_cast<size_t>(source_fd)];
    }

    HANDLE source_handle = INVALID_HANDLE_VALUE;
    if (source_fd == 0) {
        source_handle = GetStdHandle(STD_INPUT_HANDLE);
    } else if (source_fd == 1) {
        source_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    } else if (source_fd == 2) {
        source_handle = GetStdHandle(STD_ERROR_HANDLE);
    } else {
        return false;
    }

    return source_handle != nullptr && source_handle != INVALID_HANDLE_VALUE;
}

static bool preflight_validate_persistent_custom_fd_actions(
    const std::vector<RedirectionSpec::CustomFdAction>& actions,
    std::wstring& error_message) {
    std::array<bool, kCustomFdTableSize> simulated_custom_open = {};
    for (int fd = kMinCustomFd; fd <= kMaxCustomFd; ++fd) {
        simulated_custom_open[static_cast<size_t>(fd)] =
            is_valid_handle_value(g_custom_fd_table[static_cast<size_t>(fd)]);
    }

    for (const RedirectionSpec::CustomFdAction& action : actions) {
        if (!is_custom_fd_in_range(action.fd)) {
            error_message = L"ksh: custom fd out of range (expected 3-9)";
            return false;
        }

        if (action.kind == RedirectionSpec::CustomFdAction::Kind::Close) {
            simulated_custom_open[static_cast<size_t>(action.fd)] = false;
            continue;
        }

        if (action.kind == RedirectionSpec::CustomFdAction::Kind::Duplicate) {
            if (!is_custom_fd_source_available_preflight(action.duplicate_source_fd, simulated_custom_open)) {
                if (action.duplicate_source_fd >= 3 && action.duplicate_source_fd <= 9) {
                    error_message = L"ksh: source custom fd is not open: " + std::to_wstring(action.duplicate_source_fd);
                } else {
                    error_message = L"ksh: source fd is not available: " + std::to_wstring(action.duplicate_source_fd);
                }
                return false;
            }
            simulated_custom_open[static_cast<size_t>(action.fd)] = true;
            continue;
        }

        // Open actions conceptually make the destination fd available once applied.
        simulated_custom_open[static_cast<size_t>(action.fd)] = true;
    }

    return true;
}

bool apply_persistent_custom_fd_actions(const RedirectionSpec& redir, std::wstring& error_message) {
    if (!preflight_validate_persistent_custom_fd_actions(redir.custom_fd_actions, error_message)) {
        return false;
    }

    ScopedCustomFdTransaction transaction(redir.custom_fd_actions, error_message);
    if (!transaction.ready()) {
        return false;
    }

    for (const RedirectionSpec::CustomFdAction& action : redir.custom_fd_actions) {
        if (action.fd < 3 || action.fd > 9) {
            error_message = L"ksh: custom fd out of range (expected 3-9)";
            return false;
        }

        if (action.kind == RedirectionSpec::CustomFdAction::Kind::Close) {
            if (!set_persistent_custom_fd(action.fd, INVALID_HANDLE_VALUE, error_message)) {
                return false;
            }
            continue;
        }

        HANDLE new_handle = INVALID_HANDLE_VALUE;
        if (action.kind == RedirectionSpec::CustomFdAction::Kind::OpenRead) {
            if (!open_redirection_file(action.path, GENERIC_READ, OPEN_EXISTING, new_handle)) {
                error_message = L"ksh: failed opening custom fd input file: " + action.path;
                return false;
            }
        } else if (action.kind == RedirectionSpec::CustomFdAction::Kind::OpenWriteTruncate) {
            if (!open_redirection_file(action.path, GENERIC_WRITE, CREATE_ALWAYS, new_handle)) {
                error_message = L"ksh: failed opening custom fd output file: " + action.path;
                return false;
            }
        } else if (action.kind == RedirectionSpec::CustomFdAction::Kind::OpenWriteAppend) {
            if (!open_redirection_file(action.path, GENERIC_WRITE, OPEN_ALWAYS, new_handle)) {
                error_message = L"ksh: failed opening custom fd append file: " + action.path;
                return false;
            }
            SetFilePointer(new_handle, 0, nullptr, FILE_END);
        } else if (action.kind == RedirectionSpec::CustomFdAction::Kind::Duplicate) {
            if (!duplicate_handle_for_custom_fd_source(action.duplicate_source_fd, new_handle, error_message)) {
                return false;
            }
        }

        if (!set_persistent_custom_fd(action.fd, new_handle, error_message)) {
            if (new_handle != nullptr && new_handle != INVALID_HANDLE_VALUE) {
                CloseHandle(new_handle);
            }
            return false;
        }
    }

    transaction.commit();
    return true;
}

bool setup_redirection_handles(
    const RedirectionSpec* redir,
    HANDLE& std_in,
    HANDLE& std_out,
    HANDLE& std_err,
    bool& close_in,
    bool& close_out,
    bool& close_err) {
    std_in = (g_pipeline_stdin != INVALID_HANDLE_VALUE) ? g_pipeline_stdin : GetStdHandle(STD_INPUT_HANDLE);
    std_out = (g_pipeline_stdout != INVALID_HANDLE_VALUE)
        ? g_pipeline_stdout
        : ((g_subshell_stdout != INVALID_HANDLE_VALUE) ? g_subshell_stdout : GetStdHandle(STD_OUTPUT_HANDLE));
    std_err = (g_pipeline_stderr != INVALID_HANDLE_VALUE) ? g_pipeline_stderr : GetStdHandle(STD_ERROR_HANDLE);
    close_in = false;
    close_out = false;
    close_err = false;

    if (redir == nullptr || !has_any_redirection(*redir)) {
        return true;
    }

    if ((redir->stderr_to_stdout && redir->has_stderr) ||
        (redir->stdout_to_stderr && redir->has_stdout) ||
        (redir->stderr_to_stdout && redir->stdout_to_stderr)) {
        return false;
    }

    if (redir->has_stdin) {
        if (!open_redirection_file(redir->stdin_path, GENERIC_READ, OPEN_EXISTING, std_in)) {
            return false;
        }
        close_in = true;
    }

    if (redir->has_stdout) {
        const DWORD creation = redir->append_stdout ? OPEN_ALWAYS : CREATE_ALWAYS;
        if (!open_redirection_file(redir->stdout_path, GENERIC_WRITE, creation, std_out)) {
            if (close_in) CloseHandle(std_in);
            return false;
        }
        if (redir->append_stdout) {
            SetFilePointer(std_out, 0, nullptr, FILE_END);
        }
        close_out = true;
    }

    if (redir->has_stderr) {
        const DWORD creation = redir->append_stderr ? OPEN_ALWAYS : CREATE_ALWAYS;
        if (!open_redirection_file(redir->stderr_path, GENERIC_WRITE, creation, std_err)) {
            if (close_in) CloseHandle(std_in);
            if (close_out) CloseHandle(std_out);
            return false;
        }
        if (redir->append_stderr) {
            SetFilePointer(std_err, 0, nullptr, FILE_END);
        }
        close_err = true;
    }

    if (redir->stderr_to_stdout) {
        std_err = std_out;
        close_err = false;
    }

    if (redir->stdout_to_stderr) {
        std_out = std_err;
        close_out = false;
    }

    return true;
}

void close_redirection_handles(HANDLE std_in, HANDLE std_out, HANDLE std_err, bool close_in, bool close_out, bool close_err) {
    if (close_in && std_in != nullptr && std_in != INVALID_HANDLE_VALUE) {
        CloseHandle(std_in);
    }

    if (std_out == std_err && std_out != nullptr && std_out != INVALID_HANDLE_VALUE) {
        if (close_out || close_err) {
            CloseHandle(std_out);
        }
        return;
    }

    if (close_out && std_out != nullptr && std_out != INVALID_HANDLE_VALUE) {
        CloseHandle(std_out);
    }
    if (close_err && std_err != nullptr && std_err != INVALID_HANDLE_VALUE) {
        CloseHandle(std_err);
    }
}

std::wstring translate_device_path(const std::wstring& path) {
    std::wstring norm_path = path;
    for (auto& ch : norm_path) {
        if (ch == L'\\') ch = L'/';
    }
    if (norm_path == L"/dev/null") {
        return L"NUL";
    }
    if (norm_path == L"/dev/stdout" || norm_path == L"/dev/stderr") {
        return L"CONOUT$";
    }
    if (norm_path == L"/dev/stdin") {
        return L"CONIN$";
    }
    return path;
}

bool write_text_with_stdout_redirection(const std::wstring& text, const RedirectionSpec& redir) {
    if (redir.stdout_to_stderr) {
        if (g_pipeline_stderr != INVALID_HANDLE_VALUE) {
            int byte_count = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
            if (byte_count > 0) {
                std::string bytes(static_cast<size_t>(byte_count), '\0');
                WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), &bytes[0], byte_count, nullptr, nullptr);
                DWORD written = 0;
                return WriteFile(g_pipeline_stderr, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) != FALSE;
            }
        }
        std::wcerr << text;
        return true;
    }

    if (!redir.has_stdout) {
        if (g_pipeline_stdout != INVALID_HANDLE_VALUE) {
            int byte_count = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
            if (byte_count > 0) {
                std::string bytes(static_cast<size_t>(byte_count), '\0');
                WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), &bytes[0], byte_count, nullptr, nullptr);
                DWORD written = 0;
                return WriteFile(g_pipeline_stdout, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) != FALSE;
            }
        }
        std::wcout << text;
        return true;
    }

    std::wstring target_path = translate_device_path(redir.stdout_path);
    std::wofstream out_file;
    if (redir.append_stdout) {
        out_file.open(target_path.c_str(), std::ios::app);
    } else {
        out_file.open(target_path.c_str(), std::ios::trunc);
    }

    if (!out_file.is_open()) {
        std::wcerr << L"ksh: failed to open output file: " << redir.stdout_path << L"\n";
        return false;
    }

    out_file << text;
    return true;
}
