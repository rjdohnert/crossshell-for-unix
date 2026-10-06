#include "engine.hpp"

// ============================================================================
// SymbolResolver implementation
// ============================================================================

void SymbolResolver::Initialize(HANDLE hProcess) {
    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
    SymInitialize(hProcess, NULL, TRUE);
}

void SymbolResolver::Cleanup(HANDLE hProcess) {
    SymCleanup(hProcess);
}

std::string SymbolResolver::Resolve(HANDLE hProcess, DWORD64 address) {
    std::vector<char> buffer(sizeof(SYMBOL_INFO) + MAX_SYM_NAME);
    PSYMBOL_INFO pSymbol = reinterpret_cast<PSYMBOL_INFO>(buffer.data());
    pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    pSymbol->MaxNameLen = MAX_SYM_NAME;

    DWORD64 displacement = 0;
    if (SymFromAddr(hProcess, address, &displacement, pSymbol)) {
        std::ostringstream oss;
        oss << pSymbol->Name;
        if (displacement > 0) {
            oss << "+0x" << std::hex << std::uppercase << displacement;
        }
        return oss.str();
    }
    return "<unknown_symbol>";
}

// ============================================================================
// ProcessTracker implementation
// ============================================================================

ProcessTracker::~ProcessTracker() {
    CleanupAll();
}

void ProcessTracker::CleanupAll() {
    for (auto& entry : processHandles) {
        if (entry.second) {
            SymbolResolver::Cleanup(entry.second);
            CloseHandle(entry.second);
        }
    }
    processHandles.clear();

    for (auto& entry : threadHandles) {
        if (entry.second) {
            CloseHandle(entry.second);
        }
    }
    threadHandles.clear();
}

// ============================================================================
// TraceEngine implementation
// ============================================================================

