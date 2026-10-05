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
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <algorithm>
#include <ctime>
#include <sstream>
#include <cstdio>
#include <streambuf>

#pragma comment(lib, "advapi32.lib")

namespace {

struct Options {
    std::wstring logName = L"System";
    DWORD maxEntries = 50;
    bool showAll = false;
    bool help = false;
    bool version = false;
    enum class OutputFormat { Table, Json, Csv } output = OutputFormat::Table;
    std::wstring pipeCommand;
};

struct EventEntry {
    std::string timestamp;
    std::string severity;
    DWORD eventId = 0;
    std::string source;
    std::string summary;
};

class ErrptOutputGuard {
    std::streambuf* old_;
    FILE* pipe_;
    std::ostringstream capture_;
public:
    explicit ErrptOutputGuard(const std::wstring& command) : old_(nullptr), pipe_(nullptr) {
        if (!command.empty()) old_ = std::cout.rdbuf(capture_.rdbuf());
        if (!command.empty()) pipe_ = _wpopen(command.c_str(), L"w");
    }
    ~ErrptOutputGuard() {
        if (!old_) return;
        std::cout.rdbuf(old_);
        std::string text = capture_.str();
        if (pipe_) { std::fwrite(text.data(), 1, text.size(), pipe_); _pclose(pipe_); }
        else std::cout << text;
    }
};

std::string to_utf8(const std::wstring& w) {
    if (w.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), &out[0], size, nullptr, nullptr);
    return out;
}

std::wstring to_lower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(towlower(ch));
    });
    return s;
}

std::wstring resolve_log(const std::wstring& in) {
    std::wstring v = to_lower(in);
    if (v == L"system") return L"System";
    if (v == L"application" || v == L"app") return L"Application";
    if (v == L"security" || v == L"sec") return L"Security";
    return L"";
}

std::string severity_word(WORD t) {
    switch (t) {
    case EVENTLOG_ERROR_TYPE: return "ERROR";
    case EVENTLOG_WARNING_TYPE: return "WARN";
    case EVENTLOG_INFORMATION_TYPE: return "INFO";
    case EVENTLOG_AUDIT_SUCCESS: return "AUDIT_OK";
    case EVENTLOG_AUDIT_FAILURE: return "AUDIT_FAIL";
    default: return "OTHER";
    }
}

std::string format_time(DWORD ts) {
    std::time_t when = static_cast<std::time_t>(ts);
    std::tm localTm{};
    localtime_s(&localTm, &when);
    char buf[64]{};
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &localTm);
    return std::string(buf);
}

std::string first_insertion_string(const EVENTLOGRECORD* rec) {
    if (rec->NumStrings == 0 || rec->StringOffset == 0) {
        return "";
    }
    const wchar_t* p = reinterpret_cast<const wchar_t*>(reinterpret_cast<const BYTE*>(rec) + rec->StringOffset);
    if (*p == L'\0') return "";
    return to_utf8(p);
}

std::string json_escape(const std::string& value) {
    std::string out;
    for (unsigned char ch : value) {
        switch (ch) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (ch < 0x20) out += ' ';
            else out += static_cast<char>(ch);
        }
    }
    return out;
}

std::string csv_escape(const std::string& value) {
    std::string out = "\"";
    for (char ch : value) {
        if (ch == '"') out += "\"\"";
        else out += ch;
    }
    return out + "\"";
}

