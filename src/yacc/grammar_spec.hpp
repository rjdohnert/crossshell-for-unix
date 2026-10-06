#pragma once

#include "grammar_associativity.hpp"
#include "lr_item.hpp"
#include "lr1_item.hpp"
#include "parser_state.hpp"
#include "production_rule.hpp"
#include "token_symbol.hpp"
#include "yacc.hpp"

class GrammarSpec {
public:
    std::string prologue;
    std::string epilogue;
    std::string union_decl;
    std::string start_symbol;
    std::map<std::string, TokenSymbol> symbols;
    std::vector<ProductionRule> rules;
    std::set<std::string> terminals;
    std::set<std::string> nonterminals;
    
    // LALR/SLR State Machine
    std::vector<State> states;
    std::map<std::string, std::set<std::string>> first_sets;
    std::map<std::string, std::set<std::string>> follow_sets;

    int current_prec = 1;
    int token_counter = 258;
    int shift_reduce_conflicts = 0;
    int reduce_reduce_conflicts = 0;

    void AddSymbol(const std::string& name, bool is_term, int val = 0, Assoc assoc = Assoc::NONE, int prec = 0, const std::string& type = "");

    bool Parse(const std::string& filepath, std::string& err_msg);

private:
    void ParseDeclarations(const std::string& text);

    void ParseRules(const std::string& text);

    void ComputeFirstSets();

    void ComputeFollowSets();

    std::set<std::string> FirstOfSequenceWithLookahead(const std::vector<std::string>& seq, const std::string& lookahead);

    std::set<std::string> FirstOfRuleSuffixWithLookahead(const ProductionRule& r, size_t start_idx, const std::string& lookahead);

    std::set<LR1Item> ClosureLR1(std::set<LR1Item> items);

    std::set<LR1Item> GotoLR1(const std::set<LR1Item>& items, const std::string& symbol);

    std::set<Item> CoreFromLR1(const std::set<LR1Item>& items);

    void BuildLALRStateAutomaton();

    void BuildParsingTables();
};

// --- Code Generator ---
