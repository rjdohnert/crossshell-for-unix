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

#include "context.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

std::string ScriptLoader::CleanLiteral(const std::string& raw) const {
    std::string s = raw;
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    s = s.substr(start, end - start + 1);

    while (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
        s = s.substr(1, s.size() - 2);
    }
    while (s.rfind("\\\"", 0) == 0 && s.size() >= 4 && s.substr(s.size() - 2) == "\\\"") {
        s = s.substr(2, s.size() - 4);
    }

    std::string unescaped;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char next = s[i + 1];
            if (next == '"' || next == '\'' || next == '\\') {
                unescaped += next;
                ++i;
            } else if (next == 'n') {
                unescaped += '\n';
                ++i;
            } else if (next == 't') {
                unescaped += '\t';
                ++i;
            } else {
                unescaped += s[i];
            }
        } else {
            unescaped += s[i];
        }
    }
    return unescaped;
}

void ScriptLoader::ParseInlineScript(const std::string& script_text, AwkOptions& options) const {
    std::string s = script_text;
    size_t first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return;
    s = s.substr(first);

    // Check for BEGIN { ... }
    size_t begin_pos = s.find("BEGIN");
    if (begin_pos != std::string::npos) {
        size_t b_brace = s.find('{', begin_pos);
        if (b_brace != std::string::npos) {
            size_t b_close = s.find('}', b_brace);
            if (b_close != std::string::npos) {
                std::string body = s.substr(b_brace + 1, b_close - b_brace - 1);
                size_t ppos = body.find("print");
                if (ppos != std::string::npos) {
                    options.begin_action = CleanLiteral(body.substr(ppos + 5));
                } else {
                    options.begin_action = CleanLiteral(body);
                }
                s.erase(begin_pos, b_close - begin_pos + 1);
            }
        }
    }

    // Check for END { ... }
    size_t end_pos = s.find("END");
    if (end_pos != std::string::npos) {
        size_t e_brace = s.find('{', end_pos);
        if (e_brace != std::string::npos) {
            size_t e_close = s.find('}', e_brace);
            if (e_close != std::string::npos) {
                std::string body = s.substr(e_brace + 1, e_close - e_brace - 1);
                size_t ppos = body.find("print");
                if (ppos != std::string::npos) {
                    options.end_action = CleanLiteral(body.substr(ppos + 5));
                } else {
                    options.end_action = CleanLiteral(body);
                }
                s.erase(end_pos, e_close - end_pos + 1);
            }
        }
    }

    // Parse remaining Pattern / Action
    size_t l_brace = s.find('{');
    size_t r_brace = s.rfind('}');
    if (l_brace != std::string::npos && r_brace != std::string::npos && r_brace > l_brace) {
        std::string cond = s.substr(0, l_brace);
        size_t c_start = cond.find_first_not_of(" \t\r\n");
        size_t c_end = cond.find_last_not_of(" \t\r\n");
        if (c_start != std::string::npos) {
            options.condition = cond.substr(c_start, c_end - c_start + 1);
        }

        std::string action = s.substr(l_brace + 1, r_brace - l_brace - 1);
        size_t a_start = action.find_first_not_of(" \t\r\n");
        size_t a_end = action.find_last_not_of(" \t\r\n");
        if (a_start != std::string::npos) {
            action = action.substr(a_start, a_end - a_start + 1);
            if (action.rfind("print", 0) == 0) {
                std::string pbody = action.substr(5);
                size_t p_start = pbody.find_first_not_of(" \t\r\n");
                if (p_start != std::string::npos) {
                    options.print_fmt = pbody.substr(p_start);
                } else {
                    options.print_fmt = "$0";
                }
            } else {
                options.print_fmt = action;
            }
        }
    } else {
        // No braces, could be a condition or print statement
        size_t c_start = s.find_first_not_of(" \t\r\n");
        size_t c_end = s.find_last_not_of(" \t\r\n");
        if (c_start != std::string::npos) {
            std::string trimmed = s.substr(c_start, c_end - c_start + 1);
            if (trimmed.rfind("print ", 0) == 0 || trimmed == "print") {
                options.print_fmt = (trimmed == "print") ? "$0" : trimmed.substr(6);
            } else {
                options.condition = trimmed;
            }
        }
    }
}

