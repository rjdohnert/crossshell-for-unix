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

#include <iostream>
#include <iomanip>
#include <string>
#include <string_view>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <memory>
#include <optional>
#include <set>
#include <map>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <io.h>
#include <fcntl.h>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")

// ============================================================================
// Native NT API Function Pointers (for SIGSTOP / SIGCONT process suspension)
// ============================================================================
typedef LONG(NTAPI* PFN_NtSuspendProcess)(HANDLE ProcessHandle);
typedef LONG(NTAPI* PFN_NtResumeProcess)(HANDLE ProcessHandle);

// ============================================================================
// RAII Wrapper for Windows Handles
// ============================================================================
struct HandleDeleter {
    void operator()(HANDLE handle) const {
        if (handle && handle != INVALID_HANDLE_VALUE) {
            ::CloseHandle(handle);
        }
    }
};
using UniqueHandle = std::unique_ptr<void, HandleDeleter>;

// ============================================================================
// Privilege Manager: Enables SeDebugPrivilege for administrator access
// ============================================================================
class PrivilegeManager {
public:
    static bool enableDebugPrivilege() {
        HANDLE hToken = nullptr;
        if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
            return false;
        }
        UniqueHandle tokenHandle(hToken);

        TOKEN_PRIVILEGES tp{};
        LUID luid{};

        if (!::LookupPrivilegeValueW(nullptr, L"SeDebugPrivilege", &luid)) {
            return false;
        }

        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        return ::AdjustTokenPrivileges(tokenHandle.get(), FALSE, &tp, sizeof(TOKEN_PRIVILEGES), nullptr, nullptr) &&
               (::GetLastError() != ERROR_NOT_ALL_ASSIGNED);
    }
};

// ============================================================================
// Signal Definitions & Mapping
// ============================================================================
enum class SignalType {
    SIGNULL = 0,   // Check existence
    SIGHUP  = 1,   // Hangup -> Graceful close
    SIGINT  = 2,   // Interrupt -> CTRL+C / Graceful
    SIGQUIT = 3,   // Quit -> CTRL+BREAK / Graceful
    SIGKILL = 9,   // Kill -> Immediate hard termination
    SIGUSR1 = 10,  // User-defined 1
    SIGUSR2 = 12,  // User-defined 2
    SIGTERM = 15,  // Terminate -> Graceful WM_CLOSE fallback
    SIGCONT = 18,  // Continue -> Resume suspended process
    SIGSTOP = 19   // Stop -> Suspend process
};

struct SignalInfo {
    int number;
    std::string name;
    std::string description;
    SignalType type;
};

class SignalRegistry {
private:
    std::vector<SignalInfo> signals;

public:
    SignalRegistry() {
        signals = {
            { 0,  "SIGNULL", "Signal 0: Check process existence & access", SignalType::SIGNULL },
            { 1,  "SIGHUP",  "Hangup: Send WM_CLOSE to top-level windows",  SignalType::SIGHUP },
            { 2,  "SIGINT",  "Interrupt: Send Ctrl+C or WM_CLOSE",          SignalType::SIGINT },
            { 3,  "SIGQUIT", "Quit: Send Ctrl+Break or WM_CLOSE",          SignalType::SIGQUIT },
            { 9,  "SIGKILL", "Kill: Immediate, uncatchable termination",    SignalType::SIGKILL },
            { 10, "SIGUSR1", "User-defined signal 1",                      SignalType::SIGUSR1 },
            { 12, "SIGUSR2", "User-defined signal 2",                      SignalType::SIGUSR2 },
            { 15, "SIGTERM", "Terminate: Graceful termination request",    SignalType::SIGTERM },
            { 18, "SIGCONT", "Continue: Resume paused process threads",    SignalType::SIGCONT },
            { 19, "SIGSTOP", "Stop: Suspend all process threads",          SignalType::SIGSTOP }
        };
    }

