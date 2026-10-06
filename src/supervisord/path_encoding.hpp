#pragma once

#include "supervisord.hpp"

std::wstring GetExecutableDir();

std::wstring GetUserHomeDir();

std::string WideToUtf8(const std::wstring& input);

std::wstring Utf8ToWide(const std::string& input);