bool ScriptLoader::LoadScriptFile(const std::string& path, AwkOptions& options) const {
    std::ifstream script(path);
    if (!script.is_open()) return false;
    std::string line;
    auto directive_value = [](const std::string& text, size_t prefix_length) {
        std::string value = text.substr(prefix_length);
        size_t start = value.find_first_not_of(" \t");
        return start == std::string::npos ? std::string() : value.substr(start);
    };
    while (std::getline(script, line)) {
        size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos || line[first] == '#') continue;
        line = line.substr(first);
        if (line.rfind("condition=", 0) == 0 || line.rfind("condition:", 0) == 0) options.condition = directive_value(line, 10);
        else if (line.rfind("print=", 0) == 0 || line.rfind("print:", 0) == 0) options.print_fmt = directive_value(line, 6);
        else if (line.rfind("begin=", 0) == 0 || line.rfind("begin:", 0) == 0) options.begin_action = directive_value(line, 6);
        else if (line.rfind("end=", 0) == 0 || line.rfind("end:", 0) == 0) options.end_action = directive_value(line, 4);
        else if (options.condition.empty()) options.condition = line;
    }
    return true;
}

void RecordContext::load(const std::string& line, const AwkOptions& options, long long rec_nr, long long rec_fnr, const std::string& fname) {
    opts = &options;
    NR = rec_nr;
    FNR = rec_fnr;
    FILENAME = fname;
    raw_line = line;
    fields.clear();
    fields.push_back(line); // $0

    // Parse Text Fields
    if (!options.json_only) {
        if (options.fs == " ") {
            std::istringstream iss(line);
            std::string token;
            while (iss >> token) fields.push_back(token);
        } else {
            size_t start = 0, end;
            while ((end = line.find(options.fs, start)) != std::string::npos) {
                fields.push_back(line.substr(start, end - start));
                start = end + options.fs.length();
            }
            fields.push_back(line.substr(start));
        }
        NF = static_cast<long long>(fields.size()) - 1;
    }

    // Parse Object (JSON Lines auto-detection or forced)
    if (!options.text_only) {
        std::string trimmed = line;
        trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](unsigned char ch) { return !std::isspace(ch); }));
        if (!trimmed.empty() && (trimmed.front() == '{' || trimmed.front() == '[')) {
            MiniJsonParser parser(trimmed);
            parsed_obj = parser.parse_value();
            is_object = (parsed_obj.type == Value::OBJECT || parsed_obj.type == Value::ARRAY);
        } else {
            is_object = false;
            parsed_obj = Value();
        }
    }
}

Value RecordContext::resolve(const std::string& raw_token) const {
    std::string token = raw_token;
    if (!token.empty() && token[0] == '`') token.erase(0, 1);

    if (token == "NR") return Value(static_cast<double>(NR));
    if (token == "FNR") return Value(static_cast<double>(FNR));
    if (token == "NF") return Value(static_cast<double>(NF));
    if (token == "FILENAME") return Value(FILENAME);
    if (token == "FS") return Value(opts ? opts->fs : " ");
    if (token == "OFS") return Value(opts ? opts->ofs : " ");
    if (token == "$0") return Value(raw_line);
    if (token == "$NF" && NF > 0) return Value(fields.back());

    // User Variables (-v var=val)
    if (opts) {
        auto it = opts->user_vars.find(token);
        if (it != opts->user_vars.end()) return it->second;
    }

    // Positional Fields ($1, $2, ...)
    if (token.size() > 1 && token[0] == '$') {
        std::string col_id = token.substr(1);
        if (std::isdigit(static_cast<unsigned char>(col_id[0]))) {
            try {
                size_t idx = std::stoul(col_id);
                if (idx < fields.size()) return Value(fields[idx]);
            } catch (...) {}
        } else if (!header_map.empty()) {
            auto hit = header_map.find(col_id);
            if (hit != header_map.end() && hit->second < fields.size()) {
                return Value(fields[hit->second]);
            }
        }
        return Value("");
    }

    // Structured Object Path (.user.name, .items[0].id)
    if (!token.empty() && token[0] == '.') {
        std::string path = token.substr(1);
        for (size_t i = 0; i < path.size(); ++i) {
            if (path[i] == '[') path[i] = '.';
            if (path[i] == ']') path.erase(i--, 1);
        }

        std::istringstream ss(path);
        std::string segment;
        Value curr = parsed_obj;
        while (std::getline(ss, segment, '.')) {
            if (segment.empty()) continue;
            curr = curr.get_path(segment);
        }
        return curr;
    }

    // Direct header lookup fallback (.Header or Header)
    if (!header_map.empty()) {
        auto hit = header_map.find(token);
        if (hit != header_map.end() && hit->second < fields.size()) {
            return Value(fields[hit->second]);
        }
    }

    // Literal number
    try {
        size_t p;
        double d = std::stod(token, &p);
        if (p == token.size()) return Value(d);
    } catch (...) {}

    // Literal String (without quotes)
    if (token.size() >= 2 && ((token.front() == '"' && token.back() == '"') || (token.front() == '\'' && token.back() == '\''))) {
        return Value(token.substr(1, token.size() - 2));
    }

    return Value(token);
}
