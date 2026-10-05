#pragma once

#include "lsattr.hpp"
#include <string>
#include <vector>

class LsattrReporter {
public:
    static std::string ToUtf8(const std::wstring& value);
    static std::string EscapeCsv(const std::string& value);
    static std::string EscapeJson(const std::string& value);
    static void PrintRows(const std::vector<AttrRow>& rows, OutputFormat format);
};
