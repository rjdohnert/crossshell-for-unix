#define WIN32_LEAN_AND_MEAN
#include "groupadd_app.hpp"

#include "groupadd_group.hpp"
#include "groupadd_input.hpp"

#include <fcntl.h>
#include <io.h>

#include <iostream>
#include <string>
#include <utility>

namespace {
constexpr size_t kMaxInputRecordBytes = 64 * 1024;
constexpr wchar_t kHexDigits[] = L"0123456789abcdef";

std::wstring SafeGroupName(const std::wstring& name) {
    std::wstring safe;
    for (wchar_t character : name) {
        if (character < 0x20 || (character >= 0x7f && character <= 0x9f)) {
            const unsigned int code = static_cast<unsigned int>(character);
            safe += L"\\u";
            safe.push_back(kHexDigits[(code >> 12) & 0x0f]);
            safe.push_back(kHexDigits[(code >> 8) & 0x0f]);
            safe.push_back(kHexDigits[(code >> 4) & 0x0f]);
            safe.push_back(kHexDigits[code & 0x0f]);
        } else {
            safe.push_back(character);
        }
    }
    return safe;
}

enum class InputLineStatus {
    line,
    end,
    tooLong,
    error
};

InputLineStatus ReadInputLine(std::string& line) {
    line.clear();
    bool receivedData = false;
    bool tooLong = false;
    for (;;) {
        const int next = std::cin.get();
        if (next == std::char_traits<char>::eof()) {
            if (std::cin.bad() || (!std::cin.eof() && std::cin.fail())) {
                return InputLineStatus::error;
            }
            if (!receivedData && !tooLong) return InputLineStatus::end;
            return tooLong ? InputLineStatus::tooLong : InputLineStatus::line;
        }
        if (next == '\n') {
            return tooLong ? InputLineStatus::tooLong : InputLineStatus::line;
        }

        receivedData = true;
        if (line.size() < kMaxInputRecordBytes) {
            line.push_back(static_cast<char>(next));
        } else {
            tooLong = true;
        }
    }
}
}

GroupAddApplication::GroupAddApplication(CommandOptions options)
    : m_options(std::move(options)) {}

int GroupAddApplication::Run() {
    if (m_options.systemGroup && !m_options.quiet) {
        std::wcerr << L"groupadd: --system has no effect on Windows local groups\n";
    }

    if (m_options.readFromStdin ||
        (m_options.groupName.empty() &&
         _isatty(_fileno(stdin)) == 0)) {
        return ProcessInput();
    }
    if (m_options.groupName.empty()) {
        std::wcerr << L"groupadd: missing group operand\n"
                   << L"Try 'groupadd --help' for more information.\n";
        return 2;
    }

    return ProcessSingleGroup({m_options.groupName, m_options.comment});
}

int GroupAddApplication::ProcessSingleGroup(const GroupRequest& request) {
    const std::wstring safeName = SafeGroupName(request.name);
    if (m_options.verbose && !m_options.quiet) {
        std::wcout << L"groupadd: creating local group '" << safeName
                   << L"'\n";
    }

    const GroupAddResult result = CreateLocalGroup(request.name,
                                                   request.comment);
    if (result.alreadyExists) {
        if (m_options.force) {
            if (m_options.verbose && !m_options.quiet) {
                std::wcout << L"groupadd: group '" << safeName
                           << L"' already exists\n";
            }
            return 0;
        }
        std::wcerr << L"groupadd: cannot create group '" << safeName
                   << L"': " << GroupAddErrorMessage(result) << L'\n';
        return 9;
    }
    if (!result.created) {
        std::wcerr << L"groupadd: cannot create group '" << safeName
                   << L"': " << GroupAddErrorMessage(result);
        if (result.parameterError != 0) {
            std::wcerr << L" (parameter " << result.parameterError << L')';
        }
        std::wcerr << L'\n';
        return result.status == ERROR_INVALID_NAME ||
                       result.status == ERROR_INVALID_PARAMETER
                   ? 2
                   : 4;
    }

    if (m_options.verbose && !m_options.quiet) {
        std::wcout << L"groupadd: group '" << safeName
                   << L"' created successfully\n";
    }
    return 0;
}

int GroupAddApplication::ProcessInput() {
    if (_setmode(_fileno(stdin), _O_BINARY) == -1) {
        std::wcerr << L"groupadd: cannot enable binary standard input\n";
        return 4;
    }

    std::string line;
    size_t lineNumber = 0;
    size_t processed = 0;
    int overallStatus = 0;
    if (m_options.verbose && !m_options.quiet) {
        std::wcout << L"groupadd: reading UTF-8 group records from standard input\n";
    }

    for (;;) {
        const InputLineStatus inputStatus = ReadInputLine(line);
        if (inputStatus == InputLineStatus::end) break;
        if (inputStatus == InputLineStatus::error) {
            std::wcerr << L"groupadd: failed while reading standard input\n";
            return 4;
        }
        ++lineNumber;
        if (inputStatus == InputLineStatus::tooLong) {
            std::wcerr << L"groupadd: input line " << lineNumber
                       << L" exceeds the 64 KiB limit\n";
            if (overallStatus == 0) overallStatus = 2;
            continue;
        }

        GroupRequest request;
        bool skip = false;
        std::wstring error;
        if (!ParseGroupInputLine(line, m_options.comment, request, skip, error)) {
            std::wcerr << L"groupadd: input line " << lineNumber
                       << L": " << error << L'\n';
            if (overallStatus == 0) overallStatus = 2;
            continue;
        }
        if (skip) continue;

        ++processed;
        const int status = ProcessSingleGroup(request);
        if (status != 0 && overallStatus == 0) overallStatus = status;
    }

    if (m_options.verbose && !m_options.quiet) {
        std::wcout << L"groupadd: processed " << processed
                   << L" input records\n";
    }
    return overallStatus;
}
