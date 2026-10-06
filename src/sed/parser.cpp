#include "parser.hpp"

// ============================================================================
// MiniJsonParser implementation
// ============================================================================

MiniJsonParser::MiniJsonParser(std::string s) : src(std::move(s)) {}

void MiniJsonParser::skip_ws() {
    while (pos < src.size() && (std::isspace(static_cast<unsigned char>(src[pos])))) pos++;
}

char MiniJsonParser::peek() { skip_ws(); return pos < src.size() ? src[pos] : '\0'; }
char MiniJsonParser::get()  { skip_ws(); return pos < src.size() ? src[pos++] : '\0'; }

std::string MiniJsonParser::parse_string() {
    get();
    std::string s;
    while (pos < src.size()) {
        char c = src[pos++];
        if (c == '"') break;
        if (c == '\\' && pos < src.size()) {
            char esc = src[pos++];
            if (esc == 'n') s += '\n';
            else if (esc == 't') s += '\t';
            else if (esc == 'r') s += '\r';
            else s += esc;
        } else {
            s += c;
        }
    }
    return s;
}

Value MiniJsonParser::parse_number() {
    size_t start = pos;
    if (src[pos] == '-') pos++;
    while (pos < src.size() && (std::isdigit(static_cast<unsigned char>(src[pos])) || src[pos] == '.' || src[pos] == 'e' || src[pos] == 'E' || src[pos] == '+' || src[pos] == '-')) {
        pos++;
    }
    try {
        return Value(std::stod(src.substr(start, pos - start)));
    } catch (...) {
        return Value(0.0);
    }
}

Value MiniJsonParser::parse_value() {
    char c = peek();
    if (c == '{') {
        get();
        Object obj;
        while (peek() != '}' && peek() != '\0') {
            if (peek() == ',') { get(); continue; }
            std::string k = parse_string();
            if (peek() == ':') get();
            obj[k] = parse_value();
        }
        if (peek() == '}') get();
        return Value(obj);
    } else if (c == '[') {
        get();
        Array arr;
        while (peek() != ']' && peek() != '\0') {
            if (peek() == ',') { get(); continue; }
            arr.push_back(parse_value());
        }
        if (peek() == ']') get();
        return Value(arr);
    } else if (c == '"') {
        return Value(parse_string());
    } else if (std::isdigit(static_cast<unsigned char>(c)) || c == '-') {
        return parse_number();
    } else if (c == 't' || c == 'f') {
        std::string b;
        while (std::isalpha(static_cast<unsigned char>(peek()))) b += get();
        return Value(b == "true");
    } else if (c == 'n') {
        for (int i = 0; i < 4; ++i) get();
        return Value();
    }
    return Value();
}

// ============================================================================
// ScriptParser implementation
// ============================================================================

std::string ScriptParser::trim(const std::string& s) {
    auto start = std::find_if_not(s.begin(), s.end(), [](int c){ return std::isspace(c); });
    auto end = std::find_if_not(s.rbegin(), s.rend(), [](int c){ return std::isspace(c); }).base();
    return (end <= start ? std::string() : std::string(start, end));
}

std::vector<std::string> ScriptParser::split_statements(const std::string& script_text) {
    std::vector<std::string> stmts;
    std::string cur;
    bool in_sq = false;
    bool in_dq = false;
    char delim = '\0';
    int delim_needed = 0;

    for (size_t i = 0; i < script_text.size(); ++i) {
        char c = script_text[i];
        if (c == '\\' && i + 1 < script_text.size()) {
            cur += c;
            cur += script_text[++i];
            continue;
        }
        if (c == '\'' && !in_dq && delim_needed == 0) {
            in_sq = !in_sq;
            cur += c;
        } else if (c == '"' && !in_sq && delim_needed == 0) {
            in_dq = !in_dq;
            cur += c;
        } else if (!in_sq && !in_dq) {
            if (delim_needed > 0) {
                if (c == delim) {
                    delim_needed--;
                }
                cur += c;
            } else {
                if (c == ';' || c == '\n') {
                    std::string t = trim(cur);
                    if (!t.empty()) stmts.push_back(t);
                    cur.clear();
                } else {
                    if (c == '/' && cur.empty()) {
                        delim = '/';
                        delim_needed = 1;
                    } else if (c == 's' && (cur.empty() || cur.back() == ':' || cur.back() == ' ' || std::isdigit(static_cast<unsigned char>(cur.back())) || cur.back() == ',')) {
                        if (i + 1 < script_text.size()) {
                            char next = script_text[i + 1];
                            if (!std::isalnum(static_cast<unsigned char>(next)) && !std::isspace(static_cast<unsigned char>(next)) && next != ';') {
                                delim = next;
                                delim_needed = 3;
                            }
                        }
                    }
                    cur += c;
                }
            }
        } else {
            cur += c;
        }
    }
    std::string t = trim(cur);
    if (!t.empty()) stmts.push_back(t);
    return stmts;
}

