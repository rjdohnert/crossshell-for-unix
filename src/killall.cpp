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
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
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
#include <chrono>
#include <regex>
#include <memory>
#include <optional>
#include <set>
#include <map>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <sddl.h>
#include <io.h>
#include <fcntl.h>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")

// ============================================================================
// NT API Function Definitions (for SIGSTOP / SIGCONT process freeze/thaw)
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
// Privilege Manager: Escalates to SeDebugPrivilege
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
// Time / Duration Utilities
// ============================================================================
class TimeHelper {
public:
    static std::optional<std::chrono::seconds> parseDuration(std::string_view str) {
        if (str.empty()) return std::nullopt;
        
        char unit = str.back();
        std::string numPart;
        int64_t multiplier = 1;

        if (std::isalpha(static_cast<unsigned char>(unit))) {
            numPart = str.substr(0, str.size() - 1);
            switch (std::tolower(static_cast<unsigned char>(unit))) {
                case 's': multiplier = 1; break;
                case 'm': multiplier = 60; break;
                case 'h': multiplier = 3600; break;
                case 'd': multiplier = 86400; break;
                case 'w': multiplier = 604800; break;
                default: return std::nullopt;
            }
        } else {
            numPart = str;
            multiplier = 1; // Default to seconds
        }

        try {
            int64_t val = std::stoll(numPart);
            return std::chrono::seconds(val * multiplier);
        } catch (...) {
            return std::nullopt;
        }
    }

    static std::chrono::seconds getProcessAge(FILETIME creationTime) {
        FILETIME nowFt;
        ::GetSystemTimeAsFileTime(&nowFt);

        ULARGE_INTEGER createVal, nowVal;
        createVal.LowPart = creationTime.dwLowDateTime;
        createVal.HighPart = creationTime.dwHighDateTime;
        nowVal.LowPart = nowFt.dwLowDateTime;
        nowVal.HighPart = nowFt.dwHighDateTime;

        if (nowVal.QuadPart < createVal.QuadPart) return std::chrono::seconds(0);
        
        // 100-nanosecond intervals -> seconds
        uint64_t diffSec = (nowVal.QuadPart - createVal.QuadPart) / 10000000ULL;
        return std::chrono::seconds(diffSec);
    }
};

