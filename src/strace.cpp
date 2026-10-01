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

#include <windows.h>
#include <dbghelp.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <streambuf>
#include <memory>

// Link against DbgHelp library for symbol resolution
#pragma comment(lib, "dbghelp.lib")

// ============================================================================
// 1. CONSTANTS, ENUMS & DATA MODELS
// ============================================================================

constexpr DWORD STATUS_WX86_BREAKPOINT_VALUE = 0x4000001F;

enum class OutputFormat {
    Human,
    Json,
    Csv,
    Table
};

struct SummaryStats {
    DWORD calls = 0;
    double seconds = 0.0;
};

// ============================================================================
// 2. STREAMS, BUFFER GUARDS & STRING FORMATTERS
// ============================================================================

class WidePipeStreambuf : public std::wstreambuf {
public:
    explicit WidePipeStreambuf(FILE* pipe) : m_pipe(pipe) {
        setp(m_buffer, m_buffer + (sizeof(m_buffer) / sizeof(m_buffer[0])));
    }

    ~WidePipeStreambuf() override {
        sync();
    }

protected:
    int_type overflow(int_type ch) override {
        if (ch != traits_type::eof()) {
            *pptr() = static_cast<wchar_t>(ch);
            pbump(1);
        }
        return sync() == 0 ? traits_type::not_eof(ch) : traits_type::eof();
    }

    int sync() override {
        std::ptrdiff_t count = pptr() - pbase();
        if (count > 0 && std::fwrite(pbase(), sizeof(wchar_t), static_cast<size_t>(count), m_pipe) != static_cast<size_t>(count)) {
            return -1;
        }
        setp(m_buffer, m_buffer + (sizeof(m_buffer) / sizeof(m_buffer[0])));
        return std::fflush(m_pipe) == 0 ? 0 : -1;
    }

private:
    FILE* m_pipe = nullptr;
    wchar_t m_buffer[2048] = {};
};

class OutputRedirectionGuard {
public:
    OutputRedirectionGuard(const std::wstring& outputPath, const std::wstring& pipeCommand) {
        if (!outputPath.empty()) {
            m_traceFile.open(outputPath, std::ios::out | std::ios::binary);
            if (m_traceFile) {
                m_oldBuffer = std::wcout.rdbuf();
                std::wcout.rdbuf(m_traceFile.rdbuf());
                m_isFileRedirected = true;
            } else {
                std::wcerr << L"strace: failed to open output file: " << outputPath << std::endl;
            }
        } else if (!pipeCommand.empty()) {
            m_pipe = _wpopen(pipeCommand.c_str(), L"w");
            if (m_pipe) {
                m_oldBuffer = std::wcout.rdbuf();
                m_pipeBuffer = std::make_unique<WidePipeStreambuf>(m_pipe);
                std::wcout.rdbuf(m_pipeBuffer.get());
                m_isPipeRedirected = true;
            } else {
                std::wcerr << L"strace: failed to start pipe command" << std::endl;
            }
        }
    }

    ~OutputRedirectionGuard() {
        if (m_oldBuffer != nullptr) {
            std::wcout.rdbuf(m_oldBuffer);
        }
        if (m_traceFile.is_open()) {
            m_traceFile.close();
        }
        m_pipeBuffer.reset();
        if (m_pipe != nullptr) {
            _pclose(m_pipe);
            m_pipe = nullptr;
        }
    }

    OutputRedirectionGuard(const OutputRedirectionGuard&) = delete;
    OutputRedirectionGuard& operator=(const OutputRedirectionGuard&) = delete;

private:
    std::wofstream m_traceFile;
    FILE* m_pipe = nullptr;
    std::unique_ptr<WidePipeStreambuf> m_pipeBuffer;
    std::wstreambuf* m_oldBuffer = nullptr;
    bool m_isFileRedirected = false;
    bool m_isPipeRedirected = false;
};

