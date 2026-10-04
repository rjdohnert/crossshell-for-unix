/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * CrossShell for UNIX
 */

#include "evaluator.hpp"
#include "string_utils.hpp"
#include <algorithm>
#include <cctype>
#include <regex>
#include <unordered_map>
#include <vector>

size_t Evaluator::find_top_level_operator(const std::string& expression, const std::string& op) {
    int depth = 0;
    char quote = '\0';
    for (size_t i = 0; i + op.size() <= expression.size(); ++i) {
        char ch = expression[i];
        if (quote != '\0') {
            if (ch == quote) {
                size_t backslashes = 0;
                size_t k = i;
                while (k > 0 && expression[k - 1] == '\\') { ++backslashes; --k; }
                if ((backslashes % 2) == 0) quote = '\0';
            }
            continue;
        }
        if (ch == '\'' || ch == '"') { quote = ch; continue; }
        if (ch == '(') { ++depth; continue; }
        if (ch == ')' && depth > 0) { --depth; continue; }
        if (depth == 0 && expression.compare(i, op.size(), op) == 0) return i;
    }
    return std::string::npos;
}

std::string Evaluator::trim(const std::string& s) {
    auto start = std::find_if_not(s.begin(), s.end(), [](int c){ return std::isspace(c); });
    auto end = std::find_if_not(s.rbegin(), s.rend(), [](int c){ return std::isspace(c); }).base();
    return (end <= start ? std::string() : std::string(start, end));
}

bool Evaluator::is_numeric_str(const std::string& s, double& out_val) {
    if (s.empty()) return false;
    try {
        size_t p = 0;
        out_val = std::stod(s, &p);
        while (p < s.size() && std::isspace(static_cast<unsigned char>(s[p]))) p++;
        return p == s.size();
    } catch (...) {
        return false;
    }
}

std::string Evaluator::interpolate_segment(const std::string& seg, const RecordContext& ctx) {
    std::string s = trim(seg);
    if (s.empty()) return "";
    if (!s.empty() && s[0] == '`') s.erase(0, 1);

    if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
        std::string content = s.substr(1, s.size() - 2);
        while (content.size() >= 2 && ((content.front() == '"' && content.back() == '"') || (content.front() == '\'' && content.back() == '\''))) {
            content = content.substr(1, content.size() - 2);
        }
        std::string unescaped;
        for (size_t k = 0; k < content.size(); ++k) {
            if (content[k] == '\\' && k + 1 < content.size()) {
                char nxt = content[k + 1];
                if (nxt == '"' || nxt == '\'' || nxt == '\\') {
                    unescaped += nxt;
                    ++k;
                } else if (nxt == 'n') {
                    unescaped += '\n';
                    ++k;
                } else if (nxt == 't') {
                    unescaped += '\t';
                    ++k;
                } else {
                    unescaped += content[k];
                }
            } else {
                unescaped += content[k];
            }
        }
        return unescaped;
    }

    if (s == "NR" || s == "NF" || s == "FNR" || s == "FILENAME" || s == "FS" || s == "OFS" ||
        (s.size() > 1 && s[0] == '$') || (s.size() > 1 && s[0] == '.')) {
        return ctx.resolve(s).to_string();
    }

    std::string result;
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '$' || s[i] == '.') {
            size_t start = i++;
            if (s[start] == '$' && i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
                while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
            } else {
                while (i < s.size() && (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '_' || s[i] == '.' || s[i] == '[' || s[i] == ']')) ++i;
            }
            std::string symbol = s.substr(start, i - start);
            result += ctx.resolve(symbol).to_string();
        } else if (s[i] == '\\' && i + 1 < s.size()) {
            i++;
            if (s[i] == 't') result += '\t';
            else if (s[i] == 'n') result += '\n';
            else if (s[i] == 'r') result += '\r';
            else result += s[i];
            i++;
        } else {
            result += s[i++];
        }
    }
    return result;
}