void print_entries(const std::vector<EventEntry>& entries, Options::OutputFormat format) {
    if (format == Options::OutputFormat::Json) {
        std::cout << "[\n";
        for (size_t i = 0; i < entries.size(); ++i) {
            const auto& entry = entries[i];
            std::cout << "  {\"timestamp\":\"" << json_escape(entry.timestamp)
                      << "\",\"severity\":\"" << json_escape(entry.severity)
                      << "\",\"event_id\":" << entry.eventId
                      << ",\"source\":\"" << json_escape(entry.source)
                      << "\",\"summary\":\"" << json_escape(entry.summary) << "\"}"
                      << (i + 1 < entries.size() ? "," : "") << "\n";
        }
        std::cout << "]\n";
        return;
    }

    if (format == Options::OutputFormat::Csv) {
        std::cout << "timestamp,severity,event_id,source,summary\n";
        for (const auto& entry : entries) {
            std::cout << csv_escape(entry.timestamp) << ',' << csv_escape(entry.severity) << ','
                      << entry.eventId << ',' << csv_escape(entry.source) << ','
                      << csv_escape(entry.summary) << '\n';
        }
        return;
    }

    std::cout << std::left
              << std::setw(20) << "Timestamp"
              << std::setw(11) << "Severity"
              << std::setw(8) << "EventID"
              << std::setw(24) << "Source"
              << "Summary\n";
    for (const auto& entry : entries) {
        std::cout << std::setw(20) << entry.timestamp
                  << std::setw(11) << entry.severity
                  << std::setw(8) << entry.eventId
                  << std::setw(24) << entry.source.substr(0, 23)
                  << entry.summary << "\n";
    }
}

void print_help() {
    std::cout << R"(errpt(1)                 CrossShell for UNIX Reference Manual                    errpt(1)

NAME
    errpt - display Windows Event Log entries

SYNOPSIS
    errpt [OPTIONS]

DESCRIPTION
    Reads system, application, or security event logs and presents recent
    records as a table or structured data for scripts and pipelines.

OPTIONS
    -l, --log NAME        Select system, application, or security log.
    -n, --lines COUNT     Limit the number of entries (default: 50).
    -a, --all             Print all available entries.
    --output MODE         Select table, json, or csv output.
    --table               Use table output.
    --json                Emit a JSON array.
    --csv                 Emit CSV.
    --pipe COMMAND        Send formatted output through COMMAND.
    -h, --help            Display this comprehensive reference manual.
    -v, --version         Display version information and exit.

EXAMPLES
    errpt --json | jq '.[].summary'
    errpt --csv > system-events.csv
    errpt -l application --output table | findstr /i error

EXIT STATUS
    0          Help, version, or successful event-log query.
    1          Invalid options, unsupported log, or event-log failure.

CrossShell for UNIX                                                        errpt(1)
)";
    return;

    std::cout
        << "Usage: errpt [options]\n\n"
        << "Display Windows Event Log entries.\n\n"
        << "Options:\n"
        << "  -l, --log NAME      Event log: system, application, security (default: system)\n"
        << "  -n, --lines COUNT   Maximum number of entries to print (default: 50)\n"
        << "  -a, --all           Print all available entries\n"
        << "      --output MODE  Output mode: table, json, or csv (default: table)\n"
        << "      --table         Shorthand for --output table\n"
        << "      --json          Emit a JSON array to stdout for jq and other pipe tools\n"
        << "      --csv           Emit CSV to stdout for spreadsheets and scripts\n"
        << "      --pipe COMMAND  Send formatted output through COMMAND\n"
        << "  -h, --help          Show this help\n"
        << "  -v, --version       Show version\n\n"
        << "Examples:\n"
        << "  errpt --json | jq '.[].summary'\n"
        << "  errpt --csv > system-events.csv\n"
        << "  errpt -l application --output table | findstr /i error\n";
}

void print_version() {
    std::cout << "errpt v1.0.0\n";
}

