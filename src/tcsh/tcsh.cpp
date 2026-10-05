/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * CrossShellTCSH - Standardized Section Index
 * ===========================================
 * 01. Platform Defines, Includes, and Linker Pragmas
 * 02. Global State and Console Signal Forwarding
 * 03. Core Utility Helpers (encoding, wildcard, diagnostics)
 * 04. OS/Environment Discovery Helpers
 * 05. Parsing, Expansion, and Tokenization
 * 06. Built-in Commands and Dispatch
 * 07. Process Launch, Redirection, and Pipelines
 * 08. Prompt Rendering and Interactive Line Editor
 * 09. History, Completion, and Shell State Management
 * 10. Script/REPL Execution Flow
 * 11. Program Entry Point
 *
 * Section Header Convention:
 *   // SECTION NN: <area summary>
 *   // SECTION NNX: <sub-area summary>
 *
 * Maintenance Contract (Single-File Mode):
 *   1) Keep this section index aligned with actual major blocks.
 *   2) Prefer table-driven metadata (builtins/help/completions) over duplicated literals.
 *   3) Add regression coverage for behavior changes before merge.
 *   4) Keep helper direction one-way: utilities -> parse/expand -> execute -> startup.
 */

#include <windows.h>
#include <shlobj.h>
#include <conio.h>
#include <set>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <map>
#include <algorithm>
#include <memory>
#include <cctype>
#include <chrono>
#include <ctime>
#include <cerrno>
#include <cstdlib>
#include <atomic>
#include <climits>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

// --- Thread-Safe Global Interrupt Flag for Win32 Ctrl+C Handler ---
static std::atomic<bool> g_interrupted{false};
static std::atomic<DWORD> g_foregroundProcessGroupId{0};
static std::atomic<bool> g_ctrlForwarding{false};

static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) {
    if (dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_BREAK_EVENT) {
        DWORD processGroupId = g_foregroundProcessGroupId.load(std::memory_order_relaxed);
        if (processGroupId != 0 && !g_ctrlForwarding.exchange(true, std::memory_order_relaxed)) {
            GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, processGroupId);
            g_ctrlForwarding.store(false, std::memory_order_relaxed);
        }
        g_interrupted.store(true, std::memory_order_relaxed);
        return TRUE;
    }
    return FALSE;
}

// --- Helper Functions for Unicode Conversion and ANSI Console Initialization ---
static void enable_ansi_support() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE && hOut != NULL) {
        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode)) {
            SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
    HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
    if (hErr != INVALID_HANDLE_VALUE && hErr != NULL) {
        DWORD mode = 0;
        if (GetConsoleMode(hErr, &mode)) {
            SetConsoleMode(hErr, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
}

static std::wstring string_to_wstring(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], static_cast<int>(str.size()), NULL, 0);
    if (size_needed <= 0) {
        size_needed = MultiByteToWideChar(CP_ACP, 0, &str[0], static_cast<int>(str.size()), NULL, 0);
        if (size_needed <= 0) return L"";
        std::wstring wstrTo(size_needed, 0);
        MultiByteToWideChar(CP_ACP, 0, &str[0], static_cast<int>(str.size()), &wstrTo[0], size_needed);
        return wstrTo;
    }
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], static_cast<int>(str.size()), &wstrTo[0], size_needed);
    return wstrTo;
}

static std::string wstring_to_string(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()), NULL, 0, NULL, NULL);
    if (size_needed <= 0) {
        size_needed = WideCharToMultiByte(CP_ACP, 0, &wstr[0], static_cast<int>(wstr.size()), NULL, 0, NULL, NULL);
        if (size_needed <= 0) return "";
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_ACP, 0, &wstr[0], static_cast<int>(wstr.size()), &strTo[0], size_needed, NULL, NULL);
        return strTo;
    }
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

static bool wildcard_match(const std::string& pattern, const std::string& text) {
    size_t p = 0, t = 0;
    size_t starP = std::string::npos, starT = std::string::npos;
    while (t < text.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == text[t])) {
            p++; t++;
        } else if (p < pattern.size() && pattern[p] == '*') {
            starP = p++;
            starT = t;
        } else if (starP != std::string::npos) {
            p = starP + 1;
            t = ++starT;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*') p++;
    return p == pattern.size();
}

static void print_error_message(const std::string& message) {
    HANDLE hError = GetStdHandle(STD_ERROR_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    bool hasConsole = hError != INVALID_HANDLE_VALUE && hError != NULL &&
        GetConsoleScreenBufferInfo(hError, &csbi);

    if (hasConsole) {
        SetConsoleTextAttribute(hError, FOREGROUND_RED | FOREGROUND_INTENSITY);
    }

    std::cerr << message;
    std::cerr.flush();

    if (hasConsole) {
        SetConsoleTextAttribute(hError, csbi.wAttributes);
    }
}

struct TcshHelpEntry {
    const char* name;
    const char* description;
};

static const std::vector<TcshHelpEntry>& tcsh_help_entries() {
    static const std::vector<TcshHelpEntry> help_entries = {
        {":", "Null command; always succeeds."},
        {".", "Read and execute commands from a script file in the current shell."},
        {"@", "Evaluate a simple arithmetic assignment or expression."},
        {"alias", "Create or list command aliases."},
        {"bg", "Resume a stopped/background job."},
        {"builtin", "Force execution of a shell builtin."},
        {"builtins", "List the builtin command set."},
        {"cd", "Change the current directory. / and \\ are both accepted."},
        {"chdir", "Change the current directory. / and \\ are both accepted."},
        {"command", "Run a command without alias expansion."},
        {"complete", "Create or list completion rules."},
        {"continue", "Continue the innermost loop in a script."},
        {"dirs", "Print the directory stack."},
        {"disown", "Remove jobs from the job table without killing them."},
        {"echo", "Print arguments to standard output."},
        {"echotc", "Echo terminal capabilities or ANSI control codes."},
        {"export", "Export variables to the process environment."},
        {"fg", "Bring a background job to the foreground."},
        {"hash", "Show or refresh command lookup information."},
        {"history", "Print the command history buffer."},
        {"jobs", "List tracked background jobs."},
        {"kill", "Terminate a process by PID or job reference."},
        {"let", "Evaluate arithmetic and return success when non-zero."},
        {"ls-F", "List directory contents with file type indicators (/ for dir, * for exe, @ for link)."},
        {"onintr", "Control shell interrupt handling (- to ignore, label to jump)."},
        {"print", "Print arguments to standard output."},
        {"printenv", "Print shell variables and environment values."},
        {"pwd", "Print the current working directory."},
        {"pushd", "Push the current directory and change to another one. / and \\ are both accepted."},
        {"read", "Read a line from standard input into variables."},
        {"rehash", "Enable or refresh command lookup caching."},
        {"repeat", "Run a command repeatedly."},
        {"return", "Stop execution of the current sourced script."},
        {"select", "Prompt for a menu choice and store the selection."},
        {"set", "Assign or list shell variables."},
        {"setenv", "Set an environment variable."},
        {"shift", "Shift positional arguments left."},
        {"source", "Read and execute commands from a script file."},
        {"test", "Evaluate a conditional expression and set status."},
        {"trap", "Install, list, or clear trap handlers."},
        {"typeset", "Declare or assign shell variables."},
        {"unalias", "Remove an alias."},
        {"uncomplete", "Remove a completion rule."},
        {"unset", "Remove a shell variable."},
        {"unsetenv", "Remove an environment variable."},
        {"whence", "Show how a command name resolves."},
        {"where", "Locate a command on disk."},
        {"which", "Locate a command on disk."}
    };
    return help_entries;
}

static const std::vector<std::string>& tcsh_builtin_names() {
    static const std::vector<std::string> names = []() {
        std::vector<std::string> out;
        const std::vector<TcshHelpEntry>& help_entries = tcsh_help_entries();
        out.reserve(help_entries.size() + 64);

        for (const auto& entry : help_entries) {
            out.push_back(entry.name);
        }

        static const std::vector<const char*> supplemental_names = {
            "alloc", "bindkey", "break", "breaksw", "case", "clear", "cls", "default",
            "echotc", "else", "end", "endif", "endsw", "eval", "exec", "filetest",
            "foreach", "glob", "goto", "hashstat", "hup", "if", "limit", "log", "login",
            "ls-F", "migrate", "newgrp", "nice", "nohup", "notify", "onintr", "popd",
            "sched", "settc", "setty", "stop", "suspend", "switch", "telltc", "termname",
            "time", "umask", "unhash", "universe", "unlimit", "ver", "wait", "while"
        };

        for (const char* name : supplemental_names) {
            if (std::find(out.begin(), out.end(), name) == out.end()) {
                out.push_back(name);
            }
        }

        return out;
    }();

    return names;
}

// --- Dynamic Windows OS Release Query ---
static std::wstring get_registry_string(HKEY root, const wchar_t* subkey, const wchar_t* value_name) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, subkey, 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS) {
        return L"";
    }

    DWORD type = 0;
    DWORD size = 0;
    std::wstring value;
    if (RegQueryValueExW(key, value_name, nullptr, &type, nullptr, &size) == ERROR_SUCCESS &&
        (type == REG_SZ || type == REG_EXPAND_SZ) && size >= sizeof(wchar_t)) {
        value.resize(size / sizeof(wchar_t));
        if (RegQueryValueExW(key, value_name, nullptr, nullptr,
            reinterpret_cast<LPBYTE>(&value[0]), &size) == ERROR_SUCCESS) {
            while (!value.empty() && value.back() == L'\0') {
                value.pop_back();
            }
        } else {
            value.clear();
        }
    }

    RegCloseKey(key);
    return value;
}

std::wstring get_windows_release_text() {
    std::wstring product_name = get_registry_string(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
        L"ProductName");
    std::wstring display_version = get_registry_string(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
        L"DisplayVersion");

    typedef LONG(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    RtlGetVersionPtr rtl_get_version = (ntdll != nullptr)
        ? reinterpret_cast<RtlGetVersionPtr>(GetProcAddress(ntdll, "RtlGetVersion"))
        : nullptr;

    RTL_OSVERSIONINFOEXW os = {};
    os.dwOSVersionInfoSize = sizeof(os);

    wchar_t version_text[96] = L"unknown";
    if (rtl_get_version != nullptr && rtl_get_version(reinterpret_cast<PRTL_OSVERSIONINFOW>(&os)) == 0) {
        _snwprintf_s(version_text, _countof(version_text), _TRUNCATE,
            L"%lu.%lu.%lu", os.dwMajorVersion, os.dwMinorVersion, os.dwBuildNumber);
    }

    std::wstring release = product_name.empty() ? L"Windows" : product_name;
    release += L" (";
    release += version_text;
    if (!display_version.empty()) {
        release += L", ";
        release += display_version;
    }
    release += L")";
    return release;
}

// --- RAII Wrapper for Windows Handles ---
class ScopedHandle {
private:
    HANDLE h = NULL;
public:
    ScopedHandle(HANDLE h = NULL) : h(h) {}
    ~ScopedHandle() {
        if (h && h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
        }
    }
    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;
    ScopedHandle(ScopedHandle&& other) noexcept : h(other.h) { other.h = NULL; }
    ScopedHandle& operator=(ScopedHandle&& other) noexcept {
        if (this != &other) {
            reset(other.h);
            other.h = NULL;
        }
        return *this;
    }
    HANDLE get() const { return h; }
    HANDLE detach() {
        HANDLE temp = h;
        h = NULL;
        return temp;
    }
    void reset(HANDLE newH = NULL) {
        if (h && h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
        }
        h = newH;
    }
    operator HANDLE() const { return h; }
    bool isValid() const { return h != NULL && h != INVALID_HANDLE_VALUE; }
};

// --- Job Control Data Structure ---
struct Job {
    DWORD id;
    HANDLE hJob;
    HANDLE hProcess;
    std::string command;
    bool isRunning;
};

struct ScheduledTask {
    std::string timeStr;
    std::string command;
};

enum class ParsedTokenKind {
    Word,
    Pipe,
    PipeStderr,
    Background,
    RedirectInput,
    RedirectOutput,
    RedirectAppend,
    RedirectOutputStderr,
    RedirectAppendStderr
};

struct ParsedToken {
    ParsedTokenKind kind;
    std::string text;
};

struct ParsedCommand {
    struct Redirection {
        ParsedTokenKind kind;
        std::string target;
    };

    std::vector<std::string> args;
    std::vector<Redirection> redirections;
    std::string sourceText;
};

struct ParsedPipeline {
    std::vector<ParsedCommand> commands;
    bool background = false;
    bool pipeStderr = false;
};

// --- CrossShellTCSH Shell Engine Class ---
class TcshEngine {
private:
    std::string homeDir;
    std::string historyFile;
    std::string rcFile;
    std::vector<std::string> history;
    size_t historyIndex = 0;
    
    // TCSH Variables as Word Vectors (Lists/Arrays)
    std::map<std::string, std::vector<std::string>> variables;
    std::map<std::string, std::string> aliases;
    std::map<std::string, std::string> completions;
    std::map<std::string, std::string> keyBindings;
    std::map<std::string, std::string> trapHandlers;
    std::vector<std::string> positionalArgs;
    std::string scriptName;
    std::vector<std::string> dirStack;
    std::vector<Job> jobList;
    std::vector<ScheduledTask> scheduledTasks;
    std::string logFile;
    DWORD nextJobId = 1;
    bool running = true;
    bool hashEnabled = true;
    bool notifyJobs = true;
    // Tab completion state — reset on any non-Tab keypress
    std::vector<std::string> compCandidates;
    size_t   compIdx     = 0;
    std::string compStem;
    size_t   compStemPos = 0;
    bool     compActive  = false;
    std::string promptStr = "% ";
    size_t lastRedrawLineLength = 0;

    static constexpr size_t kMaxHistoryEntries = 1000;
    static constexpr size_t kMaxJobCount = 128;
    static constexpr size_t kMaxScriptDepth = 64;
    static constexpr size_t kMaxScriptLineLength = 8192;
    static constexpr size_t kMaxScriptLines = 20000;
    size_t scriptDepth = 0;

    enum class ScriptDirective {
        None,
        Break,
        Continue,
        Return
    };

    ScriptDirective scriptDirective = ScriptDirective::None;
    bool executingScript = false;

    struct ScriptFrameGuard {
        explicit ScriptFrameGuard(TcshEngine* shell) : shell(shell) {
            if (shell && shell->scriptDepth < shell->kMaxScriptDepth) {
                ++shell->scriptDepth;
            } else {
                exceeded = true;
            }
        }

        ~ScriptFrameGuard() {
            if (shell && !exceeded) {
                --shell->scriptDepth;
            }
        }

        bool ok() const { return !exceeded; }

    private:
        TcshEngine* shell;
        bool exceeded = false;
    };

    const std::vector<std::string>& builtins;

    std::string getHomeDirectory();
    void initFiles();
    void loadHistory();
    void saveHistory();
    void loadRcFile();
    void generateDefaultRcFile();
    std::string stripInlineComment(const std::string& line) const;
    std::string resolveExecutable(const std::string& inputCmd, bool& foundOnDisk);
    std::vector<std::string> tokenize(const std::string& str, char delim = ' ') const;
    bool lexCommandLine(const std::string& line, std::vector<ParsedToken>& tokens) const;
    bool parseCommandLine(const std::string& line, ParsedPipeline& pipeline) const;
    std::string joinTokens(const std::vector<std::string>& tokens) const;
    bool isBuiltinCommand(const std::string& command) const;
    std::string expandLeadingAlias(const std::string& line, const std::vector<std::string>& args) const;
    bool tryParseNonNegativeInt(const std::string& text, int& value, const std::string& commandName, const std::string& argumentName) const;
    bool shouldFallbackToCmd(const std::string& command) const;
    std::string joinPositionalArgs() const;
    void setVariableList(const std::string& key, const std::vector<std::string>& val);
    std::string getVariableString(const std::string& key) const;
    bool isVariableSet(const std::string& token) const;
    void setScriptArguments(const std::string& filename, const std::vector<std::string>& args);
    bool shiftPositionalArguments(size_t count);
    std::string applyModifier(const std::string& str, char mod) const;
    std::string expandVariables(const std::string& str);
    std::string expandHistory(const std::string& line);
    std::vector<std::string> splitCommandSequence(const std::string& line, std::vector<std::string>& operators) const;
    void executeSingleCommandLine(std::string line);
    std::string formatPrompt(const std::string& pattern) const;
    std::vector<std::string> expandGlobs(const std::vector<std::string>& args) const;
    void evaluateArithmetic(const std::string& expr);
    void updateJobs();
    std::string escapeArg(const std::string& arg);
    bool evalCondition(const std::string& expr);
    void executeScriptLines(const std::vector<std::string>& lines);

    void redrawLine(HANDLE hConsole, COORD& startPos, const std::string& prompt, const std::string& buffer, size_t cursorIndex);

public:
    explicit TcshEngine(bool loadRc = true);
    ~TcshEngine();

    void run();
    bool runScript(const std::string& filename, const std::vector<std::string>& args = {});
    int executeCommandString(const std::string& command, const std::string& name = "", const std::vector<std::string>& args = {});
    void executeCommandLine(std::string line);
    bool executeBuiltin(const std::vector<std::string>& args);
    void executePipeline(const ParsedPipeline& pipeline);
    void displayHelp();
    void displayVersion();
    std::string readLineWithEditing();
    void handleTabCompletion(std::string& currentBuffer, size_t& cursorIndex);
    bool listJobs(const std::vector<std::string>& args);
    void bringJobToForeground(DWORD jobId);
    void sendJobToBackground(DWORD jobId);
    std::string stripOuterQuotes(const std::string& str) const;
    std::string normalizePathSeparators(const std::string& path) const;
    std::string getVar(const std::string& key) const { return getVariableString(key); }
    int getStatus() const { return std::atoi(getVariableString("status").c_str()); }
};

// --- Implementation ---

std::string TcshEngine::stripOuterQuotes(const std::string& str) const {
    if (str.size() >= 2 && ((str.front() == '"' && str.back() == '"') || (str.front() == '\'' && str.back() == '\''))) {
        return str.substr(1, str.size() - 2);
    }
    return str;
}

std::string TcshEngine::normalizePathSeparators(const std::string& path) const {
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '/', '\\');
    return normalized;
}

