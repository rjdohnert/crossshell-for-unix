#pragma once

#include "tee.hpp"

class OutputFormatter {
public:
    static void Emit(int format, FILE* outputPipe, const std::string& data);
};