// ============================================================================
// Signal Definitions & Mapping
// ============================================================================
enum class SignalType {
    SIGNULL = 0,
    SIGHUP  = 1,
    SIGINT  = 2,
    SIGQUIT = 3,
    SIGKILL = 9,
    SIGTERM = 15,
    SIGCONT = 18,
    SIGSTOP = 19
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
            { 0,  "SIGNULL", "Signal 0: Query process existence and permissions", SignalType::SIGNULL },
            { 1,  "SIGHUP",  "Hangup: Send WM_CLOSE to top-level windows",        SignalType::SIGHUP },
            { 2,  "SIGINT",  "Interrupt: Send Ctrl+C event or WM_CLOSE",          SignalType::SIGINT },
            { 3,  "SIGQUIT", "Quit: Send Ctrl+Break event or WM_CLOSE",          SignalType::SIGQUIT },
            { 9,  "SIGKILL", "Kill: Immediate, uncatchable termination",          SignalType::SIGKILL },
            { 15, "SIGTERM", "Terminate: Graceful close request (with fallback)", SignalType::SIGTERM },
            { 18, "SIGCONT", "Continue: Resume frozen/suspended process threads", SignalType::SIGCONT },
            { 19, "SIGSTOP", "Stop: Suspend all execution threads",               SignalType::SIGSTOP }
        };
    }

    std::optional<SignalInfo> find(std::string_view query) const {
        std::string upper;
        for (char c : query) upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

        if (upper.rfind("SIG", 0) == 0) upper = upper.substr(3);

        try {
            size_t pos = 0;
            int num = std::stoi(upper, &pos);
            if (pos == upper.size()) {
                for (const auto& sig : signals) {
                    if (sig.number == num) return sig;
                }
            }
        } catch (...) {}

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
// Process Metadata & Query Engine
// ============================================================================
struct ProcessMeta {
    DWORD pid;
    DWORD parentPid;
    std::string name;
    std::string owner;
    FILETIME creationTime{};
    std::chrono::seconds age{0};
};

class ProcessQueryEngine {
public:
    static std::string getProcessOwner(HANDLE hProcess) {
        HANDLE hToken = nullptr;
        if (!::OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) {
            return "";
        }
        UniqueHandle tokenHandle(hToken);

        DWORD len = 0;
        ::GetTokenInformation(tokenHandle.get(), TokenUser, nullptr, 0, &len);
        if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER) return "";

        std::vector<BYTE> buffer(len);
        if (!::GetTokenInformation(tokenHandle.get(), TokenUser, buffer.data(), len, &len)) {
            return "";
        }

        auto* pTokenUser = reinterpret_cast<TOKEN_USER*>(buffer.data());
        WCHAR name[256] = {0};
        WCHAR domain[256] = {0};
        DWORD nameLen = 256, domainLen = 256;
        SID_NAME_USE snu;

        if (::LookupAccountSidW(nullptr, pTokenUser->User.Sid, name, &nameLen, domain, &domainLen, &snu)) {
            std::string utf8Name;
            int req = ::WideCharToMultiByte(CP_UTF8, 0, name, -1, nullptr, 0, nullptr, nullptr);
            if (req > 0) {
                utf8Name.resize(req - 1);
                ::WideCharToMultiByte(CP_UTF8, 0, name, -1, utf8Name.data(), req, nullptr, nullptr);
            }
            return utf8Name;
        }
        return "";
    }

    static std::vector<ProcessMeta> collectProcesses() {
        std::vector<ProcessMeta> list;
        HANDLE hSnap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap == INVALID_HANDLE_VALUE) return list;
        UniqueHandle snapHandle(hSnap);

        PROCESSENTRY32W pe{};
        pe.dwSize = sizeof(pe);

        if (::Process32FirstW(snapHandle.get(), &pe)) {
            do {
                if (pe.th32ProcessID == 0 || pe.th32ProcessID == 4) continue; // Skip System Idle / System

                std::string exeName;
                int len = ::WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, nullptr, 0, nullptr, nullptr);
                if (len > 0) {
                    exeName.resize(len - 1);
                    ::WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, exeName.data(), len, nullptr, nullptr);
                }

                ProcessMeta meta;
                meta.pid = pe.th32ProcessID;
                meta.parentPid = pe.th32ParentProcessID;
                meta.name = exeName;

                // Inspect times & owner if permissions allow
                HANDLE hProc = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, meta.pid);
                if (hProc) {
                    UniqueHandle procHandle(hProc);
                    FILETIME exitTime, kernelTime, userTime;
                    if (::GetProcessTimes(procHandle.get(), &meta.creationTime, &exitTime, &kernelTime, &userTime)) {
                        meta.age = TimeHelper::getProcessAge(meta.creationTime);
                    }
                    meta.owner = getProcessOwner(procHandle.get());
                }

                list.push_back(std::move(meta));
            } while (::Process32NextW(snapHandle.get(), &pe));
        }
        return list;
    }
};

// ============================================================================
// Process Signaler: Action Execution
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

    bool sendGracefulClose(DWORD pid, DWORD timeoutMs = 1500) const {
        EnumWndData data{ pid, {} };
        ::EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&data));

        if (data.windows.empty()) return false;

        for (HWND hwnd : data.windows) {
            ::PostMessageW(hwnd, WM_CLOSE, 0, 0);
        }

        HANDLE hProc = ::OpenProcess(SYNCHRONIZE, FALSE, pid);
        if (hProc) {
            UniqueHandle procHandle(hProc);
            return (::WaitForSingleObject(procHandle.get(), timeoutMs) == WAIT_OBJECT_0);
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

    bool sendSignal(DWORD pid, const SignalInfo& sig, bool force) const {
        if (sig.type == SignalType::SIGNULL) {
            HANDLE hProc = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (!hProc) return false;
            UniqueHandle handle(hProc);
            DWORD exitCode = 0;
            return ::GetExitCodeProcess(handle.get(), &exitCode) && (exitCode == STILL_ACTIVE);
        }

        if (sig.type == SignalType::SIGSTOP) {
            if (!NtSuspendProcess) return false;
            HANDLE hProc = ::OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
            if (!hProc) return false;
            UniqueHandle handle(hProc);
            return (NtSuspendProcess(handle.get()) >= 0);
        }

        if (sig.type == SignalType::SIGCONT) {
            if (!NtResumeProcess) return false;
            HANDLE hProc = ::OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
            if (!hProc) return false;
            UniqueHandle handle(hProc);
            return (NtResumeProcess(handle.get()) >= 0);
        }

        if (sig.type == SignalType::SIGINT || sig.type == SignalType::SIGQUIT) {
            DWORD ctrlEvent = (sig.type == SignalType::SIGINT) ? CTRL_C_EVENT : CTRL_BREAK_EVENT;
            if (::GenerateConsoleCtrlEvent(ctrlEvent, pid)) return true;
        }

        if ((sig.type == SignalType::SIGTERM || sig.type == SignalType::SIGHUP) && !force) {
            if (sendGracefulClose(pid)) return true;
        }

        // SIGKILL or forced fallback
        HANDLE hProc = ::OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
        if (!hProc) return false;
        UniqueHandle handle(hProc);

        UINT exitCode = (sig.number != 0) ? (128 + sig.number) : 1;
        return ::TerminateProcess(handle.get(), exitCode) != FALSE;
    }
};

