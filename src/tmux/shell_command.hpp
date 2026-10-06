#pragma once

#include "tmux.hpp"

std::wstring DetectDefaultShell();

std::string ShellTabName(const std::wstring& shellCmd);
