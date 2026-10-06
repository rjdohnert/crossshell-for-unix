#pragma once

#include "lr_item.hpp"
#include "parser_action.hpp"
#include "yacc.hpp"

struct State {
    int id = 0;
    std::set<Item> items;
    std::map<std::string, int> transitions; // Symbol -> State ID
    std::map<std::string, Action> actions;  // Terminal -> Action
    std::map<std::string, int> gotos;       // Non-terminal -> State ID
    std::map<int, std::set<std::string>> reduce_lookaheads; // rule_id -> lookaheads
};
