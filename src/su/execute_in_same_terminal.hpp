#pragma once

#include "su.hpp"

DWORD ExecuteInSameTerminal(
    const std::wstring& username,
    const std::wstring& domain,
    const std::wstring& password,
    const std::wstring& commandLine);
