#pragma once

#include "supervisord.hpp"

bool TryParseInt(const std::wstring& input, int& out);

bool TryParseUnsignedLongLong(const std::wstring& input, unsigned long long& out);

bool TryParseDword(const std::wstring& input, DWORD& out);

std::wstring TrimConfigToken(std::wstring s);