// ============================================================================
// Command-Line Interface & Configuration
// ============================================================================
struct Options {
    SignalInfo signal{ 15, "SIGTERM", "Terminate", SignalType::SIGTERM };
    bool force = false;
    bool tree = false;
    bool interactive = false;
    bool ignoreCase = true;
    bool exactCase = false;
    bool quiet = false;
    bool verbose = false;
    bool wait = false;
    bool useRegex = false;
    bool listSignals = false;
    bool listTable = false;
    std::string userFilter;
    std::optional<std::chrono::seconds> olderThan;
    std::optional<std::chrono::seconds> youngerThan;
    std::vector<std::string> patterns;
};

class CommandLineParser {
public:
    static void printHelp(const std::string& /*exeName*/ = "killall") {
        std::cout << R"(killall(1)          CrossShell for UNIX Reference Manual                killall(1)

    NAME
        killall - kill processes by name

    SYNOPSIS
        killall [OPTIONS] [-s SIGNAL | -SIGNAL] PATTERN...
        killall -l
        killall -L

    DESCRIPTION
        killall sends a signal to all processes running any of the specified
        commands or patterns. When no signal name is specified, SIGTERM is sent.
        Signals can be specified by name or number. It supports case-insensitive
        matching, regex pattern evaluation, process age filtering, and process trees.

    OPTIONS
        -s SIGNAL
            Specify the signal to send by name or number.

        -SIGNAL
            Shorthand signal specification (e.g., -9, -15, -KILL, -TERM, -STOP).

        -e, --exact
            Require exact case matching of process names.

        -I, --ignore-case
            Use case-insensitive matching (Windows default).

        -i, --interactive
            Interactively ask for confirmation before signaling each process.

        -r, --regexp
            Interpret patterns as regular expressions.

        -t, --tree
            Include target processes and all child processes recursively.

        -u, --user USERNAME
            Match only processes owned by the specified user.

        -o, --older-than DURATION
            Match only processes older than DURATION (e.g., 30m, 2h).

        -y, --younger-than DURATION
            Match only processes younger than DURATION.

        -w, --wait
            Wait for all signaled processes to terminate before exiting.

        -q, --quiet
            Do not complain if no matching processes are found.

        -v, --verbose
            Display detailed execution diagnostics.

        -l, --list
            List all known signal names.

        -L, --table
            Display formatted table of supported signals.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    SUPPORTED SIGNALS
        0, SIGNULL
            Probe process existence and query access.

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
        killall notepad.exe
            Send SIGTERM to all notepad.exe instances.

        killall -9 chrome.exe msedge.exe
            Forcefully terminate all Chrome and Edge instances.

        killall -r -i "node.*\.exe"
            Interactively terminate processes matching regex.

        killall -9 -o 30m cl.exe gcc.exe
            Kill compiler workers older than 30 minutes.

        killall -STOP SearchIndexer.exe
            Freeze background indexing process.

    CrossShell for UNIX                                                   killall(1)
)";
    }

    static void printVersion() {
        std::cout << "killall 1.0.0\n";
    }

    static Options parse(int argc, char* argv[], const SignalRegistry& registry) {
        Options opt;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help" || arg == "/?") {
                printHelp("killall");
                std::exit(0);
            } else if (arg == "-V" || arg == "--version") {
                printVersion();
                std::exit(0);
            } else if (arg == "-l" || arg == "--list") {
                opt.listSignals = true;
            } else if (arg == "-L" || arg == "--table") {
                opt.listTable = true;
            } else if (arg == "-e" || arg == "--exact") {
                opt.exactCase = true;
                opt.ignoreCase = false;
            } else if (arg == "-I" || arg == "--ignore-case") {
                opt.ignoreCase = true;
                opt.exactCase = false;
            } else if (arg == "-i" || arg == "--interactive") {
                opt.interactive = true;
            } else if (arg == "-r" || arg == "--regexp") {
                opt.useRegex = true;
            } else if (arg == "-t" || arg == "--tree") {
                opt.tree = true;
            } else if (arg == "-w" || arg == "--wait") {
                opt.wait = true;
            } else if (arg == "-q" || arg == "--quiet") {
                opt.quiet = true;
            } else if (arg == "-v" || arg == "--verbose") {
                opt.verbose = true;
            } else if (arg == "-u" || arg == "--user") {
                if (i + 1 >= argc) {
                    std::cerr << "killall: Option -u requires a username.\n";
                    std::exit(1);
                }
                opt.userFilter = argv[++i];
            } else if (arg == "-o" || arg == "--older-than") {
                if (i + 1 >= argc) {
                    std::cerr << "killall: Option -o requires a duration.\n";
                    std::exit(1);
                }
                opt.olderThan = TimeHelper::parseDuration(argv[++i]);
                if (!opt.olderThan) {
                    std::cerr << "killall: Invalid duration format: " << argv[i] << "\n";
                    std::exit(1);
                }
            } else if (arg == "-y" || arg == "--younger-than") {
                if (i + 1 >= argc) {
                    std::cerr << "killall: Option -y requires a duration.\n";
                    std::exit(1);
                }
                opt.youngerThan = TimeHelper::parseDuration(argv[++i]);
                if (!opt.youngerThan) {
                    std::cerr << "killall: Invalid duration format: " << argv[i] << "\n";
                    std::exit(1);
                }
            } else if (arg == "-s" || arg == "--signal") {
                if (i + 1 >= argc) {
                    std::cerr << "killall: Option -s requires a signal specification.\n";
                    std::exit(1);
                }
                auto found = registry.find(argv[++i]);
                if (!found) {
                    std::cerr << "killall: Unknown signal '" << argv[i] << "'. Use -l for list.\n";
                    std::exit(1);
                }
                opt.signal = *found;
            } else if (arg.front() == '-' && arg.size() > 1 && registry.find(arg.substr(1))) {
                opt.signal = *registry.find(arg.substr(1));
            } else if (arg.front() == '-') {
                std::cerr << "killall: Unknown option '" << arg << "'. Use -h for help.\n";
                std::exit(1);
            } else {
                opt.patterns.push_back(arg);
            }
        }

        return opt;
    }
};

