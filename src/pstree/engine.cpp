#include "engine.hpp"

bool ProcessTreeEngine::EnableVirtualTerminalAndUnicode() noexcept {
    // Set stdout stream to UTF-16 wide mode
    if (_setmode(_fileno(stdout), _O_U16TEXT) == -1) {
        return false;
    }

    // Enable Virtual Terminal Processing for ANSI Escapes in Windows Console
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE && hOut != NULL) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
    return true;
}

std::wstring ProcessTreeEngine::QueryProcessImagePath(DWORD pid) noexcept {
    if (pid == 0 || pid == 4) {
        return L""; // System / Idle process image path unavailable
    }

    ScopedHandle hProcess(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
    if (!hProcess.isValid()) {
        return L"";
    }

    wchar_t pathBuffer[MAX_PATH];
    DWORD dwSize = MAX_PATH;
    if (QueryFullProcessImageNameW(hProcess.get(), 0, pathBuffer, &dwSize)) {
        return std::wstring(pathBuffer);
    }
    return L"";
}

bool ProcessTreeEngine::SnapshotProcesses(ProcessMap& outMap) noexcept {
    ScopedHandle hSnapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!hSnapshot.isValid()) {
        return false;
    }

    PROCESSENTRY32W pe32{};
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (!Process32FirstW(hSnapshot.get(), &pe32)) {
        return false;
    }

    do {
        ProcessNode node;
        node.pid = pe32.th32ProcessID;
        node.ppid = pe32.th32ParentProcessID;
        node.name = pe32.szExeFile;
        outMap[node.pid] = std::move(node);
    } while (Process32NextW(hSnapshot.get(), &pe32));

    return true;
}
