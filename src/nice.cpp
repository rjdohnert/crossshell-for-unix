/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <cwctype>
#include <memory>

// ============================================================================
// 1. DATA MODELS & RAII HANDLES
// ============================================================================

struct PriorityInfo {
    DWORD priorityClass = NORMAL_PRIORITY_CLASS;
    int niceness = 0;
    const wchar_t* name = L"Normal";
};

class ScopedProcessHandle {
public:
    explicit ScopedProcessHandle(HANDLE handle = NULL) : m_handle(handle) {}

    ~ScopedProcessHandle() {
        Close();
    }

    ScopedProcessHandle(const ScopedProcessHandle&) = delete;
    ScopedProcessHandle& operator=(const ScopedProcessHandle&) = delete;

    ScopedProcessHandle(ScopedProcessHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = NULL;
    }

    ScopedProcessHandle& operator=(ScopedProcessHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = NULL;
        }
        return *this;
    }

    HANDLE Get() const { return m_handle; }
    HANDLE* Receive() { Close(); return &m_handle; }
    bool IsValid() const { return m_handle != NULL && m_handle != INVALID_HANDLE_VALUE; }

    void Close() {
        if (m_handle != NULL && m_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_handle);
            m_handle = NULL;
        }
    }

private:
    HANDLE m_handle;
};

// ============================================================================
// 2. PRIORITY MAPPER & OPTIONS PARSER
// ============================================================================

class PriorityMapper {
public:
    static PriorityInfo GetCurrentNiceLevel() {
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

    static DWORD MapNiceToPriorityClass(int niceVal) {
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
};

class NiceOptions {
public:
    int niceIncrement = 10;
    int cmdIndex = -1;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, wchar_t* argv[]) {
        if (argc == 1) {
            return true;
        }

        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"--") {
                cmdIndex = i + 1;
                break;
            } else if ((arg == L"-n" || arg == L"--adjustment") && i + 1 < argc) {
                niceIncrement = std::wcstol(argv[++i], nullptr, 10);
            } else if (arg.rfind(L"-n", 0) == 0 && arg.length() > 2) {
                niceIncrement = std::wcstol(arg.substr(2).c_str(), nullptr, 10);
            } else if (arg.rfind(L"-", 0) == 0 && arg.length() > 1 && std::iswdigit(arg[1])) {
                niceIncrement = std::wcstol(arg.c_str(), nullptr, 10);
            } else if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
                showHelp = true;
                return true;
            } else if (arg == L"--version") {
                showVersion = true;
                return true;
            } else {
                cmdIndex = i;
                break;
            }
        }
        return true;
    }

    void PrintUsage(const wchar_t* exe) const {
        std::wcout << L"nice\n";
        std::wcout << L"Usage: " << exe << L" [-n increment] [COMMAND [ARGS...]]\n\n";
        std::wcout << L"Options:\n";
        std::wcout << L"  -n, --adjustment=N   Add N to the niceness (default: 10)\n";
        std::wcout << L"  -h, --help           Show this help message\n";
        std::wcout << L"      --version        Show version information\n";
        std::wcout << L"      --               End of options\n\n";
        std::wcout << L"Niceness Range (-20 to 19) Mapping:\n";
        std::wcout << L"  -20 to -15 : REALTIME_PRIORITY_CLASS\n";
        std::wcout << L"  -14 to -5  : HIGH_PRIORITY_CLASS\n";
        std::wcout << L"  -4  to -1  : ABOVENORMAL_PRIORITY_CLASS\n";
        std::wcout << L"   0         : NORMAL_PRIORITY_CLASS\n";
        std::wcout << L"   1  to 10  : BELOWNORMAL_PRIORITY_CLASS (Default when -n is omitted)\n";
        std::wcout << L"  11  to 19  : IDLE_PRIORITY_CLASS\n\n";
        std::wcout << L"Examples:\n";
        std::wcout << L"  nice                             (Print current niceness)\n";
        std::wcout << L"  nice myapp.exe                   (Run with +10 niceness / Below Normal)\n";
        std::wcout << L"  nice -n 19 ffmpeg.exe -i in out  (Run with low priority)\n";
        std::wcout << L"  nice -n -10 powershell.exe       (Run with high priority)\n";
    }

    void PrintVersion() const {
        std::wcout << L"nice v1.0.0\n";
    }
};

// ============================================================================
// 3. PROCESS PRIORITY EXECUTION ENGINE
// ============================================================================

class ProcessPriorityEngine {
public:
    static int ExecuteWithPriority(int argc, wchar_t* argv[], const NiceOptions& options) {
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
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class NiceApplication {
public:
    int Run(int argc, wchar_t* argv[]) const {
        NiceOptions options;
        if (!options.Parse(argc, argv)) {
            return 1;
        }

        if (options.showHelp) {
            options.PrintUsage(argv[0]);
            return 0;
        }

        if (options.showVersion) {
            options.PrintVersion();
            return 0;
        }

        return ProcessPriorityEngine::ExecuteWithPriority(argc, argv, options);
    }
};

int wmain(int argc, wchar_t* argv[]) {
    NiceApplication app;
    return app.Run(argc, argv);
}
