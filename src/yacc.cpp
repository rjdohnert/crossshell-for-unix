/*
 * yacc - LALR(1) Parser Generator 
 * Copyright (C) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL_BODY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
Single-File Index
-----------------
1. Platform includes and terminal color utilities
2. Core grammar/parsing data models and parser configuration state
3. Grammar lexer/parser helpers and precedence handling
4. LR state construction and parse table generation logic
5. Conflict analysis/reporting and diagnostics output
6. CLI option parsing, command dispatch, and entry point
*/

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <queue>
#include <algorithm>
#include <cctype>
#include <memory>
#include <iomanip>
#include <cstdlib>
#include <limits>
#include <cstdio>
#include <streambuf>

// --- ANSI Terminal Colors for Windows CMD / PowerShell ---
namespace Color {
    const char* RESET   = "\033[0m";
    const char* BOLD    = "\033[1m";
    const char* RED     = "\033[31m";
    const char* GREEN   = "\033[32m";
    const char* YELLOW  = "\033[33m";
    const char* BLUE    = "\033[34m";
    const char* MAGENTA = "\033[35m";
    const char* CYAN    = "\033[36m";
    const char* WHITE   = "\033[37m";
    const char* GRAY    = "\033[90m";
}

// --- Data Models & Structs ---
enum class Assoc { NONE, LEFT, RIGHT, NONASSOC };

struct TokenSymbol {
    std::string name;
    int value = 0;
    bool is_terminal = true;
    Assoc assoc = Assoc::NONE;
    int precedence = 0;
    std::string type;
};

struct ProductionRule {
    int id = 0;
    std::string lhs;
    std::vector<std::string> rhs;
    std::string action;
    int precedence = 0;
    std::string explicit_prec_token;
};

struct Item {
    int rule_id = 0;
    size_t dot = 0;

    bool operator<(const Item& o) const {
        if (rule_id != o.rule_id) return rule_id < o.rule_id;
        return dot < o.dot;
    }
    bool operator==(const Item& o) const {
        return rule_id == o.rule_id && dot == o.dot;
    }
};

struct LR1Item {
    int rule_id = 0;
    size_t dot = 0;
    std::string lookahead;

    bool operator<(const LR1Item& o) const {
        if (rule_id != o.rule_id) return rule_id < o.rule_id;
        if (dot != o.dot) return dot < o.dot;
        return lookahead < o.lookahead;
    }
    bool operator==(const LR1Item& o) const {
        return rule_id == o.rule_id && dot == o.dot && lookahead == o.lookahead;
    }
};

enum class ActionType { SHIFT, REDUCE, ACCEPT, PARSE_ERROR };

struct Action {
    ActionType type = ActionType::PARSE_ERROR;
    int target = 0; // State ID for SHIFT, Rule ID for REDUCE
};

struct State {
    int id = 0;
    std::set<Item> items;
    std::map<std::string, int> transitions; // Symbol -> State ID
    std::map<std::string, Action> actions;  // Terminal -> Action
    std::map<std::string, int> gotos;       // Non-terminal -> State ID
    std::map<int, std::set<std::string>> reduce_lookaheads; // rule_id -> lookaheads
};

struct Config {
    std::string input_file;
    std::string output_file;
    std::string header_file;
    std::string verbose_file;
    std::string sym_prefix = "yy";
    std::string file_prefix;
    bool yacc_mode = false;
    bool generate_header = false;
    bool verbose = false;
    bool debug_mode = false;
    bool no_lines = false;
    bool token_table = false;
    enum class OutputFormat { Human, Json, Csv, Table } output_format = OutputFormat::Human;
    std::string pipe_command;
};

class BisonPipeBuffer : public std::streambuf {
    FILE* file_;
    char buffer_[4096];
public:
    explicit BisonPipeBuffer(FILE* file) : file_(file) { setp(buffer_, buffer_ + sizeof(buffer_)); }
    int_type overflow(int_type ch) override { if (ch != traits_type::eof()) { *pptr() = static_cast<char>(ch); pbump(1); } return sync() == 0 ? traits_type::not_eof(ch) : traits_type::eof(); }
    int sync() override { auto n = pptr() - pbase(); if (n && std::fwrite(pbase(), 1, static_cast<size_t>(n), file_) != static_cast<size_t>(n)) return -1; setp(buffer_, buffer_ + sizeof(buffer_)); return std::fflush(file_) == 0 ? 0 : -1; }
};

class BisonStructuredBuffer : public std::streambuf {
    std::streambuf* target_;
    Config::OutputFormat format_;
    std::string pending_;
    bool first_ = true;
    void write(const std::string& text) { target_->sputn(text.data(), static_cast<std::streamsize>(text.size())); }
    void emit() {
        if (pending_.empty()) return;
        if (format_ == Config::OutputFormat::Json) {
            if (!first_) write(",\n");
            first_ = false;
            std::string escaped = "{\"output\":\"";
            for (char ch : pending_) { if (ch == '"' || ch == '\\') escaped += '\\'; if (ch == '\r') escaped += "\\r"; else escaped += ch; }
            write(escaped + "\"}");
        } else if (format_ == Config::OutputFormat::Csv) {
            std::string escaped = "\"";
            for (char ch : pending_) escaped += ch == '"' ? "\"\"" : std::string(1, ch);
            write(escaped + "\"\n");
        } else {
            write(pending_ + "\n");
        }
        pending_.clear();
    }
public:
    BisonStructuredBuffer(std::streambuf* target, Config::OutputFormat format) : target_(target), format_(format) {
        if (format_ == Config::OutputFormat::Json) write("[\n");
        else if (format_ == Config::OutputFormat::Table) write("OUTPUT\n------\n");
    }
    ~BisonStructuredBuffer() override { emit(); if (format_ == Config::OutputFormat::Json) write("\n]\n"); }
    int_type overflow(int_type ch) override { if (ch != traits_type::eof()) { if (ch == '\n') emit(); else pending_.push_back(static_cast<char>(ch)); } return traits_type::not_eof(ch); }
    int sync() override { emit(); return target_->pubsync(); }
};

class BisonOutputSession {
    std::streambuf* old_;
    FILE* pipe_ = nullptr;
    BisonPipeBuffer* pipeBuffer_ = nullptr;
    BisonStructuredBuffer* structured_ = nullptr;
public:
    BisonOutputSession(Config::OutputFormat format, const std::string& command) : old_(std::cout.rdbuf()) {
        std::streambuf* target = old_;
        if (!command.empty()) {
            pipe_ = _popen(command.c_str(), "w");
            if (pipe_) { pipeBuffer_ = new BisonPipeBuffer(pipe_); target = pipeBuffer_; }
        }
        if (format != Config::OutputFormat::Human) {
            structured_ = new BisonStructuredBuffer(target, format);
            std::cout.rdbuf(structured_);
        } else if (pipe_) {
            std::cout.rdbuf(target);
        }
    }
    ~BisonOutputSession() { std::cout.flush(); std::cout.rdbuf(old_); delete structured_; delete pipeBuffer_; if (pipe_) _pclose(pipe_); }
};

// --- Helper Functions ---
void EnableVT100Colors() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (GetConsoleMode(hOut, &dwMode)) {
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
    }
#endif
}

std::string Trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::string ReplaceAll(std::string str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return str;
}

bool DecodeCharLiteralToken(const std::string& token, int& out_value);
bool ParseQuotedSymbolLiteral(const std::string& text, size_t& pos, std::string& out_literal);
std::string RewriteSemanticAction(const std::string& action, size_t rhs_size);
bool ExtractBalancedBraceBlock(const std::string& text, size_t open_pos, size_t& end_pos, std::string& block);

std::string GetBaseFilename(const std::string& filepath) {
    size_t last_slash = filepath.find_last_of("/\\");
    std::string filename = (last_slash == std::string::npos) ? filepath : filepath.substr(last_slash + 1);
    size_t last_dot = filename.find_last_of('.');
    if (last_dot != std::string::npos) {
        return filename.substr(0, last_dot);
    }
    return filename;
}

