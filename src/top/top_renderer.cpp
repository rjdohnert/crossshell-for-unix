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

        // 2. Memory and commit usage
        MEMORYSTATUSEX mem;
        mem.dwLength = sizeof(mem);
        bool memoryAvailable = GlobalMemoryStatusEx(&mem) != FALSE;
        DWORD memoryStatusError = memoryAvailable ? ERROR_SUCCESS : GetLastError();

        int cpuActive = 0, belowThreshold = 0;
        for (const auto& p : procs) {
            if (p.activity == "active") cpuActive++;
            else belowThreshold++;
        }

        // Output matching HP-UX Top Screen standard
        std::cout << "\033[H"; // Move to home (0,0)
        std::ostringstream header;
        header << "System: " << std::left << std::setw(16) << hostname
               << " " << std::right << std::setw(40) << timeStr << "\r\n"
               << "Load averages: " << std::fixed << std::setprecision(2)
               << std::setw(4) << load1 << ", " << std::setw(4) << load5 << ", " << std::setw(4) << load15 << "\r\n"
               << procs.size() << " processes: " << cpuActive << " CPU-active, "
               << belowThreshold << " below 0.5%\r\n"
               << "CPU states:\r\n"
               << "   LOAD   USER   NICE    SYS   IDLE   INTR\r\n"
               << "  " << std::setw(5) << totalLoad << "  " << std::setw(4) << cpuUser
               << "%   0.0%  " << std::setw(4) << cpuSys << "%  " << std::setw(4) << cpuIdle << "%   0.0%\r\n"
               << "Memory: ";
        if (memoryAvailable) {
            uint64_t commitLimitMb = mem.ullTotalPageFile / (1024 * 1024);
            uint64_t commitAvailableMb = mem.ullAvailPageFile / (1024 * 1024);
            uint64_t commitUsedMb = mem.ullTotalPageFile >= mem.ullAvailPageFile
                                        ? (mem.ullTotalPageFile - mem.ullAvailPageFile) / (1024 * 1024)
                                        : 0;
            header << mem.ullTotalPhys / (1024 * 1024) << "M physical ("
                   << mem.ullAvailPhys / (1024 * 1024) << "M available), Commit: "
                   << commitUsedMb << "M used / " << commitLimitMb << "M limit ("
                   << commitAvailableMb << "M available)\r\n\r\n";
        } else {
            header << "unavailable (GlobalMemoryStatusEx error " << memoryStatusError << ")\r\n\r\n";
        }
        std::cout << header.str();
    }

void HpUxTopEngine::RenderHelp() {
        std::cout << "\033[H\033[2J";
        std::cout << "============================= [TOP HELP SCREEN ] ===========================\r\n\r\n";
        std::cout << "The top program provides a continuous display of system and process activity.\r\n";
        std::cout << "Interactive commands available during runtime:\r\n\r\n";
        std::cout << "  h or ?    Display this help screen\r\n";
        std::cout << "  q         Quit top immediately\r\n";
        std::cout << "  d         Set delay interval in seconds (e.g., d 5)\r\n";
        std::cout << "  n         Set maximum number of processes to display\r\n";
        std::cout << "  u         Filter processes by username\r\n";
        std::cout << "  k         Kill a process by sending TerminateProcess signal\r\n";
        std::cout << "  c         Sort process table by %CPU consumption (default)\r\n";
        std::cout << "  m         Sort process table by Resident Memory (RES)\r\n";
        std::cout << "  p         Sort process table by Process ID (PID)\r\n";
        std::cout << "  a         Sort process table by command name\r\n";
        std::cout << "  t         Sort process table by accumulated CPU TIME\r\n\r\n";
        std::cout << "Column Descriptions:\r\n";
        std::cout << "  PID       Process ID\r\n";
        std::cout << "  USERNAME  Account name executing the process\r\n";
        std::cout << "  PRI       Static/Dynamic Scheduling Priority\r\n";
        std::cout << "  NICE      Win32 Priority Class offset equivalent\r\n";
        std::cout << "  SIZE      Virtual committed memory size (Private bytes)\r\n";
        std::cout << "  RES       Resident Working Set size in physical RAM\r\n";
        std::cout << "  ACTIVITY  Sampled CPU activity: active above 0.5%, idle otherwise\r\n";
        std::cout << "  TIME      Cumulative CPU time (Minutes:Seconds)\r\n";
        std::cout << "  %CPU      Normalized CPU usage during the last interval\r\n";
        std::cout << "  COMMAND   Executable image name\r\n\r\n";
        std::cout << "Press any key to return; Esc and Ctrl+X quit the monitor.";
        std::cout.flush();
        _getch();
        std::cout << "\033[2J"; // Clear screen returning
    }
