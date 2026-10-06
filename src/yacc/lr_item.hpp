#pragma once

#include "yacc.hpp"

struct Item {
    int rule_id = 0;
    size_t dot = 0;

    bool operator<(const Item& o) const;
    bool operator==(const Item& o) const;
};
