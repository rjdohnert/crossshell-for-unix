#pragma once

#include "ss.hpp"

struct SocketRow {
    std::wstring proto;
    std::wstring local;
    std::wstring peer;
    std::wstring state;
    DWORD pid = 0;
};
