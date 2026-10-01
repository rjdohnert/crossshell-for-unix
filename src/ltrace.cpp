/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, cmd-extended contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <cstdint>
#include <cctype>
#include <cwctype>
#include <limits>
#include <cstdio>
#include <streambuf>

#pragma comment(lib, "psapi.lib")

// Terminal Color Formatting
namespace Color {
    const std::string RESET     = "\033[0m";
    const std::string BOLD      = "\033[1m";
    const std::string BLUE      = "\033[1;34m";
    const std::string CYAN      = "\033[1;36m";
    const std::string GREEN     = "\033[1;32m";
    const std::string YELLOW    = "\033[1;33m";
    const std::string RED       = "\033[1;31m";
    const std::string MAGENTA   = "\033[1;35m";
    const std::string GRAY      = "\033[90m";
    const std::string B_CYAN    = "\033[96m";
}

// Breakpoint Metadata
struct Breakpoint {
    void* address;
    BYTE originalByte;
    std::string moduleName;
    std::string functionName;
    bool isArmed;
};

// Thread Single-Step State Tracking with Suspension List
struct ThreadStepState {
    void* rearmAddress;
    std::vector<DWORD> suspendedThreadIds;
};

// Application Configuration
struct Config {
    DWORD targetPid = 0;
    std::string commandLine = "";
    std::vector<std::string> filterModules;
    std::string outputFile = "";
    bool followChildren = true;
    bool useColor = true;
    bool verbose = false;
    bool allExports = false;
    enum class OutputFormat { Human, Json, Csv, Table } outputFormat = OutputFormat::Human;
    std::string pipeCommand;
};

class PipeStreambuf : public std::streambuf {
private:
    FILE* pipe_;
    char buffer_[4096];

protected:
    int_type overflow(int_type ch) override {
        if (ch != traits_type::eof()) {
            *pptr() = static_cast<char>(ch);
            pbump(1);
        }
        return sync() == 0 ? traits_type::not_eof(ch) : traits_type::eof();
    }

    int sync() override {
        std::ptrdiff_t count = pptr() - pbase();
        if (count > 0 && std::fwrite(pbase(), 1, static_cast<size_t>(count), pipe_) != static_cast<size_t>(count)) return -1;
        setp(buffer_, buffer_ + sizeof(buffer_));
        return std::fflush(pipe_) == 0 ? 0 : -1;
    }

public:
    explicit PipeStreambuf(FILE* pipe) : pipe_(pipe) { setp(buffer_, buffer_ + sizeof(buffer_)); }
    ~PipeStreambuf() override { sync(); }
};

std::string jsonEscape(const std::string& value) {
    std::string out;
    for (unsigned char ch : value) {
        switch (ch) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (ch < 0x20) out.push_back('?');
            else out.push_back(static_cast<char>(ch));
            break;
        }
    }
    return out;
}

std::string csvEscape(const std::string& value) {
    std::string out = "\"";
    for (char ch : value) {
        if (ch == '"') out += "\"\"";
        else out.push_back(ch);
    }
    return out + "\"";
}

// Enable Windows Console Virtual Terminal Processing (ANSI)
bool initConsole() {
    SetConsoleOutputCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return false;

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return false;
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    return SetConsoleMode(hOut, dwMode) != 0;
}

