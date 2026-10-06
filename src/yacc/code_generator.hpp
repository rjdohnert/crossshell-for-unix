#pragma once

#include "grammar_spec.hpp"
#include "yacc_config.hpp"
#include "yacc.hpp"

class CodeGenerator {
public:
    static bool Generate(const Config& config, const GrammarSpec& grammar, std::string& err_msg);
};

// --- Comprehensive Manual & Help System ---

// --- Comprehensive Manual & Help System ---
