#pragma once

#include "yacc.hpp"

struct LR1Item {
    int rule_id = 0;
    size_t dot = 0;
    std::string lookahead;

    bool operator<(const LR1Item& o) const;
    bool operator==(const LR1Item& o) const;
};
