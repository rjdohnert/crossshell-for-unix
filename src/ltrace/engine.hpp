#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "ltrace.hpp"
#include <unordered_map>
#include <fstream>

class LibraryTracer {
private:
    static constexpr DWORD kProcessVmAccess = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION;
    static constexpr DWORD kThreadDebugAccess = THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME;

    Config config;
    std::ofstream logStream;
    std::unordered_map<void*, Breakpoint> breakpoints;
    std::unordered_map<DWORD, ThreadStepState> pendingSteps;
    std::unordered_map<DWORD, HANDLE> processHandleCache;
    bool tableHeaderWritten = false;
    bool jsonFirstRecord = true;
#ifdef _WIN64
    std::unordered_map<DWORD, bool> wow64ProcessCache;
#endif
    DWORD primaryPid = 0;

    HANDLE acquireProcessHandle(DWORD processId);
    void releaseProcessHandle(DWORD processId);
#ifdef _WIN64
    bool isWow64TargetProcess(DWORD processId, HANDLE hProcess);
#endif
    static std::string wideToUtf8(const wchar_t* wstr, int len);
    bool canInspectPointer(HANDLE hProcess, uint64_t addr) const;

public:
    explicit LibraryTracer(const Config& cfg);
    ~LibraryTracer();

    bool shouldTraceModule(const std::string& moduleName);
    std::vector<DWORD> suspendOtherThreads(DWORD processId, DWORD currentThreadId);
    void resumeThreads(const std::vector<DWORD>& threadIds);
    std::string inspectArgument(HANDLE hProcess, uint64_t addr);
    void hookModuleExports(HANDLE hProcess, HMODULE hModule, const std::string& moduleName);
    std::string getModuleName(HANDLE hFile, void* baseAddr);
    void logCall(HANDLE hProcess, DWORD pid, const std::string& moduleName, const std::string& funcName,
                 uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4);
    DWORD handleException(const DEBUG_EVENT& dev);
    bool run();
};

#endif // ENGINE_HPP
