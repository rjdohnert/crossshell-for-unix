#pragma once

#include "strace.hpp"

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
