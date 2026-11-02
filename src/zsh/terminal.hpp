#pragma once

#include "zsh.hpp"

void enable_ansi_support();
std::string format_now_header_line();
std::string render_prompt();
std::string render_rprompt();
void repaint_line(const std::string& buf, size_t cursor_pos);
bool apply_interactive_typo_correction(const std::string& line, std::string& action_line, std::string& pending_edit_line);
std::string read_line_interactive(const std::string& initial_buffer = "");

size_t visible_length(const std::string& s);
std::vector<std::string> get_pathext_list();
int match_fuzzy_score(const std::string& query, const std::string& candidate);
std::vector<std::string> complete_command(const std::string& prefix);
std::vector<std::string> complete_registered_command(const std::string& command, const std::string& prefix);
bool execute_bound_widget(int key_code, std::string& buffer, size_t& cursor);
int damerau_levenshtein_distance(const std::string& s1_in, const std::string& s2_in);
std::string find_typo_correction(const std::string& token);
std::string replace_first_command_token(const std::string& line, const std::string& old_tok, const std::string& new_tok);
