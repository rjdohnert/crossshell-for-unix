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
 * SINGLE FILE INDEX: script.cpp
 * ============================================================================
 * WinScript - Object-Oriented Console Session Transcript Recorder for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & COMMAND BUILDER] ........... ScriptOptions and CommandLineBuilder classes
 * 2. [FILE WRITER UTILITIES] ............... TranscriptWriter class (file appending/writing)
 * 3. [SESSION RECORDER ENGINE] ............. ScriptEngine class (child process & pipe tee)
 * 4. [APPLICATION CONTROLLER] .............. ScriptApp class and wmain entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>
#include <string>
#include <vector>
#include <memory>

// ============================================================================
// 1. OPTIONS & COMMAND BUILDER
// ============================================================================

class CommandLineBuilder {
public:
    static std::wstring quoteArgument(const std::wstring& arg) {
        if (arg.empty()) {
            return L"\"\"";
        }
        if (arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
            return arg;
        }

        std::wstring quoted = L"\"";
        int backslashes = 0;
        for (wchar_t c : arg) {
            if (c == L'\\') {
                ++backslashes;
            } else if (c == L'\"') {
                quoted.append(backslashes * 2 + 1, L'\\');
                quoted.push_back(L'\"');
                backslashes = 0;
            } else {
                quoted.append(backslashes, L'\\');
                quoted.push_back(c);
                backslashes = 0;
            }
        }
        quoted.append(backslashes * 2, L'\\');
        quoted.push_back(L'\"');
        return quoted;
    }

    static std::wstring build(const std::vector<std::wstring>& args) {
        std::wstring out;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) {
                out.push_back(L' ');
            }
            out += quoteArgument(args[i]);
        }
        return out;
    }
};

class ScriptOptions {
public:
    bool append{false};
    std::wstring outputFile{L"typescript"};
    std::vector<std::wstring> commandArgs;

    static void printUsage(const wchar_t* progName) {
        std::wcout
            << L"Usage: " << progName << L" [OPTIONS] [OUTPUT] [COMMAND [ARGS...]]\n"
            << L"Record a command session transcript to a file.\n\n"
            << L"Options:\n"
            << L"  -a, --append      append to the output file instead of truncating it\n"
            << L"  -h, --help        display this help and exit\n"
            << L"  -V, --version     output version information and exit\n"
            << L"  --                end of options\n\n"
            << L"Notes:\n"
            << L"  If COMMAND is omitted, script launches cmd.exe.\n";
    }

    static void printVersion() {
        std::wcout << L"script 1.0.0\n";
    }

    static bool parse(int argc, wchar_t* argv[], ScriptOptions& opts) {
        bool passthrough = false;
        bool outputAssigned = false;

        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (passthrough) {
                opts.commandArgs.push_back(arg);
                continue;
            }

            if (arg == L"--") {
                passthrough = true;
                continue;
            }

            if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
                printUsage(argv[0]);
                std::exit(0);
            }
            if (arg == L"-V" || arg == L"--version") {
                printVersion();
                std::exit(0);
            }
            if (arg == L"-a" || arg == L"--append") {
                opts.append = true;
                continue;
            }
            if (!arg.empty() && arg[0] == L'-') {
                std::wcerr << L"script: unrecognized option: " << arg << L"\n";
                printUsage(argv[0]);
                return false;
            }

            if (!outputAssigned) {
                opts.outputFile = arg;
                outputAssigned = true;
                continue;
            }

            opts.commandArgs.push_back(arg);
            passthrough = true;
        }

        return true;
    }
};

// ============================================================================
// 2. FILE WRITER UTILITIES
// ============================================================================

class TranscriptWriter {
public:
    static bool writeAll(HANDLE handle, const void* data, DWORD size) {
        const BYTE* bytes = static_cast<const BYTE*>(data);
        DWORD written = 0;
        while (size > 0) {
            if (!WriteFile(handle, bytes, size, &written, nullptr)) {
                return false;
            }
            bytes += written;
            size -= written;
        }
        return true;
    }