// --- Robust C Action & Grammar Lexer ---
std::string ExtractAction(const std::string& text, size_t& pos) {
    size_t start = pos;
    int depth = 0;
    bool in_string = false;
    bool in_char = false;
    bool in_sline_comment = false;
    bool in_mline_comment = false;

    while (pos < text.length()) {
        char c = text[pos];
        char next = (pos + 1 < text.length()) ? text[pos + 1] : '\0';

        if (in_sline_comment) {
            if (c == '\n') in_sline_comment = false;
        } else if (in_mline_comment) {
            if (c == '*' && next == '/') {
                in_mline_comment = false;
                pos++;
            }
        } else if (in_string) {
            if (c == '\\' && pos + 1 < text.length()) pos++;
            else if (c == '"') in_string = false;
        } else if (in_char) {
            if (c == '\\' && pos + 1 < text.length()) pos++;
            else if (c == '\'') in_char = false;
        } else {
            if (c == '/' && next == '/') {
                in_sline_comment = true;
                pos++;
            } else if (c == '/' && next == '*') {
                in_mline_comment = true;
                pos++;
            } else if (c == '"') {
                in_string = true;
            } else if (c == '\'') {
                in_char = true;
            } else if (c == '{') {
                depth++;
            } else if (c == '}') {
                depth--;
                if (depth == 0) {
                    pos++;
                    return text.substr(start + 1, pos - start - 2);
                }
            }
        }
        pos++;
    }
    return text.substr(start + 1);
}

// --- Grammar Parser & State Machine Engine ---
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

    void AddSymbol(const std::string& name, bool is_term, int val = 0, Assoc assoc = Assoc::NONE, int prec = 0, const std::string& type = "") {
        auto it = symbols.find(name);
        if (it != symbols.end()) {
            if (!is_term && it->second.is_terminal) {
                it->second.is_terminal = false;
                terminals.erase(name);
                nonterminals.insert(name);
            }
            if (assoc != Assoc::NONE) it->second.assoc = assoc;
            if (prec > 0) it->second.precedence = prec;
            if (!type.empty()) it->second.type = type;
            return;
        }

        TokenSymbol sym;
        sym.name = name;
        sym.is_terminal = is_term;
        if (is_term) {
            int decoded_char = 0;
            if (DecodeCharLiteralToken(name, decoded_char)) {
                sym.value = decoded_char;
            } else {
                sym.value = (val > 0) ? val : token_counter++;
            }
            terminals.insert(name);
        } else {
            nonterminals.insert(name);
        }
        sym.assoc = assoc;
        sym.precedence = prec;
        sym.type = type;
        symbols[name] = sym;
    }

    bool Parse(const std::string& filepath, std::string& err_msg) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            err_msg = "Cannot open input file: " + filepath;
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();
        file.close();

        // Built-in Special Terminals
        AddSymbol("$end", true, 0);
        AddSymbol("error", true, 256);

        size_t pos = 0;
        size_t len = content.length();
        int section = 1;

        std::string decl_text = "";
        std::string rules_text = "";

        while (pos < len) {
            if (section < 3 && content.compare(pos, 2, "%%") == 0) {
                if (section == 1) section = 2;
                else if (section == 2) {
                    section = 3;
                    epilogue = content.substr(pos + 2);
                    break;
                }
                pos += 2;
                continue;
            }

            if (section == 1) decl_text += content[pos++];
            else if (section == 2) rules_text += content[pos++];
        }

        ParseDeclarations(decl_text);
        ParseRules(rules_text);

        if (rules.empty()) {
            err_msg = "No production rules found in grammar.";
            return false;
        }

        if (start_symbol.empty()) {
            start_symbol = rules[0].lhs;
        }

        // Add Augmented Production Rule 0: $accept -> start_symbol $end
        ProductionRule aug;
        aug.id = 0;
        aug.lhs = "$accept";
        aug.rhs = { start_symbol, "$end" };
        rules.insert(rules.begin(), aug);
        AddSymbol("$accept", false);

        // Compute Automaton Tables
        ComputeFirstSets();
        ComputeFollowSets();
        BuildLALRStateAutomaton();
        BuildParsingTables();

        return true;
    }

