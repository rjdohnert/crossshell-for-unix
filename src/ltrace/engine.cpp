#include "engine.hpp"
#include <tlhelp32.h>
#include <psapi.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <cwctype>
#include <limits>

HANDLE LibraryTracer::acquireProcessHandle(DWORD processId) {
    auto it = processHandleCache.find(processId);
    if (it != processHandleCache.end() && it->second) {
        return it->second;
    }

    HANDLE hProcess = OpenProcess(kProcessVmAccess, FALSE, processId);
    if (hProcess) {
        processHandleCache[processId] = hProcess;
    }
    return hProcess;
}

void LibraryTracer::releaseProcessHandle(DWORD processId) {
    auto it = processHandleCache.find(processId);
    if (it != processHandleCache.end()) {
        if (it->second) {
            CloseHandle(it->second);
        }
        processHandleCache.erase(it);
    }
#ifdef _WIN64
    wow64ProcessCache.erase(processId);
#endif
}

#ifdef _WIN64
bool LibraryTracer::isWow64TargetProcess(DWORD processId, HANDLE hProcess) {
    auto it = wow64ProcessCache.find(processId);
    if (it != wow64ProcessCache.end()) {
        return it->second;
    }

    BOOL isWow64 = FALSE;
    if (IsWow64Process(hProcess, &isWow64)) {
        wow64ProcessCache[processId] = (isWow64 != FALSE);
        return isWow64 != FALSE;
    }

    wow64ProcessCache[processId] = false;
    return false;
}
#endif

std::string LibraryTracer::wideToUtf8(const wchar_t* wstr, int len) {
    if (!wstr || len <= 0) return std::string();

    int required = WideCharToMultiByte(CP_UTF8, 0, wstr, len, nullptr, 0, nullptr, nullptr);
    if (required <= 0) return std::string();

    std::string out(static_cast<size_t>(required), '\0');
    int converted = WideCharToMultiByte(CP_UTF8, 0, wstr, len, out.data(), required, nullptr, nullptr);
    if (converted <= 0) return std::string();
    out.resize(static_cast<size_t>(converted));
    return out;
}

bool LibraryTracer::canInspectPointer(HANDLE hProcess, uint64_t addr) const {
    if (addr < 0x10000ULL) return false;
    if (addr > 0x7FFFFFFFFFFFULL) return false;

    uintptr_t p = static_cast<uintptr_t>(addr);
    MEMORY_BASIC_INFORMATION mbi = {};
    if (VirtualQueryEx(hProcess, reinterpret_cast<LPCVOID>(p), &mbi, sizeof(mbi)) == 0) {
        return false;
    }

    if (mbi.State != MEM_COMMIT) return false;
    if ((mbi.Protect & PAGE_GUARD) || (mbi.Protect & PAGE_NOACCESS)) return false;

    DWORD readableMask = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                        PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return (mbi.Protect & readableMask) != 0;
}

LibraryTracer::LibraryTracer(const Config& cfg) : config(cfg) {
    if (!config.outputFile.empty()) {
        logStream.open(config.outputFile, std::ios::out | std::ios::app);
    }
}

LibraryTracer::~LibraryTracer() {
    for (auto& [pid, handle] : processHandleCache) {
        if (handle) {
            CloseHandle(handle);
        }
    }
    processHandleCache.clear();
#ifdef _WIN64
    wow64ProcessCache.clear();
#endif

    if (logStream.is_open()) {
        logStream.close();
    }
}

