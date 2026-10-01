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

/**
 * HP-UX 11i "top" Command Clone for Windows
 * Standard: C++17
 * Compiler: MSVC (cl /std:c++17 /EHsc top.cpp) or Clang-cl
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <conio.h>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>
#include <unordered_map>
#include <memory>
#include <cmath>
#include <thread>
#include <ctime>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "advapi32.lib")

// RAII Handle Wrapper
struct UniqueHandle {
    HANDLE handle = INVALID_HANDLE_VALUE;
    UniqueHandle(HANDLE h = INVALID_HANDLE_VALUE) : handle(h) {}
    ~UniqueHandle() { if (handle && handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    UniqueHandle(UniqueHandle&& o) noexcept : handle(o.handle) { o.handle = INVALID_HANDLE_VALUE; }
    UniqueHandle& operator=(UniqueHandle&& o) noexcept {
        if (this != &o) {
            if (handle && handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
            handle = o.handle;
            o.handle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }
    operator HANDLE() const { return handle; }
    bool isValid() const { return handle != nullptr && handle != INVALID_HANDLE_VALUE; }
};

enum class SortMode { CPU, MEMORY, PID, TIME };

struct ProcessRecord {
    DWORD pid = 0;
    std::string user = "SYSTEM";
    int pri = 0;
    int nice = 0;
    uint64_t sizeKb = 0;
    uint64_t resKb = 0;
    std::string state = "sleep";
    uint64_t cpuTimeMs = 0;
    double cpuPct = 0.0;
    std::string command;
};

struct CpuSnapshot {
    uint64_t idle = 0;
    uint64_t kernel = 0;
    uint64_t user = 0;
    uint64_t timestamp = 0;
};

class HpUxTopEngine {
private:
    double load1 = 0.0, load5 = 0.0, load15 = 0.0;
    CpuSnapshot prevSystemCpu{};
    std::unordered_map<DWORD, uint64_t> prevProcTimes;
    std::chrono::steady_clock::time_point lastSampleTime;

    int refreshDelaySec = 2;
    int maxDisplayCount = 20;
    SortMode currentSort = SortMode::CPU;
    std::string userFilter = "";
    bool showHelp = false;

    // Helper: Convert FILETIME to 100ns units
    static uint64_t FileTimeToUint64(const FILETIME& ft) {
        return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    }

    std::string FormatTime(uint64_t totalSec) {
        uint64_t m = totalSec / 60;
        uint64_t s = totalSec % 60;
        std::ostringstream out;
        out << std::setfill('0') << std::setw(2) << m << ":" << std::setw(2) << s;
        return out.str();
    }

    std::string FormatBytes(uint64_t kb) {
        std::ostringstream out;
        out << std::fixed << std::setprecision(0);
        if (kb >= 1024 * 1024) out << std::setw(4) << (static_cast<double>(kb) / (1024 * 1024)) << "G";
        else if (kb >= 1024) out << std::setw(4) << (static_cast<double>(kb) / 1024) << "M";
        else out << std::setw(4) << kb << "K";
        return out.str();
    }

    std::string GetProcessUser(HANDLE hProcess) {
        UniqueHandle hToken;
        HANDLE rawToken;
        if (!OpenProcessToken(hProcess, TOKEN_QUERY, &rawToken)) return "-";
        hToken = rawToken;

        DWORD len = 0;
        GetTokenInformation(hToken, TokenUser, nullptr, 0, &len);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) return "-";

        std::vector<BYTE> buffer(len);
        if (!GetTokenInformation(hToken, TokenUser, buffer.data(), len, &len)) return "-";

        auto* pTokenUser = reinterpret_cast<TOKEN_USER*>(buffer.data());
        WCHAR name[256], domain[256];
        DWORD nameLen = 256, domainLen = 256;
        SID_NAME_USE snu;
        if (LookupAccountSidW(nullptr, pTokenUser->User.Sid, name, &nameLen, domain, &domainLen, &snu)) {
            char chName[256];
            WideCharToMultiByte(CP_UTF8, 0, name, -1, chName, 256, nullptr, nullptr);
            return std::string(chName);
        }
        return "-";
    }

public:
    HpUxTopEngine() {
        EnableVirtualTerminal();
        InitSystemSnapshot();
    }

    ~HpUxTopEngine() {
        // Show cursor again on exit
        std::cout << "\033[?25h\033[0m\n";
    }

    void EnableVirtualTerminal() {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode)) {
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, mode);
        }
        std::cout << "\033[?25l"; // Hide cursor
    }

    void InitSystemSnapshot() {
        FILETIME idle, kernel, user;
        if (GetSystemTimes(&idle, &kernel, &user)) {
            prevSystemCpu.idle = FileTimeToUint64(idle);
            prevSystemCpu.kernel = FileTimeToUint64(kernel);
            prevSystemCpu.user = FileTimeToUint64(user);
        }
        lastSampleTime = std::chrono::steady_clock::now();
    }

    void RenderHeader(const std::vector<ProcessRecord>& procs, double cpuUser, double cpuSys, double cpuIdle) {
        // 1. Hostname & Current Time
        char hostname[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD size = sizeof(hostname);
        GetComputerNameA(hostname, &size);

        auto now = std::chrono::system_clock::now();
        std::time_t timeNow = std::chrono::system_clock::to_time_t(now);
        tm tmLocal;
        localtime_s(&tmLocal, &timeNow);

        char timeStr[64];
        std::strftime(timeStr, sizeof(timeStr), "%a %b %d %H:%M:%S %Y", &tmLocal);

        // Update load average heuristic (Exponential Moving Average)
        unsigned int processorCount = std::thread::hardware_concurrency();
        double totalLoad = (cpuUser + cpuSys) / 100.0 * (std::max)(1u, processorCount);
        auto decay = [](double current, double target, double sec, double periodSec) {
            double alpha = 1.0 - std::exp(-sec / periodSec);
            return current + alpha * (target - current);
        };
        load1 = decay(load1, totalLoad, refreshDelaySec, 60.0);
        load5 = decay(load5, totalLoad, refreshDelaySec, 300.0);
        load15 = decay(load15, totalLoad, refreshDelaySec, 900.0);

        // 2. Memory
        MEMORYSTATUSEX mem;
        mem.dwLength = sizeof(mem);
        GlobalMemoryStatusEx(&mem);

        uint64_t totalPhysMb = mem.ullTotalPhys / (1024 * 1024);
        uint64_t freePhysMb = mem.ullAvailPhys / (1024 * 1024);
        uint64_t totalSwapMb = (mem.ullTotalPageFile - mem.ullTotalPhys) / (1024 * 1024);
        uint64_t freeSwapMb = (mem.ullAvailPageFile - mem.ullAvailPhys) / (1024 * 1024);

        int running = 0, sleeping = 0;
        for (const auto& p : procs) {
            if (p.state == "run") running++;
            else sleeping++;
        }

        // Output matching HP-UX Top Screen standard
        std::cout << "\033[H"; // Move to home (0,0)
        std::ostringstream header;
        header << "System: " << std::left << std::setw(16) << hostname
               << " " << std::right << std::setw(40) << timeStr << "\n"
               << "Load averages: " << std::fixed << std::setprecision(2)
               << std::setw(4) << load1 << ", " << std::setw(4) << load5 << ", " << std::setw(4) << load15 << "\n"
               << procs.size() << " processes: " << running << " running, " << sleeping << " sleeping\n"
               << "CPU states:\n"
               << "   LOAD   USER   NICE    SYS   IDLE   INTR\n"
               << "  " << std::setw(5) << totalLoad << "  " << std::setw(4) << cpuUser
               << "%   0.0%  " << std::setw(4) << cpuSys << "%  " << std::setw(4) << cpuIdle << "%   0.0%\n"
               << "Memory: " << totalPhysMb << "M (" << freePhysMb << "M free), Swap: "
               << totalSwapMb << "M (" << freeSwapMb << "M free)\n\n";
        std::cout << header.str();
    }

    void RenderHelp() {
        std::cout << "\033[H\033[2J";
        std::cout << "============================= [TOP HELP SCREEN ] ===========================\n\n";
        std::cout << "The top program provides a continuous display of system and process activity.\n";
        std::cout << "Interactive commands available during runtime:\n\n";
        std::cout << "  h or ?    Display this help screen\n";
        std::cout << "  q         Quit top immediately\n";
        std::cout << "  d         Set delay interval in seconds (e.g., d 5)\n";
        std::cout << "  n         Set maximum number of processes to display\n";
        std::cout << "  u         Filter processes by username\n";
        std::cout << "  k         Kill a process by sending TerminateProcess signal\n";
        std::cout << "  c         Sort process table by %CPU consumption (default)\n";
        std::cout << "  m         Sort process table by Resident Memory (RES)\n";
        std::cout << "  p         Sort process table by Process ID (PID)\n";
        std::cout << "  t         Sort process table by accumulated CPU TIME\n\n";
        std::cout << "Column Descriptions:\n";
        std::cout << "  PID       Process ID\n";
        std::cout << "  USERNAME  Account name executing the process\n";
        std::cout << "  PRI       Static/Dynamic Scheduling Priority\n";
        std::cout << "  NICE      Win32 Priority Class offset equivalent\n";
        std::cout << "  SIZE      Virtual committed memory size (Private bytes)\n";
        std::cout << "  RES       Resident Working Set size in physical RAM\n";
        std::cout << "  STATE     Process state (run/sleep)\n";
        std::cout << "  TIME      Cumulative CPU time (Minutes:Seconds)\n";
        std::cout << "  %CPU      Normalized CPU usage during the last interval\n";
        std::cout << "  COMMAND   Executable image name\n\n";
        std::cout << "Press ANY KEY to return to the live monitor...";
        std::cout.flush();
        _getch();
        std::cout << "\033[2J"; // Clear screen returning
    }

    void Update() {
        FILETIME idle, kernel, user;
        if (!GetSystemTimes(&idle, &kernel, &user)) return;

        uint64_t curIdle = FileTimeToUint64(idle);
        uint64_t curKernel = FileTimeToUint64(kernel);
        uint64_t curUser = FileTimeToUint64(user);

        uint64_t deltaKernel = curKernel - prevSystemCpu.kernel;
        uint64_t deltaUser = curUser - prevSystemCpu.user;
        uint64_t deltaIdle = curIdle - prevSystemCpu.idle;
        uint64_t deltaTotal = deltaKernel + deltaUser;

        prevSystemCpu.idle = curIdle;
        prevSystemCpu.kernel = curKernel;
        prevSystemCpu.user = curUser;

        // In Windows API, Kernel time already includes Idle time!
        uint64_t actualKernel = (deltaKernel >= deltaIdle) ? (deltaKernel - deltaIdle) : 0;
        double cpuUser = (deltaTotal > 0) ? (deltaUser * 100.0 / deltaTotal) : 0.0;
        double cpuSys = (deltaTotal > 0) ? (actualKernel * 100.0 / deltaTotal) : 0.0;
        double cpuIdle = (deltaTotal > 0) ? (deltaIdle * 100.0 / deltaTotal) : 0.0;

        // Snapshot Processes
        UniqueHandle snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (!snap.isValid()) return;

        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(pe);

        std::vector<ProcessRecord> records;
        std::unordered_map<DWORD, uint64_t> curProcTimes;

        if (Process32FirstW(snap, &pe)) {
            do {
                ProcessRecord rec;
                rec.pid = pe.th32ProcessID;

                char cmd[MAX_PATH];
                WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, cmd, MAX_PATH, nullptr, nullptr);
                rec.command = cmd;
                rec.pri = pe.pcPriClassBase;
                rec.nice = (rec.pri > 8) ? -(rec.pri - 8) : (8 - rec.pri);

                UniqueHandle hProc = OpenProcess(
                    PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 
                    FALSE, 
                    pe.th32ProcessID
                );

                if (hProc.isValid()) {
                    FILETIME ftCreate, ftExit, ftKernel, ftUser;
                    if (GetProcessTimes(hProc, &ftCreate, &ftExit, &ftKernel, &ftUser)) {
                        uint64_t pKernel = FileTimeToUint64(ftKernel);
                        uint64_t pUser = FileTimeToUint64(ftUser);
                        uint64_t pTotal = pKernel + pUser;
                        curProcTimes[rec.pid] = pTotal;
                        rec.cpuTimeMs = pTotal / 10000;

                        if (prevProcTimes.find(rec.pid) != prevProcTimes.end() && deltaTotal > 0) {
                            uint64_t pDelta = pTotal - prevProcTimes[rec.pid];
                            rec.cpuPct = (pDelta * 100.0) / deltaTotal;
                        }
                    }

                    PROCESS_MEMORY_COUNTERS_EX pmc;
                    if (GetProcessMemoryInfo(hProc, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
                        rec.resKb = pmc.WorkingSetSize / 1024;
                        rec.sizeKb = pmc.PrivateUsage / 1024;
                    }
                    rec.user = GetProcessUser(hProc);
                    rec.state = (rec.cpuPct > 0.5) ? "run" : "sleep";
                } else {
                    rec.state = "sleep";
                }

                if (userFilter.empty() || rec.user == userFilter) {
                    records.push_back(std::move(rec));
                }

            } while (Process32NextW(snap, &pe));
        }

        prevProcTimes = std::move(curProcTimes);

        // Sorting
        switch (currentSort) {
            case SortMode::CPU:
                std::sort(records.begin(), records.end(), [](const auto& a, const auto& b) { return a.cpuPct > b.cpuPct; });
                break;
            case SortMode::MEMORY:
                std::sort(records.begin(), records.end(), [](const auto& a, const auto& b) { return a.resKb > b.resKb; });
                break;
            case SortMode::PID:
                std::sort(records.begin(), records.end(), [](const auto& a, const auto& b) { return a.pid < b.pid; });
                break;
            case SortMode::TIME:
                std::sort(records.begin(), records.end(), [](const auto& a, const auto& b) { return a.cpuTimeMs > b.cpuTimeMs; });
                break;
        }

        // Render Page
        RenderHeader(records, cpuUser, cpuSys, cpuIdle);

        // Column Headings
        std::cout << "\033[7m"; // Invert video for column headers (classic terminal style)
        std::cout << std::right << std::setw(6) << "PID" << " " << std::left << std::setw(12) << "USERNAME"
                  << " " << std::right << std::setw(3) << "PRI" << " " << std::setw(4) << "NICE"
                  << " " << std::setw(7) << "SIZE" << " " << std::setw(7) << "RES"
                  << " " << std::left << std::setw(5) << "STATE" << " " << std::right << std::setw(7) << "TIME"
                  << " " << std::setw(6) << "%CPU" << "  " << std::left << std::setw(16) << "COMMAND" << "\n";
        std::cout << "\033[0m";

        int displayed = 0;
        for (const auto& p : records) {
            if (++displayed > maxDisplayCount) break;

            std::cout << std::right << std::setw(6) << p.pid << " " << std::left << std::setw(12) << p.user.substr(0, 12)
                      << " " << std::right << std::setw(3) << p.pri << " " << std::setw(4) << p.nice
                      << " " << std::setw(7) << FormatBytes(p.sizeKb) << " " << std::setw(7) << FormatBytes(p.resKb)
                      << " " << std::left << std::setw(5) << p.state << " " << std::right << std::setw(7) << FormatTime(p.cpuTimeMs / 1000)
                      << " " << std::fixed << std::setprecision(2) << std::setw(6) << p.cpuPct
                      << "  " << std::left << std::setw(16) << p.command.substr(0, 16) << "\n";
        }

        // Clean any residual lines underneath
        std::cout << "\033[J";
        std::cout.flush();
    }

    void HandleInput() {
        if (!_kbhit()) return;
        int ch = _getch();

        switch (ch) {
            case 'q':
            case 'Q':
                std::cout << "\033[?25h\033[0m\nTerminated by user.\n";
                exit(0);
            case 'h':
            case '?':
                RenderHelp();
                break;
            case 'c': currentSort = SortMode::CPU; break;
            case 'm': currentSort = SortMode::MEMORY; break;
            case 'p': currentSort = SortMode::PID; break;
            case 't': currentSort = SortMode::TIME; break;
            case 'd': {
                PromptLine("Change delay interval (seconds): ");
                int delay = 0;
                std::cin >> delay;
                if (delay > 0) refreshDelaySec = delay;
                break;
            }
            case 'n': {
                PromptLine("Number of processes to display: ");
                int n = 0;
                std::cin >> n;
                if (n > 0) maxDisplayCount = n;
                break;
            }
            case 'u': {
                PromptLine("Filter by username (blank for all): ");
                std::string u;
                std::getline(std::cin >> std::ws, u);
                userFilter = (u == "\"\"" || u == "none") ? "" : u;
                break;
            }
            case 'k': {
                PromptLine("PID to kill: ");
                DWORD targetPid = 0;
                std::cin >> targetPid;
                if (targetPid > 0) {
                    UniqueHandle hKill = OpenProcess(PROCESS_TERMINATE, FALSE, targetPid);
                    if (hKill.isValid() && TerminateProcess(hKill, 1)) {
                        PromptLine("Successfully terminated PID " + std::to_string(targetPid) + ".");
                    } else {
                        PromptLine("Failed to terminate PID " + std::to_string(targetPid) + ". Error: " + std::to_string(GetLastError()));
                    }
                    Sleep(1000);
                }
                break;
            }
            default: break;
        }
    }

    void PromptLine(const std::string& msg) {
        std::cout << "\033[?25h"; // Show cursor
        std::cout << "\033[24;1H\033[2K" << msg;
        std::cout.flush();
    }

    void Run() {
        std::cout << "\033[2J"; // Clear screen initially
        while (true) {
            Update();
            // Non-blocking sleep loop checking keyboard input every 100ms
            int slices = refreshDelaySec * 10;
            for (int i = 0; i < slices; ++i) {
                if (_kbhit()) {
                    HandleInput();
                    break;
                }
                Sleep(100);
            }
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--help" || arg == "-h" || arg == "/?" || arg == "-?") {
            std::cout << R"(top(1)                  CrossShell for UNIX Reference Manual                 top(1)

    NAME
        top - display Linux / HP-UX process activity and system resource monitor

    SYNOPSIS
        top [OPTIONS]

    DESCRIPTION
        top provides an ongoing look at processor activity in real time. It
        displays a listing of the most CPU-intensive tasks on the system.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -d DELAY
            Specifies the delay between screen updates in seconds.

        -n COUNT
            Specifies the maximum number of iterations or processes to show.

        -u USER
            Monitor only processes with the specified user ID or username.

        -h, --help
            Display this reference manual and exit.

        -V, --version
            Output version information and exit.

    INTERACTIVE COMMANDS
        h, ?        Display interactive help screen.
        k           Kill a process by PID.
        d           Change refresh delay interval.
        n           Change number of processes shown.
        u           Filter by username.
        q           Quit.

    EXAMPLES
        top
            Launch interactive process monitor.

    CrossShell for UNIX                                                      top(1)
)";
            return 0;
        }
        if (arg == "--version" || arg == "-V" || arg == "-v") {
            std::cout << "top 1.0.0\n";
            return 0;
        }
    }
    // Process arguments
    HpUxTopEngine engine;
    engine.Run();
    return 0;
}