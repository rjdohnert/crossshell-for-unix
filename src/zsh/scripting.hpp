#pragma once

#include "zsh.hpp"

extern bool g_script_returning;
extern int g_script_return_code;
extern int g_loop_depth;
extern int g_loop_breaking;
extern int g_loop_continuing;

struct LoopGuard {
    LoopGuard();
    ~LoopGuard();
};

extern int g_subshell_depth;
extern bool g_subshell_exiting;
extern int g_subshell_exit_code;
extern const int kMaxExecRecursionDepth;
extern map<string, string> g_heredoc_payloads;

HANDLE shell_fd_handle(int fd);
string capture_command_output(const string& cmd);
string materialize_heredoc_block(const string& block);

int execute_if_block(const string& block);
int execute_for_loop(const string& block);
int execute_cstyle_for_loop(const string& block);
int execute_case_block(const string& block);
int execute_condition_loop(const string& block, bool until_mode);
int execute_repeat_loop(const string& block);
int execute_select_loop(const string& block);
int execute_timed_command(const string& block);
int execute_always_block(const string& statement);
int execute_subshell_block(const string& statement);
int execute_mixed_pipeline(const Pipeline& pipeline);
int execute_compound_with_redirection(const string& compound_part, const string& redir_part);

int execute_statement_block_aware(const string& statement);
int execute_single_command(const string& line);
int execute_command_line(const string& line);
int parse_and_execute(const string& line);
int execute_script(const string& filepath, bool trace, bool errexit);