    std::optional<SignalInfo> find(std::string_view query) const {
        std::string upper;
        for (char c : query) upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

        if (upper.rfind("SIG", 0) == 0) {
            upper = upper.substr(3);
        }

        // Try numeric match
        try {
            size_t pos = 0;
            int num = std::stoi(upper, &pos);
            if (pos == upper.size()) {
                for (const auto& sig : signals) {
                    if (sig.number == num) return sig;
                }
            }
        } catch (...) {}

        // Try mnemonic match
        for (const auto& sig : signals) {
            std::string sigNameUpper = sig.name;
            if (sigNameUpper.rfind("SIG", 0) == 0) sigNameUpper = sigNameUpper.substr(3);
            if (sigNameUpper == upper || sig.name == upper) {
                return sig;
            }
        }
        return std::nullopt;
    }

    const std::vector<SignalInfo>& getAll() const { return signals; }
};

// ============================================================================
// Process Enumeration & Resolution Helper
// ============================================================================
struct TargetProcess {
    DWORD pid;
    std::string name;
    DWORD parentPid;
};

class ProcessEnumerator {
public:
    static std::vector<TargetProcess> getSnapshot() {
        std::vector<TargetProcess> list;
        HANDLE hSnap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap == INVALID_HANDLE_VALUE) return list;
        UniqueHandle snapHandle(hSnap);

        PROCESSENTRY32W pe{};
        pe.dwSize = sizeof(pe);

        if (::Process32FirstW(snapHandle.get(), &pe)) {
            do {
                std::string exeName;
                int len = ::WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, nullptr, 0, nullptr, nullptr);
                if (len > 0) {
                    exeName.resize(len - 1);
                    ::WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, exeName.data(), len, nullptr, nullptr);
                }
                list.push_back({ pe.th32ProcessID, exeName, pe.th32ParentProcessID });
            } while (::Process32NextW(snapHandle.get(), &pe));
        }
        return list;
    }

    static std::vector<DWORD> resolveTarget(const std::string& targetStr, bool includeTree) {
        std::set<DWORD> resolvedPids;
        auto snapshot = getSnapshot();

        bool isNumeric = !targetStr.empty() && std::all_of(targetStr.begin(), targetStr.end(), [](char c) {
            return std::isdigit(static_cast<unsigned char>(c));
        });

        if (isNumeric) {
            DWORD pid = static_cast<DWORD>(std::stoul(targetStr));
            resolvedPids.insert(pid);
        } else {
            std::string query = targetStr;
            std::transform(query.begin(), query.end(), query.begin(), ::tolower);
            for (const auto& proc : snapshot) {
                std::string procNameLower = proc.name;
                std::transform(procNameLower.begin(), procNameLower.end(), procNameLower.begin(), ::tolower);
                if (procNameLower == query || procNameLower == query + ".exe") {
                    resolvedPids.insert(proc.pid);
                }
            }
        }

        if (includeTree) {
            std::set<DWORD> treePids = resolvedPids;
            for (DWORD rootPid : resolvedPids) {
                collectChildren(rootPid, snapshot, treePids);
            }
            resolvedPids = std::move(treePids);
        }

        return std::vector<DWORD>(resolvedPids.begin(), resolvedPids.end());
    }

private:
    static void collectChildren(DWORD parentPid, const std::vector<TargetProcess>& snapshot, std::set<DWORD>& outPids) {
        for (const auto& proc : snapshot) {
            if (proc.parentPid == parentPid && proc.pid != 0 && proc.pid != parentPid) {
                if (outPids.insert(proc.pid).second) {
                    collectChildren(proc.pid, snapshot, outPids);
                }
            }
        }
    }
};

// ============================================================================
// Process Signaler: Action implementation
// ============================================================================
class ProcessSignaler {
private:
    PFN_NtSuspendProcess NtSuspendProcess = nullptr;
    PFN_NtResumeProcess NtResumeProcess = nullptr;

    struct EnumWndData {
        DWORD targetPid;
        std::vector<HWND> windows;
    };

