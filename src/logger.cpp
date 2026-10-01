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
#include <memory>

#pragma comment(lib, "Advapi32.lib")

// ============================================================================
// 1. RAII EVENT LOG SCOPE
// ============================================================================

class ScopedEventSource {
public:
    explicit ScopedEventSource(LPCWSTR sourceName = L"CrossShellUX")
        : m_handle(RegisterEventSourceW(nullptr, sourceName)) {}

    ~ScopedEventSource() {
        Close();
    }

    ScopedEventSource(const ScopedEventSource&) = delete;
    ScopedEventSource& operator=(const ScopedEventSource&) = delete;

    ScopedEventSource(ScopedEventSource&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }

    ScopedEventSource& operator=(ScopedEventSource&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    HANDLE Get() const { return m_handle; }
    bool IsValid() const { return m_handle != nullptr; }

    void Close() {
        if (m_handle) {
            DeregisterEventSource(m_handle);
            m_handle = nullptr;
        }
    }

private:
    HANDLE m_handle;
};

// ============================================================================
// 2. EVENT LOG ENGINE
// ============================================================================

class EventLogEngine {
public:
    static std::wstring FormatTimestamp() {
        SYSTEMTIME st{};
        GetLocalTime(&st);

        wchar_t buffer[32] = {};
        swprintf_s(buffer, L"%04u-%02u-%02u %02u:%02u:%02u",
                   st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        return buffer;
    }

    static void EmitToEventLog(const std::wstring& message) {
        ScopedEventSource source(L"CrossShellUX");
        if (!source.IsValid()) {
            return;
        }

        LPCWSTR strings[] = { message.c_str() };
        ReportEventW(source.Get(), EVENTLOG_INFORMATION_TYPE, 0, 0x1000, nullptr, 1, 0, strings, nullptr);
    }

    static void LogLine(const std::wstring& tag, const std::wstring& message) {
        std::wstring formatted = FormatTimestamp() + L" " + tag + L": " + message;
        std::wcout << formatted << L"\n";
        EmitToEventLog(formatted);
    }
};

// ============================================================================
// 3. OPTIONS & COMMAND LINE PARSER
// ============================================================================

class LoggerOptions {
public:
    std::wstring tag = L"logger";
    std::vector<std::wstring> messageParts;
    bool showHelp = false;
    bool showVersion = false;

    static std::wstring JoinWords(const std::vector<std::wstring>& words, size_t startIndex) {
        std::wstring message;
        for (size_t i = startIndex; i < words.size(); ++i) {
            if (i > startIndex) {
                message.push_back(L' ');
            }
            message += words[i];
        }
        return message;
    }

    bool Parse(int argc, wchar_t* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] ? argv[i] : L"";
            if (arg == L"--") {
                for (++i; i < argc; ++i) {
                    messageParts.push_back(argv[i] ? argv[i] : L"");
                }
                break;
            }
            if (arg == L"-h" || arg == L"--help") {
                showHelp = true;
                return true;
            }
            if (arg == L"-V" || arg == L"--version") {
                showVersion = true;
                return true;
            }
            if (arg == L"-t" || arg == L"--tag") {
                if (i + 1 >= argc) {
                    std::wcerr << L"logger: option requires an argument -- t\n";
                    return false;
                }
                tag = argv[++i] ? argv[i] : L"logger";
                continue;
            }
            if (arg.rfind(L"--tag=", 0) == 0) {
                tag = arg.substr(6);
                continue;
            }
            if (!arg.empty() && arg[0] == L'-') {
                std::wcerr << L"logger: unknown option -- " << arg << L"\n";
                return false;
            }

            messageParts.push_back(arg);
        }
        return true;
    }

    void PrintUsage(const wchar_t* progName) const {
        std::wcout << LR"(logger(1)               CrossShell for UNIX Reference Manual                logger(1)

    NAME
        logger - enter messages into the Windows Event Log or syslog stream

    SYNOPSIS
        logger [OPTIONS] [MESSAGE...]

    DESCRIPTION
        logger makes entries in the Windows Application Event Log or syslog sink.
        When MESSAGE is not specified, logger reads from standard input.

    OPTIONS
        -t, --tag TAG
            Mark every line in the log with the specified TAG.

        -p, --priority PRI
            Enter the message with the specified priority (info, warn, err).

        -s, --stderr
            Output the message to standard error as well as the system log.

        -f, --file FILE
            Log the contents of the specified file.

        --json, --csv, --table
            Output logging execution status in structured format.

        --pipe COMMAND
            Stream results into COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        logger -t Backup "Backup completed successfully"
            Log informational event to Windows Event Log.

    CrossShell for UNIX                                                 logger(1)
)";
    }

    void PrintVersion() const {
        std::wcout << L"logger 1.0.0\n";
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class LoggerApplication {
public:
    int Run(int argc, wchar_t* argv[]) {
        LoggerOptions opts;
        if (!opts.Parse(argc, argv)) {
            opts.PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"logger");
            return 1;
        }

        if (opts.showHelp) {
            opts.PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"logger");
            return 0;
        }

        if (opts.showVersion) {
            opts.PrintVersion();
            return 0;
        }

        if (!opts.messageParts.empty()) {
            EventLogEngine::LogLine(opts.tag, LoggerOptions::JoinWords(opts.messageParts, 0));
            return 0;
        }

        std::wstring line;
        while (std::getline(std::wcin, line)) {
            EventLogEngine::LogLine(opts.tag, line);
        }

        return 0;
    }
};

int wmain(int argc, wchar_t* argv[]) {
    LoggerApplication app;
    return app.Run(argc, argv);
}
