#define WIN32_LEAN_AND_MEAN
#include "groupdel_app.hpp"

#include "groupdel_cli.hpp"
#include "groupdel_group.hpp"
#include "groupdel_input.hpp"

#include <windows.h>
#include <lm.h>

#include <fcntl.h>
#include <io.h>

#include <iostream>
#include <string>
#include <utility>

namespace {
constexpr size_t kMaxInputRecordBytes = 64 * 1024;
constexpr wchar_t kHexDigits[] = L"0123456789abcdef";

bool IsTerminalInput() {
    return _isatty(_fileno(stdin)) != 0;
}

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

int StatusForError(unsigned long status) {
    if (status == ERROR_INVALID_NAME ||
        status == ERROR_INVALID_PARAMETER) {
        return 2;
    }
    if (status == NERR_GroupNotFound || status == ERROR_NO_SUCH_ALIAS) {
        return 6;
    }
    if (status == NERR_SpeGroupOp) return 8;
    if (status == ERROR_ACCESS_DENIED) return 10;
    return 4;
}
}

GroupDelApplication::GroupDelApplication(CommandOptions options)
    : m_options(std::move(options)) {}

int GroupDelApplication::Run() {
    if (m_options.showHelp) {
        CommandLineParser::PrintHelp();
        return 0;
    }
    if (m_options.showVersion) {
        CommandLineParser::PrintVersion();
        return 0;
    }
    if (m_options.readFromStdin ||
        (m_options.groupNames.empty() && !IsTerminalInput())) {
        return ProcessInput();
    }
    if (m_options.groupNames.empty()) {
        std::wcerr << L"groupdel: missing group operand\n"
                   << L"Try 'groupdel --help' for more information.\n";
        return 2;
    }

    int overallStatus = 0;
    for (const std::wstring& groupName : m_options.groupNames) {
        const int status = ProcessSingleGroup(groupName);
        if (status != 0 && overallStatus == 0) overallStatus = status;
    }
    return overallStatus;
}

int GroupDelApplication::ProcessSingleGroup(const std::wstring& name) {
    const std::wstring safeName = SafeGroupName(name);
    if (m_options.verbose && !m_options.quiet) {
        std::wcout << L"groupdel: deleting local group '" << safeName << L"'\n";
    }

    const GroupDelResult result = DeleteLocalGroup(name);
    if (result.notFound) {
        if (m_options.force) {
            if (m_options.verbose && !m_options.quiet) {
                std::wcout << L"groupdel: group '" << safeName
                           << L"' is already absent\n";
            }
            return 0;
        }
        std::wcerr << L"groupdel: cannot remove group '" << safeName
                   << L"': " << GroupDelErrorMessage(result.status) << L'\n';
        return 6;
    }
    if (!result.deleted) {
        std::wcerr << L"groupdel: cannot remove group '" << safeName
                   << L"': " << GroupDelErrorMessage(result.status) << L'\n';
        return StatusForError(result.status);
    }
    if (m_options.verbose && !m_options.quiet) {
        std::wcout << L"groupdel: group '" << safeName
                   << L"' deleted successfully\n";
    }
    return 0;
}

int GroupDelApplication::ProcessInput() {
    if (_setmode(_fileno(stdin), _O_BINARY) == -1) {
        std::wcerr << L"groupdel: cannot enable binary standard input\n";
        return 4;
    }

    std::string line;
    size_t lineNumber = 0;
    size_t processed = 0;
    int overallStatus = 0;
    if (m_options.verbose && !m_options.quiet) {
        std::wcout << L"groupdel: reading UTF-8 group records from standard input\n";
    }

    for (;;) {
        const InputLineStatus inputStatus = ReadInputLine(line);
        if (inputStatus == InputLineStatus::end) break;
        if (inputStatus == InputLineStatus::error) {
            std::wcerr << L"groupdel: failed while reading standard input\n";
            if (overallStatus == 0) overallStatus = 4;
            break;
        }
        ++lineNumber;
        if (inputStatus == InputLineStatus::tooLong) {
            std::wcerr << L"groupdel: input line " << lineNumber
                       << L" exceeds the 64 KiB limit\n";
            if (overallStatus == 0) overallStatus = 2;
            continue;
        }

        std::wstring groupName;
        std::wstring error;
        bool skip = false;
        if (!ParseGroupInputLine(line, groupName, skip, error)) {
            std::wcerr << L"groupdel: input line " << lineNumber
                       << L": " << error << L'\n';
            if (overallStatus == 0) overallStatus = 2;
            continue;
        }
        if (skip) continue;

        ++processed;
        const int status = ProcessSingleGroup(groupName);
        if (status != 0 && overallStatus == 0) overallStatus = status;
    }

    if (m_options.verbose && !m_options.quiet) {
        std::wcout << L"groupdel: processed " << processed
                   << L" input records\n";
    }
    return overallStatus;
}
