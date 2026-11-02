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
extern std::map<std::string, std::string> g_heredoc_payloads;

HANDLE shell_fd_handle(int fd);
std::string capture_command_output(const std::string& cmd);
std::string materialize_heredoc_block(const std::string& block);

int execute_if_block(const std::string& block);
int execute_for_loop(const std::string& block);
int execute_cstyle_for_loop(const std::string& block);
int execute_case_block(const std::string& block);
int execute_condition_loop(const std::string& block, bool until_mode);
int execute_repeat_loop(const std::string& block);
int execute_select_loop(const std::string& block);
int execute_timed_command(const std::string& block);
int execute_always_block(const std::string& statement);
int execute_subshell_block(const std::string& statement);
int execute_mixed_pipeline(const Pipeline& pipeline);
int execute_compound_with_redirection(const std::string& compound_part, const std::string& redir_part);

int execute_statement_block_aware(const std::string& statement);
int execute_single_command(const std::string& line);
int execute_command_line(const std::string& line);
int parse_and_execute(const std::string& line);
int execute_script(const std::string& filepath, bool trace, bool errexit);
