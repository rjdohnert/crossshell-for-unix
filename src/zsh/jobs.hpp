#pragma once

#include "zsh.hpp"

struct CoprocessState {
    HANDLE process = nullptr;
    HANDLE input = nullptr;
    HANDLE output = nullptr;
    DWORD pid = 0;
};

extern CoprocessState g_coprocess;
void close_coprocess(bool terminate_if_running);

extern unsigned long g_next_job_id;
extern unsigned long g_current_job_id;
extern unsigned long g_previous_job_id;
void select_current_job(unsigned long job_id);

extern atomic<bool> g_sigtstp_pending;
extern atomic<bool> g_sigint_pending;
extern atomic<bool> g_sigwinch_pending;
BOOL WINAPI console_ctrl_handler(DWORD dwCtrlType);

void set_foreground_pids(const vector<DWORD>& pids);
void clear_foreground_pids();
void init_signal_handlers();
void process_pending_traps();
void fire_exit_trap();
bool suspend_active_foreground_processes();
void suspend_win32_process(DWORD pid);
bool resume_win32_process(DWORD pid);
void check_window_resize_event();

int execute_pipeline_native(const Pipeline& pl);
bool save_pipeline_shell_state(const string& path);
bool load_pipeline_shell_state(const string& path);