    static BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam) {
        auto* data = reinterpret_cast<EnumWndData*>(lParam);
        DWORD procId = 0;
        ::GetWindowThreadProcessId(hwnd, &procId);
        if (procId == data->targetPid && ::IsWindowVisible(hwnd)) {
            data->windows.push_back(hwnd);
        }
        return TRUE;
    }

    bool sendGracefulClose(DWORD pid, DWORD timeoutMs = 2000) const {
        EnumWndData data{ pid, {} };
        ::EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&data));

        if (data.windows.empty()) {
            return false;
        }

        for (HWND hwnd : data.windows) {
            ::PostMessageW(hwnd, WM_CLOSE, 0, 0);
        }

        HANDLE hProc = ::OpenProcess(SYNCHRONIZE, FALSE, pid);
        if (hProc) {
            UniqueHandle procHandle(hProc);
            DWORD waitRes = ::WaitForSingleObject(procHandle.get(), timeoutMs);
            return (waitRes == WAIT_OBJECT_0);
        }
        return false;
    }

public:
    ProcessSignaler() {
        HMODULE hNtDll = ::GetModuleHandleW(L"ntdll.dll");
        if (hNtDll) {
            NtSuspendProcess = reinterpret_cast<PFN_NtSuspendProcess>(::GetProcAddress(hNtDll, "NtSuspendProcess"));
            NtResumeProcess = reinterpret_cast<PFN_NtResumeProcess>(::GetProcAddress(hNtDll, "NtResumeProcess"));
        }
    }

    bool dispatchSignal(DWORD pid, const SignalInfo& sig, bool force) const {
        // Signal 0: Process probe / existence check
        if (sig.type == SignalType::SIGNULL) {
            HANDLE hProc = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (!hProc) return false;
            UniqueHandle handle(hProc);
            DWORD exitCode = 0;
            return ::GetExitCodeProcess(handle.get(), &exitCode) && (exitCode == STILL_ACTIVE);
        }

        // SIGSTOP: Suspend process execution
        if (sig.type == SignalType::SIGSTOP) {
            if (!NtSuspendProcess) return false;
            HANDLE hProc = ::OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
            if (!hProc) return false;
            UniqueHandle handle(hProc);
            return (NtSuspendProcess(handle.get()) >= 0);
        }

        // SIGCONT: Resume process execution
        if (sig.type == SignalType::SIGCONT) {
            if (!NtResumeProcess) return false;
            HANDLE hProc = ::OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
            if (!hProc) return false;
            UniqueHandle handle(hProc);
            return (NtResumeProcess(handle.get()) >= 0);
        }

        // SIGINT / SIGQUIT: Console control signals
        if (sig.type == SignalType::SIGINT || sig.type == SignalType::SIGQUIT) {
            DWORD ctrlEvent = (sig.type == SignalType::SIGINT) ? CTRL_C_EVENT : CTRL_BREAK_EVENT;
            if (::GenerateConsoleCtrlEvent(ctrlEvent, pid)) {
                return true;
            }
        }

        // SIGTERM / SIGHUP: Graceful termination attempt
        if ((sig.type == SignalType::SIGTERM || sig.type == SignalType::SIGHUP) && !force) {
            if (sendGracefulClose(pid)) {
                return true;
            }
        }

        // SIGKILL or fallback for unhandled/forced termination
        HANDLE hProc = ::OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
        if (!hProc) return false;
        UniqueHandle handle(hProc);

        UINT exitCode = (sig.number != 0) ? (128 + sig.number) : 1;
        return ::TerminateProcess(handle.get(), exitCode) != FALSE;
    }
};

// ============================================================================
// Command-Line Interface & Application Controller
// ============================================================================
struct Options {
    SignalInfo signal{ 15, "SIGTERM", "Terminate", SignalType::SIGTERM };
    bool force = false;
    bool tree = false;
    bool verbose = false;
    bool listSignals = false;
    bool listTable = false;
    std::vector<std::string> targets;
};

