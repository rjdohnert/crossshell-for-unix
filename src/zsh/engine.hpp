#pragma once

#include "zsh.hpp"

struct FunctionLocalScope {
    map<string, string> vars_before;
    map<string, vector<string>> indexed_arrays_before;
    map<string, map<string, string>> assoc_arrays_before;
    set<string> integer_vars_before;
    set<string> readonly_vars_before;
    set<string> unique_arrays_before;
    set<string> local_names;
    bool local_options = false;
    map<string, bool> options_before;
};

extern vector<FunctionLocalScope> g_function_local_scopes;

void mark_function_local(const string& name);
void restore_function_locals(const FunctionLocalScope& scope);

bool split_zsh_registry_path(const string& property_path, HKEY& root, wstring& key_path, wstring& value_name);
bool read_zsh_registry_property(const string& property_path, string& value, RegistryValueMetadata& metadata);
bool write_zsh_registry_property(const string& property_path, const string& value);

string win_quote_arg(const string& arg);
string cmd_escape_meta(const string& s);

class ZshEnvironment {
public:
    map<string, string> vars;
    map<string, RegistryValueMetadata> registry_metadata;
    map<string, vector<string>> indexed_arrays;      // typeset -a
    map<string, map<string, string>> assoc_arrays;   // typeset -A
    set<string> unique_arrays;
    set<string> integer_vars;
    map<string, string> aliases;
    map<string, bool> options;
    map<string, string> functions;
    set<string> readonly_vars;
    set<string> autoload_functions;
    map<string, string> completion_definitions;
    map<string, map<string, string>> keymaps;
    map<pair<string, string>, vector<string>> styles;
    map<string, string> widgets;
    set<string> loaded_modules;
    vector<string> positional_args;
    vector<string> dir_stack;
    vector<string> history;
    vector<BackgroundJob> jobs;
    
    string home_dir;
    string zshrc_path;
    string history_path;
    string oldpwd;
    int    last_exit_code = 0;
    bool   prompt_dirty   = true;  // invalidated by cd; avoids per-keystroke Win32 API calls
    string prompt_cache;
    time_t start_time     = time(nullptr);

    ZshEnvironment();
    void load_history();
    void add_history(const string& cmd);
    string expand_vars(const string& input);
};
