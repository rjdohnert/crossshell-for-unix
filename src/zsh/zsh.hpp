#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Shell32.lib")
#include <conio.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <memory>
#include <cctype>
#include <ctime>
#include <regex>
#include <set>
#include <stack>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include <chrono>
#include <limits>
#include <climits>
#include <cerrno>
#include <io.h>
#include <fcntl.h>
#include <tlhelp32.h>

using namespace std;
namespace fs = std::filesystem;

using unique_handle = std::unique_ptr<void, void(*)(HANDLE)>;

inline void close_handle_if_valid(HANDLE h) {
    if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h);
}

inline unique_handle make_unique_handle(HANDLE h = nullptr) {
    if (h == INVALID_HANDLE_VALUE) h = nullptr;
    return unique_handle(h, close_handle_if_valid);
}

// ANSI COLOR CODES & VT100
const string COLOR_RESET     = "\033[0m";
const string COLOR_RED       = "\033[31m";
const string COLOR_GREEN     = "\033[32m";
const string COLOR_YELLOW    = "\033[33m";
const string COLOR_BLUE      = "\033[34m";
const string COLOR_MAGENTA   = "\033[35m";
const string COLOR_CYAN      = "\033[36m";
const string COLOR_WHITE     = "\033[37m";
const string COLOR_ORANGE    = "\033[38;5;208m";
const string COLOR_PINK      = "\033[38;5;205m";
const string COLOR_BR_GREEN  = "\033[92m";
const string COLOR_BR_BLUE   = "\033[94m";
const string COLOR_BR_YELLOW = "\033[93m";
const string COLOR_BR_CYAN   = "\033[96m";

const string ZSH_VERSION_STRING = "CrossShellZSH 3.7.16";

struct ProcessSubstSink {
    string path;
    string command;
};

enum class JobState { Running, Stopped, Done, Failed };

struct BackgroundJob {
    DWORD pid = 0;
    HANDLE hProcess = nullptr;
    HANDLE hJob = nullptr;
    vector<HANDLE> processes;
    vector<DWORD> pids;
    string command;
    unsigned long job_id = 0;
    JobState state = JobState::Running;
    DWORD exit_code = STILL_ACTIVE;
    DWORD launch_error = ERROR_SUCCESS;
};

struct RegistryValueMetadata {
    DWORD type = REG_NONE;
    vector<BYTE> data;
};

enum class CommandListConnector {
    Always,
    And,
    Or
};

struct SingleCmd {
    vector<string> args;
    vector<bool> glob_allowed;
    vector<pair<string, bool>> output_files;
    string input_file = "";
    string output_file = "";
    string error_file = "";
    string here_string = "";
    int dup_stdin_from = -1;
    int dup_stdout_from = -1;
    int dup_stderr_from = -1;
    bool close_stdin = false;
    bool close_stdout = false;
    bool close_stderr = false;
    bool append_out = false;
    bool append_err = false;
    map<int, string> extra_input_files;
    map<int, pair<string, bool>> extra_output_files;
    map<int, int> extra_fd_duplications;
    set<int> extra_closed_fds;
};

struct Pipeline {
    vector<SingleCmd> cmds;
    bool background = false;
    string error;
};

struct CommandListNode {
    CommandListConnector connector = CommandListConnector::Always;
    string source;
};

struct CommandListAst {
    vector<CommandListNode> nodes;
    string error;
};

struct ZshHelpTopicData {
    const char* name;
    const string* color;
    const char* title;
    const char* body;
};

// Cross-module forward declarations
class ZshEnvironment;
extern ZshEnvironment g_env;
extern bool g_login_shell;

string normalize_path_to_win(string p);
string normalize_path_to_unix(string p);
wstring string_to_wstring(const string& str);
string find_executable_in_path(const string& bin);
string current_shell_executable_path();
bool is_valid_env_var_name(const string& name);
bool contains_dangerous_alias_tokens(const string& value);
string canonicalize_option_name(const string& raw);
void apply_shell_option_token(const string& token, bool enable_default, map<string, bool>& options);
string create_temp_process_subst_path();
string quote_for_shell_path(const string& p);

int parse_and_execute(const string& line);
int execute_command_line(const string& line);
int execute_single_command(const string& line);
int execute_script(const string& filepath, bool trace = false, bool errexit = false);
string capture_command_output(const string& cmd);
bool match_wildcard(const string& pattern, const string& str);
string render_prompt();
string render_rprompt();
void enable_ansi_support();
void init_signal_handlers();
void process_pending_traps();
void fire_exit_trap();
int dispatch_command(vector<string> args);