private:
    void ParseDeclarations(const std::string& text) {
        std::istringstream stream(text);
        std::string line;
        bool in_c_block = false;

        while (std::getline(stream, line)) {
            std::string trimmed = Trim(line);
            if (trimmed.empty()) continue;

            if (trimmed == "%{" || trimmed == "%code top {" || trimmed == "%code {") {
                in_c_block = true;
                continue;
            }
            if (trimmed == "%}") {
                in_c_block = false;
                continue;
            }

            if (in_c_block) {
                prologue += line + "\n";
                continue;
            }

            if (trimmed.rfind("%union", 0) == 0) {
                size_t union_pos = text.find("%union");
                if (union_pos != std::string::npos) {
                    size_t open_b = text.find('{', union_pos);
                    size_t close_b = std::string::npos;
                    std::string block;
                    if (open_b != std::string::npos && ExtractBalancedBraceBlock(text, open_b, close_b, block)) {
                        union_decl = block;
                    }
                }
                continue;
            }

            if (trimmed.rfind("%token", 0) == 0 || trimmed.rfind("%left", 0) == 0 || 
                trimmed.rfind("%right", 0) == 0 || trimmed.rfind("%nonassoc", 0) == 0) {
                
                Assoc assoc = Assoc::NONE;
                int prec = 0;
                if (trimmed.rfind("%left", 0) == 0) { assoc = Assoc::LEFT; prec = current_prec++; }
                else if (trimmed.rfind("%right", 0) == 0) { assoc = Assoc::RIGHT; prec = current_prec++; }
                else if (trimmed.rfind("%nonassoc", 0) == 0) { assoc = Assoc::NONASSOC; prec = current_prec++; }

                std::istringstream tok_stream(trimmed);
                std::string directive;
                tok_stream >> directive;

                std::string type = "";
                std::string tok_name;
                while (tok_stream >> tok_name) {
                    if (tok_name.front() == '<' && tok_name.back() == '>') {
                        type = tok_name.substr(1, tok_name.length() - 2);
                        continue;
                    }
                    AddSymbol(tok_name, true, 0, assoc, prec, type);
                }
            } else if (trimmed.rfind("%type", 0) == 0) {
                std::istringstream tok_stream(trimmed);
                std::string dir, type_str, sym_name;
                tok_stream >> dir >> type_str;
                std::string type = (type_str.front() == '<' && type_str.back() == '>') ? type_str.substr(1, type_str.length() - 2) : "";
                while (tok_stream >> sym_name) {
                    AddSymbol(sym_name, false, 0, Assoc::NONE, 0, type);
                }
            } else if (trimmed.rfind("%start", 0) == 0) {
                std::istringstream s(trimmed);
                std::string dir, name;
                s >> dir >> name;
                if (!name.empty()) start_symbol = name;
            }
        }
    }

    void ParseRules(const std::string& text) {
        size_t pos = 0;
        size_t len = text.length();
        std::string current_lhs = "";
        int rule_counter = 1;

        while (pos < len) {
            // Skip whitespace & comments
            if (std::isspace(text[pos])) { pos++; continue; }
            if (text.compare(pos, 2, "//") == 0) {
                pos = text.find('\n', pos);
                if (pos == std::string::npos) break;
                continue;
            }
            if (text.compare(pos, 2, "/*") == 0) {
                pos = text.find("*/", pos);
                if (pos == std::string::npos) break;
                pos += 2;
                continue;
            }

            // Parse LHS Symbol
            size_t start_sym = pos;
            while (pos < len && (std::isalnum(text[pos]) || text[pos] == '_' || text[pos] == '$')) pos++;
            std::string symbol = text.substr(start_sym, pos - start_sym);
            if (symbol.empty()) { pos++; continue; }

            // Skip spaces to colon
            while (pos < len && std::isspace(text[pos])) pos++;
            if (pos < len && text[pos] == ':') {
                current_lhs = symbol;
                AddSymbol(current_lhs, false);
                pos++;
            } else {
                continue;
            }

            // Parse RHS Alternatives
            std::vector<std::string> rhs;
            std::string action = "";
            std::string prec_token = "";

            while (pos < len) {
                while (pos < len && std::isspace(text[pos])) pos++;
                if (pos >= len) break;

                char c = text[pos];

                if (c == ';') {
                    if (!rhs.empty() || !action.empty()) {
                        ProductionRule r;
                        r.id = rule_counter++;
                        r.lhs = current_lhs;
                        r.rhs = rhs;
                        r.action = action;
                        r.explicit_prec_token = prec_token;
                        rules.push_back(r);
                    }
                    pos++;
                    break;
                }

                if (c == '|') {
                    ProductionRule r;
                    r.id = rule_counter++;
                    r.lhs = current_lhs;
                    r.rhs = rhs;
                    r.action = action;
                    r.explicit_prec_token = prec_token;
                    rules.push_back(r);

                    rhs.clear();
                    action = "";
                    prec_token = "";
                    pos++;
                    continue;
                }

                if (c == '{') {
                    action = ExtractAction(text, pos);
                    continue;
                }

                if (text.compare(pos, 5, "%prec") == 0) {
                    pos += 5;
                    while (pos < len && std::isspace(text[pos])) pos++;
                    size_t st = pos;
                    while (pos < len && !std::isspace(text[pos]) && text[pos] != ';' && text[pos] != '|' && text[pos] != '{') pos++;
                    prec_token = text.substr(st, pos - st);
                    continue;
                }

                // Read RHS Symbol (Named or Character Literal)
                if (c == '\'') {
                    std::string char_tok;
                    if (ParseQuotedSymbolLiteral(text, pos, char_tok)) {
                        AddSymbol(char_tok, true);
                        rhs.push_back(char_tok);
                        continue;
                    }
                }

                size_t st = pos;
                while (pos < len && !std::isspace(text[pos]) && text[pos] != ':' && text[pos] != '|' && text[pos] != ';' && text[pos] != '{') pos++;
                std::string rhs_sym = text.substr(st, pos - st);
                if (!rhs_sym.empty()) {
                    if (symbols.find(rhs_sym) == symbols.end()) {
                        AddSymbol(rhs_sym, true); // Default to terminal if undeclared
                    }
                    rhs.push_back(rhs_sym);
                }
            }
        }

        // Infer Production Rule Precedence
        for (auto& r : rules) {
            if (!r.explicit_prec_token.empty() && symbols.count(r.explicit_prec_token)) {
                r.precedence = symbols[r.explicit_prec_token].precedence;
            } else {
                for (auto it = r.rhs.rbegin(); it != r.rhs.rend(); ++it) {
                    if (symbols.count(*it) && symbols[*it].is_terminal && symbols[*it].precedence > 0) {
                        r.precedence = symbols[*it].precedence;
                        break;
                    }
                }
            }
        }
    }

    void ComputeFirstSets() {
        for (const auto& term : terminals) {
            first_sets[term].insert(term);
        }

        bool changed = true;
        while (changed) {
            changed = false;
            for (const auto& r : rules) {
                if (r.id == 0) continue;
                size_t before_size = first_sets[r.lhs].size();
                if (r.rhs.empty()) {
                    first_sets[r.lhs].insert(""); // Epsilon
                } else {
                    for (const auto& sym : r.rhs) {
                        const auto& sym_first = first_sets[sym];
                        for (const auto& f : sym_first) {
                            if (!f.empty()) first_sets[r.lhs].insert(f);
                        }
                        if (sym_first.find("") == sym_first.end()) break;
                    }
                }
                if (first_sets[r.lhs].size() > before_size) changed = true;
            }
        }
    }

    void ComputeFollowSets() {
        follow_sets[start_symbol].insert("$end");
        follow_sets["$accept"].insert("$end");

        bool changed = true;
        while (changed) {
            changed = false;
            for (const auto& r : rules) {
                for (size_t i = 0; i < r.rhs.size(); ++i) {
                    const std::string& B = r.rhs[i];
                    if (terminals.count(B)) continue;

                    size_t before_size = follow_sets[B].size();
                    bool rest_has_epsilon = true;

                    for (size_t j = i + 1; j < r.rhs.size(); ++j) {
                        const std::string& beta = r.rhs[j];
                        for (const auto& f : first_sets[beta]) {
                            if (!f.empty()) follow_sets[B].insert(f);
                        }
                        if (first_sets[beta].find("") == first_sets[beta].end()) {
                            rest_has_epsilon = false;
                            break;
                        }
                    }

                    if (rest_has_epsilon) {
                        for (const auto& f : follow_sets[r.lhs]) {
                            follow_sets[B].insert(f);
                        }
                    }

                    if (follow_sets[B].size() > before_size) changed = true;
                }
            }
        }
    }

    std::set<std::string> FirstOfSequenceWithLookahead(const std::vector<std::string>& seq, const std::string& lookahead) {
        std::set<std::string> out;
        bool all_nullable = true;
        for (const auto& sym : seq) {
            auto it = first_sets.find(sym);
            if (it == first_sets.end()) {
                out.insert(sym);
                all_nullable = false;
                break;
            }

            for (const auto& t : it->second) {
                if (!t.empty()) out.insert(t);
            }

            if (it->second.find("") == it->second.end()) {
                all_nullable = false;
                break;
            }
        }

        if (all_nullable) out.insert(lookahead);
        return out;
    }

    std::set<std::string> FirstOfRuleSuffixWithLookahead(const ProductionRule& r, size_t start_idx, const std::string& lookahead) {
        std::set<std::string> out;
        bool all_nullable = true;

        for (size_t idx = start_idx; idx < r.rhs.size(); ++idx) {
            const std::string& sym = r.rhs[idx];
            auto it = first_sets.find(sym);
            if (it == first_sets.end()) {
                out.insert(sym);
                all_nullable = false;
                break;
            }

            for (const auto& t : it->second) {
                if (!t.empty()) out.insert(t);
            }

            if (it->second.find("") == it->second.end()) {
                all_nullable = false;
                break;
            }
        }

        if (all_nullable) out.insert(lookahead);
        return out;
    }

    std::set<LR1Item> ClosureLR1(std::set<LR1Item> items) {
        bool changed = true;
        while (changed) {
            changed = false;
            std::set<LR1Item> added;
            for (const auto& item : items) {
                const auto& r = rules[item.rule_id];
                if (item.dot >= r.rhs.size()) continue;

                const std::string& B = r.rhs[item.dot];
                if (!nonterminals.count(B)) continue;

                std::set<std::string> lookaheads = FirstOfRuleSuffixWithLookahead(r, item.dot + 1, item.lookahead);

                for (const auto& rule : rules) {
                    if (rule.lhs != B) continue;
                    for (const auto& la : lookaheads) {
                        LR1Item new_item{ rule.id, 0, la };
                        if (items.find(new_item) == items.end() && added.find(new_item) == added.end()) {
                            added.insert(new_item);
                            changed = true;
                        }
                    }
                }
            }
            items.insert(added.begin(), added.end());
        }
        return items;
    }

    std::set<LR1Item> GotoLR1(const std::set<LR1Item>& items, const std::string& symbol) {
        std::set<LR1Item> res;
        for (const auto& item : items) {
            const auto& r = rules[item.rule_id];
            if (item.dot < r.rhs.size() && r.rhs[item.dot] == symbol) {
                res.insert(LR1Item{ item.rule_id, item.dot + 1, item.lookahead });
            }
        }
        return ClosureLR1(res);
    }

    std::set<Item> CoreFromLR1(const std::set<LR1Item>& items) {
        std::set<Item> core;
        for (const auto& i : items) core.insert(Item{ i.rule_id, i.dot });
        return core;
    }

    void BuildLALRStateAutomaton() {
        struct CanonState {
            int id = 0;
            std::set<LR1Item> items;
            std::map<std::string, int> transitions;
        };

        std::vector<CanonState> canon_states;
        std::map<std::set<LR1Item>, int> lr1_state_index;
        std::queue<int> unvisited;

        CanonState c0;
        c0.id = 0;
        c0.items = ClosureLR1({ LR1Item{ 0, 0, "$end" } });
        canon_states.push_back(c0);
        lr1_state_index[c0.items] = 0;
        unvisited.push(0);

        while (!unvisited.empty()) {
            int st_id = unvisited.front();
            unvisited.pop();

            std::set<std::string> next_symbols;
            for (const auto& item : canon_states[st_id].items) {
                const auto& r = rules[item.rule_id];
                if (item.dot < r.rhs.size()) {
                    next_symbols.insert(r.rhs[item.dot]);
                }
            }

            for (const auto& sym : next_symbols) {
                std::set<LR1Item> goto_items = GotoLR1(canon_states[st_id].items, sym);
                if (goto_items.empty()) continue;

                int existing_state = -1;
                auto st_it = lr1_state_index.find(goto_items);
                if (st_it != lr1_state_index.end()) {
                    existing_state = st_it->second;
                }

                if (existing_state == -1) {
                    CanonState new_st;
                    new_st.id = static_cast<int>(canon_states.size());
                    new_st.items = goto_items;
                    canon_states.push_back(new_st);
                    existing_state = new_st.id;
                    lr1_state_index[goto_items] = existing_state;
                    unvisited.push(existing_state);
                }

                canon_states[st_id].transitions[sym] = existing_state;
            }
        }

        states.clear();
        std::map<std::set<Item>, int> core_to_lalr_state;
        std::vector<int> canon_to_lalr(canon_states.size(), -1);

        for (const auto& cst : canon_states) {
            std::set<Item> core = CoreFromLR1(cst.items);
            int lalr_id = -1;
            auto it = core_to_lalr_state.find(core);
            if (it == core_to_lalr_state.end()) {
                State st;
                st.id = static_cast<int>(states.size());
                st.items = core;
                states.push_back(st);
                lalr_id = st.id;
                core_to_lalr_state[core] = lalr_id;
            } else {
                lalr_id = it->second;
            }

            canon_to_lalr[cst.id] = lalr_id;
            for (const auto& itm : cst.items) {
                const auto& r = rules[itm.rule_id];
                if (itm.dot == r.rhs.size()) {
                    states[lalr_id].reduce_lookaheads[itm.rule_id].insert(itm.lookahead);
                }
            }
        }

        for (const auto& cst : canon_states) {
            int from = canon_to_lalr[cst.id];
            for (const auto& tr : cst.transitions) {
                int to = canon_to_lalr[tr.second];
                states[from].transitions[tr.first] = to;
            }
        }
    }

    void BuildParsingTables() {
        for (auto& st : states) {
            // 1. Shift Actions & Gotos
            for (const auto& trans : st.transitions) {
                const std::string& sym = trans.first;
                int target_st = trans.second;
                if (terminals.count(sym)) {
                    Action act;
                    act.type = ActionType::SHIFT;
                    act.target = target_st;
                    st.actions[sym] = act;
                } else {
                    st.gotos[sym] = target_st;
                }
            }

            // 2. Reduce & Accept Actions with LALR(1) Lookaheads
            for (const auto& rl : st.reduce_lookaheads) {
                int rule_id = rl.first;
                const auto& r = rules[rule_id];
                for (const auto& lookahead : rl.second) {
                    if (r.lhs == "$accept" && lookahead == "$end") {
                        Action act;
                        act.type = ActionType::ACCEPT;
                        st.actions["$end"] = act;
                        continue;
                    }

                            if (st.actions.count(lookahead)) {
                                Action existing = st.actions[lookahead];
                                if (existing.type == ActionType::SHIFT) {
                                    // Shift/Reduce Conflict Resolution
                                    int rule_prec = r.precedence;
                                    int tok_prec = symbols[lookahead].precedence;
                                    Assoc assoc = symbols[lookahead].assoc;

                                    if (rule_prec > 0 && tok_prec > 0) {
                                        if (rule_prec > tok_prec || (rule_prec == tok_prec && assoc == Assoc::LEFT)) {
                                            Action act;
                                            act.type = ActionType::REDUCE;
                                            act.target = rule_id;
                                            st.actions[lookahead] = act;
                                        } else if (rule_prec == tok_prec && assoc == Assoc::NONASSOC) {
                                            Action act;
                                            act.type = ActionType::PARSE_ERROR;
                                            st.actions[lookahead] = act;
                                        }
                                    } else {
                                        // Default Yacc Behavior: Prefer Shift over Reduce
                                        shift_reduce_conflicts++;
                                    }
                                } else if (existing.type == ActionType::REDUCE) {
                                    // Reduce/Reduce Conflict Resolution: Keep lower Rule ID
                                    reduce_reduce_conflicts++;
                                    if (rule_id < existing.target) {
                                        Action act;
                                        act.type = ActionType::REDUCE;
                                        act.target = rule_id;
                                        st.actions[lookahead] = act;
                                    }
                                }
                            } else {
                                Action act;
                                act.type = ActionType::REDUCE;
                                act.target = rule_id;
                                st.actions[lookahead] = act;
                            }
                }
            }
        }
    }
};