class CommandLineParser {
public:
    static void printHelp(const std::string& /*exeName*/ = "kill") {
        std::cout << R"(kill(1)             CrossShell for UNIX Reference Manual                 kill(1)

    NAME
        kill - send signals to Windows processes

    SYNOPSIS
        kill [-s SIGNAL | -SIGNAL] [OPTIONS] TARGET...
        kill -l [SIGNAL]
        kill -L

    DESCRIPTION
        kill sends signals to Windows processes identified by process ID (PID) or
        executable image name. It provides UNIX-compatible signaling semantics
        adapted to Windows process APIs, including graceful termination (WM_CLOSE),
        hard termination (TerminateProcess), and process suspension/resumption.
        When no targets are supplied, kill reads PIDs from standard input.

    OPTIONS
        -s SIGNAL
            Send specified signal by name or number (e.g., 9, KILL, 15, TERM).

        -SIGNAL
            Shorthand signal specification (e.g., -9, -15, -KILL, -TERM, -STOP).

        -f, --force
            Force immediate termination and skip graceful closing attempts.

        -t, --tree
            Terminate target process and all child processes recursively.

        -v, --verbose
            Display detailed execution diagnostics.

        -l, --list
            List available signal names or translate numeric signal code.

        -L, --table
            Display formatted table of supported signals.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    SUPPORTED SIGNALS
        0, SIGNULL
            Check process existence without sending a signal.

        1, SIGHUP
            Request graceful window closure.

        2, SIGINT
            Send console interrupt (Ctrl+C).

        3, SIGQUIT
            Send console break (Ctrl+Break).

        9, SIGKILL
            Hard termination via TerminateProcess.

        15, SIGTERM
            Graceful termination with fallback to hard termination.

        18, SIGCONT
            Resume suspended process threads.

        19, SIGSTOP
            Freeze process threads.

    EXAMPLES
        kill 1234
            Send SIGTERM to process with PID 1234.

        kill -9 notepad.exe
            Forcefully terminate all running notepad.exe processes.

        kill -STOP 4820
            Freeze process execution on PID 4820.

        kill -CONT 4820
            Resume execution on PID 4820.

        tasklist | awk '{print $2}' | kill -9
            Pipe PIDs directly into kill.

    CrossShell for UNIX                                                    kill(1)
)";
    }

    static void printVersion() {
        std::cout << "kill 1.0.0\n";
    }

    static Options parse(int argc, char* argv[], const SignalRegistry& registry) {
        Options opt;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help" || arg == "/?") {
                printHelp("kill");
                std::exit(0);
            } else if (arg == "-V" || arg == "--version") {
                printVersion();
                std::exit(0);
            } else if (arg == "-l" || arg == "--list") {
                opt.listSignals = true;
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    opt.targets.push_back(argv[++i]);
                }
            } else if (arg == "-L" || arg == "--table") {
                opt.listTable = true;
            } else if (arg == "-f" || arg == "--force") {
                opt.force = true;
            } else if (arg == "-t" || arg == "--tree") {
                opt.tree = true;
            } else if (arg == "-v" || arg == "--verbose") {
                opt.verbose = true;
            } else if (arg == "-s") {
                if (i + 1 >= argc) {
                    std::cerr << "kill: Option -s requires a signal argument.\n";
                    std::exit(1);
                }
                auto found = registry.find(argv[++i]);
                if (!found) {
                    std::cerr << "kill: Unknown signal '" << argv[i] << "'. Use -l for a list.\n";
                    std::exit(1);
                }
                opt.signal = *found;
            } else if (arg.front() == '-' && arg.size() > 1 && !std::isdigit(static_cast<unsigned char>(arg[1])) && registry.find(arg.substr(1))) {
                opt.signal = *registry.find(arg.substr(1));
            } else if (arg.front() == '-' && arg.size() > 1 && std::isdigit(static_cast<unsigned char>(arg[1]))) {
                auto found = registry.find(arg.substr(1));
                if (found) {
                    opt.signal = *found;
                } else {
                    std::cerr << "kill: Invalid signal specification: " << arg << "\n";
                    std::exit(1);
                }
            } else if (arg.front() == '-') {
                std::cerr << "kill: Unknown flag '" << arg << "'. Use -h for help.\n";
                std::exit(1);
            } else {
                opt.targets.push_back(arg);
            }
        }

        return opt;
    }
};

