#define WIN32_LEAN_AND_MEAN
#include "gzip_report.hpp"

#include <windows.h>

#include <cstdio>
#include <iostream>
#include <limits>
#include <string>

namespace {
void AppendJsonString(std::wstring& output, const std::wstring& value) {
    static constexpr wchar_t hex[] = L"0123456789abcdef";
    output.push_back(L'"');
    for (wchar_t character : value) {
        switch (character) {
            case L'"': output += L"\\\""; break;
            case L'\\': output += L"\\\\"; break;
            case L'\b': output += L"\\b"; break;
            case L'\f': output += L"\\f"; break;
            case L'\n': output += L"\\n"; break;
            case L'\r': output += L"\\r"; break;
            case L'\t': output += L"\\t"; break;
            default:
                if (static_cast<unsigned int>(character) < 0x20) {
                    const unsigned int value = static_cast<unsigned int>(character);
                    output += L"\\u00";
                    output.push_back(hex[(value >> 4) & 0x0f]);
                    output.push_back(hex[value & 0x0f]);
                } else {
                    output.push_back(character);
                }
                break;
        }
    }
    output.push_back(L'"');
}

void AppendCsvString(std::wstring& output, const std::wstring& value) {
    output.push_back(L'"');
    for (wchar_t character : value) {
        if (character == L'"') output.push_back(L'"');
        output.push_back(character);
    }
    output.push_back(L'"');
}

bool ToUtf8(const std::wstring& value, std::string& output) {
    if (value.empty()) {
        output.clear();
        return true;
    }
    if (value.size() > static_cast<size_t>((std::numeric_limits<int>::max)())) {
        return false;
    }
    const int inputLength = static_cast<int>(value.size());
    const int required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        value.data(), inputLength, nullptr, 0, nullptr, nullptr);
    if (required <= 0) return false;
    output.resize(static_cast<size_t>(required));
    return WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
        inputLength, output.data(), required, nullptr, nullptr) == required;
}

std::wstring BuildReport(const std::vector<GzipOperation>& operations,
                         ReportFormat format) {
    std::wstring text;
    if (format == ReportFormat::none) format = ReportFormat::table;

    if (format == ReportFormat::json) {
        text = L"{\"operations\":[";
        for (size_t i = 0; i < operations.size(); ++i) {
            if (i != 0) text.push_back(L',');
            text += L"{\"source\":";
            AppendJsonString(text, operations[i].source.native());
            text += L",\"target\":";
            AppendJsonString(text, operations[i].target.native());
            text.push_back(L'}');
        }
        text += L"]}\n";
    } else if (format == ReportFormat::csv) {
        text = L"source,target\n";
        for (const auto& operation : operations) {
            AppendCsvString(text, operation.source.native());
            text.push_back(L',');
            AppendCsvString(text, operation.target.native());
            text.push_back(L'\n');
        }
    } else {
        text = L"SOURCE\tTARGET\n";
        for (const auto& operation : operations) {
            text += operation.source.native();
            text.push_back(L'\t');
            text += operation.target.native();
            text.push_back(L'\n');
        }
    }
    return text;
}
}

bool GzipReport::dispatch(const std::vector<GzipOperation>& operations,
                          ReportFormat format, const std::wstring& pipeCommand,
                          bool stderrOutput, std::wstring& error) {
    if (format == ReportFormat::none && pipeCommand.empty()) return true;

    std::string utf8;
    if (!ToUtf8(BuildReport(operations, format), utf8)) {
        error = L"cannot encode the operation report as UTF-8";
        return false;
    }

    if (!pipeCommand.empty()) {
        FILE* pipe = _wpopen(pipeCommand.c_str(), L"wb");
        if (pipe == nullptr) {
            error = L"cannot start the --pipe command";
            return false;
        }
        const size_t written = std::fwrite(utf8.data(), 1, utf8.size(), pipe);
        const bool flushed = std::fflush(pipe) == 0;
        const int result = _pclose(pipe);
        if (written != utf8.size() || !flushed || result != 0) {
            error = L"failed to deliver the operation report through --pipe";
            return false;
        }
        return true;
    }

    std::ostream& output = stderrOutput ? std::cerr : std::cout;
    output.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
    output.flush();
    if (!output) {
        error = L"failed to write the operation report";
        return false;
    }
    return true;
}
