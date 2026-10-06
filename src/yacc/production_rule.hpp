#pragma once

#include "yacc.hpp"

struct ProductionRule {
    int id = 0;
    std::string lhs;
    std::vector<std::string> rhs;
    std::string action;
    int precedence = 0;
    std::string explicit_prec_token;
};