std::vector<SedCommand> ScriptParser::parse(const std::string& script_text) const {
    std::vector<SedCommand> commands;
    std::vector<std::string> raw_stmts = split_statements(script_text);

    for (const auto& raw_stmt : raw_stmts) {
        std::string stmt = trim(raw_stmt);
        if (stmt.empty() || stmt.front() == '#') continue;

        SedCommand cmd;
        size_t idx = 0;

        // 1. Parse Address / Predicate Prefix
        if (stmt.front() == '/') {
            size_t close_slash = std::string::npos;
            bool esc = false;
            for (size_t k = 1; k < stmt.size(); ++k) {
                if (esc) {
                    esc = false;
                } else if (stmt[k] == '\\') {
                    esc = true;
                } else if (stmt[k] == '/') {
                    close_slash = k;
                    break;
                }
            }
            if (close_slash != std::string::npos) {
                cmd.addr.type = Address::REGEX_MATCH;
                cmd.addr.regex_str = stmt.substr(1, close_slash - 1);
                idx = close_slash + 1;
            }
        } else if (std::isdigit(static_cast<unsigned char>(stmt.front()))) {
            size_t p = 0;
            cmd.addr.line1 = std::stoll(stmt, &p);
            idx = p;
            if (idx < stmt.size() && stmt[idx] == ',') {
                idx++;
                size_t p2 = 0;
                cmd.addr.line2 = std::stoll(stmt.substr(idx), &p2);
                idx += p2;
                cmd.addr.type = Address::LINE_RANGE;
            } else {
                cmd.addr.type = Address::SINGLE_LINE;
            }
        } else if (stmt.find(':') != std::string::npos && (!stmt.empty() && (stmt.front() == '.' || stmt.find("==") != std::string::npos || stmt.find(">=") != std::string::npos || stmt.find("<=") != std::string::npos))) {
            size_t colon = stmt.find(':');
            cmd.addr.type = Address::PREDICATE;
            cmd.addr.predicate_str = trim(stmt.substr(0, colon));
            idx = colon + 1;
        }

        while (idx < stmt.size() && std::isspace(static_cast<unsigned char>(stmt[idx]))) idx++;
        if (idx >= stmt.size()) continue;

        std::string op_part = stmt.substr(idx);

        // 2. Parse Commands
        if (!op_part.empty() && op_part.front() == 'd') {
            cmd.type = CommandType::DELETE_LINE;
        } else if (!op_part.empty() && op_part.front() == 'p') {
            cmd.type = CommandType::PRINT_LINE;
        } else if (!op_part.empty() && op_part.front() == 'q') {
            cmd.type = CommandType::QUIT;
        } else if (op_part.rfind("del ", 0) == 0 || op_part.rfind("unset ", 0) == 0) {
            cmd.type = CommandType::DEL_PROPERTY;
            size_t sp = op_part.find(' ');
            cmd.path = trim(op_part.substr(sp + 1));
        } else if (op_part.rfind("set ", 0) == 0) {
            cmd.type = CommandType::SET_PROPERTY;
            size_t eq = op_part.find('=');
            if (eq != std::string::npos) {
                cmd.path = trim(op_part.substr(4, eq - 4));
                cmd.set_val = parse_scalar_literal(trim(op_part.substr(eq + 1)));
            }
        } else if (!op_part.empty() && op_part.front() == 's') {
            cmd.type = CommandType::SUBSTITUTE;
            char delim = op_part.size() > 1 ? op_part[1] : '/';
            std::vector<std::string> tokens;
            std::string current;
            size_t i = 2;

            while (i < op_part.size()) {
                char c = op_part[i];
                if (c == '\\' && i + 1 < op_part.size()) {
                    char next = op_part[i + 1];
                    if (next == delim) {
                        current += delim;
                        i += 2;
                    } else if (next == '\\') {
                        current += '\\';
                        i += 2;
                    } else {
                        current += '\\';
                        current += next;
                        i += 2;
                    }
                } else if (c == delim) {
                    tokens.push_back(current);
                    current.clear();
                    i++;
                    if ((!tokens.empty() && !tokens[0].empty() && tokens[0].front() == '.' && tokens.size() == 3) ||
                        ((tokens.empty() || tokens[0].empty() || tokens[0].front() != '.') && tokens.size() == 2)) {
                        std::string flags_str = op_part.substr(i);
                        tokens.push_back(flags_str);
                        current.clear();
                        break;
                    }
                } else {
                    current += c;
                    i++;
                }
            }
            if (!current.empty() || tokens.size() < 2) {
                tokens.push_back(current);
            }

            std::string flags_token = "";
            if (!tokens.empty() && !tokens[0].empty() && tokens[0].front() == '.') {
                cmd.path = tokens[0];
                if (tokens.size() > 1) cmd.find_regex = tokens[1];
                if (tokens.size() > 2) cmd.replacement = tokens[2];
                if (tokens.size() > 3) flags_token = tokens[3];
            } else {
                if (tokens.size() > 0) cmd.find_regex = tokens[0];
                if (tokens.size() > 1) cmd.replacement = tokens[1];
                if (tokens.size() > 2) flags_token = tokens[2];
            }

            for (char f : flags_token) {
                if (f == 'g') cmd.flag_global = true;
                if (f == 'i') cmd.flag_ignore_case = true;
                if (f == 'p') cmd.flag_print = true;
            }
        }

        commands.push_back(cmd);
    }
    return commands;
}