// --- Code Generator ---
class CodeGenerator {
public:
    static bool Generate(const Config& config, const GrammarSpec& grammar, std::string& err_msg) {
        std::string pfx = config.sym_prefix;

        // 1. Generate Header File (.tab.h or .h)
        if (config.generate_header || !config.header_file.empty()) {
            std::string header_path = config.header_file;
            if (header_path.empty()) {
                header_path = config.yacc_mode ? "y.tab.h" : GetBaseFilename(config.input_file) + ".tab.h";
            }

            std::ofstream hfile(header_path);
            if (!hfile.is_open()) {
                err_msg = "Could not create header file: " + header_path;
                return false;
            }

            std::string guard = GetBaseFilename(header_path) + "_H_INCLUDED";
            std::transform(guard.begin(), guard.end(), guard.begin(), ::toupper);

            hfile << "/* Automatically generated by yacc C++ Windows CLI Utility */\n";
            hfile << "#ifndef " << guard << "\n";
            hfile << "#define " << guard << "\n\n";

            hfile << "#ifndef " << pfx << "STYPE\n";
            if (!grammar.union_decl.empty()) {
                hfile << "typedef union " << grammar.union_decl << " " << pfx << "STYPE;\n";
            } else {
                hfile << "typedef int " << pfx << "STYPE;\n";
            }
            hfile << "#define " << pfx << "STYPE_IS_DECLARED 1\n";
            hfile << "#endif\n\n";

            hfile << "enum " << pfx << "tokentype {\n";
            bool first = true;
            for (const auto& kv : grammar.symbols) {
                if (!kv.second.is_terminal || kv.first.front() == '\'') continue;
                if (!first) hfile << ",\n";
                hfile << "  " << kv.second.name << " = " << kv.second.value;
                first = false;
            }
            hfile << "\n};\n\n";

            hfile << "extern " << pfx << "STYPE " << pfx << "lval;\n";
            hfile << "int " << pfx << "parse(void);\n";
            hfile << "int " << pfx << "lex(void);\n";
            hfile << "void " << pfx << "error(const char *s);\n\n";

            hfile << "#endif /* " << guard << " */\n";
            if (hfile.fail()) {
                err_msg = "Write error while generating header file: " + header_path;
                hfile.close();
                return false;
            }
            hfile.close();
        }

        // 2. Generate Parser Source File (.tab.c or .c)
        std::string out_path = config.output_file;
        if (out_path.empty()) {
            out_path = config.yacc_mode ? "y.tab.c" : GetBaseFilename(config.input_file) + ".tab.c";
        }

        std::ofstream cfile(out_path);
        if (!cfile.is_open()) {
            err_msg = "Could not create output file: " + out_path;
            return false;
        }

        cfile << "/* High-Speed LALR(1) State Automaton Engine Generated by yacc C++ Utility */\n";
        cfile << "#include <stdio.h>\n";
        cfile << "#include <stdlib.h>\n";
        cfile << "#include <string.h>\n\n";

        cfile << "/* Prologue Code */\n";
        cfile << grammar.prologue << "\n\n";

        cfile << "#ifndef " << pfx << "STYPE\n";
        if (!grammar.union_decl.empty()) {
            cfile << "typedef union " << grammar.union_decl << " " << pfx << "STYPE;\n";
        } else {
            cfile << "typedef int " << pfx << "STYPE;\n";
        }
        cfile << "#endif\n\n";

        cfile << pfx << "STYPE " << pfx << "lval;\n";
        cfile << "int " << pfx << "char = -1;\n";
        cfile << "int " << pfx << "nerrs = 0;\n";
        cfile << "int " << pfx << "debug = " << (config.debug_mode ? "1" : "0") << ";\n\n";

        // Generate Production Rule LHS & Length Lookup Arrays
        cfile << "static const int yyr1[] = { 0";
        for (size_t i = 1; i < grammar.rules.size(); ++i) {
            cfile << ", " << grammar.rules[i].id;
        }
        cfile << " };\n";

        cfile << "static const int yyr2[] = { 0";
        for (size_t i = 1; i < grammar.rules.size(); ++i) {
            cfile << ", " << grammar.rules[i].rhs.size();
        }
        cfile << " };\n\n";

        // Generate Deterministic State Transition Execution Engine
        cfile << "/* LALR(1) State Machine Engine */\n";
        cfile << "#define YYSTACK_INIT_SIZE 256\n";
        cfile << "#define YYMAXDEPTH 1000000\n";
        cfile << "static int yygrowstack(int **yyssa, " << pfx << "STYPE **yyvsa, int *yystack_size, int **yyssp, " << pfx << "STYPE **yyvsp) {\n";
        cfile << "    if (*yystack_size >= YYMAXDEPTH) return 0;\n";
        cfile << "    int used = (int)(*yyssp - *yyssa) + 1;\n";
        cfile << "    int new_size = (*yystack_size < 1) ? YYSTACK_INIT_SIZE : (*yystack_size * 2);\n";
        cfile << "    if (new_size < *yystack_size) return 0;\n";
        cfile << "    if (new_size > YYMAXDEPTH) new_size = YYMAXDEPTH;\n";
        cfile << "    int *new_ss = (int*)malloc((size_t)new_size * sizeof(int));\n";
        cfile << "    " << pfx << "STYPE *new_vs = (" << pfx << "STYPE*)malloc((size_t)new_size * sizeof(" << pfx << "STYPE));\n";
        cfile << "    if (!new_ss || !new_vs) { free(new_ss); free(new_vs); return 0; }\n";
        cfile << "    memcpy(new_ss, *yyssa, (size_t)used * sizeof(int));\n";
        cfile << "    memcpy(new_vs, *yyvsa, (size_t)used * sizeof(" << pfx << "STYPE));\n";
        cfile << "    free(*yyssa);\n";
        cfile << "    free(*yyvsa);\n";
        cfile << "    *yyssa = new_ss;\n";
        cfile << "    *yyvsa = new_vs;\n";
        cfile << "    *yyssp = new_ss + (used - 1);\n";
        cfile << "    *yyvsp = new_vs + (used - 1);\n";
        cfile << "    *yystack_size = new_size;\n";
        cfile << "    return 1;\n";
        cfile << "}\n";
        cfile << "int " << pfx << "parse(void) {\n";
        cfile << "    int yystate = 0;\n";
        cfile << "    int yychar = -1;\n";
        cfile << "    int yystack_size = YYSTACK_INIT_SIZE;\n";
        cfile << "    " << pfx << "STYPE *yyvsa = (" << pfx << "STYPE*)malloc((size_t)yystack_size * sizeof(" << pfx << "STYPE));\n";
        cfile << "    int *yyssa = (int*)malloc((size_t)yystack_size * sizeof(int));\n";
        cfile << "    " << pfx << "STYPE *yyvsp = yyvsa;\n";
        cfile << "    int *yyssp = yyssa;\n";
        cfile << "    int yyerrstatus = 0;\n";
        cfile << "    " << pfx << "STYPE yyval;\n";
        cfile << "    int yylen = 0;\n\n";

        cfile << "    if (!yyvsa || !yyssa) {\n";
        cfile << "        free(yyvsa); free(yyssa);\n";
        cfile << "        " << pfx << "error(\"parser stack allocation failure\");\n";
        cfile << "        return 2;\n";
        cfile << "    }\n\n";

        cfile << "    *yyssp = 0;\n";
        cfile << "    while (1) {\n";
        cfile << "        yystate = *yyssp;\n";
        cfile << "        if (yychar < 0) {\n";
        cfile << "            yychar = " << pfx << "lex();\n";
        cfile << "            if (yychar < 0) yychar = 0;\n";
        cfile << "        }\n\n";

        cfile << "        switch (yystate) {\n";
        for (const auto& st : grammar.states) {
            cfile << "            case " << st.id << ": {\n";
            std::map<int, Action> by_tok;
            bool has_duplicate_tok = false;
            for (const auto& act_kv : st.actions) {
                const std::string& sym_name = act_kv.first;
                auto tok_it = grammar.symbols.find(sym_name);
                if (tok_it == grammar.symbols.end()) {
                    err_msg = "Internal generation error: missing symbol in action table: " + sym_name;
                    cfile.close();
                    return false;
                }
                int tok_val = tok_it->second.value;
                auto ins = by_tok.insert({ tok_val, act_kv.second });
                if (!ins.second) {
                    has_duplicate_tok = true;
                }
            }

            auto emit_action_body = [&](const Action& act) {
                if (act.type == ActionType::SHIFT) {
                    cfile << "                    if ((yyssp - yyssa) >= (yystack_size - 1)) {\n";
                    cfile << "                        if (!yygrowstack(&yyssa, &yyvsa, &yystack_size, &yyssp, &yyvsp)) {\n";
                    cfile << "                            " << pfx << "error(\"parser stack out of memory\");\n";
                    cfile << "                            free(yyvsa); free(yyssa);\n";
                    cfile << "                            return 2;\n";
                    cfile << "                        }\n";
                    cfile << "                    }\n";
                    cfile << "                    yyssp++; yyvsp++;\n";
                    cfile << "                    *yyssp = " << act.target << ";\n";
                    cfile << "                    *yyvsp = " << pfx << "lval;\n";
                    cfile << "                    if (yyerrstatus > 0) yyerrstatus--;\n";
                    cfile << "                    yychar = -1;\n";
                    cfile << "                    continue;\n";
                } else if (act.type == ActionType::REDUCE) {
                    cfile << "                    goto do_reduce_" << act.target << ";\n";
                } else if (act.type == ActionType::ACCEPT) {
                    cfile << "                    free(yyvsa); free(yyssa);\n";
                    cfile << "                    return 0; /* Accept */\n";
                } else {
                    cfile << "                    goto yy_syntax_error;\n";
                }
            };

            if (!has_duplicate_tok && !by_tok.empty()) {
                cfile << "                switch (yychar) {\n";
                for (const auto& kv : by_tok) {
                    cfile << "                case " << kv.first << ":\n";
                    emit_action_body(kv.second);
                    cfile << "                    break;\n";
                }
                cfile << "                default:\n";
                cfile << "                    goto yy_syntax_error;\n";
                cfile << "                }\n";
            } else {
                for (const auto& act_kv : st.actions) {
                    const std::string& sym_name = act_kv.first;
                    const Action& act = act_kv.second;
                    auto tok_it = grammar.symbols.find(sym_name);
                    if (tok_it == grammar.symbols.end()) {
                        err_msg = "Internal generation error: missing symbol in action table: " + sym_name;
                        cfile.close();
                        return false;
                    }
                    int tok_val = tok_it->second.value;

                    cfile << "                if (yychar == " << tok_val << ") {\n";
                    emit_action_body(act);
                    cfile << "                }\n";
                }
            }
            
            cfile << "                goto yy_syntax_error;\n";
            cfile << "            } break;\n";
        }
        cfile << "        }\n\n";

        cfile << "yy_syntax_error:\n";
        cfile << "        " << pfx << "nerrs++;\n";
        cfile << "        " << pfx << "error(\"syntax error\");\n";
        cfile << "        if (yychar == 0) { free(yyvsa); free(yyssa); return 1; }\n";
        cfile << "        while (yyssp >= yyssa) {\n";
        cfile << "            switch (*yyssp) {\n";
        for (const auto& st : grammar.states) {
            auto err_it = st.actions.find("error");
            if (err_it != st.actions.end() && err_it->second.type == ActionType::SHIFT) {
                cfile << "                case " << st.id << ":\n";
                cfile << "                    if ((yyssp - yyssa) >= (yystack_size - 1)) {\n";
                cfile << "                        if (!yygrowstack(&yyssa, &yyvsa, &yystack_size, &yyssp, &yyvsp)) {\n";
                cfile << "                            " << pfx << "error(\"parser stack out of memory\");\n";
                cfile << "                            free(yyvsa); free(yyssa);\n";
                cfile << "                            return 2;\n";
                cfile << "                        }\n";
                cfile << "                    }\n";
                cfile << "                    yyssp++; yyvsp++;\n";
                cfile << "                    *yyssp = " << err_it->second.target << ";\n";
                cfile << "                    yyerrstatus = 3;\n";
                cfile << "                    yychar = -1;\n";
                cfile << "                    goto yy_recovered;\n";
            }
        }
        cfile << "                default: break;\n";
        cfile << "            }\n";
        cfile << "            yyssp--; yyvsp--;\n";
        cfile << "        }\n";
        cfile << "        free(yyvsa); free(yyssa);\n";
        cfile << "        return 1;\n";
        cfile << "yy_recovered:\n";
        cfile << "        continue;\n\n";

        // Reduction Handlers with Relative Stack Reference ($1, $2, $$)
        for (size_t i = 1; i < grammar.rules.size(); ++i) {
            const auto& r = grammar.rules[i];
            cfile << "do_reduce_" << r.id << ": {\n";
            cfile << "    yylen = " << r.rhs.size() << ";\n";
            
            std::string action_code = RewriteSemanticAction(r.action, r.rhs.size());

            if (!Trim(action_code).empty()) {
                cfile << "    " << action_code << "\n";
            }

            cfile << "    yyssp -= yylen;\n";
            cfile << "    yyvsp -= yylen;\n";
            cfile << "    yystate = *yyssp;\n";

            // State GOTO Transitions
            cfile << "    switch (yystate) {\n";
            for (const auto& st : grammar.states) {
                if (st.gotos.count(r.lhs)) {
                    cfile << "        case " << st.id << ": yystate = " << st.gotos.at(r.lhs) << "; break;\n";
                }
            }
            cfile << "    }\n";
            cfile << "    if ((yyssp - yyssa) >= (yystack_size - 1)) {\n";
            cfile << "        if (!yygrowstack(&yyssa, &yyvsa, &yystack_size, &yyssp, &yyvsp)) {\n";
            cfile << "            " << pfx << "error(\"parser stack out of memory\");\n";
            cfile << "            free(yyvsa); free(yyssa);\n";
            cfile << "            return 2;\n";
            cfile << "        }\n";
            cfile << "    }\n";
            cfile << "    yyssp++; yyvsp++;\n";
            cfile << "    *yyssp = yystate;\n";
            cfile << "    *yyvsp = yyval;\n";
            cfile << "    continue;\n";
            cfile << "}\n";
        }

        cfile << "    }\n";
        cfile << "    free(yyvsa); free(yyssa);\n";
        cfile << "    return 0;\n";
        cfile << "}\n\n";

        cfile << "/* Epilogue Code */\n";
        cfile << grammar.epilogue << "\n";
        if (cfile.fail()) {
            err_msg = "Write error while generating parser source file: " + out_path;
            cfile.close();
            return false;
        }
        cfile.close();

        // 3. Generate Detailed State Analysis Report (.output)
        if (config.verbose) {
            std::string vpath = config.verbose_file;
            if (vpath.empty()) {
                vpath = GetBaseFilename(config.input_file) + ".output";
            }

            std::ofstream vfile(vpath);
            if (vfile.is_open()) {
                vfile << "Grammar State Analysis Report - yacc C++ LALR Engine\n";
                vfile << "==============================================================================\n\n";
                if (grammar.shift_reduce_conflicts > 0) {
                    vfile << "State conflicts: " << grammar.shift_reduce_conflicts << " shift/reduce\n";
                }
                if (grammar.reduce_reduce_conflicts > 0) {
                    vfile << "State conflicts: " << grammar.reduce_reduce_conflicts << " reduce/reduce\n";
                }
                vfile << "\nTerminals:\n";
                for (const auto& term : grammar.terminals) {
                    vfile << "  " << term << " (token " << grammar.symbols.at(term).value << ")\n";
                }
                vfile << "\nNonterminals:\n";
                for (const auto& nt : grammar.nonterminals) {
                    vfile << "  " << nt << "\n";
                }
                vfile << "\nProduction Rules:\n";
                for (const auto& r : grammar.rules) {
                    vfile << "  Rule " << r.id << ": " << r.lhs << " -> ";
                    for (const auto& sym : r.rhs) vfile << sym << " ";
                    vfile << "\n";
                }
                vfile << "\n\n";
                vfile << "Automation States:\n";
                vfile << "\n";
                for (const auto& st : grammar.states) {
                    vfile << "\nState " << st.id << ":\n\n";
                    for (const auto& item : st.items) {
                        const auto& r = grammar.rules[item.rule_id];
                        vfile << "  (" << r.id << ") " << r.lhs << " -> ";
                        for (size_t k = 0; k < r.rhs.size(); ++k) {
                            if (k == item.dot) vfile << ". ";
                            vfile << r.rhs[k] << " ";
                        }
                        if (item.dot == r.rhs.size()) vfile << ".";
                        vfile << "\n";
                    }
                    vfile << "\n";
                    for (const auto& act : st.actions) {
                        vfile << "    " << std::left << std::setw(15) << act.first;
                        if (act.second.type == ActionType::SHIFT) vfile << "shift, go to state " << act.second.target << "\n";
                        else if (act.second.type == ActionType::REDUCE) vfile << "reduce using rule " << act.second.target << "\n";
                        else if (act.second.type == ActionType::ACCEPT) vfile << "accept\n";
                        else vfile << "error\n";
                    }
                    for (const auto& gt : st.gotos) {
                        vfile << "    " << std::left << std::setw(15) << gt.first << "go to state " << gt.second << "\n";
                    }
                }
                if (vfile.fail()) {
                    err_msg = "Write error while generating verbose report file: " + vpath;
                    vfile.close();
                    return false;
                }
                vfile.close();
            }
        }

        return true;
    }
};

