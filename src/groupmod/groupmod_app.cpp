#define WIN32_LEAN_AND_MEAN
#include "groupmod_app.hpp"

#include "groupmod_cli.hpp"
#include "groupmod_group.hpp"
#include "groupmod_input.hpp"

#include <windows.h>
#include <lm.h>

#include <fcntl.h>
#include <io.h>

#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {
constexpr size_t kMaxInputRecordBytes = 64 * 1024;
constexpr wchar_t kHexDigits[] = L"0123456789abcdef";

bool IsTerminalInput() {
    return _isatty(_fileno(stdin)) != 0;
}

std::wstring SafeText(const std::wstring& value) {
    std::wstring safe;
    for (wchar_t character : value) {
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
    if (status == NERR_GroupExists) return 9;
    if (status == ERROR_ACCESS_DENIED) return 10;
    return 4;
}
}

GroupModApplication::GroupModApplication(CommandOptions options)
    : m_options(std::move(options)) {}

int GroupModApplication::Run() {
    if (m_options.showHelp) {
        CommandLineParser::PrintHelp();
        return 0;
    }
    if (m_options.showVersion) {
        CommandLineParser::PrintVersion();
        return 0;
    }
    if (m_options.readFromStdin ||
        (m_options.groupName.empty() && !IsTerminalInput())) {
        return ProcessInput();
    }
    if (m_options.groupName.empty()) {
        std::wcerr << L"groupmod: missing group operand\n"
                   << L"Try 'groupmod --help' for more information.\n";
        return 2;
    }
    if (!m_options.newName && !m_options.comment) {
        std::wcerr << L"groupmod: no changes specified; use -n and/or -c\n"
                   << L"Try 'groupmod --help' for more information.\n";
        return 2;
    }

    return ProcessModification(
        {m_options.groupName, m_options.newName, m_options.comment});
}

int GroupModApplication::ProcessModification(
    const GroupModification& modification) {
    if (!modification.newName && !modification.comment) {
        std::wcerr << L"groupmod: no changes specified for group '"
                   << SafeText(modification.groupName) << L"'\n";
        return 2;
    }

    if (m_options.verbose && !m_options.quiet) {
        std::wcout << L"groupmod: modifying local group '"
                   << SafeText(modification.groupName) << L"'";
        if (modification.newName) {
            std::wcout << L" [new name: '" << SafeText(*modification.newName)
                       << L"']";
        }
        if (modification.comment) {
            std::wcout << L" [comment: \"" << SafeText(*modification.comment)
                       << L"\"]";
        }
        std::wcout << L'\n';
    }

    const GroupModResult result = ModifyLocalGroup(
        modification.groupName, modification.newName, modification.comment);
    if (result.notFound && m_options.force) {
        if (m_options.verbose && !m_options.quiet) {
            std::wcout << L"groupmod: source group '"
                       << SafeText(modification.groupName)
                       << L"' is absent; nothing to change\n";
        }
        return 0;
    }
    if (!result.success) {
        std::wcerr << L"groupmod: cannot modify group '"
                   << SafeText(modification.groupName) << L"': "
                   << GroupModErrorMessage(result.status) << L'\n';
        return StatusForError(result.status);
    }
    if (m_options.verbose && !m_options.quiet) {
        std::wcout << L"groupmod: "
                   << (result.changed ? L"group modified successfully"
                                      : L"requested state already matches")
                   << L" ('" << SafeText(modification.groupName) << L"')\n";
    }
    return 0;
}

int GroupModApplication::ProcessInput() {
    if (_setmode(_fileno(stdin), _O_BINARY) == -1) {
        std::wcerr << L"groupmod: cannot enable binary standard input\n";
        return 4;
    }

    std::string line;
    size_t lineNumber = 0;
    size_t processed = 0;
    int overallStatus = 0;
    if (m_options.verbose && !m_options.quiet) {
        std::wcout << L"groupmod: reading UTF-8 modification records from standard input\n";
    }

    for (;;) {
        const InputLineStatus inputStatus = ReadInputLine(line);
        if (inputStatus == InputLineStatus::end) break;
        if (inputStatus == InputLineStatus::error) {
            std::wcerr << L"groupmod: failed while reading standard input\n";
            if (overallStatus == 0) overallStatus = 4;
            break;
        }
        ++lineNumber;
        if (inputStatus == InputLineStatus::tooLong) {
            std::wcerr << L"groupmod: input line " << lineNumber
                       << L" exceeds the 64 KiB limit\n";
            if (overallStatus == 0) overallStatus = 2;
            continue;
        }

        std::vector<std::wstring> fields;
        std::wstring error;
        bool skip = false;
        if (!ParseModificationLine(line, fields, skip, error)) {
            std::wcerr << L"groupmod: input line " << lineNumber
                       << L": " << error << L'\n';
            if (overallStatus == 0) overallStatus = 2;
            continue;
        }
        if (skip) continue;

        GroupModification modification{
            fields[0], m_options.newName, m_options.comment
        };
        if (fields.size() >= 2) {
            if (fields[1].empty()) {
                modification.newName.reset();
            } else {
                modification.newName = fields[1];
            }
        }
        if (fields.size() == 3) {
            modification.comment = fields[2];
        }

        ++processed;
        const int status = ProcessModification(modification);
        if (status != 0 && overallStatus == 0) overallStatus = status;
    }

    if (m_options.verbose && !m_options.quiet) {
        std::wcout << L"groupmod: processed " << processed
                   << L" input records\n";
    }
    return overallStatus;
}
