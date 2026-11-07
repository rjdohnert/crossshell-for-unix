#include "process_monitor.hpp"
#include "process_record.hpp"
#include "unique_handle.hpp"

namespace {
std::string WideToUtf8(const wchar_t* value) {
    if (value == nullptr || *value == L'\0') return {};

    int required = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, nullptr, 0, nullptr, nullptr);
    if (required <= 0) return "<conversion-error>";

    std::string result(static_cast<size_t>(required), '\0');
    int converted = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, result.data(), required, nullptr, nullptr);
    if (converted != required) return "<conversion-error>";

    result.resize(static_cast<size_t>(required - 1));
    return result;
}
}

uint64_t HpUxTopEngine::FileTimeToUint64(const FILETIME& ft) {
    return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

std::string HpUxTopEngine::GetProcessUser(HANDLE hProcess) {
    HANDLE rawToken = nullptr;
    if (!OpenProcessToken(hProcess, TOKEN_QUERY, &rawToken)) return "-";
    UniqueHandle hToken(rawToken);

    DWORD tokenInfoLength = 0;
    GetTokenInformation(hToken, TokenUser, nullptr, 0, &tokenInfoLength);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) return "-";

    std::vector<BYTE> tokenInfo(tokenInfoLength);
    if (!GetTokenInformation(hToken, TokenUser, tokenInfo.data(), tokenInfoLength,
                             &tokenInfoLength)) {
        return "-";
    }

    auto* tokenUser = reinterpret_cast<TOKEN_USER*>(tokenInfo.data());
    DWORD nameLength = 0;
    DWORD domainLength = 0;
    SID_NAME_USE sidNameUse;
    LookupAccountSidW(nullptr, tokenUser->User.Sid, nullptr, &nameLength, nullptr,
                      &domainLength, &sidNameUse);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || nameLength == 0) return "-";

    std::vector<wchar_t> name(nameLength);
    std::vector<wchar_t> domain(domainLength);
    if (!LookupAccountSidW(nullptr, tokenUser->User.Sid, name.data(), &nameLength,
                           domain.data(), &domainLength, &sidNameUse)) {
        return "-";
    }
    return WideToUtf8(name.data());
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
    if (!GetSystemTimes(&idle, &kernel, &user)) {
        PromptLine("top: GetSystemTimes failed (error " + std::to_string(GetLastError()) + ")");
        return;
    }

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

    uint64_t actualKernel = (deltaKernel >= deltaIdle) ? (deltaKernel - deltaIdle) : 0;
    double cpuUser = (deltaTotal > 0) ? (deltaUser * 100.0 / deltaTotal) : 0.0;
    double cpuSys = (deltaTotal > 0) ? (actualKernel * 100.0 / deltaTotal) : 0.0;
    double cpuIdle = (deltaTotal > 0) ? (deltaIdle * 100.0 / deltaTotal) : 0.0;

    UniqueHandle snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (!snap.isValid()) {
        PromptLine("top: unable to snapshot processes (error " +
                   std::to_string(GetLastError()) + ")");
        return;
    }

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);

    std::vector<ProcessRecord> records;
    std::unordered_map<DWORD, ProcessCpuSample> curProcTimes;

    if (Process32FirstW(snap, &pe)) {
        do {
            ProcessRecord rec;
            rec.pid = pe.th32ProcessID;

            rec.command = WideToUtf8(pe.szExeFile);
            rec.pri = pe.pcPriClassBase;
            rec.nice = (rec.pri > 8) ? -(rec.pri - 8) : (8 - rec.pri);

            UniqueHandle hProc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pe.th32ProcessID);

            if (hProc.isValid()) {
                FILETIME ftCreate, ftExit, ftKernel, ftUser;
                if (GetProcessTimes(hProc, &ftCreate, &ftExit, &ftKernel, &ftUser)) {
                    uint64_t pKernel = FileTimeToUint64(ftKernel);
                    uint64_t pUser = FileTimeToUint64(ftUser);
                    uint64_t pTotal = pKernel + pUser;
                    uint64_t creationTime = FileTimeToUint64(ftCreate);
                    curProcTimes[rec.pid] = {pTotal, creationTime};
                    rec.cpuTimeMs = pTotal / 10000;

                    auto previous = prevProcTimes.find(rec.pid);
                    if (previous != prevProcTimes.end() &&
                        previous->second.creationTime == creationTime &&
                        pTotal >= previous->second.cpuTime && deltaTotal > 0) {
                        uint64_t pDelta = pTotal - previous->second.cpuTime;
                        rec.cpuPct = (pDelta * 100.0) / deltaTotal;
                    }
                }

                PROCESS_MEMORY_COUNTERS_EX pmc;
                if (GetProcessMemoryInfo(hProc, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                                         sizeof(pmc))) {
                    rec.resKb = pmc.WorkingSetSize / 1024;
                    rec.sizeKb = pmc.PrivateUsage / 1024;
                }
                rec.user = GetProcessUser(hProc);
                rec.activity = (rec.cpuPct > 0.5) ? "active" : "idle";
            }

            if (userFilter.empty() || rec.user == userFilter) {
                records.push_back(std::move(rec));
            }

        } while (Process32NextW(snap, &pe));
    }

    prevProcTimes = std::move(curProcTimes);

    switch (currentSort) {
        case SortMode::CPU:
            std::sort(records.begin(), records.end(),
                      [](const auto& a, const auto& b) { return a.cpuPct > b.cpuPct; });
            break;
        case SortMode::MEMORY:
            std::sort(records.begin(), records.end(),
                      [](const auto& a, const auto& b) { return a.resKb > b.resKb; });
            break;
        case SortMode::PID:
            std::sort(records.begin(), records.end(),
                      [](const auto& a, const auto& b) { return a.pid < b.pid; });
            break;
        case SortMode::TIME:
            std::sort(records.begin(), records.end(),
                      [](const auto& a, const auto& b) { return a.cpuTimeMs > b.cpuTimeMs; });
            break;
        case SortMode::NAME:
            std::sort(records.begin(), records.end(),
                      [](const auto& a, const auto& b) { return a.command < b.command; });
            break;
    }

    RenderHeader(records, cpuUser, cpuSys, cpuIdle);

    std::cout << "\033[7m";
    std::cout << std::right << std::setw(6) << "PID" << " " << std::left << std::setw(12) << "USERNAME"
              << " " << std::right << std::setw(3) << "PRI" << " " << std::setw(4) << "NICE"
              << " " << std::setw(7) << "SIZE" << " " << std::setw(7) << "RES"
              << " " << std::left << std::setw(8) << "ACTIVITY" << " " << std::right << std::setw(7) << "TIME"
              << " " << std::setw(6) << "%CPU" << "  " << std::left << std::setw(16) << "COMMAND" << "\r\n";
    std::cout << "\033[0m";

    int displayed = 0;
    for (const auto& p : records) {
        if (++displayed > maxDisplayCount) break;

        std::cout << std::right << std::setw(6) << p.pid << " " << std::left << std::setw(12)
                  << p.user.substr(0, 12) << " " << std::right << std::setw(3) << p.pri << " "
                  << std::setw(4) << p.nice << " " << std::setw(7) << FormatBytes(p.sizeKb) << " "
                  << std::setw(7) << FormatBytes(p.resKb) << " " << std::left << std::setw(8)
                  << p.activity << " " << std::right << std::setw(7) << FormatTime(p.cpuTimeMs / 1000)
                  << " " << std::fixed << std::setprecision(2) << std::setw(6) << p.cpuPct
                  << "  " << std::left << std::setw(16) << p.command.substr(0, 16) << "\r\n";
    }

    std::cout << "\033[J";
    std::cout.flush();
}
