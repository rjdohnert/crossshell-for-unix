#pragma once

#include <string>
#include <string_view>

bool ParseGroupInputLine(std::string_view line, std::wstring& groupName,
                         bool& skip, std::wstring& error);
