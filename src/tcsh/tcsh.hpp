#pragma once

#define _CRT_SECURE_NO_WARNINGS
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

// --- Global Signals and Control Events ---
extern std::atomic<bool> g_interrupted;
extern std::atomic<DWORD> g_foregroundProcessGroupId;
extern std::atomic<bool> g_ctrlForwarding;

BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType);

// --- Console and String Utility Functions ---
void enable_ansi_support();
std::wstring string_to_wstring(const std::string& str);
std::string wstring_to_string(const std::wstring& str);
bool wildcard_match(const std::string& pattern, const std::string& text);
void print_error_message(const std::string& message);
std::wstring get_windows_release_text();

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

// --- Job Control Data Structures ---
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

// --- Parser Data Structures ---
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

// --- Help & Builtin Metadata ---
struct TcshHelpEntry {
    const char* name;
    const char* description;
};

const std::vector<TcshHelpEntry>& tcsh_help_entries();
const std::vector<std::string>& tcsh_builtin_names();