class TraceFormatter {
public:
    static std::wstring JsonEscape(const std::wstring& value) {
        std::wstring out;
        for (wchar_t ch : value) {
            switch (ch) {
            case L'"': out += L"\\\""; break;
            case L'\\': out += L"\\\\"; break;
            case L'\n': out += L"\\n"; break;
            case L'\r': out += L"\\r"; break;
            case L'\t': out += L"\\t"; break;
            default: out += (ch < 0x20) ? L'?' : ch; break;
            }
        }
        return out;
    }

    static std::wstring CsvEscape(const std::wstring& value) {
        std::wstring out = L"\"";
        for (wchar_t ch : value) {
            if (ch == L'"') out += L"\"\"";
            else out += ch;
        }
        return out + L"\"";
    }

    static std::wstring Trim(const std::wstring& input) {
        const std::wstring whitespace = L" \t\r\n";
        size_t start = input.find_first_not_of(whitespace);
        if (start == std::wstring::npos) {
            return L"";
        }
        size_t end = input.find_last_not_of(whitespace);
        return input.substr(start, end - start + 1);
    }

    template <typename T>
    static std::wstring ToHex(T value, int width = 0) {
        std::wostringstream oss;
        oss << L"0x" << std::hex << std::uppercase;
        if (width > 0) {
            oss << std::setfill(L'0') << std::setw(width);
        }
        oss << static_cast<uint64_t>(value);
        return oss.str();
    }

    static std::wstring ToWideString(const std::string& input, UINT codePage = CP_ACP) {
        if (input.empty()) return L"";
        int size = MultiByteToWideChar(codePage, 0, input.c_str(), -1, nullptr, 0);
        if (size <= 0) {
            return std::wstring(input.begin(), input.end());
        }
        std::wstring output(size - 1, L'\0');
        MultiByteToWideChar(codePage, 0, input.c_str(), -1, output.data(), size);
        return output;
    }

    static std::wstring QuoteCommandArg(const std::wstring& arg) {
        if (arg.empty()) return L"\"\"";
        bool needsQuotes = arg.find_first_of(L" \t\r\n\"") != std::wstring::npos;
        if (!needsQuotes) return arg;

        std::wstring escaped = L"\"";
        for (wchar_t ch : arg) {
            if (ch == L'"') escaped += L"\\\"";
            else escaped += ch;
        }
        escaped += L"\"";
        return escaped;
    }

    static std::wstring FormatTimestamp() {
        SYSTEMTIME st;
        GetLocalTime(&st);
        std::wostringstream oss;
        oss << std::setfill(L'0')
            << std::setw(2) << st.wHour << L":"
            << std::setw(2) << st.wMinute << L":"
            << std::setw(2) << st.wSecond << L"."
            << std::setw(3) << st.wMilliseconds;
        return oss.str();
    }

    static std::wstring GetErrorMessage(DWORD dwErrorCode) {
        LPWSTR lpMsgBuf = nullptr;
        DWORD size = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, dwErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPWSTR>(&lpMsgBuf), 0, NULL);

        std::wstring message;
        if (size && lpMsgBuf) {
            message = lpMsgBuf;
            LocalFree(lpMsgBuf);
        } else {
            message = L"Unknown error";
        }
        return message;
    }

    static std::wstring GetFileNameFromHandle(HANDLE hFile) {
        if (!hFile) return L"<unknown>";
        WCHAR szFileName[MAX_PATH] = { 0 };
        DWORD dwSize = GetFinalPathNameByHandleW(hFile, szFileName, MAX_PATH, VOLUME_NAME_DOS);
        if (dwSize > 0 && dwSize < MAX_PATH) {
            std::wstring path = szFileName;
            if (path.rfind(L"\\\\?\\", 0) == 0) {
                return path.substr(4);
            }
            return path;
        }
        return L"<unknown>";
    }
};

// ============================================================================
// 3. SYMBOL RESOLVER & OPTIONS
// ============================================================================

class SymbolResolver {
public:
    static void Initialize(HANDLE hProcess) {
        SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
        SymInitialize(hProcess, NULL, TRUE);
    }

