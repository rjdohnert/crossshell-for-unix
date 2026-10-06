#pragma once

#include "yacc.hpp"

enum class ActionType { SHIFT, REDUCE, ACCEPT, PARSE_ERROR };

struct Action {
    ActionType type = ActionType::PARSE_ERROR;
    int target = 0; // State ID for SHIFT, Rule ID for REDUCE
};
