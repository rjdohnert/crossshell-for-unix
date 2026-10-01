/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
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

/**
 * ============================================================================
 * SINGLE FILE INDEX: stdbuf.cpp
 * ============================================================================
 * WinStdbuf - Object-Oriented Standard Stream Buffer Modifier for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & BUFFER CONFIG] ............. BufferConfig and StdbufOptions classes
 * 2. [THREADED RELAY ENGINE] ............... BufferRelayEngine class (stream worker threads)
 * 3. [PROCESS CONTROLLER] .................. StdbufProcessLauncher class (child process launch)
 * 4. [APPLICATION CONTROLLER] .............. StdbufApp class and wmain entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <sstream>
#include <cstdio>
#include <memory>

// ============================================================================
// 1. OPTIONS & BUFFER CONFIG
// ============================================================================

enum class BufferMode {
    Unbuffered,   // '0'
    LineBuffered, // 'L'
    BlockBuffered // 'SIZE' (e.g. 1024, 4096)
};

struct BufferConfig {
    BufferMode mode{BufferMode::LineBuffered};
    size_t size{4096};

    static BufferConfig parse(const std::wstring& str) {
        BufferConfig cfg;
        if (str == L"0") {
            cfg.mode = BufferMode::Unbuffered;
            cfg.size = 1;
        } else if (str == L"L" || str == L"l") {
            cfg.mode = BufferMode::LineBuffered;
            cfg.size = 4096;
        } else {
            cfg.mode = BufferMode::BlockBuffered;
            try {
                cfg.size = std::stoull(str);
            } catch (...) {
                cfg.size = 4096;
            }
        }
        return cfg;
    }
};

class StdbufOptions {
public:
    BufferConfig inConfig;
    BufferConfig outConfig;
    BufferConfig errConfig;
    bool inSet{false};
    bool outSet{false};
    bool errSet{false};
    std::vector<std::wstring> commandArgs;

    static void printHelp() {
        std::wcout << LR"(stdbuf(1)               CrossShell for UNIX Reference Manual                 stdbuf(1)

    NAME
        stdbuf - run command with modified buffering operations for standard streams

    SYNOPSIS
        stdbuf [OPTIONS] COMMAND [ARGUMENTS...]

    DESCRIPTION
        Runs COMMAND, modifying buffering operations for its standard streams
        (standard input, standard output, and standard error). MODE can be '0'
        for unbuffered, 'L' for line-buffered, or an explicit byte size for
        block buffering.

    OPTIONS
        -i, --input=MODE
            Adjust standard input stream buffering.

        -o, --output=MODE
            Adjust standard output stream buffering.

        -e, --error=MODE
            Adjust standard error stream buffering.

        -h, --help
            Display this reference manual.

        -v, -V, --version
            Display version and license information.

    EXAMPLES
        stdbuf -o0 tail -f log.txt
            Run tail -f with unbuffered standard output.

        stdbuf -iL -oL my_filter
            Run my_filter with line-buffered input and output streams.

        stdbuf -o 8192 producer | consumer
            Run producer with an 8 KB output buffer.

    CrossShell for UNIX                                                    stdbuf(1)
    )";
    }

    static void printVersion() {
        std::wcout << L"stdbuf 1.0\n";
    }

    static bool parse(int argc, wchar_t* argv[], StdbufOptions& opts) {
        int i = 1;
        for (; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"--help" || arg == L"-h" || arg == L"/?") {
                printHelp();
                std::exit(0);
            } else if (arg == L"--version" || arg == L"-V") {
                printVersion();
                std::exit(0);
            } else if (arg.rfind(L"-i", 0) == 0) {
                std::wstring val = arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : L"L");
                opts.inConfig = BufferConfig::parse(val);
                opts.inSet = true;
            } else if (arg.rfind(L"--input=", 0) == 0) {
                opts.inConfig = BufferConfig::parse(arg.substr(8));
                opts.inSet = true;
            } else if (arg.rfind(L"-o", 0) == 0) {
                std::wstring val = arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : L"L");
                opts.outConfig = BufferConfig::parse(val);
                opts.outSet = true;
            } else if (arg.rfind(L"--output=", 0) == 0) {
                opts.outConfig = BufferConfig::parse(arg.substr(9));
                opts.outSet = true;
            } else if (arg.rfind(L"-e", 0) == 0) {
                std::wstring val = arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : L"L");
                opts.errConfig = BufferConfig::parse(val);
                opts.errSet = true;
            } else if (arg.rfind(L"--error=", 0) == 0) {
                opts.errConfig = BufferConfig::parse(arg.substr(8));
                opts.errSet = true;
            } else if (arg[0] == L'-') {
                std::wcerr << L"stdbuf: unrecognized option: " << arg << L"\n";
                return false;
            } else {
                break;
            }
        }

        for (; i < argc; ++i) {
            opts.commandArgs.push_back(argv[i]);
        }

        if (opts.commandArgs.empty()) {
            std::wcerr << L"stdbuf: missing operand\n";
            std::wcerr << L"Try 'stdbuf --help' for more information.\n";
            return false;
        }

        return true;
    }
};

// ============================================================================
// 2. THREADED RELAY ENGINE
// ============================================================================

class BufferRelayEngine {
public:
    static void relayOutput(HANDLE hReadPipe, HANDLE hParentWrite, BufferConfig config) {
        char buffer[4096];
        DWORD bytesRead = 0;
        std::vector<char> lineBuffer;

        while (ReadFile(hReadPipe, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
            if (config.mode == BufferMode::Unbuffered) {
                DWORD bytesWritten = 0;
                WriteFile(hParentWrite, buffer, bytesRead, &bytesWritten, NULL);
                FlushFileBuffers(hParentWrite);
            } else if (config.mode == BufferMode::LineBuffered) {
                for (DWORD i = 0; i < bytesRead; ++i) {
                    lineBuffer.push_back(buffer[i]);
                    if (buffer[i] == '\n') {
                        DWORD bytesWritten = 0;
                        WriteFile(hParentWrite, lineBuffer.data(), static_cast<DWORD>(lineBuffer.size()), &bytesWritten, NULL);
                        FlushFileBuffers(hParentWrite);
                        lineBuffer.clear();
                    }
                }
            } else {
                lineBuffer.insert(lineBuffer.end(), buffer, buffer + bytesRead);
                while (lineBuffer.size() >= config.size) {
                    DWORD bytesWritten = 0;
                    WriteFile(hParentWrite, lineBuffer.data(), static_cast<DWORD>(config.size), &bytesWritten, NULL);
                    FlushFileBuffers(hParentWrite);
                    lineBuffer.erase(lineBuffer.begin(), lineBuffer.begin() + config.size);
                }
            }
        }

        if (!lineBuffer.empty()) {
            DWORD bytesWritten = 0;
            WriteFile(hParentWrite, lineBuffer.data(), static_cast<DWORD>(lineBuffer.size()), &bytesWritten, NULL);
            FlushFileBuffers(hParentWrite);
        }
    }
};

// ============================================================================
// 3. PROCESS CONTROLLER
// ============================================================================

class StdbufProcessLauncher {
public:
    static int launch(const StdbufOptions& opts) {
        std::wstring cmdline;
        for (size_t k = 0; k < opts.commandArgs.size(); ++k) {
            if (k > 0) cmdline += L" ";
            if (opts.commandArgs[k].find(L' ') != std::wstring::npos) {
                cmdline += L"\"" + opts.commandArgs[k] + L"\"";
            } else {
                cmdline += opts.commandArgs[k];
            }
        }

        HANDLE hOutRead = NULL, hOutWrite = NULL;
        HANDLE hErrRead = NULL, hErrWrite = NULL;

        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        CreatePipe(&hOutRead, &hOutWrite, &sa, 0);
        SetHandleInformation(hOutRead, HANDLE_FLAG_INHERIT, 0);

        CreatePipe(&hErrRead, &hErrWrite, &sa, 0);
        SetHandleInformation(hErrRead, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = hOutWrite;
        si.hStdError = hErrWrite;

        PROCESS_INFORMATION pi{};
        std::vector<wchar_t> cmdBuf(cmdline.begin(), cmdline.end());
        cmdBuf.push_back(L'\0');

        BOOL created = CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
        CloseHandle(hOutWrite);
        CloseHandle(hErrWrite);

        if (!created) {
            std::wcerr << L"stdbuf: failed to run " << opts.commandArgs[0] << L"\n";
            CloseHandle(hOutRead);
            CloseHandle(hErrRead);
            return 1;
        }

        std::thread outThread(BufferRelayEngine::relayOutput, hOutRead, GetStdHandle(STD_OUTPUT_HANDLE), opts.outConfig);
        std::thread errThread(BufferRelayEngine::relayOutput, hErrRead, GetStdHandle(STD_ERROR_HANDLE), opts.errConfig);

        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        outThread.join();
        errThread.join();

        CloseHandle(hOutRead);
        CloseHandle(hErrRead);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return static_cast<int>(exitCode);
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class StdbufApp {
public:
    static int run(int argc, wchar_t* argv[]) {
        StdbufOptions options;
        if (!StdbufOptions::parse(argc, argv, options)) {
            return 1;
        }
        return StdbufProcessLauncher::launch(options);
    }
};

int wmain(int argc, wchar_t* argv[]) {
    return StdbufApp::run(argc, argv);
}