std::string TcshEngine::stripInlineComment(const std::string& line) const {
    std::string result;
    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    bool escaped = false;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (escaped) {
            result.push_back(c);
            escaped = false;
            continue;
        }

        if (c == '\\' && !inSingleQuote) {
            result.push_back(c);
            escaped = true;
            continue;
        }

        if (c == '"' && !inSingleQuote) {
            inDoubleQuote = !inDoubleQuote;
            result.push_back(c);
            continue;
        }

        if (c == '\'' && !inDoubleQuote) {
            inSingleQuote = !inSingleQuote;
            result.push_back(c);
            continue;
        }

        if (!inSingleQuote && !inDoubleQuote && c == '#' && (i == 0 || std::isspace(static_cast<unsigned char>(line[i - 1])))) {
            break;
        }

        result.push_back(c);
    }

    return result;
}

bool TcshEngine::lexCommandLine(const std::string& line, std::vector<ParsedToken>& tokens) const {
    tokens.clear();
    std::string current;
    bool inDoubleQuotes = false;
    bool inSingleQuotes = false;

    for (size_t index = 0; index < line.size(); ++index) {
        char c = line[index];
        if (c == '"' && !inSingleQuotes) {
            inDoubleQuotes = !inDoubleQuotes;
            current += c;
            continue;
        }
        if (c == '\'' && !inDoubleQuotes) {
            inSingleQuotes = !inSingleQuotes;
            current += c;
            continue;
        }

        if (!inDoubleQuotes && !inSingleQuotes) {
            if (std::isspace(static_cast<unsigned char>(c))) {
                if (!current.empty()) {
                    tokens.push_back({ ParsedTokenKind::Word, current });
                    current.clear();
                }
                continue;
            }
            if (c == '|') {
                if (!current.empty()) {
                    tokens.push_back({ ParsedTokenKind::Word, current });
                    current.clear();
                }
                if (index + 1 < line.size() && line[index + 1] == '&') {
                    tokens.push_back({ ParsedTokenKind::PipeStderr, "|&" });
                    ++index;
                } else {
                    tokens.push_back({ ParsedTokenKind::Pipe, "|" });
                }
                continue;
            }
            if (c == '<') {
                if (!current.empty()) {
                    tokens.push_back({ ParsedTokenKind::Word, current });
                    current.clear();
                }
                tokens.push_back({ ParsedTokenKind::RedirectInput, "<" });
                continue;
            }
            if (c == '>') {
                if (!current.empty()) {
                    tokens.push_back({ ParsedTokenKind::Word, current });
                    current.clear();
                }
                if (index + 1 < line.size() && line[index + 1] == '>') {
                    if (index + 2 < line.size() && line[index + 2] == '&') {
                        tokens.push_back({ ParsedTokenKind::RedirectAppendStderr, ">>&" });
                        index += 2;
                    } else {
                        tokens.push_back({ ParsedTokenKind::RedirectAppend, ">>" });
                        index += 1;
                    }
                } else if (index + 1 < line.size() && line[index + 1] == '&') {
                    tokens.push_back({ ParsedTokenKind::RedirectOutputStderr, ">&" });
                    index += 1;
                } else {
                    tokens.push_back({ ParsedTokenKind::RedirectOutput, ">" });
                }
                continue;
            }
            if (c == '&') {
                if (!current.empty()) {
                    tokens.push_back({ ParsedTokenKind::Word, current });
                    current.clear();
                }
                tokens.push_back({ ParsedTokenKind::Background, "&" });
                continue;
            }
        }
        current += c;
    }

    if (inDoubleQuotes || inSingleQuotes) {
        print_error_message("tcsh: Unterminated quote.\n");
        return false;
    }

    if (!current.empty()) {
        tokens.push_back({ ParsedTokenKind::Word, current });
    }

    return true;
}

std::string TcshEngine::joinTokens(const std::vector<std::string>& tokens) const {
    std::string joined;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (i > 0) joined += " ";
        joined += tokens[i];
    }
    return joined;
}

bool TcshEngine::parseCommandLine(const std::string& line, ParsedPipeline& pipeline) const {
    pipeline = ParsedPipeline{};
    std::vector<ParsedToken> tokens;
    if (!lexCommandLine(line, tokens)) return false;
    if (tokens.empty()) return true;

    ParsedCommand current;
    for (size_t i = 0; i < tokens.size(); ++i) {
        const ParsedToken& token = tokens[i];

        if (token.kind == ParsedTokenKind::Word) {
            current.args.push_back(token.text);
            continue;
        }

        if (token.kind == ParsedTokenKind::RedirectInput ||
            token.kind == ParsedTokenKind::RedirectOutput ||
            token.kind == ParsedTokenKind::RedirectAppend ||
            token.kind == ParsedTokenKind::RedirectOutputStderr ||
            token.kind == ParsedTokenKind::RedirectAppendStderr) {
            if (i + 1 >= tokens.size() || tokens[i + 1].kind != ParsedTokenKind::Word) {
                print_error_message("tcsh: Missing name for redirect.\n");
                return false;
            }
            current.redirections.push_back({ token.kind, tokens[i + 1].text });
            ++i;
            continue;
        }

        if (token.kind == ParsedTokenKind::Pipe || token.kind == ParsedTokenKind::PipeStderr) {
            if (current.args.empty()) {
                print_error_message("tcsh: Invalid null command.\n");
                return false;
            }
            if (token.kind == ParsedTokenKind::PipeStderr) pipeline.pipeStderr = true;
            current.sourceText = joinTokens(current.args);
            pipeline.commands.push_back(current);
            current = ParsedCommand{};
            continue;
        }

        if (token.kind == ParsedTokenKind::Background) {
            if (i != tokens.size() - 1) {
                print_error_message("tcsh: '&' must appear at end of command line.\n");
                return false;
            }
            pipeline.background = true;
        }
    }

    if (!current.args.empty()) {
        current.sourceText = joinTokens(current.args);
        pipeline.commands.push_back(current);
    } else if (!tokens.empty() && (tokens.back().kind == ParsedTokenKind::Pipe || tokens.back().kind == ParsedTokenKind::PipeStderr)) {
        print_error_message("tcsh: Invalid null command.\n");
        return false;
    }

    return true;
}

bool TcshEngine::isBuiltinCommand(const std::string& command) const {
    return std::find(builtins.begin(), builtins.end(), command) != builtins.end();
}

std::string TcshEngine::expandLeadingAlias(const std::string& line, const std::vector<std::string>& origArgs) const {
    std::vector<std::string> tokens = tokenize(line, ' ');
    if (tokens.empty()) return line;

    auto it = aliases.find(tokens[0]);
    if (it == aliases.end()) return line;

    std::string aliasBody = it->second;

    if (aliasBody.find("\\!") != std::string::npos) {
        std::string result = aliasBody;
        std::string allArgs = "";
        for (size_t i = 1; i < origArgs.size(); ++i) {
            if (i > 1) allArgs += " ";
            allArgs += origArgs[i];
        }
        
        size_t pos;
        while ((pos = result.find("\\!*")) != std::string::npos) result.replace(pos, 3, allArgs);
        while ((pos = result.find("\\!^")) != std::string::npos) result.replace(pos, 3, origArgs.size() > 1 ? origArgs[1] : "");
        while ((pos = result.find("\\!$")) != std::string::npos) result.replace(pos, 3, origArgs.size() > 1 ? origArgs.back() : "");
        
        for (size_t i = 1; i < origArgs.size(); ++i) {
            std::string tag = "\\!:" + std::to_string(i);
            while ((pos = result.find(tag)) != std::string::npos) result.replace(pos, tag.length(), origArgs[i]);
        }
        return result;
    }

    std::string expanded = aliasBody;
    for (size_t i = 1; i < tokens.size(); ++i) {
        expanded += " " + tokens[i];
    }
    return expanded;
}

bool TcshEngine::tryParseNonNegativeInt(const std::string& text, int& value, const std::string& commandName, const std::string& argumentName) const {
    if (text.empty()) {
        print_error_message(commandName + ": missing " + argumentName + "\n");
        return false;
    }

    char* end = nullptr;
    errno = 0;
    long parsed = std::strtol(text.c_str(), &end, 10);
    if (errno == ERANGE || end == text.c_str() || *end != '\0' || parsed < 0 || parsed > INT_MAX) {
        print_error_message(commandName + ": invalid " + argumentName + ": " + text + "\n");
        return false;
    }

    value = static_cast<int>(parsed);
    return true;
}

bool TcshEngine::shouldFallbackToCmd(const std::string& command) const {
    static const std::vector<std::string> cmdBuiltins = {
        "assoc", "break", "call", "cd", "chdir", "cls", "color", "copy", "date", "del",
        "dir", "echo", "endlocal", "erase", "exit", "for", "ftype", "goto", "if", "md",
        "mkdir", "mklink", "move", "path", "pause", "popd", "prompt", "pushd", "rd", "rem",
        "ren", "rename", "rmdir", "set", "setlocal", "shift", "start", "time", "title", "type",
        "ver", "verify", "vol"
    };

    std::string lowered = command;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    return std::find(cmdBuiltins.begin(), cmdBuiltins.end(), lowered) != cmdBuiltins.end();
}

std::string TcshEngine::joinPositionalArgs() const {
    std::string joined;
    for (size_t i = 0; i < positionalArgs.size(); ++i) {
        if (i > 0) joined += " ";
        joined += positionalArgs[i];
    }
    return joined;
}

void TcshEngine::setVariableList(const std::string& key, const std::vector<std::string>& val) {
    variables[key] = val;
    if (key == "prompt" && !val.empty()) promptStr = val[0];

    // Synchronize 'path' array with Windows 'PATH' environment variable
    if (key == "path") {
        std::string pathEnv = "";
        for (size_t i = 0; i < val.size(); ++i) {
            if (i > 0) pathEnv += ";";
            pathEnv += val[i];
        }
        SetEnvironmentVariableW(L"PATH", string_to_wstring(pathEnv).c_str());
    }
}

std::string TcshEngine::getVariableString(const std::string& key) const {
    auto it = variables.find(key);
    if (it != variables.end()) {
        std::string res;
        for (size_t i = 0; i < it->second.size(); ++i) {
            if (i > 0) res += " ";
            res += it->second[i];
        }
        return res;
    }
    return "";
}

bool TcshEngine::isVariableSet(const std::string& token) const {
    if (token.empty()) return false;
    if (token == "argv" || token == "#argv" || token == "0") return !scriptName.empty();
    if (variables.count(token) > 0) return true;

    std::wstring wToken = string_to_wstring(token);
    SetLastError(ERROR_SUCCESS);
    DWORD envLen = GetEnvironmentVariableW(wToken.c_str(), nullptr, 0);
    return (envLen > 0 || GetLastError() != ERROR_ENVVAR_NOT_FOUND);
}

void TcshEngine::setScriptArguments(const std::string& filename, const std::vector<std::string>& args) {
    scriptName = filename;
    positionalArgs = args;
}

bool TcshEngine::shiftPositionalArguments(size_t count) {
    if (count > positionalArgs.size()) return false;
    positionalArgs.erase(positionalArgs.begin(), positionalArgs.begin() + static_cast<std::ptrdiff_t>(count));
    return true;
}

std::string TcshEngine::applyModifier(const std::string& str, char mod) const {
    if (str.empty()) return str;
    if (mod == 'h') { // Head
        size_t p = str.find_last_of("/\\");
        return (p != std::string::npos) ? str.substr(0, p) : "";
    }
    if (mod == 't') { // Tail
        size_t p = str.find_last_of("/\\");
        return (p != std::string::npos) ? str.substr(p + 1) : str;
    }
    if (mod == 'r') { // Root
        size_t p = str.find_last_of('.');
        return (p != std::string::npos) ? str.substr(0, p) : str;
    }
    if (mod == 'e') { // Extension
        size_t p = str.find_last_of('.');
        return (p != std::string::npos) ? str.substr(p + 1) : "";
    }
    if (mod == 'u') { // Upper
        std::string res = str;
        std::transform(res.begin(), res.end(), res.begin(), [](unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
        });
        return res;
    }
    if (mod == 'l') { // Lower
        std::string res = str;
        std::transform(res.begin(), res.end(), res.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return res;
    }
    return str;
}

std::string TcshEngine::expandVariables(const std::string& str) {
    std::string res;
    bool inSingleQuote = false;
    bool inDoubleQuote = false;

    for (size_t i = 0; i < str.length(); ++i) {
        char c = str[i];
        if (c == '\'' && !inDoubleQuote) {
            inSingleQuote = !inSingleQuote;
            res += c;
        } else if (c == '"' && !inSingleQuote) {
            inDoubleQuote = !inDoubleQuote;
            res += c;
        } else if (c == '~' && !inSingleQuote && (i == 0 || std::isspace(static_cast<unsigned char>(str[i - 1])))) {
            if (i + 1 == str.length() || str[i + 1] == '/' || str[i + 1] == '\\') {
                res += homeDir;
            } else {
                res += c;
            }
        } else if (c == '$' && !inSingleQuote) {
            size_t varStart = i + 1;
            bool checkIsSet = false;
            bool checkCount = false;
            bool braceMode = false;

            if (varStart < str.length() && str[varStart] == '?') {
                checkIsSet = true;
                varStart++;
            } else if (varStart < str.length() && str[varStart] == '#') {
                checkCount = true;
                varStart++;
            } else if (varStart < str.length() && str[varStart] == '{') {
                braceMode = true;
                varStart++;
            }

            size_t varLen = 0;
            size_t braceEnd = std::string::npos;
            if (braceMode) {
                braceEnd = str.find('}', varStart);
                if (braceEnd != std::string::npos) {
                    varLen = braceEnd - varStart;
                }
            } else {
                while (varStart + varLen < str.length() &&
                      (std::isalnum(static_cast<unsigned char>(str[varStart + varLen])) || str[varStart + varLen] == '_')) {
                    varLen++;
                }
            }

            int index = -1;
            if (!braceMode && varStart + varLen < str.length() && str[varStart + varLen] == '[') {
                size_t closeBracket = str.find(']', varStart + varLen + 1);
                if (closeBracket != std::string::npos) {
                    std::string idxStr = str.substr(varStart + varLen + 1, closeBracket - (varStart + varLen + 1));
                    index = std::atoi(idxStr.c_str());
                    varLen = (closeBracket - varStart) + 1;
                }
            }

            std::string varName = str.substr(varStart, varLen);
            std::vector<char> modifiers;
            size_t modScan = varStart + varLen;
            while (modScan < str.length() && str[modScan] == ':') {
                if (modScan + 1 < str.length()) {
                    modifiers.push_back(str[modScan + 1]);
                    modScan += 2;
                } else break;
            }

            if (!varName.empty()) {
                std::string valStr;
                if (checkIsSet) {
                    valStr = isVariableSet(varName) ? "1" : "0";
                } else if (checkCount) {
                    if (variables.count(varName)) valStr = std::to_string(variables[varName].size());
                    else if (varName == "argv") valStr = std::to_string(positionalArgs.size());
                    else valStr = "0";
                } else {
                    if (variables.count(varName)) {
                        const auto& list = variables[varName];
                        if (index > 0 && static_cast<size_t>(index) <= list.size()) valStr = list[index - 1];
                        else valStr = getVariableString(varName);
                    } else if (varName == "argv") {
                        if (index > 0 && static_cast<size_t>(index) <= positionalArgs.size()) valStr = positionalArgs[index - 1];
                        else valStr = joinPositionalArgs();
                    } else {
                        std::wstring wVarName = string_to_wstring(varName);
                        DWORD dwRet = GetEnvironmentVariableW(wVarName.c_str(), nullptr, 0);
                        if (dwRet > 0) {
                            std::vector<wchar_t> envBuf(dwRet);
                            if (GetEnvironmentVariableW(wVarName.c_str(), envBuf.data(), dwRet) > 0) {
                                valStr = wstring_to_string(envBuf.data());
                            }
                        }
                    }
                }

                for (char mod : modifiers) valStr = applyModifier(valStr, mod);
                res += valStr;
                if (braceMode && braceEnd != std::string::npos) {
                    i = braceEnd;
                } else {
                    i = modScan - 1;
                }
            } else {
                res += '$';
            }
        } else {
            res += c;
        }
    }
    return res;
}

std::vector<std::string> TcshEngine::splitCommandSequence(const std::string& line, std::vector<std::string>& operators) const {
    std::vector<std::string> commands;
    operators.clear();

    std::string current;
    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    bool escaped = false;

    auto pushCurrent = [&]() {
        std::string trimmed;
        auto first = std::find_if(current.begin(), current.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        });
        if (first != current.end()) {
            trimmed = current;
            while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back()))) {
                trimmed.pop_back();
            }
            if (!trimmed.empty()) {
                commands.push_back(trimmed);
            }
        }
        current.clear();
    };

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (escaped) {
            current.push_back(c);
            escaped = false;
            continue;
        }

        if (c == '\\' && !inSingleQuote) {
            current.push_back(c);
            escaped = true;
            continue;
        }

        if (c == '"' && !inSingleQuote) {
            inDoubleQuote = !inDoubleQuote;
            current.push_back(c);
            continue;
        }

        if (c == '\'' && !inDoubleQuote) {
            inSingleQuote = !inSingleQuote;
            current.push_back(c);
            continue;
        }

        if (!inSingleQuote && !inDoubleQuote) {
            if (c == ';') {
                pushCurrent();
                operators.push_back(";");
                continue;
            }
            if (c == '&' && i + 1 < line.size() && line[i + 1] == '&') {
                pushCurrent();
                operators.push_back("&&");
                ++i;
                continue;
            }
            if (c == '|' && i + 1 < line.size() && line[i + 1] == '|') {
                pushCurrent();
                operators.push_back("||");
                ++i;
                continue;
            }
        }

        current.push_back(c);
    }

    pushCurrent();
    return commands;
}

