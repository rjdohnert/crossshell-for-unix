#pragma once

#include "tail.hpp"

class TailOutput {
public:
    static void WriteBytes(const char* data, size_t size);

    static std::string Utf8(const std::wstring& value);

    static void PrintHeader(const std::wstring& filename, bool& first_header);
};
