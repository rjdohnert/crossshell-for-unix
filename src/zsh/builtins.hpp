#pragma once

#include "zsh.hpp"

const std::vector<std::string>& zsh_builtin_command_names();
bool is_zsh_builtin_command(const std::string& name);
void print_comprehensive_help(const std::string& topic = "");

bool builtin_test_eval(const std::vector<std::string>& t);
int builtin_test(const std::vector<std::string>& args);
int builtin_double_bracket(const std::vector<std::string>& args);
int builtin_printf(const std::vector<std::string>& args);
int builtin_print(const std::vector<std::string>& args);
int builtin_cd(const std::vector<std::string>& args);
int builtin_echo(const std::vector<std::string>& args);
int builtin_read(const std::vector<std::string>& args);
int builtin_typeset(const std::vector<std::string>& args);
int builtin_jobs(const std::vector<std::string>& args);
int builtin_help(const std::vector<std::string>& args);

const char* job_state_name(JobState state);
bool resolve_job_index(const std::string& spec, size_t& index);

int dispatch_command(std::vector<std::string> args);