std::string TcshEngine::expandHistory(const std::string& line) {
    if (line.empty() || line[0] != '!') return line;
    if (history.empty()) return line;

    if (line == "!!" || line == "!*") return history.back();
    if (line == "!$") {
        std::vector<std::string> tokens = tokenize(history.back(), ' ');
        return tokens.empty() ? "" : tokens.back();
    }
    if (line.size() > 1 && std::isdigit(line[1])) {
        int idx = std::atoi(line.substr(1).c_str());
        if (idx > 0 && static_cast<size_t>(idx) <= history.size()) return history[idx - 1];
    }
    return line;
}

std::string TcshEngine::formatPrompt(const std::string& pattern) const {
    std::string res = pattern;

    wchar_t currentDirW[MAX_PATH] = { 0 };
    GetCurrentDirectoryW(MAX_PATH, currentDirW);
    std::string cwd = wstring_to_string(currentDirW);

    std::string cwdTilde = cwd;
    if (cwd.find(homeDir) == 0) {
        cwdTilde = "~" + cwd.substr(homeDir.length());
    }

    wchar_t userW[256] = { 0 };
    DWORD userLen = 256;
    GetUserNameW(userW, &userLen);
    std::string username = wstring_to_string(userW);

    wchar_t userDomainW[256] = { 0 };
    std::string userDomain;
    DWORD domainLen = GetEnvironmentVariableW(L"USERDNSDOMAIN", userDomainW, ARRAYSIZE(userDomainW));
    if (domainLen > 0 && domainLen < ARRAYSIZE(userDomainW)) {
        userDomain = wstring_to_string(userDomainW);
    } else {
        domainLen = GetEnvironmentVariableW(L"USERDOMAIN", userDomainW, ARRAYSIZE(userDomainW));
        if (domainLen > 0 && domainLen < ARRAYSIZE(userDomainW)) {
            userDomain = wstring_to_string(userDomainW);
        }
    }

    wchar_t hostW[256] = { 0 };
    DWORD hostLen = 256;
    GetComputerNameW(hostW, &hostLen);
    std::string fullHostname = wstring_to_string(hostW);
    std::string shortHostname = fullHostname;
    size_t dotPos = shortHostname.find('.');
    if (dotPos != std::string::npos) {
        shortHostname = shortHostname.substr(0, dotPos);
    }

    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    std::string promptChar = isAdmin ? "#" : "%";

    if (res == "> " || res == "# " || res == "% ") {
        return promptChar + " ";
    }

    std::string statusStr = "0";
    auto itStatus = variables.find("status");
    if (itStatus != variables.end() && !itStatus->second.empty()) {
        statusStr = itStatus->second[0];
    }

    size_t pos;
    while ((pos = res.find("%~")) != std::string::npos) res.replace(pos, 2, cwdTilde);
    while ((pos = res.find("%/")) != std::string::npos) res.replace(pos, 2, cwd);
    while ((pos = res.find("%c")) != std::string::npos) {
        size_t p = cwd.find_last_of("/\\");
        std::string tail = (p != std::string::npos) ? cwd.substr(p + 1) : cwd;
        res.replace(pos, 2, tail);
    }
    while ((pos = res.find("%n")) != std::string::npos) res.replace(pos, 2, username);
    while ((pos = res.find("%d")) != std::string::npos) res.replace(pos, 2, userDomain);
    while ((pos = res.find("%m")) != std::string::npos) res.replace(pos, 2, shortHostname);
    while ((pos = res.find("%M")) != std::string::npos) res.replace(pos, 2, fullHostname);
    while ((pos = res.find("%#")) != std::string::npos) res.replace(pos, 2, promptChar);
    while ((pos = res.find("%?")) != std::string::npos) res.replace(pos, 2, statusStr);
    while ((pos = res.find("%%")) != std::string::npos) res.replace(pos, 2, "%");

    return res;
}

std::vector<std::string> TcshEngine::expandGlobs(const std::vector<std::string>& args) const {
    std::vector<std::string> expanded;
    for (const auto& arg : args) {
        if (arg.find('*') != std::string::npos || arg.find('?') != std::string::npos) {
            std::wstring wPattern = string_to_wstring(arg);
            WIN32_FIND_DATAW fd;
            HANDLE hFind = FindFirstFileW(wPattern.c_str(), &fd);
            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    expanded.push_back(wstring_to_string(fd.cFileName));
                } while (FindNextFileW(hFind, &fd));
                FindClose(hFind);
            } else {
                expanded.push_back(arg);
            }
        } else {
            expanded.push_back(arg);
        }
    }
    return expanded;
}

void TcshEngine::evaluateArithmetic(const std::string& expr) {
    std::vector<std::string> tokens = tokenize(expr, ' ');
    if (tokens.size() < 3 || tokens[1] != "=") {
        return;
    }

    std::string varName = stripOuterQuotes(tokens[0]);
    std::string rhs = expr;
    size_t eqPos = rhs.find('=');
    if (eqPos == std::string::npos) {
        return;
    }
    rhs = rhs.substr(eqPos + 1);

    auto trimInPlace = [](std::string& value) {
        value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char ch) { return !std::isspace(ch); }));
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
            value.pop_back();
        }
    };

    auto resolveNumericToken = [&](const std::string& token, long long& outValue) -> bool {
        std::string cleaned = stripOuterQuotes(token);
        if (cleaned.empty()) return false;

        auto varIt = variables.find(cleaned);
        if (varIt != variables.end() && !varIt->second.empty()) {
            cleaned = stripOuterQuotes(varIt->second[0]);
        }

        char* endPtr = nullptr;
        errno = 0;
        long long parsed = std::strtoll(cleaned.c_str(), &endPtr, 10);
        if (errno != 0 || endPtr == cleaned.c_str() || (endPtr != nullptr && *endPtr != '\0')) {
            return false;
        }
        outValue = parsed;
        return true;
    };

    auto precedence = [](char op) -> int {
        if (op == '+' || op == '-') return 1;
        if (op == '*' || op == '/' || op == '%') return 2;
        return 0;
    };

    auto applyOp = [](long long lhs, long long rhsValue, char op, long long& outValue) -> bool {
        switch (op) {
            case '+': outValue = lhs + rhsValue; return true;
            case '-': outValue = lhs - rhsValue; return true;
            case '*': outValue = lhs * rhsValue; return true;
            case '/':
                if (rhsValue == 0) return false;
                outValue = lhs / rhsValue;
                return true;
            case '%':
                if (rhsValue == 0) return false;
                outValue = lhs % rhsValue;
                return true;
            default:
                return false;
        }
    };

    trimInPlace(rhs);
    std::vector<std::string> outputQueue;
    std::vector<char> opStack;

    auto popOperators = [&](char untilOp) {
        while (!opStack.empty() && opStack.back() != untilOp) {
            outputQueue.push_back(std::string(1, opStack.back()));
            opStack.pop_back();
        }
    };

    for (size_t i = 0; i < rhs.size();) {
        char c = rhs[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            ++i;
            continue;
        }

        bool unaryMinus = false;
        if (c == '-') {
            size_t j = i;
            while (j > 0 && std::isspace(static_cast<unsigned char>(rhs[j - 1]))) {
                --j;
            }
            if (j == 0 || rhs[j - 1] == '(' || rhs[j - 1] == '+' || rhs[j - 1] == '-' || rhs[j - 1] == '*' || rhs[j - 1] == '/' || rhs[j - 1] == '%') {
                unaryMinus = true;
            }
        }

        if (std::isdigit(static_cast<unsigned char>(c)) || unaryMinus) {
            size_t start = i;
            if (unaryMinus) {
                ++i;
            }
            while (i < rhs.size() && std::isdigit(static_cast<unsigned char>(rhs[i]))) {
                ++i;
            }
            if (i == start + (unaryMinus ? 1 : 0)) {
                return;
            }
            outputQueue.push_back(rhs.substr(start, i - start));
            continue;
        }

        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t start = i;
            ++i;
            while (i < rhs.size() && (std::isalnum(static_cast<unsigned char>(rhs[i])) || rhs[i] == '_')) {
                ++i;
            }
            outputQueue.push_back(rhs.substr(start, i - start));
            continue;
        }

        if (c == '(') {
            opStack.push_back(c);
            ++i;
            continue;
        }

        if (c == ')') {
            popOperators('(');
            if (opStack.empty() || opStack.back() != '(') {
                return;
            }
            opStack.pop_back();
            ++i;
            continue;
        }

        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%') {
            while (!opStack.empty() && opStack.back() != '(' && precedence(opStack.back()) >= precedence(c)) {
                outputQueue.push_back(std::string(1, opStack.back()));
                opStack.pop_back();
            }
            opStack.push_back(c);
            ++i;
            continue;
        }

        return;
    }

    while (!opStack.empty()) {
        if (opStack.back() == '(' || opStack.back() == ')') {
            return;
        }
        outputQueue.push_back(std::string(1, opStack.back()));
        opStack.pop_back();
    }

    std::vector<long long> valueStack;
    for (const auto& token : outputQueue) {
        if (token.size() == 1 && (token[0] == '+' || token[0] == '-' || token[0] == '*' || token[0] == '/' || token[0] == '%')) {
            if (valueStack.size() < 2) {
                return;
            }
            long long rhsValue = valueStack.back();
            valueStack.pop_back();
            long long lhs = valueStack.back();
            valueStack.pop_back();
            long long result = 0;
            if (!applyOp(lhs, rhsValue, token[0], result)) {
                return;
            }
            valueStack.push_back(result);
            continue;
        }

        long long value = 0;
        if (!resolveNumericToken(token, value)) {
            return;
        }
        valueStack.push_back(value);
    }

    if (valueStack.size() == 1) {
        setVariableList(varName, { std::to_string(valueStack.back()) });
    }
}

bool TcshEngine::evalCondition(const std::string& expr) {
    std::string cleanExpr = expr;
    cleanExpr.erase(cleanExpr.begin(), std::find_if(cleanExpr.begin(), cleanExpr.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    while (!cleanExpr.empty() && std::isspace(static_cast<unsigned char>(cleanExpr.back()))) {
        cleanExpr.pop_back();
    }

    // Outer paren stripping
    if (cleanExpr.size() >= 2 && cleanExpr.front() == '(' && cleanExpr.back() == ')') {
        int parenDepth = 0;
        bool matchesOuter = true;
        for (size_t i = 0; i < cleanExpr.size(); ++i) {
            if (cleanExpr[i] == '(') parenDepth++;
            else if (cleanExpr[i] == ')') parenDepth--;
            if (parenDepth == 0 && i < cleanExpr.size() - 1) {
                matchesOuter = false;
                break;
            }
        }
        if (matchesOuter) {
            return evalCondition(cleanExpr.substr(1, cleanExpr.size() - 2));
        }
    }

    // Quote and paren aware search for logical operators || and &&
    bool inDoubleQuote = false;
    bool inSingleQuote = false;
    int depth = 0;

    for (size_t i = 0; i < cleanExpr.size(); ++i) {
        char c = cleanExpr[i];
        if (c == '"' && !inSingleQuote) inDoubleQuote = !inDoubleQuote;
        else if (c == '\'' && !inDoubleQuote) inSingleQuote = !inSingleQuote;
        else if (!inDoubleQuote && !inSingleQuote) {
            if (c == '(') depth++;
            else if (c == ')') depth--;
            else if (depth == 0) {
                if (i + 1 < cleanExpr.size() && cleanExpr[i] == '|' && cleanExpr[i + 1] == '|') {
                    return evalCondition(cleanExpr.substr(0, i)) || evalCondition(cleanExpr.substr(i + 2));
                }
            }
        }
    }

    inDoubleQuote = false;
    inSingleQuote = false;
    depth = 0;
    for (size_t i = 0; i < cleanExpr.size(); ++i) {
        char c = cleanExpr[i];
        if (c == '"' && !inSingleQuote) inDoubleQuote = !inDoubleQuote;
        else if (c == '\'' && !inDoubleQuote) inSingleQuote = !inSingleQuote;
        else if (!inDoubleQuote && !inSingleQuote) {
            if (c == '(') depth++;
            else if (c == ')') depth--;
            else if (depth == 0) {
                if (i + 1 < cleanExpr.size() && cleanExpr[i] == '&' && cleanExpr[i + 1] == '&') {
                    return evalCondition(cleanExpr.substr(0, i)) && evalCondition(cleanExpr.substr(i + 2));
                }
            }
        }
    }

    std::vector<std::string> tokens = tokenize(cleanExpr, ' ');
    if (tokens.empty()) return false;

    if (tokens.size() >= 2 && tokens[0][0] == '-') {
        std::string op = tokens[0];
        std::string target = stripOuterQuotes(tokens[1]);
        std::wstring wTarget = string_to_wstring(target);
        DWORD attr = GetFileAttributesW(wTarget.c_str());

        if (op == "-e") return (attr != INVALID_FILE_ATTRIBUTES);
        if (op == "-d") return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
        if (op == "-f") return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
        if (op == "-r") return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_READONLY));
        if (op == "-w") return (attr != INVALID_FILE_ATTRIBUTES);
        if (op == "-x") {
            if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) return false;
            std::string lowerTarget = target;
            std::transform(lowerTarget.begin(), lowerTarget.end(), lowerTarget.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            return (lowerTarget.find(".exe") != std::string::npos || lowerTarget.find(".bat") != std::string::npos ||
                    lowerTarget.find(".cmd") != std::string::npos || lowerTarget.find(".com") != std::string::npos ||
                    lowerTarget.find(".ps1") != std::string::npos);
        }
        if (op == "-z") {
            WIN32_FILE_ATTRIBUTE_DATA data;
            if (GetFileAttributesExW(wTarget.c_str(), GetFileExInfoStandard, &data)) {
                return (data.nFileSizeHigh == 0 && data.nFileSizeLow == 0);
            }
            return false;
        }
    }

    if (tokens.size() >= 3) {
        std::string left = stripOuterQuotes(tokens[0]);
        std::string op = tokens[1];
        std::string right = stripOuterQuotes(tokens[2]);

        if (op == "==") return left == right;
        if (op == "!=") return left != right;
        if (op == "=~") return wildcard_match(right, left);
        if (op == "!~") return !wildcard_match(right, left);
        if (op == ">") return std::atoll(left.c_str()) > std::atoll(right.c_str());
        if (op == "<") return std::atoll(left.c_str()) < std::atoll(right.c_str());
        if (op == ">=") return std::atoll(left.c_str()) >= std::atoll(right.c_str());
        if (op == "<=") return std::atoll(left.c_str()) <= std::atoll(right.c_str());
    }

    return !stripOuterQuotes(tokens[0]).empty() && tokens[0] != "0";
}

void TcshEngine::updateJobs() {
    for (auto& job : jobList) {
        if (job.isRunning && job.hProcess && job.hProcess != INVALID_HANDLE_VALUE) {
            DWORD code = 0;
            if (GetExitCodeProcess(job.hProcess, &code) && code != STILL_ACTIVE) {
                job.isRunning = false;
                if (notifyJobs) {
                    std::cout << "\n[" << job.id << "] Done\t" << job.command << "\n";
                }
                if (job.hProcess) { CloseHandle(job.hProcess); job.hProcess = NULL; }
                if (job.hJob) { CloseHandle(job.hJob); job.hJob = NULL; }
            }
        }
    }

    // Reclaim finished slots to avoid exhausting the fixed job table with stale entries.
    jobList.erase(std::remove_if(jobList.begin(), jobList.end(), [](const Job& job) {
        return !job.isRunning;
    }), jobList.end());
}

std::string TcshEngine::escapeArg(const std::string& arg) {
    if (arg.empty()) return "\"\"";
    bool needsQuotes = false;
    for (char c : arg) {
        if (std::isspace(static_cast<unsigned char>(c)) || c == '"' || c == '\\' || c == '&' || c == '|' || c == '<' || c == '>') {
            needsQuotes = true;
            break;
        }
    }
    if (!needsQuotes) return arg;
    std::string escaped = "\"";
    for (size_t i = 0; i < arg.length(); ++i) {
        char c = arg[i];
        if (c == '\\') {
            size_t backslashes = 0;
            while (i < arg.length() && arg[i] == '\\') { backslashes++; i++; }
            if (i == arg.length()) escaped.append(backslashes * 2, '\\');
            else if (arg[i] == '"') { escaped.append(backslashes * 2 + 1, '\\'); escaped += '"'; }
            else { escaped.append(backslashes, '\\'); escaped += arg[i]; }
        } else if (c == '"') escaped += "\\\"";
        else escaped += c;
    }
    escaped += "\"";
    return escaped;
}

std::string TcshEngine::getHomeDirectory() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, path))) {
        return wstring_to_string(path);
    }
    wchar_t userProfile[MAX_PATH];
    DWORD len = GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH);
    if (len > 0 && len < MAX_PATH) return wstring_to_string(userProfile);
    return "C:\\";
}