// --- Comprehensive Manual & Help System ---

// --- Comprehensive Manual & Help System ---
void DisplayHelp() {
    std::cout << R"(yacc(1)                 CrossShell for UNIX Reference Manual                 yacc(1)

    NAME
        yacc - LALR(1) parser generator

    SYNOPSIS
        yacc [OPTIONS] GRAMMAR_FILE
        yacc [OPTIONS]

    DESCRIPTION
        yacc reads the grammar specification in GRAMMAR_FILE and generates an
        LR(1) / LALR(1) parser in C or C++. The generated parser translates an
        input stream of tokens according to the syntactic rules and semantic
        actions defined in the grammar.

    OPTIONS
        -d, --defines[=FILE]
            Generate an external C/C++ header file containing token declarations
            and semantic union definitions.

        -b, --file-prefix=PREFIX
            Specify the prefix to prepend to output file names.

        -o, --output=FILE
            Specify the output C/C++ parser file path.

        -p, --prefix=PREFIX
            Change the default 'yy' symbol prefix to PREFIX for parser
            variables and functions (e.g. xxparse, xxlex, yylval).

        -t, --debug
            Include parser runtime debugging code and execution traces.

        -v, --verbose
            Write an extensive grammar conflict and automaton state description
            file (.output).

        -y, --yacc
            Maintain strict POSIX Yacc compatibility; output files are named
            y.tab.c and y.tab.h by default.

        -l, --no-lines
            Suppress '#line' directives in generated C/C++ source code.

        --output FORMAT
            Select table, csv, tsv, or json output. The default is table.

        --json, -j, --csv, --tsv, --table
            Convenience shortcuts for structured output formats.

        --pipe COMMAND
            Stream formatted output directly to another command or utility.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    GRAMMAR STRUCTURE
        A standard Yacc grammar file consists of three sections separated by '%%':

            %{
            /* C/C++ declarations and header includes */
            %}

            /* Token and precedence declarations */
            %token NUMBER IDENTIFIER
            %left '+' '-'
            %left '*' '/'

            %%

            /* Grammar rules and semantic actions */
            expr: expr '+' expr   { $$ = $1 + $3; }
                | NUMBER          { $$ = $1; }
                ;

            %%

            /* C/C++ user code and helper subroutines */

    EXAMPLES
        yacc grammar.y
            Generate standard LALR(1) parser 'grammar.tab.c'.

        yacc -d -y calc.y
            Generate parser and token header in POSIX mode ('y.tab.c', 'y.tab.h').

        yacc -d -v -p math_ -o math_parser.c math.y
            Generate parser with custom prefix 'math_' and state report 'math.output'.

        yacc -d -o parser.cpp grammar.y
            Generate C++ parser source file.

    CrossShell for UNIX                                                     yacc(1)
)";
}

