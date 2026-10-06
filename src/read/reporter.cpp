#include "reporter.hpp"

std::wstring ReadReporter::jsonQuote(const std::wstring& value) {
    std::wstring out = L"\"";
    for (wchar_t ch : value) {
        if (ch == L'"' || ch == L'\\') out += L'\\';
        if (ch == L'\n') out += L'n';
        else if (ch == L'\r') out += L'r';
        else if (ch == L'\t') out += L't';
        else out += ch;
    }
    return out + L"\"";
}

std::wstring ReadReporter::csvQuote(const std::wstring& value) {
    std::wstring out = L"\"";
    for (wchar_t ch : value) {
        out += (ch == L'"') ? L"\"\"" : std::wstring(1, ch);
    }
    return out + L"\"";
}

void ReadReporter::writeHandle(HANDLE hHandle, const std::wstring& text) {
    if (text.empty()) return;

    DWORD mode;
    if (GetConsoleMode(hHandle, &mode)) {
        DWORD written = 0;
        WriteConsoleW(hHandle, text.c_str(), static_cast<DWORD>(text.length()), &written, NULL);
    } else {
        int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.length()), NULL, 0, NULL, NULL);
        if (size > 0) {
            std::string utf8Str(size, 0);
            WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.length()), &utf8Str[0], size, NULL, NULL);
            DWORD written = 0;
            WriteFile(hHandle, utf8Str.data(), static_cast<DWORD>(utf8Str.length()), &written, NULL);
        }
    }
}

int ReadReporter::output(const std::wstring& result, const ReadOptions& opts, int exitCode) {
    std::wstring formatted;
    if (opts.outputFormat == ReadOutputFormat::Json) formatted = L"{\"value\":" + jsonQuote(result) + L"}\n";
    else if (opts.outputFormat == ReadOutputFormat::Csv) formatted = L"\"value\"\n" + csvQuote(result) + L"\n";
    else if (opts.outputFormat == ReadOutputFormat::Table) formatted = L"VALUE\n-----\n" + result + L"\n";
    else formatted = result;

    if (!opts.pipeCommand.empty()) {
        FILE* pipe = _wpopen(opts.pipeCommand.c_str(), L"w");
        if (!pipe) return (exitCode == 0) ? 1 : exitCode;
        int size = WideCharToMultiByte(CP_UTF8, 0, formatted.c_str(), static_cast<int>(formatted.length()), NULL, 0, NULL, NULL);
        if (size > 0) {
            std::string utf8(static_cast<size_t>(size), '\0');
            WideCharToMultiByte(CP_UTF8, 0, formatted.c_str(), static_cast<int>(formatted.length()), &utf8[0], size, NULL, NULL);
            std::fwrite(utf8.data(), 1, utf8.size(), pipe);
        }
        std::fflush(pipe);
        _pclose(pipe);
    } else {
        writeHandle(GetStdHandle(STD_OUTPUT_HANDLE), formatted);
    }

    return exitCode;
}