// Lowercase Converter
std::string toLower(const std::string& input) {
    std::string result = input;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

// Library Tracer Engine
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

    HANDLE acquireProcessHandle(DWORD processId) {
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

    void releaseProcessHandle(DWORD processId) {
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
    bool isWow64TargetProcess(DWORD processId, HANDLE hProcess) {
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

    static std::string wideToUtf8(const wchar_t* wstr, int len) {
        if (!wstr || len <= 0) return std::string();

        int required = WideCharToMultiByte(CP_UTF8, 0, wstr, len, nullptr, 0, nullptr, nullptr);
        if (required <= 0) return std::string();

        std::string out(static_cast<size_t>(required), '\0');
        int converted = WideCharToMultiByte(CP_UTF8, 0, wstr, len, out.data(), required, nullptr, nullptr);
        if (converted <= 0) return std::string();
        out.resize(static_cast<size_t>(converted));
        return out;
    }

    bool canInspectPointer(HANDLE hProcess, uint64_t addr) const {
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

public:
    explicit LibraryTracer(const Config& cfg) : config(cfg) {
        if (!config.outputFile.empty()) {
            logStream.open(config.outputFile, std::ios::out | std::ios::app);
        }
    }

    ~LibraryTracer() {
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

    // Module Name Matching Check
    bool shouldTraceModule(const std::string& moduleName) {
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

    // Suspend Other Threads during Single-Step Execution to Prevent Race Conditions
    std::vector<DWORD> suspendOtherThreads(DWORD processId, DWORD currentThreadId) {
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

    // Resume Threads after Single-Step Re-arm
    void resumeThreads(const std::vector<DWORD>& threadIds) {
        for (DWORD tid : threadIds) {
            HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, tid);
            if (hThread) {
                ResumeThread(hThread);
                CloseHandle(hThread);
            }
        }
    }

    // Dereference String Pointers in Target Memory
    std::string inspectArgument(HANDLE hProcess, uint64_t addr) {
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

    // Parse PE Export Directory and Hook Functions
    void hookModuleExports(HANDLE hProcess, HMODULE hModule, const std::string& moduleName) {
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

    // Resolve Module Name
    std::string getModuleName(HANDLE hFile, void* baseAddr) {
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

    // Format and Output API Interception Line
    void logCall(HANDLE hProcess, DWORD pid, const std::string& moduleName, const std::string& funcName,
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

    // Exception Debug Event Handler
    DWORD handleException(const DEBUG_EVENT& dev) {
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

                            // Restore original byte at function entry point
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

                            // Log parameter values and strings
                            logCall(hProcess, processId, bp.moduleName, bp.functionName, ctx.Rcx, ctx.Rdx, ctx.R8, ctx.R9);

                            // Restore original byte at function entry point
                            SIZE_T written = 0;
                            DWORD oldProtect = 0;
                            VirtualProtectEx(hProcess, bp.address, 1, PAGE_EXECUTE_READWRITE, &oldProtect);
                            WriteProcessMemory(hProcess, bp.address, &bp.originalByte, 1, &written);
                            FlushInstructionCache(hProcess, bp.address, 1);
                            VirtualProtectEx(hProcess, bp.address, 1, oldProtect, &oldProtect);

                            // Set CPU Trap Flag to trigger SINGLE_STEP
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

                // Resume suspended sibling threads
                resumeThreads(suspended);

                return DBG_CONTINUE;
            }
        }

        return DBG_EXCEPTION_NOT_HANDLED;
    }

    // Start Debugging Session
    bool run() {
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
};

// Comprehensive Help Interface Screen
void showHelp() {
    std::cout << R"(ltrace(1)                CrossShell for UNIX Reference Manual                 ltrace(1)

    NAME
        ltrace - trace dynamic library calls made by a process

    SYNOPSIS
        ltrace [OPTIONS] -- COMMAND [ARGUMENTS...]
        ltrace [OPTIONS] -p PID

    DESCRIPTION
        Intercepts and logs dynamic link library (DLL) function calls made by a
        process. Tracing can be attached to an existing process or applied while
        launching a command. Module filters and structured output are supported.

    OPTIONS
        -p, --pid <pid>
            Attach to an existing running process by process ID.

        -l, --lib <module>
            Restrict tracing to a DLL module, such as kernel32 or user32. May be
            specified multiple times.

        -o, --output <file>
            Append trace output to FILE.

        -v, --verbose
            Display diagnostic information about symbol hooking.

        --all-exports
            Disable the default unfiltered export hook cap.

        --no-color
            Disable ANSI color output.

        --json
            Emit machine-readable JSON output.

        --csv
            Emit CSV output.

        --table
            Emit tabular output.

        --pipe <command>
            Send formatted output to COMMAND.

        -h, --help
            Display this comprehensive reference manual and exit.

        -V, --version
            Display version information and exit.

        --
            End options and introduce the command and its arguments.

    EXAMPLES
        ltrace -- notepad.exe
            Launch Notepad and trace its dynamic library calls.

        ltrace -l kernel32 -l user32 -- notepad.exe
            Trace calls made specifically to kernel32.dll and user32.dll.

        ltrace -p 4812
            Attach to process 4812 and trace its API calls.

        ltrace -o trace_log.txt -l ntdll -- ping.exe 127.0.0.1
            Write filtered traces to a file for later analysis.

    EXIT STATUS
        0
            Help, version, or a completed tracing session.
        1
            Invalid options, failed attachment or launch, or pipe startup failure.

    CrossShell for UNIX                                                    ltrace(1)
)";
}

void showVersion() {
    std::cout << Color::BOLD << Color::B_CYAN << "ltrace" << Color::RESET << " version 1.1.0\n";
}

int main(int argc, char* argv[]) {
    initConsole();
    Config config;
    FILE* outputPipe = nullptr;
    std::streambuf* oldOutputBuffer = nullptr;
    PipeStreambuf* pipeBuffer = nullptr;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            showHelp();
            return 0;
        } else if (arg == "-V" || arg == "--version") {
            showVersion();
            return 0;
        } else if (arg == "-p" || arg == "--pid") {
            if (i + 1 < argc) config.targetPid = std::strtoul(argv[++i], NULL, 10);
        } else if (arg == "-l" || arg == "--lib") {
            if (i + 1 < argc) config.filterModules.push_back(argv[++i]);
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) config.outputFile = argv[++i];
        } else if (arg == "-v" || arg == "--verbose") {
            config.verbose = true;
        } else if (arg == "--all-exports") {
            config.allExports = true;
        } else if (arg == "--no-color") {
            config.useColor = false;
        } else if (arg == "--json") {
            config.outputFormat = Config::OutputFormat::Json;
        } else if (arg == "--csv") {
            config.outputFormat = Config::OutputFormat::Csv;
        } else if (arg == "--table") {
            config.outputFormat = Config::OutputFormat::Table;
        } else if (arg == "--pipe") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --pipe requires a command\n";
                return 1;
            }
            config.pipeCommand = argv[++i];
        } else if (arg == "--") {
            std::string cmd;
            for (int j = i + 1; j < argc; ++j) {
                cmd += argv[j];
                if (j + 1 < argc) cmd += " ";
            }
            config.commandLine = cmd;
            break;
        } else if (arg[0] == '-' && arg.length() > 1) {
            std::cerr << Color::RED << "Error: Unknown option '" << arg << "'" << Color::RESET << "\n";
            std::cerr << "For usage information, run: ltrace --help\n";
            return 1;
        } else {
            if (config.commandLine.empty()) {
                config.commandLine = arg;
            }
        }
    }

    if (config.targetPid == 0 && config.commandLine.empty()) {
        std::cerr << Color::RED << "Error: No target program or PID specified." << Color::RESET << "\n";
        std::cerr << "For usage information, run: ltrace --help\n";
        return 1;
    }

    if (!config.pipeCommand.empty()) {
        outputPipe = _popen(config.pipeCommand.c_str(), "w");
        if (!outputPipe) {
            std::cerr << "Error: Failed to start pipe command '" << config.pipeCommand << "'\n";
            return 1;
        }
        pipeBuffer = new PipeStreambuf(outputPipe);
        oldOutputBuffer = std::cout.rdbuf(pipeBuffer);
    }

    LibraryTracer tracer(config);
    if (!tracer.run()) {
        if (oldOutputBuffer) std::cout.rdbuf(oldOutputBuffer);
        if (pipeBuffer) { delete pipeBuffer; _pclose(outputPipe); }
        return 1;
    }

    if (oldOutputBuffer) std::cout.rdbuf(oldOutputBuffer);
    if (pipeBuffer) { delete pipeBuffer; _pclose(outputPipe); }

    return 0;
}