bool LibraryTracer::shouldTraceModule(const std::string& moduleName) {
    if (config.filterModules.empty()) return true;

    std::string lowerMod = toLower(moduleName);
    for (const auto& filter : config.filterModules) {
        std::string lowerFilter = toLower(filter);
        if (lowerMod.find(lowerFilter) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::vector<DWORD> LibraryTracer::suspendOtherThreads(DWORD processId, DWORD currentThreadId) {
    std::vector<DWORD> suspended;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return suspended;

    THREADENTRY32 te = { sizeof(te) };
    if (Thread32First(hSnap, &te)) {
        do {
            if (te.th32OwnerProcessID == processId && te.th32ThreadID != currentThreadId) {
                HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
                if (hThread) {
                    if (SuspendThread(hThread) != (DWORD)-1) {
                        suspended.push_back(te.th32ThreadID);
                    }
                    CloseHandle(hThread);
                }
            }
        } while (Thread32Next(hSnap, &te));
    }
    CloseHandle(hSnap);
    return suspended;
}

void LibraryTracer::resumeThreads(const std::vector<DWORD>& threadIds) {
    for (DWORD tid : threadIds) {
        HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, tid);
        if (hThread) {
            ResumeThread(hThread);
            CloseHandle(hThread);
        }
    }
}

std::string LibraryTracer::inspectArgument(HANDLE hProcess, uint64_t addr) {
    if (!canInspectPointer(hProcess, addr)) {
        std::ostringstream oss;
        oss << "0x" << std::hex << addr;
        return oss.str();
    }

    // Attempt ANSI String Inspection
    char bufA[64] = {0};
    SIZE_T bytesRead = 0;
    if (ReadProcessMemory(hProcess, reinterpret_cast<void*>(addr), bufA, sizeof(bufA) - 1, &bytesRead) && bytesRead > 0) {
        bool isAscii = true;
        size_t len = 0;
        for (size_t i = 0; i < bytesRead && bufA[i] != '\0'; ++i) {
            if (!std::isprint(static_cast<unsigned char>(bufA[i])) && bufA[i] != '\r' && bufA[i] != '\n' && bufA[i] != '\t') {
                isAscii = false;
                break;
            }
            len++;
        }
        if (isAscii && len >= 2) {
            return "\"" + std::string(bufA, len) + "\"";
        }
    }

    // Attempt UTF-16 Wide String Inspection
    wchar_t bufW[64] = {0};
    if (ReadProcessMemory(hProcess, reinterpret_cast<void*>(addr), bufW, sizeof(bufW) - sizeof(wchar_t), &bytesRead) && bytesRead > 0) {
        bool isWide = true;
        size_t len = 0;
        for (size_t i = 0; i < (bytesRead / sizeof(wchar_t)) && bufW[i] != L'\0'; ++i) {
            if (!std::iswprint(bufW[i]) && bufW[i] != L'\r' && bufW[i] != L'\n' && bufW[i] != L'\t') {
                isWide = false;
                break;
            }
            len++;
        }
        if (isWide && len >= 2) {
            std::string narrowStr = wideToUtf8(bufW, static_cast<int>(len));
            if (!narrowStr.empty()) {
                return "L\"" + narrowStr + "\"";
            }
        }
    }

    std::ostringstream oss;
    oss << "0x" << std::hex << addr;
    return oss.str();
}

void LibraryTracer::hookModuleExports(HANDLE hProcess, HMODULE hModule, const std::string& moduleName) {
    IMAGE_DOS_HEADER dosHeader;
    SIZE_T bytesRead = 0;
    BYTE* baseAddr = reinterpret_cast<BYTE*>(hModule);

    if (!ReadProcessMemory(hProcess, baseAddr, &dosHeader, sizeof(dosHeader), &bytesRead) || dosHeader.e_magic != IMAGE_DOS_SIGNATURE) {
        return;
    }

    DWORD ntSignature = 0;
    if (!ReadProcessMemory(hProcess, baseAddr + dosHeader.e_lfanew, &ntSignature, sizeof(ntSignature), &bytesRead) || ntSignature != IMAGE_NT_SIGNATURE) {
        return;
    }

    IMAGE_FILE_HEADER fileHeader;
    BYTE* fileHeaderPtr = baseAddr + dosHeader.e_lfanew + sizeof(DWORD);
    if (!ReadProcessMemory(hProcess, fileHeaderPtr, &fileHeader, sizeof(fileHeader), &bytesRead)) {
        return;
    }

    WORD optionalMagic = 0;
    BYTE* optionalHeaderPtr = fileHeaderPtr + sizeof(IMAGE_FILE_HEADER);
    if (!ReadProcessMemory(hProcess, optionalHeaderPtr, &optionalMagic, sizeof(optionalMagic), &bytesRead)) {
        return;
    }

    DWORD exportRVA = 0;
    DWORD exportSize = 0;
    if (optionalMagic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        IMAGE_OPTIONAL_HEADER64 opt64;
        if (!ReadProcessMemory(hProcess, optionalHeaderPtr, &opt64, sizeof(opt64), &bytesRead)) {
            return;
        }
        exportRVA = opt64.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
        exportSize = opt64.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
    } else if (optionalMagic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        IMAGE_OPTIONAL_HEADER32 opt32;
        if (!ReadProcessMemory(hProcess, optionalHeaderPtr, &opt32, sizeof(opt32), &bytesRead)) {
            return;
        }
        exportRVA = opt32.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
        exportSize = opt32.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
    } else {
        return;
    }

    if (exportRVA == 0 || exportSize == 0) return;

    IMAGE_EXPORT_DIRECTORY exportDir;
    if (!ReadProcessMemory(hProcess, baseAddr + exportRVA, &exportDir, sizeof(exportDir), &bytesRead)) return;

    std::vector<DWORD> functions(exportDir.NumberOfFunctions);
    std::vector<DWORD> names(exportDir.NumberOfNames);
    std::vector<WORD> ordinals(exportDir.NumberOfNames);

    ReadProcessMemory(hProcess, baseAddr + exportDir.AddressOfFunctions, functions.data(), sizeof(DWORD) * exportDir.NumberOfFunctions, &bytesRead);
    ReadProcessMemory(hProcess, baseAddr + exportDir.AddressOfNames, names.data(), sizeof(DWORD) * exportDir.NumberOfNames, &bytesRead);
    ReadProcessMemory(hProcess, baseAddr + exportDir.AddressOfNameOrdinals, ordinals.data(), sizeof(WORD) * exportDir.NumberOfNames, &bytesRead);

    size_t hookLimit = (config.filterModules.empty() && !config.allExports)
        ? 512
        : (std::numeric_limits<size_t>::max)();
    size_t hookedCount = 0;
    bool capped = false;
    for (DWORD i = 0; i < exportDir.NumberOfNames; ++i) {
        if (hookedCount >= hookLimit) {
            capped = true;
            break;
        }

        char funcNameBuffer[256] = {0};
        ReadProcessMemory(hProcess, baseAddr + names[i], funcNameBuffer, sizeof(funcNameBuffer) - 1, &bytesRead);
        std::string funcName(funcNameBuffer);

        WORD ordinal = ordinals[i];
        DWORD funcRVA = functions[ordinal];
        if (funcRVA == 0) continue;

        // Ignore forwarded exports
        if (funcRVA >= exportRVA && funcRVA < exportRVA + exportSize) continue;

        void* targetAddr = baseAddr + funcRVA;
        if (breakpoints.find(targetAddr) != breakpoints.end()) continue;

        BYTE origByte = 0;
        if (!ReadProcessMemory(hProcess, targetAddr, &origByte, 1, &bytesRead) || origByte == 0xCC) continue;

        BYTE int3 = 0xCC;
        SIZE_T bytesWritten = 0;
        DWORD oldProtect = 0;

        VirtualProtectEx(hProcess, targetAddr, 1, PAGE_EXECUTE_READWRITE, &oldProtect);
        if (WriteProcessMemory(hProcess, targetAddr, &int3, 1, &bytesWritten)) {
            FlushInstructionCache(hProcess, targetAddr, 1);

            Breakpoint bp;
            bp.address = targetAddr;
            bp.originalByte = origByte;
            bp.moduleName = moduleName;
            bp.functionName = funcName;
            bp.isArmed = true;

            breakpoints[targetAddr] = bp;
            hookedCount++;
        }
        VirtualProtectEx(hProcess, targetAddr, 1, oldProtect, &oldProtect);
    }

    if (config.verbose) {
        if (config.useColor) {
            std::cout << Color::GRAY << "[+] Hooked " << hookedCount << " symbols in " << moduleName << Color::RESET << "\n";
        } else {
            std::cout << "[+] Hooked " << hookedCount << " symbols in " << moduleName << "\n";
        }

        if (capped) {
            if (config.useColor) {
                std::cout << Color::GRAY << "[*] Hook limit reached for " << moduleName << " (use -l/--lib to narrow tracing or --all-exports to disable the cap)." << Color::RESET << "\n";
            } else {
                std::cout << "[*] Hook limit reached for " << moduleName << " (use -l/--lib to narrow tracing or --all-exports to disable the cap).\n";
            }
        }
    }
}

std::string LibraryTracer::getModuleName(HANDLE hFile, void* baseAddr) {
    char pathBuf[MAX_PATH] = {0};
    if (hFile && GetFinalPathNameByHandleA(hFile, pathBuf, MAX_PATH, VOLUME_NAME_DOS)) {
        std::string p(pathBuf);
        size_t pos = p.find_last_of("\\/");
        if (pos != std::string::npos) return p.substr(pos + 1);
        return p;
    }
    std::ostringstream oss;
    oss << "module_" << std::hex << reinterpret_cast<uintptr_t>(baseAddr);
    return oss.str();
}

void LibraryTracer::logCall(HANDLE hProcess, DWORD pid, const std::string& moduleName, const std::string& funcName,
                            uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4) {
    std::string s1 = inspectArgument(hProcess, arg1);
    std::string s2 = inspectArgument(hProcess, arg2);
    std::string s3 = inspectArgument(hProcess, arg3);
    std::string s4 = inspectArgument(hProcess, arg4);

    std::ostringstream line;
    if (config.outputFormat == Config::OutputFormat::Json) {
        if (!jsonFirstRecord) std::cout << ",\n";
        jsonFirstRecord = false;
        line << "{\"pid\":" << pid << ",\"module\":\"" << jsonEscape(moduleName)
             << "\",\"function\":\"" << jsonEscape(funcName) << "\",\"arg1\":\""
             << jsonEscape(s1) << "\",\"arg2\":\"" << jsonEscape(s2)
             << "\",\"arg3\":\"" << jsonEscape(s3) << "\",\"arg4\":\""
             << jsonEscape(s4) << "\"}";
    } else if (config.outputFormat == Config::OutputFormat::Csv) {
        line << csvEscape(std::to_string(pid)) << "," << csvEscape(moduleName) << ","
             << csvEscape(funcName) << "," << csvEscape(s1) << "," << csvEscape(s2) << ","
             << csvEscape(s3) << "," << csvEscape(s4);
    } else if (config.outputFormat == Config::OutputFormat::Table) {
        if (!tableHeaderWritten) {
            std::cout << "PID\tMODULE\tFUNCTION\tARG1\tARG2\tARG3\tARG4\n";
            tableHeaderWritten = true;
        }
        line << pid << "\t" << moduleName << "\t" << funcName << "\t" << s1 << "\t" << s2 << "\t" << s3 << "\t" << s4;
    } else if (config.useColor) {
        line << Color::GRAY << "[PID " << std::setw(5) << pid << "] " << Color::RESET
             << Color::CYAN << moduleName << Color::RESET << "!"
             << Color::GREEN << Color::BOLD << funcName << Color::RESET << "("
             << Color::YELLOW << s1 << Color::RESET << ", "
             << Color::YELLOW << s2 << Color::RESET << ", "
             << Color::YELLOW << s3 << Color::RESET << ", "
             << Color::YELLOW << s4 << Color::RESET << ")";
    } else {
        line << "[PID " << std::setw(5) << pid << "] "
             << moduleName << "!" << funcName << "("
             << s1 << ", " << s2 << ", " << s3 << ", " << s4 << ")";
    }

    std::cout << line.str() << "\n";

    if (logStream.is_open()) {
        std::ostringstream raw;
        raw << "[PID " << pid << "] " << moduleName << "!" << funcName
            << "(" << s1 << ", " << s2 << ", " << s3 << ", " << s4 << ")\n";
        logStream << raw.str();
        logStream.flush();
    }
}

DWORD LibraryTracer::handleException(const DEBUG_EVENT& dev) {
    const EXCEPTION_RECORD& er = dev.u.Exception.ExceptionRecord;
    DWORD threadId = dev.dwThreadId;
    DWORD processId = dev.dwProcessId;

    if (er.ExceptionCode == EXCEPTION_BREAKPOINT) {
        void* exceptionAddr = er.ExceptionAddress;
        auto it = breakpoints.find(exceptionAddr);

        if (it != breakpoints.end()) {
            Breakpoint& bp = it->second;

            HANDLE hThread = OpenThread(kThreadDebugAccess, FALSE, threadId);
            HANDLE hProcess = acquireProcessHandle(processId);

            if (hThread && hProcess) {
#ifdef _WIN64
                if (isWow64TargetProcess(processId, hProcess)) {
                    WOW64_CONTEXT ctx32 = { 0 };
                    ctx32.ContextFlags = WOW64_CONTEXT_CONTROL | WOW64_CONTEXT_INTEGER;

                    if (Wow64GetThreadContext(hThread, &ctx32)) {
                        ctx32.Eip--; // Rewind EIP past the 0xCC byte

                        uint32_t a1 = 0, a2 = 0, a3 = 0, a4 = 0;
                        SIZE_T stackRead = 0;
                        ReadProcessMemory(hProcess, reinterpret_cast<const void*>(static_cast<uintptr_t>(ctx32.Esp + 4)), &a1, sizeof(a1), &stackRead);
                        ReadProcessMemory(hProcess, reinterpret_cast<const void*>(static_cast<uintptr_t>(ctx32.Esp + 8)), &a2, sizeof(a2), &stackRead);
                        ReadProcessMemory(hProcess, reinterpret_cast<const void*>(static_cast<uintptr_t>(ctx32.Esp + 12)), &a3, sizeof(a3), &stackRead);
                        ReadProcessMemory(hProcess, reinterpret_cast<const void*>(static_cast<uintptr_t>(ctx32.Esp + 16)), &a4, sizeof(a4), &stackRead);
                        logCall(hProcess, processId, bp.moduleName, bp.functionName, a1, a2, a3, a4);

                        SIZE_T written = 0;
                        DWORD oldProtect = 0;
                        VirtualProtectEx(hProcess, bp.address, 1, PAGE_EXECUTE_READWRITE, &oldProtect);
                        WriteProcessMemory(hProcess, bp.address, &bp.originalByte, 1, &written);
                        FlushInstructionCache(hProcess, bp.address, 1);
                        VirtualProtectEx(hProcess, bp.address, 1, oldProtect, &oldProtect);

                        ctx32.EFlags |= 0x100;
                        Wow64SetThreadContext(hThread, &ctx32);

                        pendingSteps[threadId] = { bp.address, {} };
                    }
                } else {
                    CONTEXT ctx = { 0 };
                    ctx.ContextFlags = CONTEXT_FULL;

                    if (GetThreadContext(hThread, &ctx)) {
                        ctx.Rip--; // Rewind RIP past the 0xCC byte

                        logCall(hProcess, processId, bp.moduleName, bp.functionName, ctx.Rcx, ctx.Rdx, ctx.R8, ctx.R9);

                        SIZE_T written = 0;
                        DWORD oldProtect = 0;
                        VirtualProtectEx(hProcess, bp.address, 1, PAGE_EXECUTE_READWRITE, &oldProtect);
                        WriteProcessMemory(hProcess, bp.address, &bp.originalByte, 1, &written);
                        FlushInstructionCache(hProcess, bp.address, 1);
                        VirtualProtectEx(hProcess, bp.address, 1, oldProtect, &oldProtect);

                        ctx.EFlags |= 0x100;
                        SetThreadContext(hThread, &ctx);

                        pendingSteps[threadId] = { bp.address, {} };
                    }
                }
#else
                CONTEXT ctx = { 0 };
                ctx.ContextFlags = CONTEXT_FULL;

                if (GetThreadContext(hThread, &ctx)) {
                    ctx.Eip--;

                    uint32_t a1 = 0, a2 = 0, a3 = 0, a4 = 0;
                    SIZE_T stackRead = 0;
                    ReadProcessMemory(hProcess, reinterpret_cast<const void*>(ctx.Esp + 4), &a1, sizeof(a1), &stackRead);
                    ReadProcessMemory(hProcess, reinterpret_cast<const void*>(ctx.Esp + 8), &a2, sizeof(a2), &stackRead);
                    ReadProcessMemory(hProcess, reinterpret_cast<const void*>(ctx.Esp + 12), &a3, sizeof(a3), &stackRead);
                    ReadProcessMemory(hProcess, reinterpret_cast<const void*>(ctx.Esp + 16), &a4, sizeof(a4), &stackRead);
                    logCall(hProcess, processId, bp.moduleName, bp.functionName, a1, a2, a3, a4);

                    SIZE_T written = 0;
                    DWORD oldProtect = 0;
                    VirtualProtectEx(hProcess, bp.address, 1, PAGE_EXECUTE_READWRITE, &oldProtect);
                    WriteProcessMemory(hProcess, bp.address, &bp.originalByte, 1, &written);
                    FlushInstructionCache(hProcess, bp.address, 1);
                    VirtualProtectEx(hProcess, bp.address, 1, oldProtect, &oldProtect);

                    ctx.EFlags |= 0x100;
                    SetThreadContext(hThread, &ctx);

                    pendingSteps[threadId] = { bp.address, {} };
                }
#endif
                CloseHandle(hThread);
            }
            return DBG_CONTINUE;
        }
    } else if (er.ExceptionCode == EXCEPTION_SINGLE_STEP) {
        auto itStep = pendingSteps.find(threadId);
        if (itStep != pendingSteps.end()) {
            void* rearmAddr = itStep->second.rearmAddress;
            std::vector<DWORD> suspended = itStep->second.suspendedThreadIds;
            pendingSteps.erase(itStep);

            HANDLE hProcess = acquireProcessHandle(processId);
            if (hProcess) {
                BYTE int3 = 0xCC;
                SIZE_T written = 0;
                DWORD oldProtect = 0;

                VirtualProtectEx(hProcess, rearmAddr, 1, PAGE_EXECUTE_READWRITE, &oldProtect);
                WriteProcessMemory(hProcess, rearmAddr, &int3, 1, &written);
                FlushInstructionCache(hProcess, rearmAddr, 1);
                VirtualProtectEx(hProcess, rearmAddr, 1, oldProtect, &oldProtect);
            }

            resumeThreads(suspended);
            return DBG_CONTINUE;
        }
    }

    return DBG_EXCEPTION_NOT_HANDLED;
}

bool LibraryTracer::run() {
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };

    if (config.targetPid != 0) {
        primaryPid = config.targetPid;
        if (!DebugActiveProcess(config.targetPid)) {
            std::cerr << Color::RED << "Error: Failed to attach to PID " << config.targetPid 
                      << ". (Error code: " << GetLastError() << ")" << Color::RESET << "\n";
            return false;
        }
        std::cout << Color::GREEN << "[+] Attached to process PID " << config.targetPid << Color::RESET << "\n";
    } else if (!config.commandLine.empty()) {
        std::vector<char> cmdBuf(config.commandLine.begin(), config.commandLine.end());
        cmdBuf.push_back('\0');

        BOOL success = CreateProcessA(
            NULL, cmdBuf.data(), NULL, NULL, FALSE,
            DEBUG_ONLY_THIS_PROCESS | CREATE_NEW_CONSOLE,
            NULL, NULL, &si, &pi
        );

        if (!success) {
            std::cerr << Color::RED << "Error: Failed to launch command '" << config.commandLine 
                      << "'. (Error code: " << GetLastError() << ")" << Color::RESET << "\n";
            return false;
        }
        primaryPid = pi.dwProcessId;
        std::cout << Color::GREEN << "[+] Process launched with PID " << primaryPid << Color::RESET << "\n";
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    } else {
        std::cerr << Color::RED << "Error: No target specified." << Color::RESET << "\n";
        return false;
    }

    if (config.outputFormat == Config::OutputFormat::Json) std::cout << "[\n";
    DEBUG_EVENT debugEvent;
    bool processActive = true;

    // Debugger Control Loop
    while (processActive) {
        if (!WaitForDebugEvent(&debugEvent, INFINITE)) {
            break;
        }

        DWORD dwContinueStatus = DBG_CONTINUE;

        switch (debugEvent.dwDebugEventCode) {
        case CREATE_PROCESS_DEBUG_EVENT: {
            std::string modName = getModuleName(debugEvent.u.CreateProcessInfo.hFile, debugEvent.u.CreateProcessInfo.lpBaseOfImage);
            if (debugEvent.u.CreateProcessInfo.hFile) {
                CloseHandle(debugEvent.u.CreateProcessInfo.hFile);
            }
            if (shouldTraceModule(modName)) {
                HANDLE hProc = acquireProcessHandle(debugEvent.dwProcessId);
                if (hProc) {
                    hookModuleExports(hProc, static_cast<HMODULE>(debugEvent.u.CreateProcessInfo.lpBaseOfImage), modName);
                }
            }
            break;
        }

        case LOAD_DLL_DEBUG_EVENT: {
            std::string modName = getModuleName(debugEvent.u.LoadDll.hFile, debugEvent.u.LoadDll.lpBaseOfDll);
            if (debugEvent.u.LoadDll.hFile) {
                CloseHandle(debugEvent.u.LoadDll.hFile);
            }

            if (shouldTraceModule(modName)) {
                HANDLE hProc = acquireProcessHandle(debugEvent.dwProcessId);
                if (hProc) {
                    hookModuleExports(hProc, static_cast<HMODULE>(debugEvent.u.LoadDll.lpBaseOfDll), modName);
                }
            }
            break;
        }

        case EXCEPTION_DEBUG_EVENT:
            dwContinueStatus = handleException(debugEvent);
            break;

        case EXIT_PROCESS_DEBUG_EVENT:
            releaseProcessHandle(debugEvent.dwProcessId);
            if (debugEvent.dwProcessId == primaryPid) {
                processActive = false;
                std::cout << Color::GRAY << "[*] Primary target process " << primaryPid << " exited." << Color::RESET << "\n";
            }
            break;

        default:
            break;
        }

        ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId, dwContinueStatus);
    }

    if (config.outputFormat == Config::OutputFormat::Json) std::cout << "\n]\n";
    return true;
}
