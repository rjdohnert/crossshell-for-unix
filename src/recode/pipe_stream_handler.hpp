#pragma once

#include "recode.hpp"

class PipeStreamHandler {
public:
    static void configureBinaryMode();

    static std::vector<uint8_t> readFromStdin();

    static void writeToStdout(const std::vector<uint8_t>& data);
};