// ============================================================================
// Application Controller & Execution Engine
// ============================================================================
class KillallApp {
private:
    Options options;
    SignalRegistry registry;
    ProcessSignaler signaler;

    bool askConfirmation(const std::string& name, DWORD pid) {
        // Read directly from console (CONIN$) to allow interactive prompts during piping
        FILE* conin = nullptr;
        if (fopen_s(&conin, "CONIN$", "r") != 0 || !conin) {
            std::cout << "Kill " << name << "(" << pid << ")? (y/n): ";
            std::string ans;
            std::cin >> ans;
            return (!ans.empty() && (ans[0] == 'y' || ans[0] == 'Y'));
        }

        std::cerr << "Kill " << name << "(" << pid << ")? (y/n): ";
        char ch = 0;
        int r = fgetc(conin);
        fclose(conin);
        return (r == 'y' || r == 'Y');
    }

    bool matchesPattern(const std::string& procName, const std::string& pattern) {
        if (options.useRegex) {
            try {
                auto flags = std::regex::ECMAScript;
                if (options.ignoreCase) flags |= std::regex::icase;
                std::regex re(pattern, flags);
                return std::regex_search(procName, re);
            } catch (const std::regex_error&) {
                return false;
            }
        }

        std::string pName = procName;
        std::string pat = pattern;

        if (options.ignoreCase) {
            std::transform(pName.begin(), pName.end(), pName.begin(), ::tolower);
            std::transform(pat.begin(), pat.end(), pat.begin(), ::tolower);
        }

        if (pName == pat) return true;
        if (pName == pat + ".exe") return true;
        return false;
    }

    void collectTreeChildren(DWORD parentPid, const std::vector<ProcessMeta>& snapshot, std::set<DWORD>& treePids) {
        for (const auto& proc : snapshot) {
            if (proc.parentPid == parentPid && proc.pid != parentPid) {
                if (treePids.insert(proc.pid).second) {
                    collectTreeChildren(proc.pid, snapshot, treePids);
                }
            }
        }
    }