void DisplayVersion() {
    std::cout << "yacc (CrossShell) 7.0.2\n"
              << "Copyright (C) 2026 Roberto J Dohnert. All rights reserved.\n";
}

// --- Main CLI Entry Point ---
static int bison_main(int argc, char* argv[]) {
    EnableVT100Colors();

    try {

    if (argc < 2) {
        DisplayHelp();
        return 0;
    }

    Config config;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help" || arg == "/?" || arg == "-?") {
            DisplayHelp();
            return 0;
        } else if (arg == "-V" || arg == "--version") {
            DisplayVersion();
            return 0;
        } else if (arg == "-d" || arg == "--defines") {
            config.generate_header = true;
        } else if (arg.rfind("--defines=", 0) == 0) {
            config.generate_header = true;
            config.header_file = arg.substr(10);
        } else if (arg == "-y" || arg == "--yacc") {
            config.yacc_mode = true;
            config.generate_header = true;
        } else if (arg == "-v" || arg == "--verbose") {
            config.verbose = true;
        } else if (arg == "-t" || arg == "--debug") {
            config.debug_mode = true;
        } else if (arg == "-l" || arg == "--no-lines") {
            config.no_lines = true;
        } else if (arg == "--json" || arg == "-j") {
            config.output_format = Config::OutputFormat::Json;
        } else if (arg == "--csv") {
            config.output_format = Config::OutputFormat::Csv;
        } else if (arg == "--table") {
            config.output_format = Config::OutputFormat::Table;
        } else if (arg == "--output" && i + 1 < argc) {
            std::string fmt = argv[++i];
            if (fmt == "json") config.output_format = Config::OutputFormat::Json;
            else if (fmt == "csv") config.output_format = Config::OutputFormat::Csv;
            else if (fmt == "table") config.output_format = Config::OutputFormat::Table;
        } else if (arg == "--pipe" && i + 1 < argc) {
            config.pipe_command = argv[++i];
        } else if (arg == "-o" && i + 1 < argc) {
            config.output_file = argv[++i];
        } else if (arg.rfind("--output=", 0) == 0) {
            config.output_file = arg.substr(9);
        } else if (arg == "-p" && i + 1 < argc) {
            config.sym_prefix = argv[++i];
        } else if (arg.rfind("--prefix=", 0) == 0) {
            config.sym_prefix = arg.substr(9);
        } else if (arg == "-b" && i + 1 < argc) {
            config.file_prefix = argv[++i];
        } else if (arg.rfind("--file-prefix=", 0) == 0) {
            config.file_prefix = arg.substr(14);
        } else if (!arg.empty() && arg.front() != '-') {
            config.input_file = arg;
        }
    }

    if (config.input_file.empty()) {
        std::cerr << Color::RED << "[!] Error: No input grammar file specified.\n"
                  << "    Run 'yacc --help' for usage guidance." << Color::RESET << std::endl;
        return 1;
    }

    BisonOutputSession output_session(config.output_format, config.pipe_command);

    GrammarSpec grammar;
    std::string parse_err;

    std::cout << Color::BOLD << Color::CYAN << "[+] Processing grammar file: " << config.input_file << Color::RESET << std::endl;

    if (!grammar.Parse(config.input_file, parse_err)) {
        std::cerr << Color::RED << "[!] Syntax Error: " << parse_err << Color::RESET << std::endl;
        return 1;
    }

    std::string gen_err;
    if (!CodeGenerator::Generate(config, grammar, gen_err)) {
        std::cerr << Color::RED << "[!] Code Generation Error: " << gen_err << Color::RESET << std::endl;
        return 1;
    }

    std::cout << Color::BOLD << Color::GREEN << "[+] LALR(1) Parser generated (" 
              << grammar.states.size() << " states)" << Color::RESET << std::endl;

    if (grammar.shift_reduce_conflicts > 0 || grammar.reduce_reduce_conflicts > 0) {
        std::cout << Color::YELLOW << "[!] Warnings during state generation:\n";
        if (grammar.shift_reduce_conflicts > 0)
            std::cout << "  -> " << grammar.shift_reduce_conflicts << " shift/reduce conflict(s)\n";
        if (grammar.reduce_reduce_conflicts > 0)
            std::cout << "  -> " << grammar.reduce_reduce_conflicts << " reduce/reduce conflict(s)\n";
        std::cout << Color::RESET;
    }

    if (config.generate_header) {
        std::string hname = config.header_file.empty() ? (config.yacc_mode ? "y.tab.h" : GetBaseFilename(config.input_file) + ".tab.h") : config.header_file;
        std::cout << "  -> Header file: " << Color::YELLOW << hname << Color::RESET << "\n";
    }
    std::string cname = config.output_file.empty() ? (config.yacc_mode ? "y.tab.c" : GetBaseFilename(config.input_file) + ".tab.c") : config.output_file;
    std::cout << "  -> Source file: " << Color::YELLOW << cname << Color::RESET << "\n";

    if (config.verbose) {
        std::string vname = config.verbose_file.empty() ? GetBaseFilename(config.input_file) + ".output" : config.verbose_file;
        std::cout << "  -> Report file: " << Color::YELLOW << vname << Color::RESET << "\n";
    }

    return 0;
    } catch (const std::exception& e) {
        std::cerr << Color::RED << "[!] Fatal error: " << e.what() << Color::RESET << std::endl;
        return 1;
    } catch (...) {
        std::cerr << Color::RED << "[!] Fatal error: unexpected exception" << Color::RESET << std::endl;
        return 1;
    }
}

