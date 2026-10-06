#pragma once

#include "supervisord.hpp"

std::vector<wchar_t> CreateEnvironmentBlock(const std::map<std::wstring, std::wstring>& customEnv);