TcshEngine::TcshEngine(bool loadRc) : builtins(tcsh_builtin_names()) {
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    // Enable VT100 / ANSI Escape Sequences for Console Colors
    enable_ansi_support();

    homeDir = getHomeDirectory();
    historyFile = homeDir + "\\.tcsh_history";
    logFile = homeDir + "\\.tcsh_log";
    rcFile = homeDir + "\\.tcshrc";

    setVariableList("prompt", { "% " });
    setVariableList("version", { "CrossShellTCSH 6.24.00" });
    setVariableList("status", { "0" });

    initFiles();
    loadHistory();
    if (loadRc) {
        loadRcFile();
    }
}

int TcshEngine::executeCommandString(const std::string& command, const std::string& name, const std::vector<std::string>& args) {
    std::string previousScriptName = scriptName;
    std::vector<std::string> previousPositionalArgs = positionalArgs;
    bool previousExecutingScript = executingScript;

    setScriptArguments(name.empty() ? "tcsh" : name, args);
    executingScript = true;

    executeCommandLine(command);

    int exitCode = std::atoi(getVariableString("status").c_str());

    scriptName = previousScriptName;
    positionalArgs = previousPositionalArgs;
    executingScript = previousExecutingScript;

    return exitCode;
}

TcshEngine::~TcshEngine() {
    saveHistory();
    for (auto& job : jobList) {
        if (job.hProcess && job.hProcess != INVALID_HANDLE_VALUE) CloseHandle(job.hProcess);
        if (job.hJob && job.hJob != INVALID_HANDLE_VALUE) CloseHandle(job.hJob);
    }
}

void TcshEngine::initFiles() {
    std::ifstream checkRc(rcFile);
    if (!checkRc.good()) {
        std::string altRc = homeDir + "\\.cshrc";
        std::ifstream checkAlt(altRc);
        if (checkAlt.good()) {
            rcFile = altRc;
        } else {
            generateDefaultRcFile();
        }
    }

    std::ifstream checkHist(historyFile);
    if (!checkHist.good()) std::ofstream createHist(historyFile);
}

void TcshEngine::generateDefaultRcFile() {
    std::ofstream out(rcFile);
    if (out.is_open()) {
        out << "# tcsh Configuration File Auto-Generated\n";
        out << "# Default prompt: classic tcsh style ('% ' for user, '# ' for admin).\n";
        out << "# Add %~ or %n@%m if you prefer path/host context in your prompt.\n";
        out << "set prompt = \"% \"\n";
        out << "set history = 1000\n";
        out << "complete cd 'p/1/d/'\n";
        out.close();
        std::cout << "[tcsh] Auto-generated config file in: " << rcFile << "\n";
    }
}

void TcshEngine::loadHistory() {
    std::ifstream in(historyFile);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) history.push_back(line);
        if (history.size() >= kMaxHistoryEntries) break;
    }
    historyIndex = history.size();
}

void TcshEngine::saveHistory() {
    std::ofstream out(historyFile);
    size_t start = history.size() > kMaxHistoryEntries ? history.size() - kMaxHistoryEntries : 0;
    for (size_t i = start; i < history.size(); ++i) out << history[i] << "\n";
}

void TcshEngine::loadRcFile() {
    std::ifstream in(rcFile);
    std::string line;
    while (std::getline(in, line)) {
        std::string trimmed = line;
        trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
        while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back()))) {
            trimmed.pop_back();
        }

        if (trimmed == "set prompt = >") {
            line = "set prompt = \"% \"";
        }

        if (!line.empty() && line[0] != '#') executeCommandLine(line);
    }
}

std::string TcshEngine::resolveExecutable(const std::string& inputCmd, bool& foundOnDisk) {
    foundOnDisk = false;
    std::wstring wCmd = string_to_wstring(inputCmd);
    DWORD attr = GetFileAttributesW(wCmd.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        foundOnDisk = true;
        return inputCmd;
    }

    if (inputCmd.find('\\') != std::string::npos || inputCmd.find('/') != std::string::npos) {
        const std::vector<std::string> exts = { ".exe", ".cmd", ".bat", ".com" };
        for (const auto& ext : exts) {
            std::string testPath = inputCmd + ext;
            std::wstring wTestPath = string_to_wstring(testPath);
            attr = GetFileAttributesW(wTestPath.c_str());
            if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
                foundOnDisk = true;
                return testPath;
            }
        }
    }

    wchar_t szPath[MAX_PATH];
    LPWSTR lpFilePart;
    if (SearchPathW(NULL, wCmd.c_str(), L".exe", MAX_PATH, szPath, &lpFilePart) > 0) { foundOnDisk = true; return wstring_to_string(szPath); }
    if (SearchPathW(NULL, wCmd.c_str(), L".cmd", MAX_PATH, szPath, &lpFilePart) > 0) { foundOnDisk = true; return wstring_to_string(szPath); }
    if (SearchPathW(NULL, wCmd.c_str(), L".bat", MAX_PATH, szPath, &lpFilePart) > 0) { foundOnDisk = true; return wstring_to_string(szPath); }
    if (SearchPathW(NULL, wCmd.c_str(), L".com", MAX_PATH, szPath, &lpFilePart) > 0) { foundOnDisk = true; return wstring_to_string(szPath); }

    foundOnDisk = false;
    return inputCmd;
}

std::vector<std::string> TcshEngine::tokenize(const std::string& str, char delim) const {
    std::vector<std::string> tokens;
    std::string current;
    bool inDoubleQuotes = false;
    bool inSingleQuotes = false;
    for (size_t i = 0; i < str.length(); ++i) {
        char c = str[i];
        if (c == '"' && !inSingleQuotes) {
            inDoubleQuotes = !inDoubleQuotes;
            current += c;
        } else if (c == '\'' && !inDoubleQuotes) {
            inSingleQuotes = !inSingleQuotes;
            current += c;
        } else if (c == delim && !inDoubleQuotes && !inSingleQuotes) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

void TcshEngine::displayVersion() {
    std::cout << "\nCrossShellTCSH 6.24.00\n";
    std::cout << "CrossShellTCSH tcsh-compatible shell\n";
    std::cout << "Copyright (C) 2026, Roberto J. Dohnert.\n";
    std::cout << "Licensed under the BSD-3-Clause license.\n\n";
    std::cout << "System: " << wstring_to_string(get_windows_release_text()) << "\n\n";
}

void TcshEngine::displayHelp() {
    const std::vector<TcshHelpEntry>& helpEntries = tcsh_help_entries();

    std::cout << "\nCrossShellTCSH                General Help\n";
    std::cout << "---------------------------------------------\n";
    for (const auto& entry : helpEntries) {
        std::cout << std::left << std::setw(12) << entry.name << " - " << entry.description << "\n";
    }
    std::cout << "\n---------------------------------------------\n";
    std::cout << "Job Control        : jobs, fg, bg, stop, kill, wait, disown, &\n";
    std::cout << "Directory Navigation: cd, chdir, pushd, popd, dirs (accept / or \\)\n";
    std::cout << "Variables          : set, unset, export, typeset, printenv, read\n";
    std::cout << "Execution Control   : command, builtin, whence, hash, test, let, return, select, trap\n";
    std::cout << "Text and Output     : echo, print, repeat, time\n";
    std::cout << "Prompt Format      : %n user, %d domain, %m host, %M fullhost, %~ cwd, %/ fullcwd, %# role, %? status\n";
    std::cout << "Prompt How-To       : put 'set prompt = \"...\"' in ~/.tcshrc (or ~/.cshrc)\n";
    std::cout << "                     Default style: set prompt = \"% \"\n";
    std::cout << "                     Remove identity: set prompt = \"%~ %# \"\n";
    std::cout << "                     Prompt char only: set prompt = \"%# \"\n";
    std::cout << "                     Include host: set prompt = \"%n@%m %~ %# \"\n";
    std::cout << "---------------------------------------------\n\n";
}

void TcshEngine::handleTabCompletion(std::string& currentBuffer, size_t& cursorIndex) {
    // Find the token being completed.
    size_t stem_start = cursorIndex;
    while (stem_start > 0 && !isspace((unsigned char)currentBuffer[stem_start-1])) --stem_start;
    std::string stem = currentBuffer.substr(stem_start, cursorIndex - stem_start);

    auto str_low = [](std::string s) -> std::string {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)tolower(c); });
        return s;
    };

    auto common_pfx = [](const std::vector<std::string>& v) -> std::string {
        if (v.empty()) return "";
        std::string p = v[0];
        for (size_t i = 1; i < v.size(); ++i) {
            size_t j = 0;
            while (j < p.size() && j < v[i].size() && tolower((unsigned char)p[j]) == tolower((unsigned char)v[i][j])) ++j;
            p = p.substr(0, j);
        }
        return p;
    };

    if (!compActive) {
        // Determine context: first token = command, otherwise = path.
        bool is_cmd = (stem_start == 0) ||
                      currentBuffer.substr(0, stem_start).find_first_not_of(" \t") == std::string::npos;
        bool has_sep = stem.find_first_of("/\\") != std::string::npos;

        std::vector<std::string> cands;
        std::set<std::string> seen;

        if (is_cmd && !has_sep) {
            // Command completion: builtins + aliases + PATH executables.
            std::string p_low = str_low(stem);
            for (const auto& b : builtins) {
                if (str_low(b).substr(0, p_low.size()) == p_low && !seen.count(b))
                    { cands.push_back(b); seen.insert(b); }
            }
            for (const auto& [k,v] : aliases) {
                if (str_low(k).substr(0, p_low.size()) == p_low && !seen.count(k))
                    { cands.push_back(k); seen.insert(k); }
            }
            char* pe = getenv("PATH");
            if (pe) {
                std::istringstream ss(pe); std::string dir;
                while (std::getline(ss, dir, ';')) {
                    WIN32_FIND_DATAW fd;
                    std::wstring pat = string_to_wstring(dir + "\\" + stem + "*");
                    HANDLE hf = FindFirstFileW(pat.c_str(), &fd);
                    if (hf == INVALID_HANDLE_VALUE) continue;
                    do {
                        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                        std::string name = wstring_to_string(fd.cFileName);
                        std::string base = name;
                        if (base.size() > 4) {
                            std::string ext = str_low(base.substr(base.size()-4));
                            if (ext==".exe"||ext==".com"||ext==".bat"||ext==".cmd")
                                base = base.substr(0, base.size()-4);
                        }
                        if (!seen.count(base)) { cands.push_back(base); seen.insert(base); }
                    } while (FindNextFileW(hf, &fd));
                    FindClose(hf);
                }
            }
            if (cands.empty()) is_cmd = false; // fall through to path
        }

        if (!is_cmd || has_sep) {
            // Path completion.
            std::string dir_part, file_part;
            size_t sep = stem.find_last_of("/\\");
            if (sep == std::string::npos) { dir_part = ".\\*"; file_part = stem; }
            else { dir_part = stem.substr(0, sep+1) + "*"; file_part = stem.substr(sep+1); }
            std::string search = (sep == std::string::npos) ? stem + "*" : stem.substr(0, sep+1) + file_part + "*";
            WIN32_FIND_DATAW fd;
            HANDLE hf = FindFirstFileW(string_to_wstring(search).c_str(), &fd);
            if (hf != INVALID_HANDLE_VALUE) {
                do {
                    std::wstring wn(fd.cFileName);
                    if (wn == L"." || wn == L"..") continue;
                    std::string match = wstring_to_string(fd.cFileName);
                    std::string full = (sep == std::string::npos) ? match : stem.substr(0, sep+1) + match;
                    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) full += "/";
                    if (!seen.count(full)) { cands.push_back(full); seen.insert(full); }
                } while (FindNextFileW(hf, &fd));
                FindClose(hf);
            }
        }

        std::sort(cands.begin(), cands.end());
        compCandidates = cands;
        compStem     = stem;
        compStemPos  = stem_start;
        compIdx      = 0;

        if (cands.empty()) { putchar('\a'); fflush(stdout); return; }

        std::string cp = common_pfx(cands);
        currentBuffer.replace(stem_start, cursorIndex - stem_start, cp);
        cursorIndex = stem_start + cp.size();

        if (cands.size() == 1) {
            if (!currentBuffer.empty() && currentBuffer.back() != '/' && currentBuffer.back() != '\\')
                { currentBuffer.insert(cursorIndex, 1, ' '); cursorIndex++; }
            compActive = false;
        } else {
            compActive = true;
            compStem   = cp;
            // Print candidates below.
            std::string list = "\n";
            for (size_t i = 0; i < cands.size(); ++i)
                list += cands[i] + (i+1 < cands.size() ? "  " : "\n");
            fwrite(list.data(), 1, list.size(), stdout);
            fflush(stdout);
        }
    } else {
        // Cycle through candidates.
        const std::string& cand = compCandidates[compIdx];
        currentBuffer.replace(compStemPos, cursorIndex - compStemPos, cand);
        cursorIndex = compStemPos + cand.size();
        compIdx = (compIdx + 1) % compCandidates.size();
    }
}

void TcshEngine::redrawLine(HANDLE hConsole, COORD& startPos, const std::string& prompt, const std::string& buffer, size_t cursorIndex) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) return;
    SHORT bufferWidth = (csbi.dwSize.X > 0) ? csbi.dwSize.X : 80;

    std::string formattedPrompt = formatPrompt(prompt);
    std::string fullLine = formattedPrompt + buffer;
    std::wstring wFullLine = string_to_wstring(fullLine);
    DWORD written;

    SetConsoleCursorPosition(hConsole, startPos);

    size_t clearLength = (lastRedrawLineLength > fullLine.length()) ? lastRedrawLineLength : fullLine.length();
    DWORD totalClearLength = static_cast<DWORD>(clearLength + static_cast<size_t>(bufferWidth));
    FillConsoleOutputCharacterW(hConsole, L' ', totalClearLength, startPos, &written);
    FillConsoleOutputAttribute(hConsole, csbi.wAttributes, totalClearLength, startPos, &written);

    SetConsoleCursorPosition(hConsole, startPos);
    WriteConsoleW(hConsole, wFullLine.c_str(), static_cast<DWORD>(wFullLine.length()), &written, NULL);
    lastRedrawLineLength = fullLine.length();

    SHORT totalOffset = static_cast<SHORT>(formattedPrompt.length() + cursorIndex);
    COORD targetPos;
    targetPos.Y = startPos.Y + (startPos.X + totalOffset) / bufferWidth;
    targetPos.X = (startPos.X + totalOffset) % bufferWidth;

    if (targetPos.Y >= csbi.dwSize.Y) targetPos.Y = csbi.dwSize.Y - 1;

    SetConsoleCursorPosition(hConsole, targetPos);
}

std::string TcshEngine::readLineWithEditing() {
    updateJobs();
    std::cout.flush();
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    COORD startPos = csbi.dwCursorPosition;

    std::string buffer = "";
    size_t cursorIndex = 0;
    historyIndex = history.size();
    lastRedrawLineLength = 0;

    redrawLine(hConsole, startPos, promptStr, buffer, cursorIndex);

    while (true) {
        if (g_interrupted.load(std::memory_order_relaxed)) {
            g_interrupted.store(false, std::memory_order_relaxed);
            std::string trapCommand;
            auto trapIt = trapHandlers.find("INT");
            if (trapIt == trapHandlers.end()) trapIt = trapHandlers.find("BREAK");
            if (trapIt != trapHandlers.end()) trapCommand = trapIt->second;

            buffer = "";
            cursorIndex = 0;
            std::cout << "\n";
            GetConsoleScreenBufferInfo(hConsole, &csbi);
            startPos = csbi.dwCursorPosition;
            if (!trapCommand.empty()) {
                executeCommandLine(trapCommand);
            }
            redrawLine(hConsole, startPos, promptStr, buffer, cursorIndex);
        }

        int ch = _getch();
        if (ch == 13) { std::cout << "\n"; compActive = false; compCandidates.clear(); break; }
        else if (ch == 8 && cursorIndex > 0) {
            compActive = false; compCandidates.clear();
            buffer.erase(cursorIndex - 1, 1);
            cursorIndex--;
            redrawLine(hConsole, startPos, promptStr, buffer, cursorIndex);
        } else if (ch == 9) {
            bool wasActive = compActive;
            handleTabCompletion(buffer, cursorIndex);
            if (!wasActive && compActive) {
                // Candidate list was just printed; re-anchor so redrawLine targets the new row.
                GetConsoleScreenBufferInfo(hConsole, &csbi);
                startPos = csbi.dwCursorPosition;
            }
            redrawLine(hConsole, startPos, promptStr, buffer, cursorIndex);
        } else if (ch == 0 || ch == 224) {
            compActive = false; compCandidates.clear();
            int code = _getch();
            if (code == 75 && cursorIndex > 0) cursorIndex--;
            else if (code == 77 && cursorIndex < buffer.length()) cursorIndex++;
            else if (code == 71) cursorIndex = 0;
            else if (code == 79) cursorIndex = buffer.length();
            else if (code == 72 && !history.empty() && historyIndex > 0) {
                buffer = history[--historyIndex];
                cursorIndex = buffer.length();
            } else if (code == 80) {
                if (historyIndex + 1 < history.size()) buffer = history[++historyIndex];
                else { historyIndex = history.size(); buffer = ""; }
                cursorIndex = buffer.length();
            }
            redrawLine(hConsole, startPos, promptStr, buffer, cursorIndex);
        } else if (ch >= 32 && ch <= 126) {
            compActive = false; compCandidates.clear();
            buffer.insert(cursorIndex, 1, static_cast<char>(ch));
            cursorIndex++;
            redrawLine(hConsole, startPos, promptStr, buffer, cursorIndex);
        }
    }

    return buffer;
}