bool DecodeCharLiteralToken(const std::string& token, int& out_value) {
    if (token.size() < 3 || token.front() != '\'' || token.back() != '\'') {
        return false;
    }

    std::string body = token.substr(1, token.size() - 2);
    if (body.empty()) return false;

    unsigned int value = 0;
    if (body[0] != '\\') {
        if (body.size() != 1) return false;
        value = static_cast<unsigned char>(body[0]);
    } else {
        if (body.size() == 1) return false;
        char esc = body[1];
        switch (esc) {
            case 'n': value = '\n'; break;
            case 't': value = '\t'; break;
            case 'r': value = '\r'; break;
            case '0': value = '\0'; break;
            case '\\': value = '\\'; break;
            case '\'': value = '\''; break;
            case '"': value = '"'; break;
            case 'a': value = '\a'; break;
            case 'b': value = '\b'; break;
            case 'f': value = '\f'; break;
            case 'v': value = '\v'; break;
            case '?': value = '?'; break;
            case 'x': {
                if (body.size() < 3) return false;
                value = 0;
                for (size_t i = 2; i < body.size(); ++i) {
                    char ch = body[i];
                    value <<= 4;
                    if (ch >= '0' && ch <= '9') value |= static_cast<unsigned int>(ch - '0');
                    else if (ch >= 'a' && ch <= 'f') value |= static_cast<unsigned int>(ch - 'a' + 10);
                    else if (ch >= 'A' && ch <= 'F') value |= static_cast<unsigned int>(ch - 'A' + 10);
                    else return false;
                }
                break;
            }
            default: {
                if (esc >= '0' && esc <= '7') {
                    value = static_cast<unsigned int>(esc - '0');
                    size_t i = 2;
                    size_t count = 1;
                    while (i < body.size() && count < 3 && body[i] >= '0' && body[i] <= '7') {
                        value = (value * 8U) + static_cast<unsigned int>(body[i] - '0');
                        ++i;
                        ++count;
                    }
                    if (i != body.size()) return false;
                } else {
                    return false;
                }
                break;
            }
        }
    }

    out_value = static_cast<int>(value & 0xFFU);
    return true;
}

