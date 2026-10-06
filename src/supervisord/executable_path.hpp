#pragma once

#include "supervisord.hpp"

bool ResolveExecutablePath(const std::wstring& command, std::wstring& resolvedPath);
