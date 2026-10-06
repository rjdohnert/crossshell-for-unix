#pragma once

#include "yacc.hpp"

void EnableVT100Colors();

std::string Trim(const std::string& str);

std::string ReplaceAll(std::string str, const std::string& from, const std::string& to);

bool DecodeCharLiteralToken(const std::string& token, int& out_value);
bool ParseQuotedSymbolLiteral(const std::string& text, size_t& pos, std::string& out_literal);
std::string RewriteSemanticAction(const std::string& action, size_t rhs_size);
bool ExtractBalancedBraceBlock(const std::string& text, size_t open_pos, size_t& end_pos, std::string& block);

std::string GetBaseFilename(const std::string& filepath);

// --- Robust C Action & Grammar Lexer ---
std::string ExtractAction(const std::string& text, size_t& pos);

// --- Grammar Parser & State Machine Engine ---