int TraceEngine::Execute(TraceOptions& options) {
    OutputRedirectionGuard redir(options.outputPath, options.pipeCommand);

    if (options.outputFormat == OutputFormat::Json) {
        std::wcout << L"[\n";
    }

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    ProcessTracker tracker;

    if (options.attachToExistingProcess) {
        HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, options.targetPid);
        if (!hProcess) {
            DWORD err = GetLastError();
            TraceReporter::EmitTraceLine(options, L"[-] Failed to attach to PID " + std::to_wstring(options.targetPid) + L". Error " + std::to_wstring(err) + L": " + TraceFormatter::GetErrorMessage(err), NULL, 0, false);
            return 1;
        }
        if (!DebugActiveProcess(options.targetPid)) {
            DWORD err = GetLastError();
            TraceReporter::EmitTraceLine(options, L"[-] Failed to debug attach to PID " + std::to_wstring(options.targetPid) + L". Error " + std::to_wstring(err) + L": " + TraceFormatter::GetErrorMessage(err), NULL, 0, false);
            CloseHandle(hProcess);
            return 1;
        }
        pi.hProcess = hProcess;
        pi.dwProcessId = options.targetPid;
        tracker.processHandles[options.targetPid] = hProcess;
        tracker.activeProcessCount = 1;
        if (!options.quiet) {
            TraceReporter::EmitTraceLine(options, L"[*] Attached to existing process PID: " + std::to_wstring(options.targetPid), NULL, 0, false);
        }
    } else {
        std::wstring commandLine;
        for (size_t i = 0; i < options.commandParts.size(); ++i) {
            if (i > 0) commandLine += L" ";
            commandLine += TraceFormatter::QuoteCommandArg(options.commandParts[i]);
        }

        if (!options.quiet) {
            TraceReporter::EmitTraceLine(options, L"[*] Launching target process: " + commandLine, NULL, 0, false);
        }

        DWORD creationFlags = DEBUG_ONLY_THIS_PROCESS | CREATE_NEW_CONSOLE;
        if (options.followChildren) {
            creationFlags = DEBUG_PROCESS | CREATE_NEW_CONSOLE;
        }

        BOOL success = CreateProcessW(
            NULL,
            &commandLine[0],
            NULL,
            NULL,
            FALSE,
            creationFlags,
            NULL,
            NULL,
            &si,
            &pi
        );

        if (!success) {
            DWORD err = GetLastError();
            TraceReporter::EmitTraceLine(options, L"[-] Failed to launch target. Error " + std::to_wstring(err) + L": " + TraceFormatter::GetErrorMessage(err), NULL, 0, false);
            return 1;
        }

        if (!options.quiet) {
            TraceReporter::EmitTraceLine(options, L"[*] Process created successfully (PID: " + std::to_wstring(pi.dwProcessId) + L")", NULL, 0, false);
        }

        tracker.processHandles[pi.dwProcessId] = pi.hProcess;
        tracker.activeProcessCount = 1;
    }

    if (!options.quiet) {
        TraceReporter::EmitTraceLine(options, L"------------------------------------------------------------------", NULL, 0, false);
    }

    DEBUG_EVENT dbgEvent;
    bool keepDebugging = true;

    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    std::unordered_map<std::wstring, SummaryStats> summaryData;

    while (keepDebugging) {
        if (!WaitForDebugEvent(&dbgEvent, INFINITE)) {
            break;
        }

        LARGE_INTEGER startPerf, endPerf;
        QueryPerformanceCounter(&startPerf);

        DWORD continueStatus = DBG_CONTINUE;
        std::wstring eventName = L"UNKNOWN";

        switch (dbgEvent.dwDebugEventCode) {
        case CREATE_PROCESS_DEBUG_EVENT: {
            eventName = L"CREATE_PROCESS";
            CREATE_PROCESS_DEBUG_INFO& info = dbgEvent.u.CreateProcessInfo;
            std::wstring exeName = TraceFormatter::GetFileNameFromHandle(info.hFile);

            tracker.processHandles[dbgEvent.dwProcessId] = info.hProcess;
            tracker.threadHandles[dbgEvent.dwThreadId] = info.hThread;
            if (tracker.activeProcessCount == 0 || dbgEvent.dwProcessId != pi.dwProcessId) {
                ++tracker.activeProcessCount;
            }

            if (TraceReporter::ShouldPrintEvent(options, L"process")) {
                TraceReporter::EmitTraceLine(options, L"[+ CREATE_PROCESS] Image: " + exeName +
                    L" | Base: " + TraceFormatter::ToHex(reinterpret_cast<uintptr_t>(info.lpBaseOfImage)), info.hThread, (DWORD64)info.lpBaseOfImage);
            }

            SymbolResolver::Initialize(info.hProcess);
            if (info.hFile) CloseHandle(info.hFile);
            break;
        }

        case EXIT_PROCESS_DEBUG_EVENT: {
            eventName = L"EXIT_PROCESS";
            EXIT_PROCESS_DEBUG_INFO& info = dbgEvent.u.ExitProcess;
            if (TraceReporter::ShouldPrintEvent(options, L"process")) {
                TraceReporter::EmitTraceLine(options, L"[- EXIT_PROCESS] Exit Code: " + std::to_wstring(info.dwExitCode));
            }

            auto processIt = tracker.processHandles.find(dbgEvent.dwProcessId);
            if (processIt != tracker.processHandles.end()) {
                if (processIt->second) {
                    SymbolResolver::Cleanup(processIt->second);
                    CloseHandle(processIt->second);
                }
                tracker.processHandles.erase(processIt);
            }

            auto threadIt = tracker.threadHandles.find(dbgEvent.dwThreadId);
            if (threadIt != tracker.threadHandles.end()) {
                if (threadIt->second) {
                    CloseHandle(threadIt->second);
                }
                tracker.threadHandles.erase(threadIt);
            }

            if (dbgEvent.dwProcessId == pi.dwProcessId) {
                pi.hProcess = nullptr;
            }
            if (dbgEvent.dwThreadId == pi.dwThreadId) {
                pi.hThread = nullptr;
            }

            if (tracker.activeProcessCount > 0) {
                --tracker.activeProcessCount;
            }
            keepDebugging = (tracker.activeProcessCount > 0);
            break;
        }

        case CREATE_THREAD_DEBUG_EVENT: {
            eventName = L"CREATE_THREAD";
            CREATE_THREAD_DEBUG_INFO& info = dbgEvent.u.CreateThread;
            tracker.threadHandles[dbgEvent.dwThreadId] = info.hThread;
            if (TraceReporter::ShouldPrintEvent(options, L"thread")) {
                TraceReporter::EmitTraceLine(options, L"[+ CREATE_THREAD] TID: " + std::to_wstring(dbgEvent.dwThreadId) +
                    L" | StartAddr: " + TraceFormatter::ToHex(reinterpret_cast<uintptr_t>(info.lpStartAddress)), info.hThread, (DWORD64)info.lpStartAddress);
            }
            break;
        }

        case EXIT_THREAD_DEBUG_EVENT: {
            eventName = L"EXIT_THREAD";
            EXIT_THREAD_DEBUG_INFO& info = dbgEvent.u.ExitThread;
            if (TraceReporter::ShouldPrintEvent(options, L"thread")) {
                TraceReporter::EmitTraceLine(options, L"[- EXIT_THREAD] TID: " + std::to_wstring(dbgEvent.dwThreadId) +
                    L" | Exit Code: " + std::to_wstring(info.dwExitCode));
            }

            auto threadIt = tracker.threadHandles.find(dbgEvent.dwThreadId);
            if (threadIt != tracker.threadHandles.end()) {
                if (threadIt->second) {
                    CloseHandle(threadIt->second);
                }
                tracker.threadHandles.erase(threadIt);
            }

            if (dbgEvent.dwThreadId == pi.dwThreadId) {
                pi.hThread = nullptr;
            }
            break;
        }

        case LOAD_DLL_DEBUG_EVENT: {
            eventName = L"LOAD_DLL";
            LOAD_DLL_DEBUG_INFO& info = dbgEvent.u.LoadDll;
            std::wstring dllName = TraceFormatter::GetFileNameFromHandle(info.hFile);

            if (TraceReporter::ShouldPrintEvent(options, L"dll")) {
                TraceReporter::EmitTraceLine(options, L"[+ LOAD_DLL] Base: " + TraceFormatter::ToHex(reinterpret_cast<uintptr_t>(info.lpBaseOfDll)) +
                    L" | Path: " + dllName, tracker.threadHandles[dbgEvent.dwThreadId], (DWORD64)info.lpBaseOfDll);
            }

            if (info.hFile) CloseHandle(info.hFile);
            break;
        }

        case UNLOAD_DLL_DEBUG_EVENT: {
            eventName = L"UNLOAD_DLL";
            UNLOAD_DLL_DEBUG_INFO& info = dbgEvent.u.UnloadDll;
            if (TraceReporter::ShouldPrintEvent(options, L"dll")) {
                TraceReporter::EmitTraceLine(options, L"[- UNLOAD_DLL] Base: " + TraceFormatter::ToHex(reinterpret_cast<uintptr_t>(info.lpBaseOfDll)), tracker.threadHandles[dbgEvent.dwThreadId]);
            }
            break;
        }

        case OUTPUT_DEBUG_STRING_EVENT: {
            eventName = L"DEBUG_MSG";
            OUTPUT_DEBUG_STRING_INFO& info = dbgEvent.u.DebugString;
            WORD len = info.nDebugStringLength;
            if (len > 0) {
                HANDLE currentProcess = tracker.processHandles[dbgEvent.dwProcessId];
                if (!currentProcess) {
                    currentProcess = pi.hProcess;
                }

                std::wstring debugText;
                if (currentProcess) {
                    if (info.fUnicode) {
                        std::vector<wchar_t> buffer(len);
                        SIZE_T bytesRead = 0;
                        if (ReadProcessMemory(currentProcess, info.lpDebugStringData, buffer.data(), len * sizeof(wchar_t), &bytesRead)) {
                            size_t charCount = bytesRead / sizeof(wchar_t);
                            debugText.assign(buffer.data(), buffer.data() + charCount);
                        }
                    } else {
                        std::vector<char> buffer(len);
                        SIZE_T bytesRead = 0;
                        if (ReadProcessMemory(currentProcess, info.lpDebugStringData, buffer.data(), len, &bytesRead)) {
                            int wlen = MultiByteToWideChar(CP_ACP, 0, buffer.data(), static_cast<int>(bytesRead), nullptr, 0);
                            if (wlen > 0) {
                                debugText.resize(wlen);
                                MultiByteToWideChar(CP_ACP, 0, buffer.data(), static_cast<int>(bytesRead), debugText.data(), wlen);
                            }
                        }
                    }
                }

                if (TraceReporter::ShouldPrintEvent(options, L"debug") && !debugText.empty()) {
                    while (!debugText.empty() && (debugText.back() == L'\0' || debugText.back() == L'\r' || debugText.back() == L'\n')) {
                        debugText.pop_back();
                    }
                    if (!debugText.empty()) {
                        if (debugText.length() > options.stringLimit) {
                            debugText = debugText.substr(0, options.stringLimit) + L"...";
                        }
                        TraceReporter::EmitTraceLine(options, L"[* DEBUG_MSG] " + debugText, tracker.threadHandles[dbgEvent.dwThreadId]);
                    }
                }
            }
            break;
        }

        case EXCEPTION_DEBUG_EVENT: {
            EXCEPTION_DEBUG_INFO& info = dbgEvent.u.Exception;
            DWORD code = info.ExceptionRecord.ExceptionCode;
            PVOID addr = info.ExceptionRecord.ExceptionAddress;

            HANDLE currentProcess = tracker.processHandles[dbgEvent.dwProcessId];
            if (!currentProcess) {
                currentProcess = pi.hProcess;
            }
            std::string symName = currentProcess ? SymbolResolver::Resolve(currentProcess, reinterpret_cast<DWORD64>(addr)) : "<unknown_symbol>";

            if (code == STATUS_BREAKPOINT || code == STATUS_WX86_BREAKPOINT_VALUE || code == STATUS_SINGLE_STEP) {
                eventName = (code == STATUS_SINGLE_STEP) ? L"SINGLE_STEP" : L"BREAKPOINT";
                if (code == STATUS_SINGLE_STEP) {
                    if (TraceReporter::ShouldPrintEvent(options, L"exception")) {
                        TraceReporter::EmitTraceLine(options, L"[! TRACE_SINGLE_STEP] Address: " + TraceFormatter::ToHex(reinterpret_cast<uintptr_t>(addr)) +
                            L" (" + TraceFormatter::ToWideString(symName) + L")", tracker.threadHandles[dbgEvent.dwThreadId], reinterpret_cast<DWORD64>(addr));
                    }
                } else {
                    if (TraceReporter::ShouldPrintEvent(options, L"exception")) {
                        TraceReporter::EmitTraceLine(options, L"[! TRACE_BREAKPOINT] Address: " + TraceFormatter::ToHex(reinterpret_cast<uintptr_t>(addr)) +
                            L" (" + TraceFormatter::ToWideString(symName) + L")", tracker.threadHandles[dbgEvent.dwThreadId], reinterpret_cast<DWORD64>(addr));
                    }
                }
                continueStatus = DBG_CONTINUE;
            } else {
                eventName = L"EXCEPTION";
                if (TraceReporter::ShouldPrintEvent(options, L"exception")) {
                    TraceReporter::EmitTraceLine(options, L"[! EXCEPTION] Code: " + TraceFormatter::ToHex(static_cast<uint64_t>(code)) +
                        L" at Address: " + TraceFormatter::ToHex(reinterpret_cast<uintptr_t>(addr)) +
                        L" (" + TraceFormatter::ToWideString(symName) + L")", tracker.threadHandles[dbgEvent.dwThreadId], reinterpret_cast<DWORD64>(addr));
                }

                continueStatus = DBG_EXCEPTION_NOT_HANDLED;
            }
            break;
        }

        default:
            break;
        }

        QueryPerformanceCounter(&endPerf);
        double elapsed = static_cast<double>(endPerf.QuadPart - startPerf.QuadPart) / freq.QuadPart;

        if (dbgEvent.dwDebugEventCode != 0) {
            SummaryStats& stats = summaryData[eventName];
            stats.calls++;
            stats.seconds += elapsed;
        }

        ContinueDebugEvent(dbgEvent.dwProcessId, dbgEvent.dwThreadId, continueStatus);
    }

    if (options.attachToExistingProcess && pi.hProcess && tracker.activeProcessCount > 0) {
        DebugActiveProcessStop(options.targetPid);
    }

    if (pi.hThread) {
        CloseHandle(pi.hThread);
        pi.hThread = nullptr;
    }

    if (pi.hProcess) {
        CloseHandle(pi.hProcess);
        pi.hProcess = nullptr;
    }

    if (options.summaryOnly && options.outputFormat == OutputFormat::Human) {
        TraceReporter::PrintSummary(summaryData);
    }

    if (!options.quiet) {
        TraceReporter::EmitTraceLine(options, L"------------------------------------------------------------------", NULL, 0, false);
        TraceReporter::EmitTraceLine(options, L"[*] Tracing complete.", NULL, 0, false);
    }

    if (options.outputFormat == OutputFormat::Json) {
        std::wcout << L"\n]\n";
    }

    return 0;
}