bool TcshEngine::executeBuiltin(const std::vector<std::string>& args) {
    if (args.empty()) return false;
    const std::string cmd = args[0];

    auto okStatus = [&]() {
        setVariableList("status", { "0" });
        return true;
    };

    auto failStatus = [&]() {
        setVariableList("status", { "1" });
        return true;
    };

    auto joinArgs = [&](size_t startIndex) {
        std::string result;
        for (size_t i = startIndex; i < args.size(); ++i) {
            if (i > startIndex) result += " ";
            result += stripOuterQuotes(args[i]);
        }
        return result;
    };

    if (cmd == "exit" || cmd == "logout") { running = false; return okStatus(); }
    if (cmd == ":") { return okStatus(); }
    if (cmd == "help" || cmd == "builtins") { displayHelp(); return okStatus(); }
    if (cmd == "--version" || cmd == "version" || cmd == "-v" || cmd == "ver") { displayVersion(); return okStatus(); }
    if (cmd == "." || cmd == "source") {
        if (args.size() < 2) {
            print_error_message(cmd + ": Missing script file\n");
            return failStatus();
        }
        std::vector<std::string> scriptArgs;
        for (size_t i = 2; i < args.size(); ++i) scriptArgs.push_back(stripOuterQuotes(args[i]));
        if (!runScript(stripOuterQuotes(args[1]), scriptArgs)) {
            return failStatus();
        }
        return okStatus();
    }
    if (cmd == "pwd") {
        wchar_t currentDirW[MAX_PATH] = { 0 };
        GetCurrentDirectoryW(MAX_PATH, currentDirW);
        std::cout << wstring_to_string(currentDirW) << "\n";
        return okStatus();
    }
    if (cmd == "history") {
        for (size_t i = 0; i < history.size(); ++i) std::cout << (i + 1) << "  " << history[i] << "\n";
        return okStatus();
    }
    if (cmd == "alias") {
        if (args.size() == 1) {
            for (const auto& entry : aliases) std::cout << entry.first << "\t" << entry.second << "\n";
        } else if (args.size() >= 3) {
            aliases[stripOuterQuotes(args[1])] = stripOuterQuotes(args[2]);
        }
        return okStatus();
    }
    if (cmd == "unalias") {
        if (args.size() < 2) {
            print_error_message("unalias: Missing alias name\n");
            return failStatus();
        }
        aliases.erase(stripOuterQuotes(args[1]));
        return okStatus();
    }
    if (cmd == "complete") {
        if (args.size() == 1) {
            for (const auto& entry : completions) std::cout << entry.first << "\t" << entry.second << "\n";
        } else if (args.size() >= 3) {
            completions[stripOuterQuotes(args[1])] = stripOuterQuotes(args[2]);
        }
        return okStatus();
    }
    if (cmd == "uncomplete") {
        if (args.size() < 2) {
            print_error_message("uncomplete: Missing completion name\n");
            return failStatus();
        }
        completions.erase(stripOuterQuotes(args[1]));
        return okStatus();
    }
    if (cmd == "print") {
        for (size_t i = 1; i < args.size(); ++i) std::cout << stripOuterQuotes(args[i]) << (i + 1 < args.size() ? " " : "");
        std::cout << "\n";
        return okStatus();
    }
    if (cmd == "command") {
        const std::string remainder = joinArgs(1);
        if (remainder.empty()) {
            print_error_message("command: missing command name\n");
            return failStatus();
        }

        ParsedPipeline pipeline;
        if (!parseCommandLine(remainder, pipeline) || pipeline.commands.empty()) return failStatus();
        executePipeline(pipeline);
        return okStatus();
    }
    if (cmd == "builtin") {
        if (args.size() < 2) {
            print_error_message("builtin: missing builtin name\n");
            return failStatus();
        }
        std::vector<std::string> builtinArgs(args.begin() + 1, args.end());
        if (!executeBuiltin(builtinArgs)) {
            print_error_message("builtin: not a builtin: " + stripOuterQuotes(args[1]) + "\n");
            return failStatus();
        }
        return okStatus();
    }
    if (cmd == "whence" || cmd == "hash") {
        if (args.size() < 2) {
            std::cout << (cmd == "hash" ? "hashing: " : "") << (hashEnabled ? "enabled" : "disabled") << "\n";
            return okStatus();
        }

        for (size_t i = 1; i < args.size(); ++i) {
            const std::string name = stripOuterQuotes(args[i]);
            auto aliasIt = aliases.find(name);
            if (aliasIt != aliases.end()) {
                std::cout << name << " is an alias for " << aliasIt->second << "\n";
                continue;
            }

            if (isBuiltinCommand(name)) {
                std::cout << name << " is a shell builtin\n";
                continue;
            }

            bool foundOnDisk = false;
            std::string resolved = resolveExecutable(name, foundOnDisk);
            if (foundOnDisk) {
                std::cout << resolved << "\n";
            } else {
                std::cout << name << " not found\n";
            }
        }
        return okStatus();
    }
    if (cmd == "test") {
        const std::string expr = joinArgs(1);
        if (expr.empty()) return failStatus();
        return evalCondition(expr) ? okStatus() : failStatus();
    }
    if (cmd == "let") {
        const std::string expr = joinArgs(1);
        if (expr.empty()) return failStatus();

        std::vector<std::string> tokens = tokenize(expr, ' ');
        long long result = 0;
        auto resolveValue = [&](const std::string& token) -> long long {
            std::string clean = stripOuterQuotes(token);
            auto it = variables.find(clean);
            if (it != variables.end() && !it->second.empty()) {
                return std::atoll(it->second[0].c_str());
            }
            return std::atoll(clean.c_str());
        };

        if (tokens.size() >= 3 && tokens[1] == "=") {
            result = resolveValue(tokens[2]);
            if (tokens.size() >= 5) {
                const std::string op = tokens[3];
                const long long rhs = resolveValue(tokens[4]);
                if (op == "+") result += rhs;
                else if (op == "-") result -= rhs;
                else if (op == "*") result *= rhs;
                else if (op == "/") result = (rhs != 0) ? (result / rhs) : 0;
                else if (op == "%") result = (rhs != 0) ? (result % rhs) : 0;
            }
            setVariableList(tokens[0], { std::to_string(result) });
        } else if (!tokens.empty()) {
            result = resolveValue(tokens[0]);
        }

        return (result != 0) ? okStatus() : failStatus();
    }
    if (cmd == "return") {
        if (!executingScript) {
            print_error_message("return: not in a sourced script\n");
            return failStatus();
        }
        scriptDirective = ScriptDirective::Return;
        return okStatus();
    }
    if (cmd == "select") {
        if (args.size() < 3) {
            print_error_message("select: usage: select name item...\n");
            return failStatus();
        }

        const std::string varName = stripOuterQuotes(args[1]);
        std::vector<std::string> items;
        for (size_t i = 2; i < args.size(); ++i) {
            if (args[i] == "in") continue;
            items.push_back(stripOuterQuotes(args[i]));
        }
        if (items.empty()) return failStatus();

        for (size_t i = 0; i < items.size(); ++i) {
            std::cout << (i + 1) << ") " << items[i] << "\n";
        }
        std::cout << "? ";
        std::string choiceLine;
        if (!std::getline(std::cin, choiceLine)) return failStatus();
        int choice = std::atoi(choiceLine.c_str());
        if (choice < 1 || static_cast<size_t>(choice) > items.size()) return failStatus();

        setVariableList(varName, { items[static_cast<size_t>(choice) - 1] });
        return okStatus();
    }
    if (cmd == "trap") {
        if (args.size() == 1) {
            for (const auto& entry : trapHandlers) {
                std::cout << entry.second << "\t" << entry.first << "\n";
            }
            return okStatus();
        }

        auto normalizeTrap = [](std::string value) {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
                return static_cast<char>(std::toupper(ch));
            });
            if (value.rfind("SIG", 0) == 0) value.erase(0, 3);
            if (value == "QUIT") value = "BREAK";
            return value;
        };

        const bool clearMode = (args[1] == "-");
        std::string action = clearMode ? "" : stripOuterQuotes(args[1]);
        size_t signalStart = clearMode ? 2 : 2;
        if (signalStart >= args.size()) return failStatus();

        for (size_t i = signalStart; i < args.size(); ++i) {
            std::string sig = normalizeTrap(stripOuterQuotes(args[i]));
            if (sig.empty()) continue;
            if (clearMode) {
                trapHandlers.erase(sig);
            } else {
                trapHandlers[sig] = action;
            }
        }
        return okStatus();
    }
    if (cmd == "disown") {
        auto closeJobHandles = [](Job& job) {
            if (job.hProcess && job.hProcess != INVALID_HANDLE_VALUE) {
                CloseHandle(job.hProcess);
                job.hProcess = NULL;
            }
            if (job.hJob && job.hJob != INVALID_HANDLE_VALUE) {
                CloseHandle(job.hJob);
                job.hJob = NULL;
            }
        };

        if (args.size() == 1) {
            if (!jobList.empty()) {
                jobList.erase(std::remove_if(jobList.begin(), jobList.end(), [&](Job& job) {
                    if (!job.isRunning) {
                        closeJobHandles(job);
                        return true;
                    }
                    return false;
                }), jobList.end());
            }
            return okStatus();
        }

        for (size_t i = 1; i < args.size(); ++i) {
            std::string spec = stripOuterQuotes(args[i]);
            if (!spec.empty() && spec[0] == '%') spec.erase(spec.begin());
            int jobId = 0;
            if (!tryParseNonNegativeInt(spec, jobId, "disown", "job id")) return failStatus();
            jobList.erase(std::remove_if(jobList.begin(), jobList.end(), [&](Job& job) {
                if (static_cast<int>(job.id) == jobId) {
                    closeJobHandles(job);
                    return true;
                }
                return false;
            }), jobList.end());
        }
        return okStatus();
    }
    if (cmd == "printenv") {
        if (args.size() == 1) {
            for (const auto& entry : variables) {
                std::cout << entry.first << "=" << getVariableString(entry.first) << "\n";
            }
        } else {
            for (size_t i = 1; i < args.size(); ++i) {
                const std::string name = stripOuterQuotes(args[i]);
                const std::string value = getVariableString(name);
                if (!value.empty()) {
                    std::cout << value << "\n";
                } else {
                    std::wstring wName = string_to_wstring(name);
                    DWORD dwRet = GetEnvironmentVariableW(wName.c_str(), nullptr, 0);
                    if (dwRet > 0) {
                        std::vector<wchar_t> envBuf(dwRet);
                        if (GetEnvironmentVariableW(wName.c_str(), envBuf.data(), dwRet) > 0) {
                            std::cout << wstring_to_string(envBuf.data()) << "\n";
                        }
                    }
                }
            }
        }
        return okStatus();
    }
    if (cmd == "read") {
        std::string line;
        if (!std::getline(std::cin, line)) return failStatus();

        std::vector<std::string> values = tokenize(line, ' ');
        if (args.size() < 2) {
            setVariableList("REPLY", { line });
            return okStatus();
        }

        size_t valueIndex = 0;
        for (size_t i = 1; i < args.size(); ++i) {
            const std::string name = stripOuterQuotes(args[i]);
            if (i + 1 == args.size()) {
                std::string remainder;
                for (; valueIndex < values.size(); ++valueIndex) {
                    if (!remainder.empty()) remainder += " ";
                    remainder += values[valueIndex];
                }
                setVariableList(name, { remainder });
                break;
            }

            if (valueIndex < values.size()) {
                setVariableList(name, { values[valueIndex++] });
            } else {
                setVariableList(name, { "" });
            }
        }
        return okStatus();
    }
    if (cmd == "export") {
        if (args.size() == 1) {
            for (const auto& entry : variables) {
                SetEnvironmentVariableW(string_to_wstring(entry.first).c_str(), string_to_wstring(getVariableString(entry.first)).c_str());
            }
            return okStatus();
        }

        for (size_t i = 1; i < args.size(); ++i) {
            std::string spec = stripOuterQuotes(args[i]);
            size_t eq = spec.find('=');
            if (eq != std::string::npos) {
                std::string name = spec.substr(0, eq);
                std::string value = spec.substr(eq + 1);
                setVariableList(name, { value });
                SetEnvironmentVariableW(string_to_wstring(name).c_str(), string_to_wstring(value).c_str());
            } else {
                SetEnvironmentVariableW(string_to_wstring(spec).c_str(), string_to_wstring(getVariableString(spec)).c_str());
            }
        }
        return okStatus();
    }
    if (cmd == "typeset") {
        if (args.size() == 1) {
            for (const auto& p : variables) std::cout << p.first << "\t" << getVariableString(p.first) << "\n";
            return okStatus();
        }
        std::vector<std::string> setArgs = args;
        setArgs[0] = "set";
        return executeBuiltin(setArgs);
    }
    if (cmd == "dirs") {
        wchar_t cwdW[MAX_PATH] = { 0 };
        std::string currentDir = GetCurrentDirectoryW(MAX_PATH, cwdW) ? wstring_to_string(cwdW) : homeDir;
        std::cout << currentDir;
        for (const auto& entry : dirStack) std::cout << " " << entry;
        std::cout << "\n";
        return okStatus();
    }
    if (cmd == "pushd") {
        wchar_t cwdW[MAX_PATH] = { 0 };
        if (!GetCurrentDirectoryW(MAX_PATH, cwdW)) return failStatus();

        std::string currentDir = wstring_to_string(cwdW);
        std::string targetDir;
        if (args.size() >= 2) {
            targetDir = normalizePathSeparators(stripOuterQuotes(args[1]));
        } else if (!dirStack.empty()) {
            targetDir = normalizePathSeparators(dirStack.back());
            dirStack.pop_back();
        } else {
            print_error_message("pushd: No directory specified\n");
            return failStatus();
        }

        if (SetCurrentDirectoryW(string_to_wstring(targetDir).c_str())) {
            dirStack.push_back(currentDir);
            return okStatus();
        }

        print_error_message("pushd: No such file or directory: " + targetDir + "\n");
        return failStatus();
    }
    if (cmd == "popd") {
        if (dirStack.empty()) {
            print_error_message("popd: Directory stack empty\n");
            return failStatus();
        }

        std::string targetDir = normalizePathSeparators(dirStack.back());
        dirStack.pop_back();
        if (!SetCurrentDirectoryW(string_to_wstring(targetDir).c_str())) {
            print_error_message("popd: Failed to change directory\n");
            return failStatus();
        }

        return okStatus();
    }
    if (cmd == "jobs") { return listJobs(args) ? okStatus() : failStatus(); }
    if (cmd == "fg") {
        DWORD jobId = 0;
        if (args.size() >= 2) {
            std::string jobSpec = stripOuterQuotes(args[1]);
            if (!jobSpec.empty() && jobSpec[0] == '%') jobSpec.erase(jobSpec.begin());
            int parsed = 0;
            if (!tryParseNonNegativeInt(jobSpec, parsed, "fg", "job id") || parsed <= 0) return failStatus();
            jobId = static_cast<DWORD>(parsed);
        } else if (!jobList.empty()) {
            jobId = jobList.back().id;
        }
        if (jobId == 0) {
            print_error_message("fg: no current job\n");
            return failStatus();
        }
        bringJobToForeground(jobId);
        return okStatus();
    }
    if (cmd == "bg") {
        DWORD jobId = 0;
        if (args.size() >= 2) {
            std::string jobSpec = stripOuterQuotes(args[1]);
            if (!jobSpec.empty() && jobSpec[0] == '%') jobSpec.erase(jobSpec.begin());
            int parsed = 0;
            if (!tryParseNonNegativeInt(jobSpec, parsed, "bg", "job id") || parsed <= 0) return failStatus();
            jobId = static_cast<DWORD>(parsed);
        } else if (!jobList.empty()) {
            jobId = jobList.back().id;
        }
        if (jobId == 0) {
            print_error_message("bg: no current job\n");
            return failStatus();
        }
        sendJobToBackground(jobId);
        return okStatus();
    }
    if (cmd == "wait") {
        if (args.size() == 1) {
            for (auto& job : jobList) {
                if (job.isRunning && job.hProcess) WaitForSingleObject(job.hProcess, INFINITE);
            }
            updateJobs();
            return okStatus();
        }

        std::string jobSpec = stripOuterQuotes(args[1]);
        if (!jobSpec.empty() && jobSpec[0] == '%') jobSpec.erase(jobSpec.begin());
        int parsed = 0;
        if (!tryParseNonNegativeInt(jobSpec, parsed, "wait", "job id") || parsed <= 0) return failStatus();

        for (auto& job : jobList) {
            if (static_cast<int>(job.id) == parsed && job.isRunning && job.hProcess) {
                WaitForSingleObject(job.hProcess, INFINITE);
                updateJobs();
                return okStatus();
            }
        }

        print_error_message("wait: No such job\n");
        return failStatus();
    }
    if (cmd == "break") { scriptDirective = ScriptDirective::Break; return okStatus(); }
    if (cmd == "continue") { scriptDirective = ScriptDirective::Continue; return okStatus(); }
    if (cmd == "eval") {
        const std::string expr = joinArgs(1);
        if (!expr.empty()) executeCommandLine(expr);
        return okStatus();
    }
    if (cmd == "exec") {
        const std::string expr = joinArgs(1);
        if (!expr.empty()) executeCommandLine(expr);
        running = false;
        return okStatus();
    }
    if (cmd == "repeat") {
        if (args.size() < 3) {
            print_error_message("repeat: Usage: repeat count command\n");
            return failStatus();
        }

        int count = 0;
        if (!tryParseNonNegativeInt(stripOuterQuotes(args[1]), count, "repeat", "count")) return failStatus();
        const std::string expr = joinArgs(2);
        for (int i = 0; i < count; ++i) executeCommandLine(expr);
        return okStatus();
    }
    if (cmd == "which" || cmd == "where") {
        if (args.size() < 2) {
            print_error_message(cmd + ": Missing command name\n");
            return failStatus();
        }

        for (size_t i = 1; i < args.size(); ++i) {
            const std::string name = stripOuterQuotes(args[i]);
            bool foundOnDisk = false;
            const std::string resolved = resolveExecutable(name, foundOnDisk);
            if (foundOnDisk) {
                std::cout << resolved << "\n";
            } else if (isBuiltinCommand(name)) {
                std::cout << name << " is a shell builtin\n";
            } else {
                print_error_message(cmd + ": " + name + ": not found\n");
            }
        }
        return okStatus();
    }
    if (cmd == "rehash") { hashEnabled = true; return okStatus(); }
    if (cmd == "unhash") { hashEnabled = false; return okStatus(); }
    if (cmd == "hashstat") {
        std::cout << "hashing: " << (hashEnabled ? "enabled" : "disabled") << "\n";
        return okStatus();
    }
    if (cmd == "time") {
        if (args.size() == 1) {
            auto now = std::chrono::system_clock::now();
            std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
            std::cout << std::ctime(&nowTime);
            return okStatus();
        }

        const auto started = std::chrono::steady_clock::now();
        executeCommandLine(joinArgs(1));
        const auto finished = std::chrono::steady_clock::now();
        const std::chrono::duration<double> elapsed = finished - started;
        std::cout << "real\t" << elapsed.count() << "s\n";
        return okStatus();
    }
    if (cmd == "umask") {
        if (args.size() >= 2) {
            setVariableList("umask", { stripOuterQuotes(args[1]) });
        } else {
            std::cout << getVariableString("umask") << "\n";
        }
        return okStatus();
    }
    if (cmd == "notify") { notifyJobs = true; return okStatus(); }
    if (cmd == "hup") {
        for (auto& job : jobList) {
            if (job.isRunning && job.hProcess) {
                TerminateProcess(job.hProcess, 1);
                job.isRunning = false;
                if (job.hProcess && job.hProcess != INVALID_HANDLE_VALUE) {
                    CloseHandle(job.hProcess);
                    job.hProcess = NULL;
                }
                if (job.hJob && job.hJob != INVALID_HANDLE_VALUE) {
                    CloseHandle(job.hJob);
                    job.hJob = NULL;
                }
            }
        }
        updateJobs();
        return okStatus();
    }
    if (cmd == "clear" || cmd == "cls") {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (hOut == INVALID_HANDLE_VALUE || hOut == NULL || !GetConsoleScreenBufferInfo(hOut, &csbi)) return failStatus();
        COORD home = { 0, 0 };
        DWORD cellCount = static_cast<DWORD>(csbi.dwSize.X) * static_cast<DWORD>(csbi.dwSize.Y);
        DWORD written = 0;
        bool ok = FillConsoleOutputCharacterW(hOut, L' ', cellCount, home, &written) != FALSE;
        ok = ok && (FillConsoleOutputAttribute(hOut, csbi.wAttributes, cellCount, home, &written) != FALSE);
        ok = ok && (SetConsoleCursorPosition(hOut, home) != FALSE);
        setVariableList("status", { ok ? "0" : "1" });
        return true;
    }
    if (cmd == "kill") {
        if (args.size() < 2) {
            print_error_message("kill: Missing PID or Job ID\n");
            return failStatus();
        }

        const std::string target = stripOuterQuotes(args[1]);
        if (!target.empty() && target[0] == '%') {
            int jobId = 0;
            if (!tryParseNonNegativeInt(target.substr(1), jobId, "kill", "job id") || jobId <= 0) return failStatus();

            auto it = std::find_if(jobList.begin(), jobList.end(), [jobId](const Job& job) {
                return static_cast<int>(job.id) == jobId;
            });
            if (it == jobList.end() || !it->isRunning || !it->hProcess) {
                print_error_message("kill: No such job\n");
                return failStatus();
            }

            bool ok = TerminateProcess(it->hProcess, 1) != FALSE;
            if (it->hJob && it->hJob != INVALID_HANDLE_VALUE) TerminateJobObject(it->hJob, 1);
            it->isRunning = false;
            if (it->hProcess && it->hProcess != INVALID_HANDLE_VALUE) { CloseHandle(it->hProcess); it->hProcess = NULL; }
            if (it->hJob && it->hJob != INVALID_HANDLE_VALUE) { CloseHandle(it->hJob); it->hJob = NULL; }
            if (!ok) print_error_message("kill: Failed to terminate job\n");
            setVariableList("status", { ok ? "0" : "1" });
            return true;
        }

        int pid = 0;
        if (!tryParseNonNegativeInt(target, pid, "kill", "pid") || pid <= 0) return failStatus();
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
        if (!hProcess) {
            print_error_message("kill: No such process or access denied\n");
            return failStatus();
        }
        bool ok = TerminateProcess(hProcess, 1) != FALSE;
        CloseHandle(hProcess);
        if (!ok) print_error_message("kill: Failed to terminate process\n");
        setVariableList("status", { ok ? "0" : "1" });
        return true;
    }
    if (cmd == "cd" || cmd == "chdir") {
        std::string target = (args.size() > 1) ? stripOuterQuotes(args[1]) : homeDir;
        if (target == "-") {
            target = getVariableString("owd");
            if (target.empty()) target = homeDir;
        }
        target = normalizePathSeparators(target);

        wchar_t cwdW[MAX_PATH] = { 0 };
        if (GetCurrentDirectoryW(MAX_PATH, cwdW)) {
            setVariableList("owd", { wstring_to_string(cwdW) });
        }

        if (!SetCurrentDirectoryW(string_to_wstring(target).c_str())) {
            print_error_message("cd: No such file or directory: " + target + "\n");
            return failStatus();
        }

        if (GetCurrentDirectoryW(MAX_PATH, cwdW)) {
            setVariableList("cwd", { wstring_to_string(cwdW) });
        }
        return okStatus();
    }
    if (cmd == "set") {
        if (args.size() == 1) {
            for (const auto& p : variables) std::cout << p.first << "\t" << getVariableString(p.first) << "\n";
            return okStatus();
        }
        if (args.size() >= 4 && args[2] == "=" && args[3] == "(") {
            std::vector<std::string> list;
            for (size_t i = 4; i < args.size(); ++i) {
                if (args[i] == ")") break;
                list.push_back(stripOuterQuotes(args[i]));
            }
            setVariableList(stripOuterQuotes(args[1]), list);
            return okStatus();
        }
        if (args.size() >= 3 && args[2] == "=") {
            std::string varSpec = stripOuterQuotes(args[1]);
            size_t bracketOpen = varSpec.find('[');
            size_t bracketClose = varSpec.find(']');
            if (bracketOpen != std::string::npos && bracketClose != std::string::npos && bracketClose > bracketOpen) {
                std::string varName = varSpec.substr(0, bracketOpen);
                int idx = std::atoi(varSpec.substr(bracketOpen + 1, bracketClose - bracketOpen - 1).c_str());
                if (variables.count(varName) && idx >= 1 && static_cast<size_t>(idx) <= variables[varName].size()) {
                    variables[varName][idx - 1] = stripOuterQuotes(args[3]);
                }
            } else {
                setVariableList(varSpec, { stripOuterQuotes(args[3]) });
            }
            return okStatus();
        }
        return failStatus();
    }
    if (cmd == "unset") {
        if (args.size() > 1) variables.erase(stripOuterQuotes(args[1]));
        return okStatus();
    }
    if (cmd == "setenv") {
        if (args.size() < 3) return failStatus();
        SetEnvironmentVariableW(string_to_wstring(stripOuterQuotes(args[1])).c_str(), string_to_wstring(stripOuterQuotes(args[2])).c_str());
        return okStatus();
    }
    if (cmd == "unsetenv") {
        if (args.size() < 2) return failStatus();
        SetEnvironmentVariableW(string_to_wstring(stripOuterQuotes(args[1])).c_str(), NULL);
        return okStatus();
    }
    if (cmd == "echo") {
        for (size_t i = 1; i < args.size(); ++i) std::cout << stripOuterQuotes(args[i]) << (i + 1 < args.size() ? " " : "");
        std::cout << "\n";
        return okStatus();
    }
    if (cmd == "@") {
        if (args.size() > 1) {
            std::string expr;
            for (size_t i = 1; i < args.size(); ++i) expr += args[i] + " ";
            evaluateArithmetic(expr);
        }
        return okStatus();
    }
    if (cmd == "shift") {
        shiftPositionalArguments(1);
        return okStatus();
    }

    if (cmd == "alloc") {
        std::cout << "variables=" << variables.size()
                  << " aliases=" << aliases.size()
                  << " completions=" << completions.size()
                  << " jobs=" << jobList.size()
                  << " history=" << history.size() << "\n";
        return okStatus();
    }
    if (cmd == "bindkey") {
        if (args.size() == 1) {
            for (const auto& entry : keyBindings) {
                std::cout << entry.first << "\t" << entry.second << "\n";
            }
        } else if (args.size() >= 3) {
            keyBindings[stripOuterQuotes(args[1])] = joinArgs(2);
        }
        return okStatus();
    }
    if (cmd == "breaksw") { scriptDirective = ScriptDirective::Break; return okStatus(); }
    if (cmd == "case" || cmd == "default" || cmd == "else" || cmd == "end" || cmd == "endif" || cmd == "endsw" || cmd == "foreach" || cmd == "switch" || cmd == "while") { return okStatus(); }
    if (cmd == "filetest") {
        const std::string expr = joinArgs(1);
        return expr.empty() ? failStatus() : (evalCondition(expr) ? okStatus() : failStatus());
    }
    if (cmd == "glob") {
        if (args.size() > 1) {
            std::vector<std::string> globs(args.begin() + 1, args.end());
            std::vector<std::string> expanded = expandGlobs(globs);
            for (const auto& item : expanded) std::cout << item << "\n";
        }
        return okStatus();
    }
    if (cmd == "goto") {
        if (args.size() < 2) return failStatus();
        print_error_message("goto: handled inside script execution context\n");
        return okStatus();
    }
    if (cmd == "if") {
        const std::string expr = joinArgs(1);
        return expr.empty() ? failStatus() : (evalCondition(expr) ? okStatus() : failStatus());
    }
    if (cmd == "limit") {
        if (args.size() == 1) {
            std::cout << getVariableString("limits") << "\n";
        } else {
            setVariableList("limits", { joinArgs(1) });
        }
        return okStatus();
    }
    if (cmd == "log") {
        if (args.size() == 1) {
            std::ifstream logIn(logFile);
            std::string line;
            while (std::getline(logIn, line)) std::cout << line << "\n";
        } else {
            std::ofstream logOut(logFile, std::ios::app);
            logOut << joinArgs(1) << "\n";
        }
        return okStatus();
    }
    if (cmd == "login") {
        wchar_t userNameW[256] = { 0 };
        DWORD len = ARRAYSIZE(userNameW);
        if (GetUserNameW(userNameW, &len)) {
            std::string userName = wstring_to_string(userNameW);
            setVariableList("login", { userName });
            std::cout << userName << "\n";
        }
        return okStatus();
    }
    if (cmd == "migrate") {
        setVariableList("migrate", { joinArgs(1) });
        std::cout << "migrate: " << joinArgs(1) << "\n";
        return okStatus();
    }
    if (cmd == "newgrp") {
        const std::string groupName = (args.size() > 1) ? stripOuterQuotes(args[1]) : getVariableString("login");
        setVariableList("group", { groupName });
        std::cout << groupName << "\n";
        return okStatus();
    }
    if (cmd == "nice") {
        if (args.size() < 2) return failStatus();
        std::string expr = joinArgs(1);
        if (!expr.empty() && expr[0] == '-' && args.size() > 2) {
            size_t firstSpace = expr.find(' ');
            if (firstSpace != std::string::npos) expr = expr.substr(firstSpace + 1);
        }
        executeCommandLine(expr);
        return okStatus();
    }
    if (cmd == "nohup") {
        if (args.size() < 2) return failStatus();
        executeCommandLine(joinArgs(1) + " &");
        return okStatus();
    }
    if (cmd == "sched") {
        if (args.size() == 1) {
            for (const auto& task : scheduledTasks) {
                std::cout << task.timeStr << "\t" << task.command << "\n";
            }
        } else if (args.size() >= 3) {
            ScheduledTask task;
            task.timeStr = stripOuterQuotes(args[1]);
            task.command = joinArgs(2);
            if (task.timeStr == "now" || task.timeStr == "NOW") {
                executeCommandLine(task.command);
            } else {
                scheduledTasks.push_back(task);
            }
        }
        return okStatus();
    }
    if (cmd == "stop" || cmd == "suspend") {
        if (args.size() < 2) {
            print_error_message(cmd + ": Missing job id\n");
            return failStatus();
        }
        std::string target = stripOuterQuotes(args[1]);
        if (!target.empty() && target[0] == '%') target.erase(target.begin());
        int jobId = 0;
        if (!tryParseNonNegativeInt(target, jobId, cmd, "job id") || jobId <= 0) return failStatus();
        auto it = std::find_if(jobList.begin(), jobList.end(), [jobId](const Job& job) { return static_cast<int>(job.id) == jobId; });
        if (it == jobList.end()) {
            print_error_message(cmd + ": No such job\n");
            return failStatus();
        }
        it->isRunning = false;
        std::cout << "[" << it->id << "] stopped\n";
        return okStatus();
    }
    if (cmd == "settc" || cmd == "setty" || cmd == "termname" || cmd == "telltc" || cmd == "universe") {
        if (cmd == "termname") {
            if (args.size() == 1) {
                std::cout << getVariableString("termname") << "\n";
            } else {
                setVariableList("termname", { joinArgs(1) });
                std::cout << joinArgs(1) << "\n";
            }
        } else if (cmd == "telltc") {
            std::cout << getVariableString("settc") << "\n" << getVariableString("setty") << "\n";
        } else {
            setVariableList(cmd, { joinArgs(1) });
            if (!joinArgs(1).empty()) std::cout << joinArgs(1) << "\n";
        }
        return okStatus();
    }
    if (cmd == "unlimit") {
        setVariableList("limits", {});
        return okStatus();
    }
    if (cmd == "echotc") {
        if (args.size() < 2) {
            print_error_message("echotc: Too few arguments.\n");
            return failStatus();
        }
        for (size_t i = 1; i < args.size(); ++i) {
            std::string cap = stripOuterQuotes(args[i]);
            std::transform(cap.begin(), cap.end(), cap.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (cap == "home" || cap == "ho") {
                std::cout << "\x1b[H";
            } else if (cap == "clear" || cap == "cl") {
                std::cout << "\x1b[2J\x1b[H";
            } else if (cap == "cd") {
                std::cout << "\x1b[J";
            } else if (cap == "ce") {
                std::cout << "\x1b[K";
            } else if (cap == "bold" || cap == "md") {
                std::cout << "\x1b[1m";
            } else if (cap == "standout" || cap == "so") {
                std::cout << "\x1b[7m";
            } else if (cap == "underline" || cap == "us") {
                std::cout << "\x1b[4m";
            } else if (cap == "normal" || cap == "me" || cap == "se" || cap == "ue") {
                std::cout << "\x1b[0m";
            } else if (cap == "bell" || cap == "bl") {
                std::cout << "\a";
            } else if (cap == "up" || cap == "up1") {
                std::cout << "\x1b[A";
            } else if (cap == "down" || cap == "do1") {
                std::cout << "\x1b[B";
            } else if (cap == "left" || cap == "le") {
                std::cout << "\x1b[D";
            } else if (cap == "right" || cap == "nd") {
                std::cout << "\x1b[C";
            } else if (cap == "cols" || cap == "co" || cap == "lines" || cap == "li") {
                CONSOLE_SCREEN_BUFFER_INFO csbi;
                HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
                if (hOut != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(hOut, &csbi)) {
                    if (cap == "cols" || cap == "co") {
                        std::cout << csbi.dwSize.X << "\n";
                    } else {
                        std::cout << (csbi.srWindow.Bottom - csbi.srWindow.Top + 1) << "\n";
                    }
                }
            } else {
                print_error_message("echotc: Unknown terminal capability `" + cap + "'.\n");
                return failStatus();
            }
        }
        std::cout.flush();
        return okStatus();
    }
    if (cmd == "ls-F" || cmd == "ls_F") {
        std::vector<std::string> targets;
        for (size_t i = 1; i < args.size(); ++i) {
            std::string t = stripOuterQuotes(args[i]);
            if (!t.empty() && t[0] != '-') targets.push_back(t);
        }
        if (targets.empty()) targets.push_back(".");

        auto formatEntry = [](const std::string& name, DWORD attr) -> std::string {
            if (attr == INVALID_FILE_ATTRIBUTES) return name;
            if (attr & FILE_ATTRIBUTE_REPARSE_POINT) return name + "@";
            if (attr & FILE_ATTRIBUTE_DIRECTORY) return name + "/";
            std::string lower = name;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (lower.size() >= 4) {
                std::string ext = lower.substr(lower.size() - 4);
                if (ext == ".exe" || ext == ".bat" || ext == ".cmd" || ext == ".com" || ext == ".ps1") {
                    return name + "*";
                }
            }
            return name;
        };

        for (size_t t = 0; t < targets.size(); ++t) {
            std::string target = normalizePathSeparators(targets[t]);
            std::wstring wTarget = string_to_wstring(target);
            DWORD attr = GetFileAttributesW(wTarget.c_str());

            if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
                std::cout << formatEntry(target, attr) << "\n";
                continue;
            }

            std::string searchPattern = target;
            if (!searchPattern.empty() && searchPattern.back() != '\\' && searchPattern.back() != '/') {
                searchPattern += "\\*";
            } else {
                searchPattern += "*";
            }

            WIN32_FIND_DATAW fd;
            HANDLE hFind = FindFirstFileW(string_to_wstring(searchPattern).c_str(), &fd);
            if (hFind == INVALID_HANDLE_VALUE) {
                print_error_message("ls-F: " + target + ": No such file or directory\n");
                return failStatus();
            }

            std::vector<std::string> entries;
            do {
                std::wstring wName = fd.cFileName;
                if (wName == L"." || wName == L"..") continue;
                std::string name = wstring_to_string(wName);
                entries.push_back(formatEntry(name, fd.dwFileAttributes));
            } while (FindNextFileW(hFind, &fd));
            FindClose(hFind);

            std::sort(entries.begin(), entries.end());
            for (size_t i = 0; i < entries.size(); ++i) {
                std::cout << entries[i] << (i + 1 < entries.size() ? "  " : "\n");
            }
        }
        return okStatus();
    }
    if (cmd == "onintr") {
        if (args.size() == 1) {
            trapHandlers.erase("INT");
            trapHandlers.erase("BREAK");
            return okStatus();
        }
        std::string action = stripOuterQuotes(args[1]);
        if (action == "-") {
            trapHandlers["INT"] = ":";
            trapHandlers["BREAK"] = ":";
        } else {
            std::string handler = (action.rfind("goto ", 0) == 0) ? action : ("goto " + action);
            trapHandlers["INT"] = handler;
            trapHandlers["BREAK"] = handler;
        }
        return okStatus();
    }

    return false;
}

