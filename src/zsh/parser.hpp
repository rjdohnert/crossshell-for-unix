#pragma once

#include "zsh.hpp"

struct HeredocSpec {
    std::string delim;
    bool strip_tabs = false;
    bool expand_body = true;
};

std::string trim_copy(const std::string& s);
std::vector<std::string> tokenize_words(const std::string& s);
Pipeline parse_pipeline(const std::string& line);

bool is_word_char(char c);
bool is_statement_boundary_before(const std::string& s, size_t pos);
size_t find_matching_done(const std::string& s, size_t start);
size_t find_matching_fi(const std::string& s, size_t start);
size_t find_top_level_keyword(const std::string& s, const std::string& word, size_t start = 0);
size_t find_top_level_word(const std::string& s, const std::string& word, size_t start = 0);
bool starts_with_word_trimmed(const std::string& s, const std::string& word);
std::string strip_optional_trailing_semicolon(std::string s);

std::vector<std::string> split_lines_preserve_empty(const std::string& s);
bool parse_heredoc_specs(const std::string& header, std::vector<HeredocSpec>& specs, std::string& rewritten_header);
bool has_unterminated_heredoc(const std::string& block);

CommandListAst parse_command_list_ast(const std::string& line);
