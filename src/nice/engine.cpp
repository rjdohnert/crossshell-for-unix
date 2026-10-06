#include "engine.hpp"

PriorityInfo PriorityMapper::GetCurrentNiceLevel() {
    DWORD dwClass = GetPriorityClass(GetCurrentProcess());
    switch (dwClass) {
        case REALTIME_PRIORITY_CLASS:     return { dwClass, -20, L"Realtime" };
        case HIGH_PRIORITY_CLASS:         return { dwClass, -10, L"High" };
        case ABOVE_NORMAL_PRIORITY_CLASS: return { dwClass, -5,  L"Above Normal" };
        case NORMAL_PRIORITY_CLASS:       return { dwClass, 0,   L"Normal" };
        case BELOW_NORMAL_PRIORITY_CLASS: return { dwClass, 10,  L"Below Normal" };
        case IDLE_PRIORITY_CLASS:         return { dwClass, 19,  L"Idle / Low" };
        default:                          return { dwClass, 0,   L"Normal" };
    }
}

DWORD PriorityMapper::MapNiceToPriorityClass(int niceVal) {
    if (niceVal <= -15) {
        return REALTIME_PRIORITY_CLASS;
    } else if (niceVal <= -5) {
        return HIGH_PRIORITY_CLASS;
    } else if (niceVal < 0) {
        return ABOVE_NORMAL_PRIORITY_CLASS;
    } else if (niceVal == 0) {
        return NORMAL_PRIORITY_CLASS;
    } else if (niceVal <= 10) {
        return BELOW_NORMAL_PRIORITY_CLASS;
    } else {
        return IDLE_PRIORITY_CLASS;
    }
}

int ProcessPriorityEngine::ExecuteWithPriority(int argc, wchar_t* argv[], const NiceOptions& options) {
    if (options.cmdIndex == -1 || options.cmdIndex >= argc) {
        PriorityInfo current = PriorityMapper::GetCurrentNiceLevel();
        std::wcout << current.niceness << L" (" << current.name << L")\n";
        return 0;
    }

    DWORD priorityClass = PriorityMapper::MapNiceToPriorityClass(options.niceIncrement);

    std::wstring commandLine;
    for (int i = options.cmdIndex; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg.find(L' ') != std::wstring::npos) {
            commandLine += L"\"" + arg + L"\"";
        } else {
            commandLine += arg;
        }
        if (i + 1 < argc) commandLine += L" ";
    }

    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION pi = {};

    BOOL created = CreateProcessW(
        NULL,
        const_cast<LPWSTR>(commandLine.c_str()),
        NULL, NULL,
        TRUE,
        priorityClass,
        NULL, NULL,
        &si, &pi
    );

    if (!created) {
        DWORD err = GetLastError();
        std::wcerr << L"nice: " << argv[options.cmdIndex] << L": Failed to launch process (Error Code: " 
                  << err << L")\n";
        return 1;
    }

    ScopedProcessHandle hProcess(pi.hProcess);
    ScopedProcessHandle hThread(pi.hThread);

    WaitForSingleObject(hProcess.Get(), INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(hProcess.Get(), &exitCode);

    return static_cast<int>(exitCode);
}
