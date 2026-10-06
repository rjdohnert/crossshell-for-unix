#pragma once

#include "suspend.hpp"

class OutputFormatter {
public:
    static void Emit(int format, const std::wstring& pipeCommand, bool resume, bool all_ok);
};
