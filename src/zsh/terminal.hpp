#pragma once

#include "zsh.hpp"

void enable_ansi_support();
string format_now_header_line();
string render_prompt();
string render_rprompt();
void repaint_line(const string& buf, size_t cursor_pos);
bool apply_interactive_typo_correction(const string& line, string& action_line, string& pending_edit_line);
string read_line_interactive(const string& initial_buffer = "");

size_t visible_length(const string& s);
vector<string> get_pathext_list();
int match_fuzzy_score(const string& query, const string& candidate);
vector<string> complete_command(const string& prefix);
vector<string> complete_registered_command(const string& command, const string& prefix);
bool execute_bound_widget(int key_code, string& buffer, size_t& cursor);
int damerau_levenshtein_distance(const string& s1_in, const string& s2_in);
string find_typo_correction(const string& token);
string replace_first_command_token(const string& line, const string& old_tok, const string& new_tok);
