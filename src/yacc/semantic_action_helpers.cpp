#include "grammar_helpers.hpp"
#include "semantic_action_helpers.hpp"

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
