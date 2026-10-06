#pragma once

#include "traceroute.hpp"

struct ProbeResult {
    bool responded = false;
    double rttMs = 0.0;
    IN_ADDR responderIp = {};
    BYTE icmpType = 0;
    BYTE icmpCode = 0;
};
