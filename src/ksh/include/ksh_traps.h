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

#ifndef CROSSSHELL_KSH_TRAPS_H
#define CROSSSHELL_KSH_TRAPS_H

#include "ksh_types.h"

class ScopedDebugPrivilege {
public:
    ScopedDebugPrivilege() {
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token_handle_)) {
            return;
        }

        LUID privilege_luid;
        if (!LookupPrivilegeValueW(nullptr, L"SeDebugPrivilege", &privilege_luid)) {
            return;
        }

        TOKEN_PRIVILEGES requested_state;
        ZeroMemory(&requested_state, sizeof(requested_state));
        requested_state.PrivilegeCount = 1;
        requested_state.Privileges[0].Luid = privilege_luid;
        requested_state.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        DWORD previous_state_size = sizeof(previous_state_);
        const BOOL adjusted = AdjustTokenPrivileges(
            token_handle_,
            FALSE,
            &requested_state,
            sizeof(previous_state_),
            &previous_state_,
            &previous_state_size);
        const DWORD adjust_error = GetLastError();
        if (!adjusted || adjust_error == ERROR_NOT_ALL_ASSIGNED) {
            ZeroMemory(&previous_state_, sizeof(previous_state_));
            return;
        }

        has_previous_state_ = true;
    }

    ~ScopedDebugPrivilege() {
        if (token_handle_ == nullptr) {
            return;
        }

        if (has_previous_state_) {
            AdjustTokenPrivileges(token_handle_, FALSE, &previous_state_, 0, nullptr, nullptr);
        }

        CloseHandle(token_handle_);
    }

private:
    HANDLE token_handle_ = nullptr;
    TOKEN_PRIVILEGES previous_state_ = {};
    bool has_previous_state_ = false;
};

enum class KillBuiltinSignal {
    Hup,
    Int,
    Quit,
    Kill,
    Term
};

std::wstring normalize_trap_event_name(const std::wstring& raw);
bool has_trap_handler(const std::wstring& event_name);
bool parse_kill_builtin_signal(const std::wstring& raw_signal, KillBuiltinSignal& signal);
std::wstring signal_spec_to_trap_event(const std::wstring& raw_signal);
bool force_kill_process_by_pid(DWORD pid);
bool graceful_kill_process_by_pid(DWORD pid);
bool send_console_event_to_pid(DWORD pid, DWORD control_event);
bool execute_registered_trap(const std::wstring& event_name, bool& should_exit_shell);
bool queue_pending_trap_event(const std::wstring& event_name);
void process_pending_traps(bool& should_exit_shell);
void run_exit_trap_once(bool& should_exit_shell);
BOOL WINAPI ksh_console_ctrl_handler(DWORD ctrl_type);

#endif // CROSSSHELL_KSH_TRAPS_H