    static void Cleanup(HANDLE hProcess) {
        SymCleanup(hProcess);
    }

    static std::string Resolve(HANDLE hProcess, DWORD64 address) {
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
};

class TraceOptions {
public:
    bool followChildren = false;
    bool timestamp = false;
    bool verbose = false;
    bool quiet = false;
    bool filterEvents = false;
    std::vector<std::wstring> eventFilters;
    bool attachToExistingProcess = false;
    DWORD targetPid = 0;
    bool summaryOnly = false;
    bool printIp = false;
    DWORD stringLimit = 32;
    OutputFormat outputFormat = OutputFormat::Human;
    bool jsonFirstRecord = true;
    std::wstring outputPath;
    std::wstring pipeCommand;
    std::vector<std::wstring> commandParts;

    bool Parse(int argc, wchar_t* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"--help") {
                PrintHelp();
                return false;
            }
            if (arg == L"--version") {
                PrintVersion();
                return false;
            }
            if (arg == L"-f" || arg == L"-F" || arg == L"--follow") {
                followChildren = true;
                continue;
            }
            if (arg == L"-p" || arg == L"--pid") {
                if (i + 1 >= argc) {
                    std::wcerr << L"strace: option requires an argument -- " << arg.substr(1) << std::endl;
                    return false;
                }
                std::wstring pidText = argv[++i];
                attachToExistingProcess = true;
                targetPid = static_cast<DWORD>(std::wcstoul(pidText.c_str(), nullptr, 10));
                continue;
            }
            if (arg == L"-tt" || arg == L"--timestamp") {
                timestamp = true;
                continue;
            }
            if (arg == L"-v" || arg == L"--verbose") {
                verbose = true;
                continue;
            }
            if (arg == L"-q" || arg == L"-qq" || arg == L"--quiet") {
                quiet = true;
                continue;
            }
            if (arg == L"-c" || arg == L"--summary-only") {
                summaryOnly = true;
                continue;
            }
            if (arg == L"-i" || arg == L"--instruction-pointer") {
                printIp = true;
                continue;
            }
            if (arg == L"-s" || arg == L"--string-limit") {
                if (i + 1 >= argc) {
                    std::wcerr << L"strace: option requires an argument -- " << arg.substr(1) << std::endl;
                    return false;
                }
                std::wstring limitText = argv[++i];
                stringLimit = static_cast<DWORD>(std::wcstoul(limitText.c_str(), nullptr, 10));
                continue;
            }
            if (arg == L"-e" || arg == L"--events") {
                if (i + 1 >= argc) {
                    std::wcerr << L"strace: option requires an argument -- " << arg.substr(1) << std::endl;
                    return false;
                }
                std::wstring eventSpec = argv[++i];
                if (eventSpec.rfind(L"trace=", 0) == 0) {
                    eventSpec = eventSpec.substr(6);
                }

                std::wstringstream stream(eventSpec);
                std::wstring token;
                filterEvents = true;
                while (std::getline(stream, token, L',')) {
                    std::wstring cleaned = TraceFormatter::Trim(token);
                    if (!cleaned.empty()) {
                        eventFilters.push_back(cleaned);
                    }
                }
                if (eventFilters.empty()) {
                    std::wcerr << L"strace: no event classes specified" << std::endl;
                    return false;
                }
                continue;
            }
            if (arg == L"-o" || arg == L"--output") {
                if (i + 1 >= argc) {
                    std::wcerr << L"strace: option requires an argument -- " << arg.substr(1) << std::endl;
                    return false;
                }
                outputPath = argv[++i];
                continue;
            }
            if (arg == L"--json" || arg == L"--csv" || arg == L"--table") {
                outputFormat = (arg == L"--json") ? OutputFormat::Json
                    : (arg == L"--csv" ? OutputFormat::Csv : OutputFormat::Table);
                continue;
            }
            if (arg == L"--pipe") {
                if (i + 1 >= argc) {
                    std::wcerr << L"strace: option requires an argument -- pipe" << std::endl;
                    return false;
                }
                pipeCommand = argv[++i];
                continue;
            }
            commandParts.push_back(arg);
        }