    std::vector<std::string> gatherPatterns() {
        std::vector<std::string> patterns = options.patterns;
        bool hasStdInPipe = !_isatty(_fileno(stdin));

        if (patterns.empty() || (patterns.size() == 1 && patterns[0] == "-") || hasStdInPipe) {
            if (hasStdInPipe || (patterns.size() == 1 && patterns[0] == "-")) {
                if (!patterns.empty() && patterns[0] == "-") patterns.clear();
                std::string token;
                while (std::cin >> token) {
                    patterns.push_back(token);
                }
            }
        }
        return patterns;
    }

public:
    explicit KillallApp(Options opt) : options(std::move(opt)) {}

    int run() {
        if (options.listSignals) {
            for (const auto& sig : registry.getAll()) {
                std::cout << std::setw(2) << sig.number << ") " << std::setw(8) << std::left << sig.name;
                if (sig.number % 4 == 0) std::cout << "\n";
            }
            std::cout << "\n";
            return 0;
        }

        if (options.listTable) {
            std::cout << std::left << std::setw(6) << "NUM" << std::setw(12) << "NAME" << "DESCRIPTION\n";
            std::cout << std::string(75, '-') << "\n";
            for (const auto& sig : registry.getAll()) {
                std::cout << std::setw(6) << sig.number << std::setw(12) << sig.name << sig.description << "\n";
            }
            return 0;
        }

        auto patterns = gatherPatterns();
        if (patterns.empty()) {
            if (!options.quiet) {
                std::cerr << "killall: No process name patterns specified.\n";
                std::cerr << "         Use 'killall --help' for usage instructions.\n";
            }
            return 1;
        }

        PrivilegeManager::enableDebugPrivilege();
        auto snapshot = ProcessQueryEngine::collectProcesses();

        std::map<DWORD, ProcessMeta> targetMap;

        for (const auto& pat : patterns) {
            bool foundAny = false;
            for (const auto& proc : snapshot) {
                if (!matchesPattern(proc.name, pat)) continue;

                // Apply owner filter
                if (!options.userFilter.empty()) {
                    std::string u1 = proc.owner, u2 = options.userFilter;
                    std::transform(u1.begin(), u1.end(), u1.begin(), ::tolower);
                    std::transform(u2.begin(), u2.end(), u2.begin(), ::tolower);
                    if (u1 != u2) continue;
                }

                // Apply age filters
                if (options.olderThan && proc.age < *options.olderThan) continue;
                if (options.youngerThan && proc.age > *options.youngerThan) continue;

                targetMap[proc.pid] = proc;
                foundAny = true;
            }

            if (!foundAny && !options.quiet) {
                std::cerr << "killall: " << pat << ": no process found\n";
            }
        }

        if (targetMap.empty()) {
            return options.quiet ? 0 : 1;
        }

        // Expand process tree if requested
        if (options.tree) {
            std::set<DWORD> treePids;
            for (const auto& [pid, _] : targetMap) {
                treePids.insert(pid);
                collectTreeChildren(pid, snapshot, treePids);
            }
            for (DWORD pid : treePids) {
                if (targetMap.find(pid) == targetMap.end()) {
                    auto it = std::find_if(snapshot.begin(), snapshot.end(), [pid](const ProcessMeta& m) {
                        return m.pid == pid;
                    });
                    if (it != snapshot.end()) targetMap[pid] = *it;
                }
            }
        }

        std::vector<HANDLE> waitHandles;
        int failureCount = 0;

        for (const auto& [pid, meta] : targetMap) {
            if (options.interactive) {
                if (!askConfirmation(meta.name, pid)) {
                    continue;
                }
            }

            if (options.wait) {
                HANDLE hProc = ::OpenProcess(SYNCHRONIZE, FALSE, pid);
                if (hProc) waitHandles.push_back(hProc);
            }

            bool ok = signaler.sendSignal(pid, options.signal, options.force);
            if (ok) {
                if (options.verbose) {
                    std::cout << "Killed " << meta.name << "(" << pid << ") with " << options.signal.name << "\n";
                }
            } else {
                failureCount++;
                if (!options.quiet) {
                    std::cerr << "killall: Failed to signal " << meta.name << "(" << pid << ")\n";
                }
            }
        }

        // Wait for processes to exit if -w is specified
        if (options.wait && !waitHandles.empty()) {
            if (options.verbose) std::cout << "Waiting for processes to exit...\n";
            for (HANDLE h : waitHandles) {
                ::WaitForSingleObject(h, INFINITE);
                ::CloseHandle(h);
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
        KillallApp app(std::move(options));
        return app.run();
    } catch (const std::exception& ex) {
        std::cerr << "killall: Fatal Exception: " << ex.what() << "\n";
        return 1;
    }
}