#include "engine.hpp"

std::wstring ProcessMatcherEngine::ToLowerCopy(const std::wstring& input) {
    std::wstring result;
    result.reserve(input.size());
    for (wchar_t ch : input) {
        result.push_back(static_cast<wchar_t>(std::towlower(ch)));
    }
    return result;
}

bool ProcessMatcherEngine::CollectProcesses(std::vector<ProcessInfo>& out) {
    out.clear();

    ScopedSnapshotHandle snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!snap.IsValid()) {
        return false;
    }

    PROCESSENTRY32W pe = {};
    pe.dwSize = sizeof(pe);
    if (!Process32FirstW(snap.Get(), &pe)) {
        return false;
    }

    do {
        ProcessInfo p;
        p.pid = pe.th32ProcessID;
        p.name = pe.szExeFile;
        out.push_back(p);
    } while (Process32NextW(snap.Get(), &pe));

    return true;
}

bool ProcessMatcherEngine::IsMatch(const std::wstring& name, const PkillOptions& options) {
    std::wstring lhs = name;
    std::wstring rhs = options.pattern;
    if (options.ignoreCase) {
        lhs = ToLowerCopy(lhs);
        rhs = ToLowerCopy(rhs);
    }

    if (options.exact) {
        return lhs == rhs;
    }
    return lhs.find(rhs) != std::wstring::npos;
}

int ProcessMatcherEngine::TerminateMatchingProcesses(const PkillOptions& options) {
    std::vector<ProcessInfo> processes;
    if (!CollectProcesses(processes)) {
        std::wcerr << L"pkill: failed to enumerate processes\n";
        return 2;
    }

    const DWORD selfPid = GetCurrentProcessId();
    int matched = 0;
    int killed = 0;

    for (const auto& p : processes) {
        if (!IsMatch(p.name, options)) {
            continue;
        }
        if (p.pid == selfPid || p.pid == 0) {
            continue;
        }

        ++matched;
        ScopedProcessHandle hProcess(OpenProcess(PROCESS_TERMINATE, FALSE, p.pid));
        if (!hProcess.IsValid()) {
            std::wcerr << L"pkill: cannot open process " << p.pid << L" (" << p.name << L")\n";
            continue;
        }

        if (TerminateProcess(hProcess.Get(), 1)) {
            ++killed;
            if (options.echo) {
                std::wcout << L"killed " << p.pid << L" " << p.name << L"\n";
            }
        } else {
            std::wcerr << L"pkill: failed to terminate " << p.pid << L" (" << p.name << L")\n";
        }
    }

    if (matched == 0 || killed == 0) {
        return 1;
    }
    return 0;
}
