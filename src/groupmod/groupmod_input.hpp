#pragma once

#include <string>
#include <string_view>
#include <vector>

bool ParseModificationLine(std::string_view line,
                           std::vector<std::wstring>& fields, bool& skip,
                           std::wstring& error);
