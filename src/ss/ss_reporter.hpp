#pragma once

#include "socket_row.hpp"
#include "ss.hpp"

class SsReporter {
public:
    static void Emit(const std::vector<SocketRow>& rows);
};
