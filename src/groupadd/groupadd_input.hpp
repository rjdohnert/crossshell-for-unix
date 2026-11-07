#pragma once

#include "groupadd_types.hpp"

#include <string>
#include <string_view>

bool ParseGroupInputLine(std::string_view line,
                         const std::wstring& defaultComment,
                         GroupRequest& request, bool& skip,
                         std::wstring& error);
