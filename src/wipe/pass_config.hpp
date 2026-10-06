#pragma once

#include "wipe.hpp"

enum PassType { PASS_ZERO, PASS_ONE, PASS_PATTERN, PASS_RANDOM };

struct PassConfig {
    PassType type;
    unsigned char pattern = 0x00;
};
