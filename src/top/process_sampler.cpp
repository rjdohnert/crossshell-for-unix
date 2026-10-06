#include "process_monitor.hpp"
#include "process_record.hpp"
#include "unique_handle.hpp"

uint64_t HpUxTopEngine::FileTimeToUint64(const FILETIME& ft) {
        return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    }

std::string HpUxTopEngine::GetProcessUser(HANDLE hProcess) {
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

void HpUxTopEngine::InitSystemSnapshot() {
        FILETIME idle, kernel, user;
        if (GetSystemTimes(&idle, &kernel, &user)) {
            prevSystemCpu.idle = FileTimeToUint64(idle);
            prevSystemCpu.kernel = FileTimeToUint64(kernel);
            prevSystemCpu.user = FileTimeToUint64(user);
        }
        lastSampleTime = std::chrono::steady_clock::now();
    }

void HpUxTopEngine::Update() {
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
