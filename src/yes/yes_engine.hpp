#pragma once

#include "yes.hpp"

class YesEngine {
private:
    static inline volatile LONG stopRequested = 0;
    static constexpr size_t BUFFER_SIZE = 64 * 1024;

    static BOOL WINAPI consoleControlHandler(DWORD controlType);

public:
    static int execute(const std::string& inputLine);
};
