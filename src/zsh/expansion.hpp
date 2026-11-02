#pragma once

#include "zsh.hpp"

struct GlobQualifiers {
    bool has_qualifier = false;
    bool regular_only = false;     // .
    bool directory_only = false;   // /
    bool symlink_only = false;     // @
    bool hidden_only = false;      // H or h
    bool executable_only = false;  // *
    bool order_mtime_desc = false; // om
    bool order_mtime_asc = false;  // Om
    bool order_size_desc = false;  // ol
    bool order_size_asc = false;   // Ol
    bool order_name_desc = false;  // On
    bool order_name_asc = false;   // on
    bool null_glob = false;        // N
    int mtime_mode = 0;            // 0: none, -1: m-N (< N days), 1: m+N (> N days), 2: mN (== N days)
    double mtime_days = 0.0;
    size_t select_index = 0;       // 1-based [N]
    size_t select_end = 0;         // 1-based [start,end]
};

struct DirEntryInfo {
    std::string name;
    DWORD attributes = 0;
    std::uintmax_t size = 0;
    FILETIME mtime = {0, 0};
    bool is_directory = false;
    bool is_symlink = false;
    bool is_hidden = false;
    bool is_regular = false;
    bool is_executable = false;
    double age_days = 0.0;
};

long long eval_math_expr(const std::string& expr);
int eval_arithmetic_command(const std::string& expr);
bool match_wildcard(const std::string& pattern, const std::string& str);
std::vector<std::string> expand_globs(const std::vector<std::string>& args, const std::vector<bool>* glob_allowed = nullptr);
std::vector<std::string> expand_brace_word(const std::string& word);
std::vector<std::string> split_ifs_words(const std::string& value, const std::string& separators);

extern std::vector<std::string> g_process_subst_temp_files;
extern std::vector<ProcessSubstSink> g_process_subst_sinks;
extern int g_process_subst_eval_depth;
