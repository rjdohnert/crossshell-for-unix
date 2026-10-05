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

std::wstring normalize_trap_event_name(const std::wstring& raw) {
    std::wstring token = trim_copy(raw);
    if (token.empty()) {
        return L"";
    }

    if (token.size() >= 3) {
        std::wstring prefix;
        prefix.reserve(3);
        prefix.push_back(static_cast<wchar_t>(std::towupper(token[0])));
        prefix.push_back(static_cast<wchar_t>(std::towupper(token[1])));
        prefix.push_back(static_cast<wchar_t>(std::towupper(token[2])));
        if (prefix == L"SIG") {
            token = token.substr(3);
        }
    }

    std::wstring upper;
    upper.reserve(token.size());
    for (wchar_t ch : token) {
        upper.push_back(static_cast<wchar_t>(std::towupper(ch)));
    }

    if (upper == L"0" || upper == L"EXIT") return L"EXIT";
    if (upper == L"1" || upper == L"HUP") return L"HUP";
    if (upper == L"2" || upper == L"INT") return L"INT";
    if (upper == L"3" || upper == L"BREAK") return L"BREAK";
    if (upper == L"QUIT") return L"QUIT";
    if (upper == L"8" || upper == L"FPE") return L"FPE";
    if (upper == L"10" || upper == L"USR1") return L"USR1";
    if (upper == L"11" || upper == L"SEGV") return L"SEGV";
    if (upper == L"12" || upper == L"USR2") return L"USR2";
    if (upper == L"14" || upper == L"ALRM") return L"ALRM";
    if (upper == L"15" || upper == L"TERM") return L"TERM";
    if (upper == L"17" || upper == L"CHLD" || upper == L"CLD") return L"CHLD";

    return L"";
}

bool has_trap_handler(const std::wstring& event_name) {
    return g_trap_handlers.find(event_name) != g_trap_handlers.end();
}

BOOL WINAPI ksh_console_ctrl_handler(DWORD ctrl_type) {
    switch (ctrl_type) {
    case CTRL_C_EVENT: {
        const bool int_active = InterlockedCompareExchange(&g_int_trap_active, 0, 0) != 0;
        if (int_active) {
            InterlockedExchange(&g_pending_int_trap, 1);
            return TRUE;
        }
        const bool fg_active = InterlockedCompareExchange(&g_foreground_process_active, 0, 0) != 0;
        const bool interactive = InterlockedCompareExchange(&g_in_interactive_loop, 0, 0) != 0;
        if (fg_active || interactive) {
            InterlockedExchange(&g_pending_int_trap, 1);
            return TRUE;
        }
        return FALSE;
    }
    case CTRL_BREAK_EVENT: {
        const bool break_active = InterlockedCompareExchange(&g_break_trap_active, 0, 0) != 0;
        if (break_active) {
            InterlockedExchange(&g_pending_break_trap, 1);
            return TRUE;
        }
        const bool fg_active = InterlockedCompareExchange(&g_foreground_process_active, 0, 0) != 0;
        const bool interactive = InterlockedCompareExchange(&g_in_interactive_loop, 0, 0) != 0;
        if (fg_active || interactive) {
            InterlockedExchange(&g_pending_break_trap, 1);
            return TRUE;
        }
        return FALSE;
    }
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT: {
        const bool hup_active = InterlockedCompareExchange(&g_hup_trap_active, 0, 0) != 0;
        const bool term_active = InterlockedCompareExchange(&g_term_trap_active, 0, 0) != 0;
        if (!hup_active && !term_active) {
            return FALSE;
        }
        if (hup_active) {
            InterlockedExchange(&g_pending_hup_trap, 1);
        }
        InterlockedExchange(&g_pending_term_trap, 1);
        return TRUE;
    }
    default:
        return FALSE;
    }
}

bool execute_registered_trap(const std::wstring& event_name, bool& should_exit_shell) {
    std::map<std::wstring, std::wstring>::const_iterator it = g_trap_handlers.find(event_name);
    if (it == g_trap_handlers.end()) {
        return true;
    }

    const std::wstring command = it->second;
    if (command.empty()) {
        return true;
    }

    if (g_running_trap_handler) {
        return true;
    }

    ScopedTrapExecutionFlag trap_guard(g_running_trap_handler);
    bool trap_exit_requested = false;
    bool ok = execute_command_line(command, trap_exit_requested);
    if (trap_exit_requested) {
        should_exit_shell = true;
    }
    return ok;
}

void process_pending_traps(bool& should_exit_shell) {
    if (InterlockedExchange(&g_pending_int_trap, 0) != 0) {
        if (!has_trap_handler(L"INT")) {
            ksh_env.variables[L"?"] = L"130";
        }
        execute_registered_trap(L"INT", should_exit_shell);
    }

    if (InterlockedExchange(&g_pending_break_trap, 0) != 0) {
        if (!has_trap_handler(L"BREAK")) {
            ksh_env.variables[L"?"] = L"131";
        }
        execute_registered_trap(L"BREAK", should_exit_shell);
    }

    if (InterlockedExchange(&g_pending_hup_trap, 0) != 0) {
        if (!has_trap_handler(L"HUP")) {
            ksh_env.variables[L"?"] = L"129";
            should_exit_shell = true;
        }
        execute_registered_trap(L"HUP", should_exit_shell);
    }

    if (InterlockedExchange(&g_pending_alrm_trap, 0) != 0) {
        execute_registered_trap(L"ALRM", should_exit_shell);
    }

    if (InterlockedExchange(&g_pending_usr1_trap, 0) != 0) {
        execute_registered_trap(L"USR1", should_exit_shell);
    }

    if (InterlockedExchange(&g_pending_usr2_trap, 0) != 0) {
        execute_registered_trap(L"USR2", should_exit_shell);
    }

    if (InterlockedExchange(&g_pending_segv_trap, 0) != 0) {
        execute_registered_trap(L"SEGV", should_exit_shell);
    }

    if (InterlockedExchange(&g_pending_fpe_trap, 0) != 0) {
        execute_registered_trap(L"FPE", should_exit_shell);
    }

    if (InterlockedExchange(&g_pending_term_trap, 0) != 0) {
        if (!has_trap_handler(L"TERM")) {
            ksh_env.variables[L"?"] = L"143";
            should_exit_shell = true;
        }
        execute_registered_trap(L"TERM", should_exit_shell);
    }

    if (InterlockedExchange(&g_pending_chld_trap, 0) != 0) {
        execute_registered_trap(L"CHLD", should_exit_shell);
    }
}

