#pragma once

#include "zsh.hpp"

struct FunctionLocalScope {
    std::map<std::string, std::string> vars_before;
    std::map<std::string, std::vector<std::string>> indexed_arrays_before;
    std::map<std::string, std::map<std::string, std::string>> assoc_arrays_before;
    std::set<std::string> integer_vars_before;
    std::set<std::string> readonly_vars_before;
    std::set<std::string> unique_arrays_before;
    std::set<std::string> local_names;
    bool local_options = false;
    std::map<std::string, bool> options_before;
};

extern std::vector<FunctionLocalScope> g_function_local_scopes;

void mark_function_local(const std::string& name);
void restore_function_locals(const FunctionLocalScope& scope);

bool split_zsh_registry_path(const std::string& property_path, HKEY& root, std::wstring& key_path, std::wstring& value_name);
bool read_zsh_registry_property(const std::string& property_path, std::string& value, RegistryValueMetadata& metadata);
bool write_zsh_registry_property(const std::string& property_path, const std::string& value);

std::vector<std::string> find_all_executables_in_path(const std::string& bin);

std::string win_quote_arg(const std::string& arg);
std::string cmd_escape_meta(const std::string& s);

class ZshEnvironment {
public:
    std::map<std::string, std::string> vars;
    std::map<std::string, RegistryValueMetadata> registry_metadata;
    std::map<std::string, std::vector<std::string>> indexed_arrays;      // typeset -a
    std::map<std::string, std::map<std::string, std::string>> assoc_arrays;   // typeset -A
    std::set<std::string> unique_arrays;
    std::set<std::string> integer_vars;
    std::map<std::string, std::string> aliases;
    std::map<std::string, bool> options;
    std::map<std::string, std::string> functions;
    std::set<std::string> readonly_vars;
    std::set<std::string> autoload_functions;
    std::map<std::string, std::string> completion_definitions;
    std::map<std::string, std::map<std::string, std::string>> keymaps;
    std::map<std::pair<std::string, std::string>, std::vector<std::string>> styles;
    std::map<std::string, std::string> widgets;
    std::set<std::string> loaded_modules;
    std::vector<std::string> positional_args;
    std::vector<std::string> dir_stack;
    std::vector<std::string> history;
    std::vector<BackgroundJob> jobs;
    
    std::set<std::string> disabled_builtins;
    std::set<std::string> disabled_aliases;
    std::set<std::string> disabled_functions;
    std::map<std::string, std::string> command_hash_table;
    std::map<std::string, std::string> named_directories;
    std::map<std::string, std::string> resource_limits;
    std::vector<std::pair<time_t, std::string>> scheduled_events;
    
    std::string home_dir;
    std::string zshrc_path;
    std::string history_path;
    std::string oldpwd;
    int    last_exit_code = 0;
    bool   prompt_dirty   = true;  // invalidated by cd; avoids per-keystroke Win32 API calls
    std::string prompt_cache;
    time_t start_time     = time(nullptr);

    ZshEnvironment();
    void load_history();
    void add_history(const std::string& cmd);
    std::string expand_vars(const std::string& input);
};
