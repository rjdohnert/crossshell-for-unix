#include "engine.hpp"

// ============================================================================
// ProcessSnapshot Implementation
// ============================================================================

bool ProcessSnapshot::Collect(std::vector<ProcessRecord>& out) {
    out.clear();

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return false;
    }

    PROCESSENTRY32W pe = {};
    pe.dwSize = sizeof(pe);
    if (!Process32FirstW(snap, &pe)) {
        CloseHandle(snap);
        return false;
    }

    do {
        ProcessRecord p;
        p.pid = pe.th32ProcessID;
        p.name = pe.szExeFile;
        out.push_back(p);
    } while (Process32NextW(snap, &pe));

    CloseHandle(snap);
    return true;
}

// ============================================================================
// ProcessMatcher Implementation
// ============================================================================

std::wstring ProcessMatcher::ToLowerCopy(const std::wstring& input) {
    std::wstring result;
    result.reserve(input.size());
    for (wchar_t ch : input) {
        result.push_back(static_cast<wchar_t>(std::towlower(ch)));
    }
    return result;
}

bool ProcessMatcher::IsMatch(const std::wstring& name, const PgrepOptions& options) {
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