// ============================================================================
// Application Entry and Pipeline Orchestrator
// ============================================================================
class KillApplication {
private:
    Options options;
    SignalRegistry registry;
    ProcessSignaler signaler;

    void printSignalList() const {
        if (!options.targets.empty()) {
            for (const auto& t : options.targets) {
                auto sig = registry.find(t);
                if (sig) {
                    std::cout << sig->number << " " << sig->name << "\n";
                } else {
                    std::cerr << "kill: Unknown signal " << t << "\n";
                }
            }
            return;
        }

        for (const auto& sig : registry.getAll()) {
            std::cout << std::setw(2) << sig.number << ") " << std::setw(8) << std::left << sig.name;
            if (sig.number % 4 == 0) std::cout << "\n";
        }
        std::cout << "\n";
    }

    void printSignalTable() const {
        std::cout << std::left << std::setw(6) << "NUM"
                  << std::setw(12) << "NAME"
                  << "DESCRIPTION / WINDOWS ACTION\n";
        std::cout << std::string(75, '-') << "\n";
        for (const auto& sig : registry.getAll()) {
            std::cout << std::setw(6) << sig.number
                      << std::setw(12) << sig.name
                      << sig.description << "\n";
        }
    }

    std::vector<std::string> collectTargets() {
        std::vector<std::string> allTargets = options.targets;

        // If no targets were specified or '-' is passed, read stdin (piping support)
        bool hasStdInPipe = !_isatty(_fileno(stdin));
        if (allTargets.empty() || (allTargets.size() == 1 && allTargets[0] == "-") || hasStdInPipe) {
            if (hasStdInPipe || (allTargets.size() == 1 && allTargets[0] == "-")) {
                if (!allTargets.empty() && allTargets[0] == "-") {
                    allTargets.clear();
                }
                std::string token;
                while (std::cin >> token) {
                    allTargets.push_back(token);
                }
            }
        }
        return allTargets;
    }

public:
    KillApplication(Options opt) : options(std::move(opt)) {}

    int run() {
        if (options.listSignals) {
            printSignalList();
            return 0;
        }
        if (options.listTable) {
            printSignalTable();
            return 0;
        }

        auto targets = collectTargets();
        if (targets.empty()) {
            std::cerr << "kill: No target processes specified.\n";
            std::cerr << "      Use 'kill --help' for usage syntax.\n";
            return 1;
        }

        PrivilegeManager::enableDebugPrivilege();

        int failureCount = 0;
        int successCount = 0;

        for (const auto& targetStr : targets) {
            auto pids = ProcessEnumerator::resolveTarget(targetStr, options.tree);

            if (pids.empty()) {
                std::cerr << "kill: No active process matching '" << targetStr << "' found.\n";
                failureCount++;
                continue;
            }

            for (DWORD pid : pids) {
                if (pid == 0 || pid == 4) { // Guard against System Idle & System PID
                    if (options.verbose) {
                        std::cerr << "kill: Protected system process (PID " << pid << ") skipped.\n";
                    }
                    continue;
                }

                bool ok = signaler.dispatchSignal(pid, options.signal, options.force);
                if (ok) {
                    successCount++;
                    if (options.verbose) {
                        std::cout << "kill: Signal " << options.signal.name
                                  << " (" << options.signal.number << ") sent to PID " << pid << ".\n";
                    }
                } else {
                    failureCount++;
                    std::cerr << "kill: Failed to send signal " << options.signal.name
                              << " to PID " << pid << " (Error " << ::GetLastError() << ").\n";
                }
            }
        }

        return (failureCount == 0) ? 0 : 1;
    }
};

// ============================================================================
// Main Entry Point
// ============================================================================
int main(int argc, char* argv[]) {
    try {
        SignalRegistry registry;
        Options options = CommandLineParser::parse(argc, argv, registry);
        KillApplication app(std::move(options));
        return app.run();
    } catch (const std::exception& ex) {
        std::cerr << "kill: Fatal Exception: " << ex.what() << "\n";
        return 1;
    }
}