        if (!attachToExistingProcess && commandParts.empty()) {
            PrintHelp();
            return false;
        }

        return true;
    }

    void PrintHelp() const {
           std::wcout << LR"(strace(1)               CrossShell for UNIX Reference Manual                  strace(1)

    NAME
        strace - trace process, thread, DLL, exception, and debug events

    SYNOPSIS
        strace [OPTIONS] COMMAND [ARGUMENTS...]
        strace [OPTIONS] --pid PID

    DESCRIPTION
        Launches a target under the Windows debugger or attaches to an existing
        process, then reports process creation, thread activity, DLL loading,
        debug strings, and exceptions.

    OPTIONS
        -f, --follow; -F
            Trace child processes created by the target.

        -p, --pid PID
            Attach to an existing process instead of launching a command.

        -tt, --timestamp
            Prefix event lines with a local timestamp.

        -v, --verbose
            Print additional detail for selected events.

        -q, -qq, --quiet
            Suppress informational banners and show only traced events.

        -e, --events LIST
            Restrict output to process, thread, dll, exception, or debug events.
            UNIX syntax such as trace=process,dll is also accepted.

        -o, --output FILE
            Write trace output to FILE instead of standard output.

        --json, --csv, --table
            Select JSON, CSV, or tabular output.

        --pipe COMMAND
            Send formatted output through COMMAND.

        -c, --summary-only
            Print time, call, and error summaries for each event class on exit.

        -i, --instruction-pointer
            Prefix trace lines with the instruction pointer address.

        -s, --string-limit LIMIT
            Set the maximum printed string size; the default is 32.

        --help
            Display this comprehensive reference manual and exit.

        --version
            Display version information and exit.

    NOTES
        Use cmd /c when tracing shell commands or pipelines. Native executables are
        best suited to debugger-based tracing.

    EXAMPLES
        strace cmd /c dir
        strace -f -tt cmd /c dir
        strace -F -q -e trace=process,thread notepad.exe
        strace -p 1234 -tt -e process,thread
        strace -e process,dll -v notepad.exe
        strace -o trace.txt notepad.exe
        strace -c cmd /c dir
        strace -i cmd /c dir
        strace -s 128 notepad.exe

    EXIT STATUS
        0          Successful tracing session.
        1          Help, version, parse, attachment, launch, or tracing setup failure.

    CrossShell for UNIX                                                       strace(1)
    )";
    }

    void PrintVersion() const {
        std::wcout << L"strace v1.0.0\n";
    }
};

// ============================================================================
// 4. TRACE REPORTER & PROCESS TRACKER
// ============================================================================

class TraceReporter {
public:
    static std::wstring FormatIpPrefix(const TraceOptions& options, HANDLE hThread, DWORD64 defaultIp) {
        if (!options.printIp) return L"";
        DWORD64 ip = defaultIp;
        if (ip == 0 && hThread != NULL) {
            CONTEXT context = {};
            context.ContextFlags = CONTEXT_CONTROL;
            if (GetThreadContext(hThread, &context)) {
#ifdef _M_X64
                ip = context.Rip;
#else
                ip = context.Eip;
#endif
            }
        }
        return L"[" + TraceFormatter::ToHex(ip, 16) + L"] ";
    }

