#include "format_helper.hpp"

std::wstring FormatHelper::FormatBytesIec(ULONGLONG bytes) {
        static const wchar_t* units[] = { L"B", L"KiB", L"MiB", L"GiB", L"TiB" };
        double value = static_cast<double>(bytes);
        size_t unit_index = 0;
        while (value >= 1024.0 && unit_index + 1 < _countof(units)) {
            value /= 1024.0;
            ++unit_index;
        }
        wchar_t buffer[64];
        if (unit_index == 0) swprintf_s(buffer, L"%llu %s", bytes, units[unit_index]);
        else swprintf_s(buffer, L"%.2f %s", value, units[unit_index]);
        return buffer;
    }

std::wstring FormatHelper::FormatDurationSeconds(ULONGLONG hundred_ns) {
        double seconds = static_cast<double>(hundred_ns) / 10000000.0;
        wchar_t buffer[64];
        swprintf_s(buffer, L"%.3fs", seconds);
        return buffer;
    }

bool FormatHelper::ParseUnsigned(const std::wstring& text, unsigned long long& value) {
        try {
            size_t consumed = 0;
            unsigned long long parsed = std::stoull(text, &consumed, 10);
            if (consumed != text.size()) return false;
            value = parsed;
            return true;
        } catch (...) {
            return false;
        }
    }

std::wstring FormatHelper::QuoteArgument(const std::wstring& arg) {
        if (arg.empty()) return L"\"\"";
        bool needs_quotes = false;
        for (wchar_t ch : arg) {
            if (iswspace(ch) || ch == L'"') { needs_quotes = true; break; }
        }
        if (!needs_quotes) return arg;
        std::wstring quoted;
        quoted.push_back(L'"');
        size_t backslashes = 0;
        for (wchar_t ch : arg) {
            if (ch == L'\\') { ++backslashes; continue; }
            if (ch == L'"') { quoted.append(backslashes * 2 + 1, L'\\'); quoted.push_back(L'"'); backslashes = 0; continue; }
            if (backslashes > 0) { quoted.append(backslashes, L'\\'); backslashes = 0; }
            quoted.push_back(ch);
        }
        if (backslashes > 0) quoted.append(backslashes * 2, L'\\');
        quoted.push_back(L'"');
        return quoted;
    }

std::wstring FormatHelper::BuildCommandLine(const std::vector<std::wstring>& args) {
        std::wstring command_line;
        for (size_t i = 0; i < args.size(); ++i) {
            if (!command_line.empty()) command_line.push_back(L' ');
            command_line += QuoteArgument(args[i]);
        }
        return command_line;
    }
