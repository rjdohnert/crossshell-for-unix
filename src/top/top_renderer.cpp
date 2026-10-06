#include "process_monitor.hpp"
#include "process_record.hpp"

std::string HpUxTopEngine::FormatTime(uint64_t totalSec) {
        uint64_t m = totalSec / 60;
        uint64_t s = totalSec % 60;
        std::ostringstream out;
        out << std::setfill('0') << std::setw(2) << m << ":" << std::setw(2) << s;
        return out.str();
    }

std::string HpUxTopEngine::FormatBytes(uint64_t kb) {
        std::ostringstream out;
        out << std::fixed << std::setprecision(0);
        if (kb >= 1024 * 1024) out << std::setw(4) << (static_cast<double>(kb) / (1024 * 1024)) << "G";
        else if (kb >= 1024) out << std::setw(4) << (static_cast<double>(kb) / 1024) << "M";
        else out << std::setw(4) << kb << "K";
        return out.str();
    }

void HpUxTopEngine::RenderHeader(const std::vector<ProcessRecord>& procs, double cpuUser, double cpuSys, double cpuIdle) {
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

void HpUxTopEngine::RenderHelp() {
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