bool ParseQuotedSymbolLiteral(const std::string& text, size_t& pos, std::string& out_literal) {
    if (pos >= text.size() || text[pos] != '\'') return false;

    size_t i = pos + 1;
    bool escaped = false;
    while (i < text.size()) {
        char c = text[i];
        if (escaped) {
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '\'') {
            out_literal = text.substr(pos, i - pos + 1);
            pos = i + 1;
            return true;
        }
        ++i;
    }
    return false;
}

std::string RewriteSemanticAction(const std::string& action, size_t rhs_size) {
    std::string out;
    out.reserve(action.size() + 32);

    bool in_string = false;
    bool in_char = false;
    bool in_sline_comment = false;
    bool in_mline_comment = false;

    for (size_t i = 0; i < action.size();) {
        char c = action[i];
        char next = (i + 1 < action.size()) ? action[i + 1] : '\0';

        if (in_sline_comment) {
            out.push_back(c);
            if (c == '\n') in_sline_comment = false;
            ++i;
            continue;
        }

        if (in_mline_comment) {
            out.push_back(c);
            if (c == '*' && next == '/') {
                out.push_back('/');
                i += 2;
                in_mline_comment = false;
            } else {
                ++i;
            }
            continue;
        }

        if (in_string) {
            out.push_back(c);
            if (c == '\\' && i + 1 < action.size()) {
                out.push_back(action[i + 1]);
                i += 2;
                continue;
            }
            if (c == '"') in_string = false;
            ++i;
            continue;
        }

        if (in_char) {
            out.push_back(c);
            if (c == '\\' && i + 1 < action.size()) {
                out.push_back(action[i + 1]);
                i += 2;
                continue;
            }
            if (c == '\'') in_char = false;
            ++i;
            continue;
        }

        if (c == '/' && next == '/') {
            out.push_back('/');
            out.push_back('/');
            i += 2;
            in_sline_comment = true;
            continue;
        }
        if (c == '/' && next == '*') {
            out.push_back('/');
            out.push_back('*');
            i += 2;
            in_mline_comment = true;
            continue;
        }
        if (c == '"') {
            out.push_back(c);
            in_string = true;
            ++i;
            continue;
        }
        if (c == '\'') {
            out.push_back(c);
            in_char = true;
            ++i;
            continue;
        }

        if (c == '$') {
            if (next == '$') {
                out += "yyval";
                i += 2;
                continue;
            }

            if (next == '<') {
                size_t tag_end = action.find('>', i + 2);
                if (tag_end != std::string::npos && tag_end + 1 < action.size()) {
                    std::string tag = action.substr(i + 2, tag_end - (i + 2));
                    char after = action[tag_end + 1];
                    if (after == '$') {
                        out += "yyval." + tag;
                        i = tag_end + 2;
                        continue;
                    }
                    if (std::isdigit(static_cast<unsigned char>(after))) {
                        size_t j = tag_end + 1;
                        int index = 0;
                        while (j < action.size() && std::isdigit(static_cast<unsigned char>(action[j]))) {
                            index = (index * 10) + (action[j] - '0');
                            ++j;
                        }
                        if (index >= 1 && static_cast<size_t>(index) <= rhs_size) {
                            int rel = index - static_cast<int>(rhs_size);
                            out += "yyvsp[" + std::to_string(rel) + "]." + tag;
                        } else {
                            out += action.substr(i, j - i);
                        }
                        i = j;
                        continue;
                    }
                }
            }

            if (std::isdigit(static_cast<unsigned char>(next))) {
                size_t j = i + 1;
                int index = 0;
                while (j < action.size() && std::isdigit(static_cast<unsigned char>(action[j]))) {
                    index = (index * 10) + (action[j] - '0');
                    ++j;
                }

                if (index >= 1 && static_cast<size_t>(index) <= rhs_size) {
                    int rel = index - static_cast<int>(rhs_size);
                    out += "yyvsp[" + std::to_string(rel) + "]";
                } else {
                    out += action.substr(i, j - i);
                }
                i = j;
                continue;
            }
        }

        if (c == '<') {
            size_t tag_end = action.find('>', i + 1);
            if (tag_end != std::string::npos && tag_end + 2 < action.size() && action[tag_end + 1] == '$' &&
                std::isdigit(static_cast<unsigned char>(action[tag_end + 2]))) {
                std::string tag = action.substr(i + 1, tag_end - (i + 1));
                size_t j = tag_end + 2;
                int index = 0;
                while (j < action.size() && std::isdigit(static_cast<unsigned char>(action[j]))) {
                    index = (index * 10) + (action[j] - '0');
                    ++j;
                }
                if (index >= 1 && static_cast<size_t>(index) <= rhs_size) {
                    int rel = index - static_cast<int>(rhs_size);
                    out += "yyvsp[" + std::to_string(rel) + "]." + tag;
                    i = j;
                    continue;
                }
            }
        }

        out.push_back(c);
        ++i;
    }

    return out;
}

bool ExtractBalancedBraceBlock(const std::string& text, size_t open_pos, size_t& end_pos, std::string& block) {
    if (open_pos >= text.size() || text[open_pos] != '{') return false;

    int depth = 0;
    bool in_string = false;
    bool in_char = false;
    bool in_sline_comment = false;
    bool in_mline_comment = false;

    for (size_t i = open_pos; i < text.size(); ++i) {
        char c = text[i];
        char next = (i + 1 < text.size()) ? text[i + 1] : '\0';

        if (in_sline_comment) {
            if (c == '\n') in_sline_comment = false;
            continue;
        }
        if (in_mline_comment) {
            if (c == '*' && next == '/') {
                in_mline_comment = false;
                ++i;
            }
            continue;
        }
        if (in_string) {
            if (c == '\\' && i + 1 < text.size()) ++i;
            else if (c == '"') in_string = false;
            continue;
        }
        if (in_char) {
            if (c == '\\' && i + 1 < text.size()) ++i;
            else if (c == '\'') in_char = false;
            continue;
        }

        if (c == '/' && next == '/') {
            in_sline_comment = true;
            ++i;
            continue;
        }
        if (c == '/' && next == '*') {
            in_mline_comment = true;
            ++i;
            continue;
        }
        if (c == '"') {
            in_string = true;
            continue;
        }
        if (c == '\'') {
            in_char = true;
            continue;
        }

        if (c == '{') {
            ++depth;
        } else if (c == '}') {
            --depth;
            if (depth == 0) {
                end_pos = i;
                block = text.substr(open_pos, end_pos - open_pos + 1);
                return true;
            }
        }
    }

    return false;
}

class BisonApplication { public: int run(int argc, char* argv[]) const { return bison_main(argc, argv); } };
int main(int argc, char* argv[]) { return BisonApplication().run(argc, argv); }