void TcshEngine::executePipeline(const ParsedPipeline& pipeline) {
    ScopedHandle hInput;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    for (size_t i = 0; i < pipeline.commands.size(); ++i) {
        bool isLast = (i == pipeline.commands.size() - 1);
        ScopedHandle hReadPipe, hWritePipe;

        if (!isLast) {
            HANDLE rPipe = NULL, wPipe = NULL;
            if (CreatePipe(&rPipe, &wPipe, &sa, 0)) {
                hReadPipe.reset(rPipe);
                hWritePipe.reset(wPipe);
                SetHandleInformation(hReadPipe.get(), HANDLE_FLAG_INHERIT, 0);
            }
        }

        const ParsedCommand& command = pipeline.commands[i];
        std::vector<std::string> args = expandGlobs(command.args);
        if (args.empty()) continue;

        ScopedHandle redirectedInput, redirectedOutput;
        bool redirectStderr = pipeline.pipeStderr;

        for (const auto& redirection : command.redirections) {
            std::wstring wTarget = string_to_wstring(stripOuterQuotes(redirection.target));
            bool isErrRedirect = (redirection.kind == ParsedTokenKind::RedirectOutputStderr || redirection.kind == ParsedTokenKind::RedirectAppendStderr);

            if (redirection.kind == ParsedTokenKind::RedirectInput) {
                HANDLE fileHandle = CreateFileW(wTarget.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                if (fileHandle != INVALID_HANDLE_VALUE) {
                    redirectedInput.reset(fileHandle);
                }
                continue;
            }

            DWORD desiredAccess = (redirection.kind == ParsedTokenKind::RedirectAppend || redirection.kind == ParsedTokenKind::RedirectAppendStderr) ? FILE_APPEND_DATA : GENERIC_WRITE;
            DWORD creationDisposition = (redirection.kind == ParsedTokenKind::RedirectAppend || redirection.kind == ParsedTokenKind::RedirectAppendStderr) ? OPEN_ALWAYS : CREATE_ALWAYS;

            HANDLE fileHandle = CreateFileW(wTarget.c_str(), desiredAccess, FILE_SHARE_READ, &sa, creationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);
            if (fileHandle != INVALID_HANDLE_VALUE) {
                redirectedOutput.reset(fileHandle);
                if (isErrRedirect) redirectStderr = true;
            }
        }

        // Support Built-in Commands in Pipelines
        if (isBuiltinCommand(args[0])) {
            std::stringstream ss;
            std::streambuf* oldCout = nullptr;
            if (!isLast || redirectedOutput.isValid()) {
                oldCout = std::cout.rdbuf(ss.rdbuf());
            }

            bool handledBuiltin = executeBuiltin(args);

            if (oldCout != nullptr) {
                std::cout.rdbuf(oldCout);
            }

            if (handledBuiltin) {
                if (!isLast || redirectedOutput.isValid()) {
                    std::string builtinOutput = ss.str();
                    HANDLE hOutHandle = redirectedOutput.isValid() ? redirectedOutput.get() : hWritePipe.get();
                    DWORD dwWritten = 0;
                    WriteFile(hOutHandle, builtinOutput.c_str(), static_cast<DWORD>(builtinOutput.length()), &dwWritten, NULL);
                }
                hInput = std::move(hReadPipe);
                continue;
            }
        }

        // Enable inheritance on hInput so CreateProcessW can pass standard input to Child process
        if (hInput.isValid()) {
            SetHandleInformation(hInput.get(), HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        }

        STARTUPINFOW si = { sizeof(STARTUPINFOW) };
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = redirectedInput.isValid() ? redirectedInput.get() : (hInput.isValid() ? hInput.get() : GetStdHandle(STD_INPUT_HANDLE));
        si.hStdOutput = redirectedOutput.isValid() ? redirectedOutput.get() : ((!isLast) ? hWritePipe.get() : GetStdHandle(STD_OUTPUT_HANDLE));
        si.hStdError = redirectStderr ? si.hStdOutput : GetStdHandle(STD_ERROR_HANDLE);

        bool foundOnDisk = false;
        std::string resolvedExe = resolveExecutable(stripOuterQuotes(args[0]), foundOnDisk);
        std::string finalCmdLine = "";

        if (foundOnDisk) {
            finalCmdLine = "\"" + resolvedExe + "\"";
            for (size_t a = 1; a < args.size(); ++a) finalCmdLine += " " + escapeArg(stripOuterQuotes(args[a]));
        } else {
            if (!shouldFallbackToCmd(stripOuterQuotes(args[0]))) {
                print_error_message("tcsh: Command not found: " + args[0] + "\n");
                setVariableList("status", { "1" });
                return;
            }
            finalCmdLine = "cmd.exe /c " + command.sourceText;
        }

        std::wstring wFinalCmdLine = string_to_wstring(finalCmdLine);
        std::vector<wchar_t> cmdBuf(wFinalCmdLine.begin(), wFinalCmdLine.end());
        cmdBuf.push_back(L'\0');

        PROCESS_INFORMATION pi = { 0 };
        DWORD creationFlags = CREATE_NEW_PROCESS_GROUP;
        if (CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, TRUE, creationFlags, NULL, NULL, &si, &pi)) {
            ScopedHandle hProcess(pi.hProcess);
            ScopedHandle hThread(pi.hThread);
            hInput = std::move(hReadPipe);

            if (pipeline.background && isLast) {
                updateJobs();
                if (jobList.size() >= kMaxJobCount) {
                    print_error_message("tcsh: too many jobs; dropping background job\n");
                    setVariableList("status", { "1" });
                    return;
                }
                HANDLE hJob = CreateJobObjectW(NULL, NULL);
                if (hJob != NULL) {
                    AssignProcessToJobObject(hJob, hProcess.get());
                }
                jobList.push_back({ nextJobId++, hJob, hProcess.detach(), command.sourceText, true });
                setVariableList("status", { "0" });
            } else if (isLast) {
                g_foregroundProcessGroupId.store(pi.dwProcessId, std::memory_order_relaxed);
                WaitForSingleObject(hProcess.get(), INFINITE);
                g_foregroundProcessGroupId.store(0, std::memory_order_relaxed);
                DWORD exitCode = 0;
                if (GetExitCodeProcess(hProcess.get(), &exitCode)) {
                    setVariableList("status", { std::to_string(exitCode) });
                }
            }
        } else {
            print_error_message("tcsh: failed to create process\n");
            setVariableList("status", { "1" });
            return;
        }
    }
}

void TcshEngine::executeScriptLines(const std::vector<std::string>& lines) {
    ScriptFrameGuard guard(this);
    if (!guard.ok()) {
        print_error_message("tcsh: script nesting too deep\n");
        return;
    }

    size_t pc = 0;
    while (pc < lines.size() && running) {
        if (g_interrupted.load(std::memory_order_relaxed)) {
            g_interrupted.store(false, std::memory_order_relaxed);
            auto trapIt = trapHandlers.find("INT");
            if (trapIt == trapHandlers.end()) trapIt = trapHandlers.find("BREAK");
            if (trapIt != trapHandlers.end()) {
                std::string handler = trapIt->second;
                if (!handler.empty() && handler != ":") {
                    if (handler.rfind("goto ", 0) == 0) {
                        std::string label = handler.substr(5) + ":";
                        bool found = false;
                        for (size_t l = 0; l < lines.size(); ++l) {
                            if (lines[l].find(label) != std::string::npos) { pc = l; found = true; break; }
                        }
                        if (found) continue;
                    } else {
                        executeCommandLine(handler);
                        continue;
                    }
                } else {
                    // Ignored (onintr -)
                    continue;
                }
            } else {
                break;
            }
        }
        if (pc >= kMaxScriptLines) {
            print_error_message("tcsh: script contains too many lines\n");
            return;
        }

        std::string line = stripInlineComment(lines[pc]);
        if (line.size() > kMaxScriptLineLength) {
            print_error_message("tcsh: script line too long\n");
            return;
        }
        std::string trimmed = line;
        trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](unsigned char ch) { return !std::isspace(ch); }));

        if (trimmed.empty() || trimmed[0] == '#') { pc++; continue; }

        std::vector<std::string> tokens = tokenize(trimmed, ' ');
        if (tokens.empty()) { pc++; continue; }

        // Goto Label Jump
        if (tokens[0] == "goto" && tokens.size() > 1) {
            std::string label = tokens[1] + ":";
            bool found = false;
            for (size_t l = 0; l < lines.size(); ++l) {
                if (lines[l].find(label) != std::string::npos) { pc = l; found = true; break; }
            }
            if (found) continue;
        }

        // Switch / Case Control Block Engine
        if (tokens[0] == "switch" && tokens.size() > 1) {
            std::string matchVal = stripOuterQuotes(expandVariables(tokens[1]));
            size_t endswPos = pc + 1;
            while (endswPos < lines.size()) {
                std::vector<std::string> lineTokens = tokenize(lines[endswPos], ' ');
                if (!lineTokens.empty() && lineTokens[0] == "endsw") break;
                endswPos++;
            }

            bool executingCase = false;
            for (size_t l = pc + 1; l < endswPos; ++l) {
                std::vector<std::string> lineTokens = tokenize(lines[l], ' ');
                if (lineTokens.empty()) continue;

                if (lineTokens[0] == "case") {
                    if (lineTokens.size() > 1) {
                        std::string pattern = stripOuterQuotes(lineTokens[1]);
                        if (!pattern.empty() && pattern.back() == ':') pattern.pop_back();
                        if (wildcard_match(pattern, matchVal)) executingCase = true;
                    }
                } else if (lineTokens[0] == "default:") {
                    executingCase = true;
                } else if (lineTokens[0] == "breaksw") {
                    break;
                } else if (executingCase) {
                    executeCommandLine(lines[l]);
                }
            }
            pc = endswPos + 1;
            continue;
        }

        // Foreach Loop Engine
        if (tokens[0] == "foreach") {
            std::string varName;
            std::vector<std::string> loopItems;

            size_t openParen = trimmed.find('(');
            size_t closeParen = trimmed.rfind(')');
            if (openParen != std::string::npos && closeParen != std::string::npos && closeParen > openParen) {
                std::string header = trimmed.substr(0, openParen);
                std::vector<std::string> headerTokens = tokenize(header, ' ');
                if (headerTokens.size() > 1) {
                    varName = stripOuterQuotes(headerTokens[1]);
                }

                std::string itemText = trimmed.substr(openParen + 1, closeParen - openParen - 1);
                std::vector<std::string> itemTokens = tokenize(itemText, ' ');
                for (const auto& item : itemTokens) {
                    std::string cleaned = stripOuterQuotes(item);
                    if (!cleaned.empty()) loopItems.push_back(cleaned);
                }
            }

            if (varName.empty() && !loopItems.empty()) {
                varName = "item";
            }

            size_t blockStart = pc + 1;
            size_t blockEnd = blockStart;
            int depth = 1;
            while (blockEnd < lines.size() && depth > 0) {
                std::vector<std::string> lineTokens = tokenize(lines[blockEnd], ' ');
                if (!lineTokens.empty()) {
                    if (lineTokens[0] == "foreach" || lineTokens[0] == "while") depth++;
                    else if (lineTokens[0] == "end") depth--;
                }
                if (depth == 0) break;
                blockEnd++;
            }

            std::vector<std::string> blockLines(lines.begin() + blockStart, lines.begin() + blockEnd);
            for (const auto& item : loopItems) {
                setVariableList(varName, { item });
                executeScriptLines(blockLines);
                if (scriptDirective == ScriptDirective::Continue) {
                    scriptDirective = ScriptDirective::None;
                    continue;
                }
                if (scriptDirective == ScriptDirective::Break) {
                    scriptDirective = ScriptDirective::None;
                    pc = blockEnd + 1;
                    goto script_loop_continue;
                }
            }
            pc = blockEnd + 1;
            continue;
        }

        // While Loop Engine
        if (tokens[0] == "while" && tokens.size() >= 2) {
            std::string condStr = trimmed.substr(trimmed.find('('));
            size_t blockStart = pc + 1;
            size_t blockEnd = blockStart;
            int depth = 1;
            while (blockEnd < lines.size() && depth > 0) {
                std::vector<std::string> lineTokens = tokenize(lines[blockEnd], ' ');
                if (!lineTokens.empty()) {
                    if (lineTokens[0] == "foreach" || lineTokens[0] == "while") depth++;
                    else if (lineTokens[0] == "end") depth--;
                }
                if (depth == 0) break;
                blockEnd++;
            }

            std::vector<std::string> blockLines(lines.begin() + blockStart, lines.begin() + blockEnd);
            while (evalCondition(condStr) && running && !g_interrupted.load(std::memory_order_relaxed)) {
                executeScriptLines(blockLines);
                if (scriptDirective == ScriptDirective::Continue) {
                    scriptDirective = ScriptDirective::None;
                    continue;
                }
                if (scriptDirective == ScriptDirective::Break) {
                    scriptDirective = ScriptDirective::None;
                    break;
                }
            }
            pc = blockEnd + 1;
            continue;
        }

        // Single-line or Multi-line If Engine
        if (tokens[0] == "if" && tokens.size() > 1) {
            bool hasThen = false;
            std::vector<std::string> lineTokens = tokenize(trimmed, ' ');
            if (!lineTokens.empty() && lineTokens.back() == "then") {
                hasThen = true;
            }

            if (hasThen) {
                std::string conditionText;
                size_t firstParen = trimmed.find('(');
                if (firstParen != std::string::npos) {
                    int pDepth = 0;
                    size_t matchParen = std::string::npos;
                    for (size_t i = firstParen; i < trimmed.length(); ++i) {
                        if (trimmed[i] == '(') pDepth++;
                        else if (trimmed[i] == ')') {
                            pDepth--;
                            if (pDepth == 0) {
                                matchParen = i;
                                break;
                            }
                        }
                    }
                    if (matchParen != std::string::npos) {
                        conditionText = trimmed.substr(firstParen + 1, matchParen - firstParen - 1);
                    }
                }

                if (conditionText.empty()) {
                    size_t start = trimmed.find_first_not_of(" \t", 2);
                    size_t thenPos = trimmed.find("then");
                    if (start != std::string::npos && thenPos != std::string::npos) {
                        conditionText = trimmed.substr(start, thenPos - start);
                    }
                }

                while (!conditionText.empty() && std::isspace(static_cast<unsigned char>(conditionText.back()))) {
                    conditionText.pop_back();
                }
                while (!conditionText.empty() && std::isspace(static_cast<unsigned char>(conditionText.front()))) {
                    conditionText.erase(conditionText.begin());
                }

                size_t elsePos = std::string::npos;
                size_t endifPos = std::string::npos;
                int ifDepth = 1;
                for (size_t l = pc + 1; l < lines.size(); ++l) {
                    std::vector<std::string> subTokens = tokenize(lines[l], ' ');
                    if (subTokens.empty()) continue;
                    if (subTokens[0] == "if") {
                        ifDepth++;
                    } else if (subTokens[0] == "else" && ifDepth == 1) {
                        elsePos = l;
                    } else if (subTokens[0] == "endif") {
                        ifDepth--;
                        if (ifDepth == 0) {
                            endifPos = l;
                            break;
                        }
                    }
                }

                bool condResult = evalCondition(conditionText);
                if (condResult) {
                    if (elsePos != std::string::npos) {
                        std::vector<std::string> blockLines(lines.begin() + pc + 1, lines.begin() + elsePos);
                        executeScriptLines(blockLines);
                    } else if (endifPos != std::string::npos) {
                        std::vector<std::string> blockLines(lines.begin() + pc + 1, lines.begin() + endifPos);
                        executeScriptLines(blockLines);
                    }
                } else if (elsePos != std::string::npos && endifPos != std::string::npos) {
                    std::vector<std::string> blockLines(lines.begin() + elsePos + 1, lines.begin() + endifPos);
                    executeScriptLines(blockLines);
                }

                if (endifPos != std::string::npos) {
                    pc = endifPos + 1;
                } else {
                    pc++;
                }
                continue;
            } else {
                size_t firstParen = trimmed.find('(');
                if (firstParen != std::string::npos) {
                    int pDepth = 0;
                    size_t matchParen = std::string::npos;
                    for (size_t i = firstParen; i < trimmed.length(); ++i) {
                        if (trimmed[i] == '(') pDepth++;
                        else if (trimmed[i] == ')') {
                            pDepth--;
                            if (pDepth == 0) {
                                matchParen = i;
                                break;
                            }
                        }
                    }

                    if (matchParen != std::string::npos && matchParen + 1 < trimmed.length()) {
                        std::string condPart = trimmed.substr(firstParen, matchParen - firstParen + 1);
                        std::string subCmd = trimmed.substr(matchParen + 1);
                        if (evalCondition(condPart)) {
                            executeCommandLine(subCmd);
                        }
                    }
                }
                pc++;
                continue;
            }
        }

        executeCommandLine(trimmed);
        if (scriptDirective == ScriptDirective::Return) return;
        if (scriptDirective == ScriptDirective::Break || scriptDirective == ScriptDirective::Continue) return;
        pc++;

    script_loop_continue:
        if (scriptDirective == ScriptDirective::Return) {
            return;
        }
        if (scriptDirective == ScriptDirective::Break || scriptDirective == ScriptDirective::Continue) {
            return;
        }
    }
}