    static bool writeString(HANDLE handle, const std::string& text) {
        return writeAll(handle, text.data(), static_cast<DWORD>(text.size()));
    }
};

// ============================================================================
// 3. SESSION RECORDER ENGINE
// ============================================================================

class ScriptEngine {
private:
    ScriptOptions options;

public:
    explicit ScriptEngine(ScriptOptions opts) : options(std::move(opts)) {}

    int execute(const wchar_t* progName) {
        HANDLE fileHandle = CreateFileW(
            options.outputFile.c_str(),
            FILE_GENERIC_WRITE,
            FILE_SHARE_READ,
            nullptr,
            options.append ? OPEN_ALWAYS : CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (fileHandle == INVALID_HANDLE_VALUE) {
            std::wcerr << L"script: cannot open output file: " << options.outputFile << L"\n";
            return 1;
        }

        if (options.append) {
            SetFilePointer(fileHandle, 0, nullptr, FILE_END);
        }

        HANDLE stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        HANDLE stdinHandle = GetStdHandle(STD_INPUT_HANDLE);

        HANDLE pipeRead = nullptr;
        HANDLE pipeWrite = nullptr;
        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        if (!CreatePipe(&pipeRead, &pipeWrite, &sa, 0)) {
            std::wcerr << L"script: failed to create output pipe\n";
            CloseHandle(fileHandle);
            return 1;
        }

        SetHandleInformation(pipeRead, HANDLE_FLAG_INHERIT, 0);

        std::wstring cmdline;
        if (options.commandArgs.empty()) {
            wchar_t comspec[MAX_PATH];
            DWORD len = GetEnvironmentVariableW(L"COMSPEC", comspec, MAX_PATH);
            cmdline = (len > 0 && len < MAX_PATH) ? comspec : L"cmd.exe";
        } else {
            cmdline = CommandLineBuilder::build(options.commandArgs);
        }

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = stdinHandle;
        si.hStdOutput = pipeWrite;
        si.hStdError = pipeWrite;

        PROCESS_INFORMATION pi{};
        std::vector<wchar_t> cmdBuffer(cmdline.begin(), cmdline.end());
        cmdBuffer.push_back(L'\0');

        std::wcout << L"Script started, file is " << options.outputFile << L"\n";
        TranscriptWriter::writeString(fileHandle, "Script started on Windows\r\n");

        BOOL created = CreateProcessW(
            nullptr,
            cmdBuffer.data(),
            nullptr,
            nullptr,
            TRUE,
            0,
            nullptr,
            nullptr,
            &si,
            &pi);

        CloseHandle(pipeWrite);

        if (!created) {
            std::wcerr << L"script: failed to execute command: " << cmdline << L"\n";
            CloseHandle(pipeRead);
            CloseHandle(fileHandle);
            return 1;
        }

        BYTE buffer[4096];
        DWORD bytesRead = 0;
        while (ReadFile(pipeRead, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0) {
            TranscriptWriter::writeAll(stdoutHandle, buffer, bytesRead);
            TranscriptWriter::writeAll(fileHandle, buffer, bytesRead);
        }

        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(pipeRead);

        TranscriptWriter::writeString(fileHandle, "\r\nScript done on Windows\r\n");
        CloseHandle(fileHandle);

        std::wcout << L"Script done, file is " << options.outputFile << L"\n";
        return static_cast<int>(exitCode);
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class ScriptApp {
public:
    static int run(int argc, wchar_t* argv[]) {
        ScriptOptions options;
        if (!ScriptOptions::parse(argc, argv, options)) {
            return 1;
        }

        ScriptEngine engine(std::move(options));
        return engine.execute(argv[0]);
    }
};

int wmain(int argc, wchar_t* argv[]) {
    return ScriptApp::run(argc, argv);
}
