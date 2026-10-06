#pragma once

#include "zsh.hpp"

const vector<string>& zsh_builtin_command_names();
bool is_zsh_builtin_command(const string& name);
void print_comprehensive_help(const string& topic = "");

bool builtin_test_eval(const vector<string>& t);
int builtin_test(const vector<string>& args);
int builtin_double_bracket(const vector<string>& args);
int builtin_printf(const vector<string>& args);
int builtin_print(const vector<string>& args);
int builtin_cd(const vector<string>& args);
int builtin_echo(const vector<string>& args);
int builtin_read(const vector<string>& args);
int builtin_typeset(const vector<string>& args);
int builtin_jobs(const vector<string>& args);
int builtin_help(const vector<string>& args);

const char* job_state_name(JobState state);
bool resolve_job_index(const string& spec, size_t& index);

int dispatch_command(vector<string> args);