void TcshEngine::executeSingleCommandLine(std::string line) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) return;
    if (line.size() > kMaxScriptLineLength) {
        print_error_message("tcsh: command line too long\n");
        setVariableList("status", { "1" });
        return;
    }

    line = stripInlineComment(line);
    line = expandHistory(line);
    line = expandVariables(line);

    ParsedPipeline pipeline;
    if (!parseCommandLine(line, pipeline) || pipeline.commands.empty()) return;

    if (pipeline.commands.size() == 1) {
        ParsedCommand& command = pipeline.commands[0];
        if (command.args.empty()) return;

        line = expandLeadingAlias(line, command.args);
        if (!parseCommandLine(line, pipeline) || pipeline.commands.empty()) {
            setVariableList("status", { "1" });
            return;
        }

        if (executeBuiltin(pipeline.commands[0].args)) return;
    }

    executePipeline(pipeline);
}

void TcshEngine::executeCommandLine(std::string line) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) return;
    if (line.size() > kMaxScriptLineLength) {
        print_error_message("tcsh: command line too long\n");
        setVariableList("status", { "1" });
        return;
    }

    std::vector<std::string> operators;
    std::vector<std::string> commands = splitCommandSequence(line, operators);
    int lastStatus = std::atoi(getVariableString("status").c_str());

    for (size_t i = 0; i < commands.size(); ++i) {
        if (i > 0) {
            const std::string& op = operators[i - 1];
            if (op == "&&" && lastStatus != 0) break;
            if (op == "||" && lastStatus == 0) break;
        }

        executeSingleCommandLine(commands[i]);
        lastStatus = std::atoi(getVariableString("status").c_str());
    }
}

