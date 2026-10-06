#include "argument_formatter.hpp"

std::wstring ArgumentFormatter::QuoteArgument(const std::wstring& arg) {
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

std::wstring ArgumentFormatter::JoinCommandArgs(const std::vector<std::wstring>& args) {
        std::wstring result;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) result += L" ";
            result += QuoteArgument(args[i]);
        }
        return result;
    }

bool ArgumentFormatter::ParseNumber(const std::wstring& text, double& outValue) {
        wchar_t* end = nullptr;
        double parsed = std::wcstod(text.c_str(), &end);
        if (end == text.c_str() || *end != L'\0' || parsed <= 0.0) {
            return false;
        }
        outValue = parsed;
        return true;
    }
