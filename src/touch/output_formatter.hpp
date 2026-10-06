#pragma once

#include "touch.hpp"

class OutputFormatter {
public:
    static std::wstring Quote(const std::wstring& value);

    static void EmitHeader(int format, std::wostream& out);

    static void EmitRecord(const std::wstring& target, const std::wstring& status, int format, std::wostream& out);
};
