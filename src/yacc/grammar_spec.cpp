#include "grammar_associativity.hpp"
#include "grammar_helpers.hpp"
#include "grammar_spec.hpp"
#include "lr_item.hpp"
#include "lr1_item.hpp"
#include "parser_action.hpp"
#include "parser_state.hpp"
#include "production_rule.hpp"
#include "token_symbol.hpp"

void GrammarSpec::AddSymbol(const std::string& name, bool is_term, int val , Assoc assoc , int prec , const std::string& type ) {
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

bool GrammarSpec::Parse(const std::string& filepath, std::string& err_msg) {
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

void GrammarSpec::ParseDeclarations(const std::string& text) {
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

void GrammarSpec::ParseRules(const std::string& text) {
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

void GrammarSpec::ComputeFirstSets() {
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

void GrammarSpec::ComputeFollowSets() {
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

std::set<std::string> GrammarSpec::FirstOfSequenceWithLookahead(const std::vector<std::string>& seq, const std::string& lookahead) {
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

std::set<std::string> GrammarSpec::FirstOfRuleSuffixWithLookahead(const ProductionRule& r, size_t start_idx, const std::string& lookahead) {
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

std::set<LR1Item> GrammarSpec::ClosureLR1(std::set<LR1Item> items) {
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

std::set<LR1Item> GrammarSpec::GotoLR1(const std::set<LR1Item>& items, const std::string& symbol) {
        std::set<LR1Item> res;
        for (const auto& item : items) {
            const auto& r = rules[item.rule_id];
            if (item.dot < r.rhs.size() && r.rhs[item.dot] == symbol) {
                res.insert(LR1Item{ item.rule_id, item.dot + 1, item.lookahead });
            }
        }
        return ClosureLR1(res);
    }

std::set<Item> GrammarSpec::CoreFromLR1(const std::set<LR1Item>& items) {
        std::set<Item> core;
        for (const auto& i : items) core.insert(Item{ i.rule_id, i.dot });
        return core;
    }

void GrammarSpec::BuildLALRStateAutomaton() {
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

void GrammarSpec::BuildParsingTables() {
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
