#include "reporter.hpp"

// ============================================================================
// WidePipeStreambuf implementation
// ============================================================================

WidePipeStreambuf::WidePipeStreambuf(FILE* pipe) : m_pipe(pipe) {
    setp(m_buffer, m_buffer + (sizeof(m_buffer) / sizeof(m_buffer[0])));
}

WidePipeStreambuf::~WidePipeStreambuf() {
    sync();
}

std::wstreambuf::int_type WidePipeStreambuf::overflow(int_type ch) {
    if (ch != traits_type::eof()) {
        *pptr() = static_cast<wchar_t>(ch);
        pbump(1);
    }
    return sync() == 0 ? traits_type::not_eof(ch) : traits_type::eof();
}

int WidePipeStreambuf::sync() {
    std::ptrdiff_t count = pptr() - pbase();
    if (count > 0 && std::fwrite(pbase(), sizeof(wchar_t), static_cast<size_t>(count), m_pipe) != static_cast<size_t>(count)) {
        return -1;
    }
    setp(m_buffer, m_buffer + (sizeof(m_buffer) / sizeof(m_buffer[0])));
    return std::fflush(m_pipe) == 0 ? 0 : -1;
}

// ============================================================================
// OutputRedirectionGuard implementation
// ============================================================================

OutputRedirectionGuard::OutputRedirectionGuard(const std::wstring& outputPath, const std::wstring& pipeCommand) {
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

OutputRedirectionGuard::~OutputRedirectionGuard() {
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

// ============================================================================
// TraceFormatter implementation
// ============================================================================

std::wstring TraceFormatter::JsonEscape(const std::wstring& value) {
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

std::wstring TraceFormatter::CsvEscape(const std::wstring& value) {
    std::wstring out = L"\"";
    for (wchar_t ch : value) {
        if (ch == L'"') out += L"\"\"";
        else out += ch;
    }
    return out + L"\"";
}

std::wstring TraceFormatter::Trim(const std::wstring& input) {
    const std::wstring whitespace = L" \t\r\n";
    size_t start = input.find_first_not_of(whitespace);
    if (start == std::wstring::npos) {
        return L"";
    }
    size_t end = input.find_last_not_of(whitespace);
    return input.substr(start, end - start + 1);
}

std::wstring TraceFormatter::ToWideString(const std::string& input, UINT codePage) {
    if (input.empty()) return L"";
    int size = MultiByteToWideChar(codePage, 0, input.c_str(), -1, nullptr, 0);
    if (size <= 0) {
        return std::wstring(input.begin(), input.end());
    }
    std::wstring output(size - 1, L'\0');
    MultiByteToWideChar(codePage, 0, input.c_str(), -1, output.data(), size);
    return output;
}

std::wstring TraceFormatter::QuoteCommandArg(const std::wstring& arg) {
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

std::wstring TraceFormatter::FormatTimestamp() {
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

std::wstring TraceFormatter::GetErrorMessage(DWORD dwErrorCode) {
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

std::wstring TraceFormatter::GetFileNameFromHandle(HANDLE hFile) {
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

// ============================================================================
// TraceReporter implementation
// ============================================================================

std::wstring TraceReporter::FormatIpPrefix(const TraceOptions& options, HANDLE hThread, DWORD64 defaultIp) {
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

void TraceReporter::EmitTraceLine(TraceOptions& options, const std::wstring& line, HANDLE hThread, DWORD64 defaultIp, bool isEvent) {
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

bool TraceReporter::ShouldPrintEvent(const TraceOptions& options, const std::wstring& category) {
    if (!options.filterEvents) return true;
    for (const auto& filter : options.eventFilters) {
        if (filter == category) return true;
    }
    return false;
}

void TraceReporter::PrintSummary(const std::unordered_map<std::wstring, SummaryStats>& summaryData) {
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