void run_exit_trap_once(bool& should_exit_shell) {
    if (g_exit_trap_executed) {
        return;
    }

    g_exit_trap_executed = true;
    execute_registered_trap(L"EXIT", should_exit_shell);
}

struct KillWindowEnumData {
    DWORD pid;
    bool window_found;
};

static BOOL CALLBACK kill_window_enum_proc(HWND window_handle, LPARAM parameter) {
    KillWindowEnumData* data = reinterpret_cast<KillWindowEnumData*>(parameter);
    DWORD window_pid = 0;
    GetWindowThreadProcessId(window_handle, &window_pid);

    if (window_pid == data->pid && IsWindowVisible(window_handle)) {
        PostMessageW(window_handle, WM_CLOSE, 0, 0);
        data->window_found = true;
    }

    return TRUE;
}

bool parse_kill_builtin_signal(const std::wstring& raw_signal, KillBuiltinSignal& signal) {
    std::wstring normalized_signal = normalize_trap_event_name(raw_signal);
    if (normalized_signal == L"HUP") {
        signal = KillBuiltinSignal::Hup;
        return true;
    }
    if (normalized_signal == L"INT") {
        signal = KillBuiltinSignal::Int;
        return true;
    }
    if (normalized_signal == L"BREAK" || normalized_signal == L"QUIT") {
        signal = KillBuiltinSignal::Quit;
        return true;
    }
    if (normalized_signal == L"TERM") {
        signal = KillBuiltinSignal::Term;
        return true;
    }

    std::wstring upper_signal;
    upper_signal.reserve(raw_signal.size());
    for (wchar_t ch : raw_signal) {
        upper_signal.push_back(static_cast<wchar_t>(std::towupper(ch)));
    }
    if (upper_signal.rfind(L"SIG", 0) == 0) {
        upper_signal = upper_signal.substr(3);
    }

    if (upper_signal == L"9" || upper_signal == L"KILL") {
        signal = KillBuiltinSignal::Kill;
        return true;
    }

    return false;
}

std::wstring signal_spec_to_trap_event(const std::wstring& raw_signal) {
    std::wstring normalized_signal = normalize_trap_event_name(raw_signal);
    if (normalized_signal == L"QUIT") {
        return L"BREAK";
    }
    return normalized_signal;
}

bool force_kill_process_by_pid(DWORD pid) {
    HANDLE process_handle = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (process_handle == nullptr) {
        return false;
    }

    const BOOL terminated = TerminateProcess(process_handle, 1);
    CloseHandle(process_handle);
    return terminated != FALSE;
}

bool graceful_kill_process_by_pid(DWORD pid) {
    KillWindowEnumData enum_data = { pid, false };
    EnumWindows(kill_window_enum_proc, reinterpret_cast<LPARAM>(&enum_data));
    if (!enum_data.window_found) {
        return force_kill_process_by_pid(pid);
    }
    return true;
}

bool send_console_event_to_pid(DWORD pid, DWORD control_event) {
    if (!AttachConsole(pid)) {
        return false;
    }

    SetConsoleCtrlHandler(nullptr, TRUE);
    const BOOL generated = GenerateConsoleCtrlEvent(control_event, 0);
    FreeConsole();
    SetConsoleCtrlHandler(nullptr, FALSE);
    return generated != FALSE;
}

bool queue_pending_trap_event(const std::wstring& event_name) {
    if (event_name == L"INT") {
        InterlockedExchange(&g_pending_int_trap, 1);
        return true;
    }
    if (event_name == L"BREAK") {
        InterlockedExchange(&g_pending_break_trap, 1);
        return true;
    }
    if (event_name == L"HUP") {
        InterlockedExchange(&g_pending_hup_trap, 1);
        return true;
    }
    if (event_name == L"ALRM") {
        InterlockedExchange(&g_pending_alrm_trap, 1);
        return true;
    }
    if (event_name == L"USR1") {
        InterlockedExchange(&g_pending_usr1_trap, 1);
        return true;
    }
    if (event_name == L"USR2") {
        InterlockedExchange(&g_pending_usr2_trap, 1);
        return true;
    }
    if (event_name == L"SEGV") {
        InterlockedExchange(&g_pending_segv_trap, 1);
        return true;
    }
    if (event_name == L"FPE") {
        InterlockedExchange(&g_pending_fpe_trap, 1);
        return true;
    }
    if (event_name == L"TERM") {
        InterlockedExchange(&g_pending_term_trap, 1);
        return true;
    }
    if (event_name == L"CHLD") {
        InterlockedExchange(&g_pending_chld_trap, 1);
        return true;
    }
    return false;
}