bool Evaluator::eval_condition(const std::string& cond, const RecordContext& ctx) const {
    if (cond.empty()) return true;

    std::string trimmed = trim(cond);
    while (trimmed.size() >= 2 && trimmed.front() == '(' && trimmed.back() == ')') {
        int depth = 0;
        bool balanced = true;
        char quote = '\0';
        for (size_t i = 0; i < trimmed.size() - 1; ++i) {
            char ch = trimmed[i];
            if (quote != '\0') {
                if (ch == quote) {
                    size_t backslashes = 0;
                    size_t k = i;
                    while (k > 0 && trimmed[k - 1] == '\\') { ++backslashes; --k; }
                    if ((backslashes % 2) == 0) quote = '\0';
                }
                continue;
            }
            if (ch == '\'' || ch == '"') quote = ch;
            else if (ch == '(') ++depth;
            else if (ch == ')' && --depth == 0) { balanced = false; break; }
        }
        if (!balanced || depth != 1 || quote != '\0') break;
        trimmed = trim(trimmed.substr(1, trimmed.size() - 2));
    }

    // Split only outside quoted strings and parenthesized expressions.
    size_t or_pos = find_top_level_operator(trimmed, "||");
    if (or_pos != std::string::npos) {
        return eval_condition(trimmed.substr(0, or_pos), ctx) || eval_condition(trimmed.substr(or_pos + 2), ctx);
    }

    // Logical AND (&&)
    size_t and_pos = find_top_level_operator(trimmed, "&&");
    if (and_pos != std::string::npos) {
        return eval_condition(trimmed.substr(0, and_pos), ctx) && eval_condition(trimmed.substr(and_pos + 2), ctx);
    }

    // Negation (!)
    if (!trimmed.empty() && trimmed.front() == '!') {
        return !eval_condition(trimmed.substr(1), ctx);
    }

    // Comparison Operators
    std::regex op_re;
    std::smatch match;
    if (AwkSafe::CompileRegex(R"((.+?)\s*(==|!=|>=|<=|>|<|~|!~)\s*(.+))", op_re) && std::regex_match(trimmed, match, op_re)) {
        std::string lhs_str = trim(match[1].str());
        std::string op      = match[2].str();
        std::string rhs_str = trim(match[3].str());

        Value lhs = ctx.resolve(lhs_str);
        Value rhs = ctx.resolve(rhs_str);

        double l_num = 0.0, r_num = 0.0;
        bool l_is_num = is_numeric_str(lhs.to_string(), l_num);
        bool r_is_num = is_numeric_str(rhs.to_string(), r_num);

        if (op == "==") {
            return (l_is_num && r_is_num) ? (l_num == r_num) : (lhs.to_string() == rhs.to_string());
        }
        if (op == "!=") {
            return (l_is_num && r_is_num) ? (l_num != r_num) : (lhs.to_string() != rhs.to_string());
        }
        if (op == "~" || op == "!~") {
            std::regex::flag_type flags = std::regex::ECMAScript;
            if (ctx.opts && ctx.opts->ignore_case) flags |= std::regex::icase;
            static std::unordered_map<std::string, std::regex> regex_cache;
            std::string cache_key = (ctx.opts && ctx.opts->ignore_case ? "i:" : "s:") + rhs.to_string();
            auto cached = regex_cache.find(cache_key);
            if (cached == regex_cache.end()) {
                std::regex new_re;
                if (!AwkSafe::CompileRegex(rhs.to_string(), new_re, flags)) return false;
                cached = regex_cache.emplace(cache_key, std::move(new_re)).first;
            }
            bool matched = std::regex_search(lhs.to_string(), cached->second);
            return (op == "~") ? matched : !matched;
        }

        if (l_is_num && r_is_num) {
            if (op == "<")  return l_num < r_num;
            if (op == "<=") return l_num <= r_num;
            if (op == ">")  return l_num > r_num;
            if (op == ">=") return l_num >= r_num;
        } else {
            if (op == "<")  return lhs.to_string() < rhs.to_string();
            if (op == "<=") return lhs.to_string() <= rhs.to_string();
            if (op == ">")  return lhs.to_string() > rhs.to_string();
            if (op == ">=") return lhs.to_string() >= rhs.to_string();
        }
    }

    return ctx.resolve(trimmed).to_bool();
}

std::string Evaluator::interpolate(const std::string& fmt, const RecordContext& ctx) const {
    if (fmt.empty()) return ctx.raw_line;

    std::vector<std::string> parts;
    std::string current;
    char in_quote = '\0';
    for (size_t i = 0; i < fmt.size(); ++i) {
        char c = fmt[i];
        if (in_quote) {
            if (c == in_quote && (i == 0 || fmt[i - 1] != '\\')) in_quote = '\0';
            current += c;
        } else if (c == '"' || c == '\'') {
            in_quote = c;
            current += c;
        } else if (c == ',') {
            parts.push_back(trim(current));
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.empty() || !parts.empty()) {
        parts.push_back(trim(current));
    }

    std::string ofs = ctx.opts ? ctx.opts->ofs : " ";

    if (parts.size() > 1) {
        std::string out;
        for (size_t p = 0; p < parts.size(); ++p) {
            if (p > 0) out += ofs;
            out += interpolate_segment(parts[p], ctx);
        }
        return out;
    }

    return interpolate_segment(fmt, ctx);
}
