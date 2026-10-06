#include "trace_formatter.hpp"

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