bool parse_args(int argc, wchar_t* argv[], Options& opt) {
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i] ? argv[i] : L"";
        if (arg == L"-h" || arg == L"--help") {
            opt.help = true;
            return true;
        }
        if (arg == L"-v" || arg == L"--version") {
            opt.version = true;
            return true;
        }
        if (arg == L"-a" || arg == L"--all") {
            opt.showAll = true;
            continue;
        }
        if ((arg == L"-l" || arg == L"--log") && i + 1 < argc) {
            std::wstring log = resolve_log(argv[++i]);
            if (log.empty()) {
                std::wcerr << L"errpt: unsupported log '" << argv[i] << L"'\n";
                return false;
            }
            opt.logName = log;
            continue;
        }
        if ((arg == L"-n" || arg == L"--lines") && i + 1 < argc) {
            wchar_t* end = nullptr;
            unsigned long v = wcstoul(argv[++i], &end, 10);
            if (end == argv[i] || *end != L'\0' || v == 0) {
                std::wcerr << L"errpt: invalid count '" << argv[i] << L"'\n";
                return false;
            }
            opt.maxEntries = static_cast<DWORD>(v);
            continue;
        }
        if (arg == L"--json" || arg == L"--output=json") {
            opt.output = Options::OutputFormat::Json;
            continue;
        }
        if (arg == L"--csv" || arg == L"--output=csv") {
            opt.output = Options::OutputFormat::Csv;
            continue;
        }
        if (arg == L"--pipe" && i + 1 < argc) { opt.pipeCommand = argv[++i]; continue; }
        if (arg == L"--table" || arg == L"--output=table") {
            opt.output = Options::OutputFormat::Table;
            continue;
        }
        if (arg == L"--output" && i + 1 < argc) {
            std::wstring mode = to_lower(argv[++i] ? argv[i] : L"");
            if (mode == L"json") opt.output = Options::OutputFormat::Json;
            else if (mode == L"csv") opt.output = Options::OutputFormat::Csv;
            else if (mode == L"table") opt.output = Options::OutputFormat::Table;
            else {
                std::wcerr << L"errpt: unsupported output mode '" << mode << L"'\n";
                return false;
            }
            continue;
        }

        std::wcerr << L"errpt: unknown option '" << arg << L"'\n";
        return false;
    }
    return true;
}

} // namespace

static int errpt_main(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);

    Options opt;
    if (!parse_args(argc, argv, opt)) {
        std::cerr << "Try 'errpt --help' for usage.\n";
        return 2;
    }
    if (opt.help) {
        print_help();
        return 0;
    }
    if (opt.version) {
        print_version();
        return 0;
    }

    ErrptOutputGuard outputGuard(opt.pipeCommand);

    HANDLE hLog = OpenEventLogW(nullptr, opt.logName.c_str());
    if (!hLog) {
        std::wcerr << L"errpt: cannot open log '" << opt.logName << L"' (error " << GetLastError() << L")\n";
        return 1;
    }

    constexpr DWORD kBufferSize = 1 << 16;
    std::vector<BYTE> buffer(kBufferSize);
    DWORD bytesRead = 0;
    DWORD minBytes = 0;
    DWORD printed = 0;

    std::vector<EventEntry> entries;

    DWORD flags = EVENTLOG_BACKWARDS_READ | EVENTLOG_SEQUENTIAL_READ;
    while (ReadEventLogW(hLog, flags, 0, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, &minBytes)) {
        DWORD offset = 0;
        while (offset < bytesRead) {
            const EVENTLOGRECORD* rec = reinterpret_cast<const EVENTLOGRECORD*>(buffer.data() + offset);
            const wchar_t* sourceW = reinterpret_cast<const wchar_t*>(reinterpret_cast<const BYTE*>(rec) + sizeof(EVENTLOGRECORD));
            std::string source = to_utf8(sourceW ? sourceW : L"");
            std::string summary = first_insertion_string(rec);
            if (summary.empty()) {
                summary = "(no insertion text)";
            }

            entries.push_back({ format_time(rec->TimeGenerated), severity_word(rec->EventType),
                                rec->EventID & 0xFFFF, std::move(source), std::move(summary) });

            ++printed;
            if (!opt.showAll && printed >= opt.maxEntries) {
                CloseEventLog(hLog);
                print_entries(entries, opt.output);
                return 0;
            }

            offset += rec->Length;
        }
    }

    DWORD err = GetLastError();
    CloseEventLog(hLog);

    if (err != ERROR_HANDLE_EOF && err != ERROR_SUCCESS) {
        std::cerr << "errpt: failed while reading log (error " << err << ")\n";
        return 1;
    }

    print_entries(entries, opt.output);
    return 0;
}

class ErrptApplication { public: int run(int argc, wchar_t* argv[]) const { return errpt_main(argc, argv); } };
int wmain(int argc, wchar_t* argv[]) { return ErrptApplication().run(argc, argv); }

