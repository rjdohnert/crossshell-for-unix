#pragma once

#include "grammar_associativity.hpp"
#include "yacc.hpp"

struct TokenSymbol {
    std::string name;
    int value = 0;
    bool is_terminal = true;
    Assoc assoc = Assoc::NONE;
    int precedence = 0;
    std::string type;
};
