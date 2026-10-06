#pragma once

#include "test.hpp"

class OutputFormatter {
public:
    static void Emit(int format, const std::wstring& pipeCommand, bool result);
};
