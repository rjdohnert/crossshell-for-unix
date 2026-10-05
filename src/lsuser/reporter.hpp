#pragma once

#include "lsuser.hpp"
#include "options.hpp"
#include <string>
#include <vector>

class LsuserReporter {
public:
    static std::wstring GetDisplayValue(const AccountRecord& account, const std::string& attribute);
    static int GetColumnWidth(const std::string& attribute);
    static std::string ToUtf8(const std::wstring& value);
    static std::string CsvEscape(const std::string& value);
    static std::string JsonEscape(const std::string& value);
    static void Emit(const LsuserOptions& opts, const std::vector<AccountRecord>& accounts);
};
