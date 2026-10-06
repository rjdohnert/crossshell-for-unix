#pragma once

#include "su.hpp"

std::wstring QuoteForCommandLine(const std::wstring& value);

std::wstring BuildShellCommand(const std::wstring& customCommand, bool loginShell);
