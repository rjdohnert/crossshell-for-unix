#pragma once

#include "zsh.hpp"

struct HeredocSpec {
    string delim;
    bool strip_tabs = false;
    bool expand_body = true;
};

string trim_copy(const string& s);
vector<string> tokenize_words(const string& s);
Pipeline parse_pipeline(const string& line);

bool is_word_char(char c);
bool is_statement_boundary_before(const string& s, size_t pos);
size_t find_matching_done(const string& s, size_t start);
size_t find_matching_fi(const string& s, size_t start);
size_t find_top_level_keyword(const string& s, const string& word, size_t start = 0);
size_t find_top_level_word(const string& s, const string& word, size_t start = 0);
bool starts_with_word_trimmed(const string& s, const string& word);
string strip_optional_trailing_semicolon(string s);

vector<string> split_lines_preserve_empty(const string& s);
bool parse_heredoc_specs(const string& header, vector<HeredocSpec>& specs, string& rewritten_header);
bool has_unterminated_heredoc(const string& block);

CommandListAst parse_command_list_ast(const string& line);