bool TcshEngine::runScript(const std::string& filename, const std::vector<std::string>& args) {
    std::ifstream script(filename);
    if (!script.is_open()) {
        print_error_message("tcsh: Cannot open script file: " + filename + "\n");
        return false;
    }

    std::string previousScriptName = scriptName;
    std::vector<std::string> previousPositionalArgs = positionalArgs;
    bool previousExecutingScript = executingScript;

    setScriptArguments(filename, args);
    executingScript = true;
    std::vector<std::string> lines;
    std::string rawLine;
    std::string accumulated = "";
    while (std::getline(script, rawLine)) {
        if (!rawLine.empty() && rawLine.back() == '\r') rawLine.pop_back();
        if (!rawLine.empty() && rawLine.back() == '\\') {
            rawLine.pop_back();
            accumulated += rawLine + " ";
        } else {
            accumulated += rawLine;
            if (accumulated.size() > kMaxScriptLineLength) {
                print_error_message("tcsh: script line too long\n");
                executingScript = previousExecutingScript;
                scriptName = previousScriptName;
                positionalArgs = previousPositionalArgs;
                scriptDirective = ScriptDirective::None;
                return false;
            }
            lines.push_back(accumulated);
            if (lines.size() > kMaxScriptLines) {
                print_error_message("tcsh: script contains too many lines\n");
                executingScript = previousExecutingScript;
                scriptName = previousScriptName;
                positionalArgs = previousPositionalArgs;
                scriptDirective = ScriptDirective::None;
                return false;
            }
            accumulated = "";
        }
    }
    if (!accumulated.empty()) {
        if (accumulated.size() > kMaxScriptLineLength) {
            print_error_message("tcsh: script line too long\n");
            executingScript = previousExecutingScript;
            scriptName = previousScriptName;
            positionalArgs = previousPositionalArgs;
            scriptDirective = ScriptDirective::None;
            return false;
        }
        lines.push_back(accumulated);
        if (lines.size() > kMaxScriptLines) {
            print_error_message("tcsh: script contains too many lines\n");
            executingScript = previousExecutingScript;
            scriptName = previousScriptName;
            positionalArgs = previousPositionalArgs;
            scriptDirective = ScriptDirective::None;
            return false;
        }
    }

    executeScriptLines(lines);
    executingScript = previousExecutingScript;
    scriptName = previousScriptName;
    positionalArgs = previousPositionalArgs;
    scriptDirective = ScriptDirective::None;
    return true;
}

bool TcshEngine::listJobs(const std::vector<std::string>& args) {
    updateJobs();

    bool longFormat = false;
    bool pidOnly = false;
    bool endOfOptions = false;
    std::vector<DWORD> requestedJobIds;
    for (size_t i = 1; i < args.size(); ++i) {
        const std::string token = stripOuterQuotes(args[i]);
        if (!endOfOptions && token == "--") {
            endOfOptions = true;
            continue;
        }
        if (!endOfOptions && token.size() > 1 && token[0] == '-') {
            for (size_t flagIndex = 1; flagIndex < token.size(); ++flagIndex) {
                if (token[flagIndex] == 'l') longFormat = true;
                else if (token[flagIndex] == 'p') pidOnly = true;
                else {
                    print_error_message("jobs: invalid option: " + token + "\n");
                    return false;
                }
            }
            continue;
        }

        std::string jobSpec = token;
        if (!jobSpec.empty() && jobSpec[0] == '%') jobSpec.erase(jobSpec.begin());
        int parsed = 0;
        if (!tryParseNonNegativeInt(jobSpec, parsed, "jobs", "job id") || parsed <= 0) return false;
        requestedJobIds.push_back(static_cast<DWORD>(parsed));
    }

    for (const auto& job : jobList) {
        if (!requestedJobIds.empty() &&
            std::find(requestedJobIds.begin(), requestedJobIds.end(), job.id) == requestedJobIds.end()) {
            continue;
        }

        DWORD pid = (job.hProcess != NULL) ? GetProcessId(job.hProcess) : 0;
        if (pidOnly) {
            std::cout << pid << "\n";
        } else if (longFormat) {
            std::cout << "[" << job.id << "] " << pid << " "
                      << (job.isRunning ? "Running" : "Done") << "\t" << job.command << "\n";
        } else {
            std::cout << "[" << job.id << "] " << (job.isRunning ? "Running" : "Done") << "\t" << job.command << "\n";
        }
    }
    return true;
}

void TcshEngine::bringJobToForeground(DWORD jobId) {
    for (auto& job : jobList) {
        if (job.id == jobId) {
            if (!job.isRunning || !job.hProcess) {
                print_error_message("fg: job not running\n");
                setVariableList("status", { "1" });
                return;
            }

            WaitForSingleObject(job.hProcess, INFINITE);
            DWORD exitCode = 0;
            if (GetExitCodeProcess(job.hProcess, &exitCode)) {
                setVariableList("status", { std::to_string(exitCode) });
            } else {
                setVariableList("status", { "1" });
            }
            job.isRunning = false;
            if (job.hProcess && job.hProcess != INVALID_HANDLE_VALUE) { CloseHandle(job.hProcess); job.hProcess = NULL; }
            if (job.hJob && job.hJob != INVALID_HANDLE_VALUE) { CloseHandle(job.hJob); job.hJob = NULL; }
            return;
        }
    }

    print_error_message("fg: No such job\n");
    setVariableList("status", { "1" });
}

void TcshEngine::sendJobToBackground(DWORD jobId) {
    for (auto& job : jobList) {
        if (job.id == jobId) {
            if (!job.isRunning) {
                print_error_message("bg: job already finished\n");
                setVariableList("status", { "1" });
                return;
            }

            std::cout << "[" << job.id << "] " << job.command << " &\n";
            setVariableList("status", { "0" });
            return;
        }
    }

    print_error_message("bg: No such job\n");
    setVariableList("status", { "1" });
}

void TcshEngine::run() {
    // Keep startup close to real tcsh: minimal shell/version line before first prompt.
    std::cout << "CrossShellTCSH 6.24.00\n\n";

    while (running) {
        std::string line = readLineWithEditing();
        if (!line.empty()) {
            history.push_back(line);
            executeCommandLine(line);
            scriptDirective = ScriptDirective::None;
        }
    }
}

static int runInternalSelfTests() {
    int passed = 0;
    int failed = 0;

    auto assert_eq = [&](const std::string& actual, const std::string& expected, const std::string& test_name) {
        if (actual == expected) {
            std::cout << "PASS: " << test_name << "\n";
            passed++;
        } else {
            std::cerr << "FAIL: " << test_name << " (Expected: '" << expected << "', Actual: '" << actual << "')\n";
            failed++;
        }
    };

    auto assert_true = [&](bool condition, const std::string& test_name) {
        if (condition) {
            std::cout << "PASS: " << test_name << "\n";
            passed++;
        } else {
            std::cerr << "FAIL: " << test_name << " (Expected: true, Actual: false)\n";
            failed++;
        }
    };

    std::cout << "--- Running CrossShellTCSH Internal Self-Tests ---\n";

    // 1. Quoting and String Utilities
    {
        TcshEngine shell(false);
        assert_eq(shell.stripOuterQuotes("\"hello\""), "hello", "stripOuterQuotes double quotes");
        assert_eq(shell.stripOuterQuotes("'world'"), "world", "stripOuterQuotes single quotes");
        assert_eq(shell.stripOuterQuotes("unquoted"), "unquoted", "stripOuterQuotes unquoted");
        assert_eq(shell.normalizePathSeparators("foo/bar/baz"), "foo\\bar\\baz", "normalizePathSeparators");
    }

    // 2. Wildcard Matching
    {
        assert_true(wildcard_match("*.txt", "readme.txt"), "wildcard_match simple star");
        assert_true(wildcard_match("file?.log", "file1.log"), "wildcard_match question mark");
        assert_true(!wildcard_match("*.cpp", "tcsh.h"), "wildcard_match mismatch");
        assert_true(wildcard_match("src/*/*.cpp", "src/dir/file.cpp"), "wildcard_match nested path");
    }

    // 3. Variable Assignment and Scope
    {
        TcshEngine shell(false);
        shell.executeCommandLine("set myvar = testvalue");
        assert_eq(shell.getVar("myvar"), "testvalue", "set scalar variable");

        shell.executeCommandLine("set mylist = ( alpha beta gamma )");
        assert_eq(shell.getVar("mylist"), "alpha beta gamma", "set list variable");

        shell.executeCommandLine("setenv TCSH_TEST_VAR custom_val");
        char buf[256] = {};
        GetEnvironmentVariableA("TCSH_TEST_VAR", buf, sizeof(buf));
        assert_eq(std::string(buf), "custom_val", "setenv sets environment variable");

        shell.executeCommandLine("unsetenv TCSH_TEST_VAR");
        DWORD len = GetEnvironmentVariableA("TCSH_TEST_VAR", buf, sizeof(buf));
        assert_true(len == 0, "unsetenv removes environment variable");

        shell.executeCommandLine("unset myvar");
        assert_eq(shell.getVar("myvar"), "", "unset removes shell variable");
    }

    // 4. Arithmetic (@) Evaluation
    {
        TcshEngine shell(false);
        shell.executeCommandLine("@ x = 10 + 5");
        assert_eq(shell.getVar("x"), "15", "@ addition");

        shell.executeCommandLine("@ y = 3 * 4 + 2");
        assert_eq(shell.getVar("y"), "14", "@ multiplication precedence");

        shell.executeCommandLine("@ z = (20 - 5) / 3");
        assert_eq(shell.getVar("z"), "5", "@ parenthesized division");

        shell.executeCommandLine("@ m = 17 % 5");
        assert_eq(shell.getVar("m"), "2", "@ modulo");

        shell.executeCommandLine("@ sub = 10 - 25");
        assert_eq(shell.getVar("sub"), "-15", "@ subtraction negative result");
    }

    // 5. Conditional Expressions (test)
    {
        TcshEngine shell(false);
        shell.executeCommandLine("test \"hello\" == \"hello\"");
        assert_eq(shell.getVar("status"), "0", "test equality returns 0");

        shell.executeCommandLine("test \"apple\" != \"orange\"");
        assert_eq(shell.getVar("status"), "0", "test inequality returns 0");

        shell.executeCommandLine("test \"alpha\" == \"beta\"");
        assert_eq(shell.getVar("status"), "1", "test false equality returns 1");
    }

    // 6. Aliases
    {
        TcshEngine shell(false);
        shell.executeCommandLine("alias myecho echo");
        shell.executeCommandLine("unalias myecho");
        assert_eq(shell.getVar("status"), "0", "alias and unalias command");
    }

    // 7. Directory Stack (pushd, popd, dirs)
    {
        TcshEngine shell(false);
        shell.executeCommandLine("dirs");
        assert_eq(shell.getVar("status"), "0", "dirs executes successfully");
    }

    // 8. Status Codes & Logic Sequencing
    {
        TcshEngine shell(false);
        shell.executeCommandLine("true");
        assert_eq(shell.getVar("status"), "0", "true returns 0");

        shell.executeCommandLine("false");
        assert_eq(shell.getVar("status"), "1", "false returns 1");

        shell.executeCommandLine("set seq = 0");
        shell.executeCommandLine("true && set seq = 1");
        assert_eq(shell.getVar("seq"), "1", "&& sequence on success");

        shell.executeCommandLine("false && set seq = 2");
        assert_eq(shell.getVar("seq"), "1", "&& sequence skipped on failure");

        shell.executeCommandLine("false || set seq = 3");
        assert_eq(shell.getVar("seq"), "3", "|| sequence on failure");
    }

    // 9. One-shot -c command execution
    {
        TcshEngine shell(false);
        int code = shell.executeCommandString("set result = success", "tcsh");
        assert_eq(std::to_string(code), "0", "executeCommandString returns 0");
        assert_eq(shell.getVar("result"), "success", "executeCommandString sets variable");
    }

    // 10. echotc Builtin
    {
        TcshEngine shell(false);
        shell.executeCommandLine("echotc normal");
        assert_eq(shell.getVar("status"), "0", "echotc normal succeeds");

        shell.executeCommandLine("echotc bold");
        assert_eq(shell.getVar("status"), "0", "echotc bold succeeds");

        shell.executeCommandLine("echotc non_existent_cap");
        assert_eq(shell.getVar("status"), "1", "echotc invalid cap returns 1");
    }

    // 11. ls-F Builtin
    {
        TcshEngine shell(false);
        shell.executeCommandLine("ls-F src");
        assert_eq(shell.getVar("status"), "0", "ls-F src succeeds");
    }

    // 12. onintr Builtin
    {
        TcshEngine shell(false);
        shell.executeCommandLine("onintr -");
        assert_eq(shell.getVar("status"), "0", "onintr - succeeds");

        shell.executeCommandLine("onintr cleanup_handler");
        assert_eq(shell.getVar("status"), "0", "onintr label succeeds");

        shell.executeCommandLine("onintr");
        assert_eq(shell.getVar("status"), "0", "onintr restore succeeds");
    }

    std::cout << "\n--- Self-Test Summary: " << passed << " passed, " << failed << " failed ---\n";
    return (failed == 0) ? 0 : 1;
}

int main(int argc, char* argv[]) {
    enable_ansi_support();

    bool loadRc = true;
    std::string commandString;
    std::string scriptFile;
    std::vector<std::string> scriptArgs;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help" || arg == "-help") {
            TcshEngine shell(false);
            shell.displayHelp();
            return 0;
        }

        if (arg == "-v" || arg == "--version" || arg == "-version") {
            TcshEngine shell(false);
            shell.displayVersion();
            return 0;
        }

        if (arg == "--self-test") {
            return runInternalSelfTests();
        }

        if (arg == "-f" || arg == "--no-rcs" || arg == "-fast") {
            loadRc = false;
            continue;
        }

        if (arg == "-c") {
            if (i + 1 < argc) {
                commandString = argv[++i];
                std::string scriptName = "tcsh";
                if (i + 1 < argc) {
                    scriptName = argv[++i];
                }
                for (int j = i + 1; j < argc; ++j) {
                    scriptArgs.push_back(argv[j]);
                }
                TcshEngine shell(loadRc);
                return shell.executeCommandString(commandString, scriptName, scriptArgs);
            } else {
                print_error_message("tcsh: -c requires an argument\n");
                return 1;
            }
        }

        if (arg == "--") {
            if (i + 1 < argc) {
                scriptFile = argv[++i];
                for (int j = i + 1; j < argc; ++j) {
                    scriptArgs.push_back(argv[j]);
                }
            }
            break;
        }

        // Script file passed as argument
        scriptFile = arg;
        for (int j = i + 1; j < argc; ++j) {
            scriptArgs.push_back(argv[j]);
        }
        break;
    }

    TcshEngine shell(loadRc);
    if (!scriptFile.empty()) {
        bool ok = shell.runScript(scriptFile, scriptArgs);
        return ok ? shell.getStatus() : 1;
    }

    shell.run();
    return 0;
}
