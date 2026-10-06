#include "engine.hpp"

std::wstring ProcessRecord::getField(const std::wstring& key) const {
    std::wstring k = key;
    std::transform(k.begin(), k.end(), k.begin(), ::towlower);

    if (k == L"user" || k == L"owner")      return user;
    if (k == L"pid")                        return std::to_wstring(pid);
    if (k == L"ppid")                       return std::to_wstring(ppid);
    if (k == L"mem" || k == L"rss" || k == L"pmem") {
        if (workingSetKB >= 1024) {
            wchar_t buf[32];
            swprintf_s(buf, L"%.1f MB", workingSetKB / 1024.0);
            return buf;
        }
        return std::to_wstring(workingSetKB) + L" KB";
    }
    if (k == L"vsz" || k == L"vsize")       return std::to_wstring(virtualSizeKB) + L" KB";
    if (k == L"cpu" || k == L"time" || k == L"cputime") return cpuTime;
    if (k == L"threads" || k == L"thcnt" || k == L"nlwp") return std::to_wstring(threads);
    if (k == L"pri" || k == L"priority")    return std::to_wstring(priority);
    if (k == L"stat" || k == L"state")      return status;
    if (k == L"stime" || k == L"start")     return startTime;
    if (k == L"comm" || k == L"command" || k == L"cmd" || k == L"name") return name;

    return L"-";
}

std::vector<ProcessRecord> ProcessCollector::collect() {
    std::vector<ProcessRecord> records;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return records;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(snapshot, &pe)) {
        do {
            ProcessRecord rec;
            rec.pid = pe.th32ProcessID;
            rec.ppid = pe.th32ParentProcessID;
            rec.threads = pe.cntThreads;
            rec.priority = pe.pcPriClassBase;
            rec.name = pe.szExeFile;

            enrichProcessData(rec);
            records.push_back(rec);
        } while (Process32NextW(snapshot, &pe));
    }

    CloseHandle(snapshot);
    return records;
}

void ProcessCollector::enrichProcessData(ProcessRecord& rec) {
    if (rec.pid == 0) {
        rec.name = L"[System Idle Process]";
        rec.user = L"NT AUTHORITY\\SYSTEM";
        return;
    }

    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, rec.pid);
    if (!hProc) {
        hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, rec.pid);
    }

    if (hProc) {
        // Memory Info
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(hProc, &pmc, sizeof(pmc))) {
            rec.workingSetKB = pmc.WorkingSetSize / 1024;
            rec.virtualSizeKB = pmc.PagefileUsage / 1024;
        }

        // CPU & Start Times
        FILETIME ftCreate, ftExit, ftKernel, ftUser;
        if (GetProcessTimes(hProc, &ftCreate, &ftExit, &ftKernel, &ftUser)) {
            rec.startTime = formatFileTime(ftCreate);
            rec.cpuTime = formatDuration(ftKernel, ftUser);
        }

        // Token Owner / Username Resolution
        HANDLE hToken = nullptr;
        if (OpenProcessToken(hProc, TOKEN_QUERY, &hToken)) {
            DWORD len = 0;
            GetTokenInformation(hToken, TokenUser, nullptr, 0, &len);
            if (len > 0) {
                std::vector<BYTE> buf(len);
                if (GetTokenInformation(hToken, TokenUser, buf.data(), len, &len)) {
                    auto pUser = reinterpret_cast<TOKEN_USER*>(buf.data());
                    WCHAR name[256], domain[256];
                    DWORD nLen = 256, dLen = 256;
                    SID_NAME_USE use;
                    if (LookupAccountSidW(nullptr, pUser->User.Sid, name, &nLen, domain, &dLen, &use)) {
                        rec.user = std::wstring(domain) + L"\\" + name;
                    }
                }
            }
            CloseHandle(hToken);
        }
        CloseHandle(hProc);
    }
}

std::wstring ProcessCollector::formatFileTime(const FILETIME& ft) {
    FILETIME localFt;
    FileTimeToLocalFileTime(&ft, &localFt);
    SYSTEMTIME st;
    FileTimeToSystemTime(&localFt, &st);
    wchar_t buf[64];
    swprintf_s(buf, L"%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
    return buf;
}

std::wstring ProcessCollector::formatDuration(const FILETIME& kernel, const FILETIME& user) {
    ULARGE_INTEGER k, u;
    k.LowPart = kernel.dwLowDateTime;   k.HighPart = kernel.dwHighDateTime;
    u.LowPart = user.dwLowDateTime;       u.HighPart = user.dwHighDateTime;
    unsigned long long total100ns = k.QuadPart + u.QuadPart;
    unsigned long long totalSec = total100ns / 10000000ULL;

    unsigned long long hrs = totalSec / 3600;
    unsigned long long mins = (totalSec % 3600) / 60;
    unsigned long long secs = totalSec % 60;

    wchar_t buf[64];
    swprintf_s(buf, L"%02llu:%02llu:%02llu", hrs, mins, secs);
    return buf;
}
