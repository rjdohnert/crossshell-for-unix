#include "command_line_formatter.hpp"

std::wstring CommandLineFormatter::QuoteArgument(const std::wstring& arg) {
        if (arg.empty()) return L"\"\"";
        if (arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) return arg;

        std::wstring quoted = L"\"";
        int backslashes = 0;
        for (wchar_t c : arg) {
            if (c == L'\\') {
                ++backslashes;
            } else if (c == L'\"') {
                quoted.append(backslashes * 2 + 1, L'\\');
                quoted.push_back(L'\"');
                backslashes = 0;
            } else {
                quoted.append(backslashes, L'\\');
                quoted.push_back(c);
                backslashes = 0;
            }
        }
        quoted.append(backslashes * 2, L'\\');
        quoted.push_back(L'\"');
        return quoted;
    }

std::wstring CommandLineFormatter::BuildCommandLine(const std::vector<std::wstring>& args) {
        std::wstring out;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) out.push_back(L' ');
            out += QuoteArgument(args[i]);
        }
        return out;
    }

std::wstring CommandLineFormatter::EscapeSingleQuoted(const std::wstring& s) {
        std::wstring out;
        out.reserve(s.size());
        for (wchar_t c : s) {
            if (c == L'\'') out += L"''";
            else out.push_back(c);
        }
        return out;
    }
