#pragma once

#include "umask.hpp"

class UmaskFormatter {
public:
    static std::string WideToUtf8(const std::wstring& wstr);

    static std::string FormatOctalMask(unsigned int mask);

    static std::string FormatSymbolicMask(unsigned int mask);
};
