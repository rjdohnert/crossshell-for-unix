#pragma once

#include "grammar_helpers.hpp"
#include "yacc.hpp"

bool DecodeCharLiteralToken(const std::string& token, int& out_value);

bool ParseQuotedSymbolLiteral(const std::string& text, size_t& pos, std::string& out_literal);

std::string RewriteSemanticAction(const std::string& action, size_t rhs_size);

bool ExtractBalancedBraceBlock(const std::string& text, size_t open_pos, size_t& end_pos, std::string& block);
