#pragma once

#include "buffer_config.hpp"
#include "stdbuf.hpp"

class BufferRelayEngine {
public:
    static void relayOutput(HANDLE hReadPipe, HANDLE hParentWrite, BufferConfig config);
};
