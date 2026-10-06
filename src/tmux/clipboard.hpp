#pragma once

#include "tmux.hpp"

bool SetOSClipboard(const std::wstring& text);

std::wstring GetOSClipboard();
