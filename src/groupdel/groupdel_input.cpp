#define WIN32_LEAN_AND_MEAN
#include "groupdel_input.hpp"

#include <windows.h>

#include <limits>
#include <utility>
#include <vector>

namespace {
std::string TrimAscii(std::string_view text) {
    const size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    const size_t last = text.find_last_not_of(" \t\r\n");
    return std::string(text.substr(first, last - first + 1));
}

bool ValidateUtf8(std::string_view input, std::wstring& error) {
    if (input.find('\0') != std::string_view::npos) {
        error = L"input records cannot contain NUL characters";
        return false;
    }
    if (input.size() >
        static_cast<size_t>((std::numeric_limits<int>::max)())) {
        error = L"input record is too long";
        return false;
    }
    if (input.empty()) return true;

    const int inputLength = static_cast<int>(input.size());
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(),
                            inputLength, nullptr, 0) <= 0) {
        error = L"input record is not valid UTF-8";
        return false;
    }
    return true;
}

bool ToWideUtf8(std::string_view input, std::wstring& output,
                std::wstring& error) {
    output.clear();
    if (input.empty()) return true;

    const int inputLength = static_cast<int>(input.size());
    const int required = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, input.data(), inputLength, nullptr, 0);
    if (required <= 0) {
        error = L"input record is not valid UTF-8";
        return false;
    }
    output.resize(static_cast<size_t>(required));
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(),
                            inputLength, output.data(), required) != required) {
        output.clear();
        error = L"input record is not valid UTF-8";
        return false;
    }
    return true;
}

bool FindDelimiter(std::string_view text, char& delimiter) {
    bool quoted = false;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '"') {
            if (quoted && i + 1 < text.size() && text[i + 1] == '"') {
                ++i;
            } else {
                quoted = !quoted;
            }
        } else if (!quoted && (text[i] == ',' || text[i] == '\t')) {
            delimiter = text[i];
            return true;
        }
    }
    return false;
}

bool ParseField(std::string_view text, size_t& position, char delimiter,
                std::string& field, std::wstring& error) {
    while (position < text.size() && text[position] == ' ') ++position;
    field.clear();
    if (position < text.size() && text[position] == '"') {
        ++position;
        bool closed = false;
        while (position < text.size()) {
            const char character = text[position++];
            if (character != '"') {
                field.push_back(character);
                continue;
            }
            if (position < text.size() && text[position] == '"') {
                field.push_back('"');
                ++position;
            } else {
                closed = true;
                break;
            }
        }
        if (!closed) {
            error = L"unterminated quoted input field";
            return false;
        }
        while (position < text.size() && text[position] == ' ') ++position;
        if (position < text.size() &&
            (delimiter == '\0' || text[position] != delimiter)) {
            error = L"unexpected data after quoted input field";
            return false;
        }
        return true;
    }

    const size_t start = position;
    while (position < text.size() &&
           (delimiter == '\0' || text[position] != delimiter)) {
        if (text[position] == '"') {
            error = L"quote in an unquoted input field";
            return false;
        }
        ++position;
    }
    field = TrimAscii(text.substr(start, position - start));
    return true;
}

bool ParseFields(std::string_view text, char delimiter,
                 std::vector<std::string>& fields, std::wstring& error) {
    fields.clear();
    size_t position = 0;
    for (;;) {
        std::string field;
        if (!ParseField(text, position, delimiter, field, error)) return false;
        fields.push_back(std::move(field));
        if (position == text.size()) return true;
        if (delimiter == '\0' || text[position] != delimiter) {
            error = L"invalid input delimiter";
            return false;
        }
        ++position;
        if (fields.size() >= 2) {
            error = L"input records may contain only a group name and description";
            return false;
        }
    }
}
}

bool ParseGroupInputLine(std::string_view line, std::wstring& groupName,
                         bool& skip, std::wstring& error) {
    groupName.clear();
    skip = false;
    error.clear();

    if (line.size() >= 3 &&
        static_cast<unsigned char>(line[0]) == 0xef &&
        static_cast<unsigned char>(line[1]) == 0xbb &&
        static_cast<unsigned char>(line[2]) == 0xbf) {
        line.remove_prefix(3);
    }
    if (!ValidateUtf8(line, error)) return false;

    const std::string trimmed = TrimAscii(line);
    if (trimmed.empty() || trimmed[0] == '#') {
        skip = true;
        return true;
    }

    char delimiter = '\0';
    FindDelimiter(trimmed, delimiter);
    std::vector<std::string> fields;
    if (!ParseFields(trimmed, delimiter, fields, error)) return false;
    if (fields.empty() || fields.size() > 2 || fields[0].empty()) {
        error = L"input record requires a nonempty group name";
        return false;
    }
    return ToWideUtf8(fields[0], groupName, error);
}