    static void EmitTraceLine(TraceOptions& options, const std::wstring& line, HANDLE hThread = NULL, DWORD64 defaultIp = 0, bool isEvent = true) {
        if (isEvent && options.summaryOnly) return;
        if (!isEvent && options.outputFormat != OutputFormat::Human) return;

        std::wstring prefix = L"";
        if (options.timestamp) {
            prefix += L"[" + TraceFormatter::FormatTimestamp() + L"] ";
        }
        if (isEvent && options.printIp) {
            prefix += FormatIpPrefix(options, hThread, defaultIp);
        }
        if (options.outputFormat == OutputFormat::Json) {
            if (!options.jsonFirstRecord) std::wcout << L",\n";
            options.jsonFirstRecord = false;
            std::wcout << L"{\"type\":\"event\",\"message\":\"" << TraceFormatter::JsonEscape(prefix + line) << L"\"}" << std::endl;
        } else if (options.outputFormat == OutputFormat::Csv) {
            std::wcout << TraceFormatter::CsvEscape(L"event") << L"," << TraceFormatter::CsvEscape(prefix + line) << std::endl;
        } else if (options.outputFormat == OutputFormat::Table) {
            static bool headerWritten = false;
            if (!headerWritten) { std::wcout << L"TYPE\tMESSAGE\n"; headerWritten = true; }
            std::wcout << L"event\t" << prefix << line << std::endl;
        } else {
            std::wcout << prefix << line << std::endl;
        }
    }

    static bool ShouldPrintEvent(const TraceOptions& options, const std::wstring& category) {
        if (!options.filterEvents) return true;
        for (const auto& filter : options.eventFilters) {
            if (filter == category) return true;
        }
        return false;
    }

    static void PrintSummary(const std::unordered_map<std::wstring, SummaryStats>& summaryData) {
        double totalSeconds = 0.0;
        DWORD totalCalls = 0;
        for (const auto& entry : summaryData) {
            totalSeconds += entry.second.seconds;
            totalCalls += entry.second.calls;
        }

        struct SummaryEntry {
            std::wstring name;
            SummaryStats stats;
        };
        std::vector<SummaryEntry> sortedEntries;
        for (const auto& entry : summaryData) {
            sortedEntries.push_back({ entry.first, entry.second });
        }
        std::sort(sortedEntries.begin(), sortedEntries.end(), [](const SummaryEntry& a, const SummaryEntry& b) {
            return a.stats.seconds > b.stats.seconds;
        });

        std::wcout << std::endl;
        std::wcout << std::left 
                   << std::setw(10) << L"% time"
                   << std::setw(12) << L"seconds"
                   << std::setw(12) << L"usecs/call"
                   << std::setw(10) << L"calls"
                   << L"event" << std::endl;
        std::wcout << std::wstring(64, L'-') << std::endl;

        for (const auto& entry : sortedEntries) {
            double pct = (totalSeconds > 0.0) ? (entry.stats.seconds / totalSeconds * 100.0) : 0.0;
            double usecsPerCall = (entry.stats.calls > 0) ? (entry.stats.seconds / entry.stats.calls * 1000000.0) : 0.0;

            std::wcout << std::left << std::fixed << std::setprecision(2)
                       << std::setw(10) << pct
                       << std::setw(12) << entry.stats.seconds
                       << std::setw(12) << static_cast<DWORD>(usecsPerCall)
                       << std::setw(10) << entry.stats.calls
                       << entry.name << std::endl;
        }
        std::wcout << std::wstring(64, L'-') << std::endl;
        std::wcout << std::left
                   << std::setw(10) << 100.00
                   << std::setw(12) << totalSeconds
                   << std::setw(12) << L""
                   << std::setw(10) << totalCalls
                   << L"total" << std::endl;
    }
};

class ProcessTracker {
public:
    std::unordered_map<DWORD, HANDLE> processHandles;
    std::unordered_map<DWORD, HANDLE> threadHandles;
    size_t activeProcessCount = 0;

    ~ProcessTracker() {
        CleanupAll();
    }

    void CleanupAll() {
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
};

// ============================================================================
// 5. TRACE ENGINE & APPLICATION CONTROLLER
// ============================================================================

class TraceEngine {
public:
    static int Execute(TraceOptions& options) {
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
};

class StraceApplication {
public:
    int Run(int argc, wchar_t* argv[]) const {
        TraceOptions options;
        if (!options.Parse(argc, argv)) {
            return 1;
        }

        return TraceEngine::Execute(options);
    }
};

int wmain(int argc, wchar_t* argv[]) {
    StraceApplication app;
    return app.Run(argc, argv);
}
