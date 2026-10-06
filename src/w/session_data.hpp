#pragma once

#include "w.hpp"

struct SessionData {
    DWORD sessionId;
    std::wstring username;
    std::wstring tty;
    std::wstring fromHost;
    ULONGLONG logonTime;
    ULONGLONG idleTimeMs;
    ULONGLONG jcpu100ns;
    ULONGLONG pcpu100ns;
    std::wstring whatProcess;
    bool isConnected;
};
