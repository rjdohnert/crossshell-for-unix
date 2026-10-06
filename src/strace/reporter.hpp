#pragma once

#include "strace.hpp"
#include "options.hpp"

class WidePipeStreambuf : public std::wstreambuf {
public:
    explicit WidePipeStreambuf(FILE* pipe);
    ~WidePipeStreambuf() override;

protected:
    int_type overflow(int_type ch) override;
    int sync() override;

private:
    FILE* m_pipe = nullptr;
    wchar_t m_buffer[2048] = {};
};

class OutputRedirectionGuard {
public:
    OutputRedirectionGuard(const std::wstring& outputPath, const std::wstring& pipeCommand);
    ~OutputRedirectionGuard();

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
    static std::wstring JsonEscape(const std::wstring& value);
    static std::wstring CsvEscape(const std::wstring& value);
    static std::wstring Trim(const std::wstring& input);

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

    static std::wstring ToWideString(const std::string& input, UINT codePage = CP_ACP);
    static std::wstring QuoteCommandArg(const std::wstring& arg);
    static std::wstring FormatTimestamp();
    static std::wstring GetErrorMessage(DWORD dwErrorCode);
    static std::wstring GetFileNameFromHandle(HANDLE hFile);
};

class TraceReporter {
public:
    static std::wstring FormatIpPrefix(const TraceOptions& options, HANDLE hThread, DWORD64 defaultIp);
    static void EmitTraceLine(TraceOptions& options, const std::wstring& line, HANDLE hThread = NULL, DWORD64 defaultIp = 0, bool isEvent = true);
    static bool ShouldPrintEvent(const TraceOptions& options, const std::wstring& category);
    static void PrintSummary(const std::unordered_map<std::wstring, SummaryStats>& summaryData);
};
