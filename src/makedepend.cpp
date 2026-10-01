/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <stack>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

// ============================================================================
// Version & Metadata
// ============================================================================
constexpr const char* PROGRAM_NAME = "makedepend";
constexpr const char* PROGRAM_VERSION = "3.1.0";
constexpr const char* DEFAULT_DELIMITER = "# DO NOT DELETE THIS LINE -- make depend depends on it.";

// ============================================================================
// Path Helper & Case-Insensitive Comparator
// ============================================================================
struct PathHelper {
    static string to_lower(const string& p) {
        string s = p;
        transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
            return tolower(c);
        });
        return s;
    }

    static string normalize(const string& p) {
        fs::path path(p);
        string s = path.lexically_normal().string();
        replace(s.begin(), s.end(), '\\', '/');
        return s;
    }

    static string lower_key(const string& p) {
        return to_lower(normalize(p));
    }
};

struct CaseInsensitivePathCompare {
    bool operator()(const string& a, const string& b) const {
        return PathHelper::lower_key(a) < PathHelper::lower_key(b);
    }
};

// ============================================================================
// Comment & Raw String Cleanup Engine
// ============================================================================
struct LineCleanerState {
    bool inBlockComment = false;
    bool inRawString = false;
    string rawDelimiter;
};

string clean_line(const string& line, LineCleanerState& state) {
    string result;
    bool inString = false;
    bool inChar = false;

    for (size_t i = 0; i < line.length(); ++i) {
        char c = line[i];
        char nextC = (i + 1 < line.length()) ? line[i + 1] : '\0';

        // 1. Inside C++11 Raw String Literal R"delim(...)delim"
        if (state.inRawString) {
            result += c;
            string endMatch = ")" + state.rawDelimiter + "\"";
            if (result.length() >= endMatch.length() && 
                result.compare(result.length() - endMatch.length(), endMatch.length(), endMatch) == 0) {
                state.inRawString = false;
                state.rawDelimiter.clear();
            }
            continue;
        }

        // 2. Inside Block Comment
        if (state.inBlockComment) {
            if (c == '*' && nextC == '/') {
                state.inBlockComment = false;
                i++; // Skip '/'
            }
            continue;
        }

        // 3. Inside Double-Quoted String
        if (inString) {
            result += c;
            if (c == '\\' && nextC != '\0') {
                result += nextC;
                i++; // Skip escaped char
            } else if (c == '"') {
                inString = false;
            }
            continue;
        }

        // 4. Inside Single-Quoted Char Literal
        if (inChar) {
            result += c;
            if (c == '\\' && nextC != '\0') {
                result += nextC;
                i++; // Skip escaped char
            } else if (c == '\'') {
                inChar = false;
            }
            continue;
        }

        // 5. Check Raw String Start: R"delim(
        if (c == 'R' && nextC == '"') {
            size_t parenPos = line.find('(', i + 2);
            if (parenPos != string::npos) {
                state.inRawString = true;
                state.rawDelimiter = line.substr(i + 2, parenPos - (i + 2));
                result += line.substr(i, parenPos - i + 1);
                i = parenPos;
                continue;
            }
        }

        // 6. Check Block Comment Start
        if (c == '/' && nextC == '*') {
            state.inBlockComment = true;
            i++;
            continue;
        }

        // 7. Check Line Comment Start
        if (c == '/' && nextC == '/') {
            break; // Ignore rest of line
        }

        // 8. Check Standard String / Char Starts
        if (c == '"') inString = true;
        else if (c == '\'') inChar = true;

        result += c;
    }
    return result;
}

// ============================================================================
// Macro Definition Data Structure
// ============================================================================
struct MacroDef {
    string name;
    bool isFunctionLike = false;
    vector<string> params;
    bool isVariadic = false;
    string body;
};

// ============================================================================
// Preprocessor Expression Evaluator & Macro Processor
// ============================================================================
class ExpressionEvaluator {
public:
    static long long evaluate(string expr, const map<string, MacroDef>& macros) {
        expr = process_defined(expr, macros);
        expr = expand_macros(expr, macros);

        try {
            ExpressionEvaluator eval(expr);
            return eval.parse_expr();
        } catch (...) {
            return 0; // Fallback for unparseable preprocessor expressions
        }
    }

    // Full macro expansion supporting function-like macros, #, ##, and __VA_ARGS__
    static string expand_macros(string text, const map<string, MacroDef>& macros, int depth = 0) {
        if (depth > 10) return text;
        bool changed = false;
        string result;
        size_t i = 0;

        while (i < text.length()) {
            // Preserve Numeric Literals (Hex 0x..., Binary 0b..., Octal, Decimal)
            if (isdigit(static_cast<unsigned char>(text[i]))) {
                size_t start = i;
                if (text[i] == '0' && i + 1 < text.length() && (text[i + 1] == 'x' || text[i + 1] == 'X')) {
                    i += 2;
                    while (i < text.length() && isxdigit(static_cast<unsigned char>(text[i]))) i++;
                } else if (text[i] == '0' && i + 1 < text.length() && (text[i + 1] == 'b' || text[i + 1] == 'B')) {
                    i += 2;
                    while (i < text.length() && (text[i] == '0' || text[i] == '1')) i++;
                } else {
                    while (i < text.length() && isdigit(static_cast<unsigned char>(text[i]))) i++;
                }
                while (i < text.length() && (text[i] == 'u' || text[i] == 'U' || text[i] == 'l' || text[i] == 'L')) i++;
                result += text.substr(start, i - start);
            }
            // Process Identifiers
            else if (isalpha(static_cast<unsigned char>(text[i])) || text[i] == '_') {
                size_t start = i;
                while (i < text.length() && (isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_')) i++;
                string token = text.substr(start, i - start);

                auto it = macros.find(token);
                if (it != macros.end()) {
                    const MacroDef& def = it->second;
                    
                    if (!def.isFunctionLike) {
                        result += "(" + def.body + ")";
                        changed = true;
                    } else {
                        // Look for '(' skipping whitespace
                        size_t p = i;
                        while (p < text.length() && isspace(static_cast<unsigned char>(text[p]))) p++;
                        
                        if (p < text.length() && text[p] == '(') {
                            size_t arg_start = p + 1;
                            int paren_depth = 1;
                            vector<string> args;
                            string cur_arg;
                            size_t curr = arg_start;

                            while (curr < text.length() && paren_depth > 0) {
                                char c = text[curr];
                                if (c == '(') paren_depth++;
                                else if (c == ')') paren_depth--;

                                if ((c == ',' && paren_depth == 1) || (paren_depth == 0)) {
                                    size_t s = cur_arg.find_first_not_of(" \t\n\r");
                                    size_t e = cur_arg.find_last_not_of(" \t\n\r");
                                    args.push_back((s != string::npos) ? cur_arg.substr(s, e - s + 1) : "");
                                    cur_arg.clear();
                                } else {
                                    cur_arg += c;
                                }
                                curr++;
                            }

                            if (paren_depth == 0) {
                                i = curr; // Advance position past ')'
                                string expanded = def.body;

                                for (size_t k = 0; k < def.params.size(); ++k) {
                                    string param = def.params[k];
                                    string val = "";
                                    
                                    if (def.isVariadic && param == "__VA_ARGS__") {
                                        for (size_t v = k; v < args.size(); ++v) {
                                            if (v > k) val += ", ";
                                            val += args[v];
                                        }
                                    } else if (k < args.size()) {
                                        val = args[k];
                                    }

                                    // Stringification operator #param
                                    string hash_param = "#" + param;
                                    size_t hpos = 0;
                                    while ((hpos = expanded.find(hash_param, hpos)) != string::npos) {
                                        expanded.replace(hpos, hash_param.length(), "\"" + val + "\"");
                                        hpos += val.length() + 2;
                                    }

                                    // Parameter substitution
                                    size_t ppos = 0;
                                    while ((ppos = expanded.find(param, ppos)) != string::npos) {
                                        bool valid_prev = (ppos == 0 || (!isalnum(static_cast<unsigned char>(expanded[ppos - 1])) && expanded[ppos - 1] != '_'));
                                        bool valid_next = (ppos + param.length() >= expanded.length() || (!isalnum(static_cast<unsigned char>(expanded[ppos + param.length()])) && expanded[ppos + param.length()] != '_'));
                                        if (valid_prev && valid_next) {
                                            expanded.replace(ppos, param.length(), val);
                                            ppos += val.length();
                                        } else {
                                            ppos += param.length();
                                        }
                                    }
                                }

                                // Token Pasting operator ##
                                size_t concat_pos = 0;
                                while ((concat_pos = expanded.find("##")) != string::npos) {
                                    size_t left = concat_pos;
                                    while (left > 0 && isspace(static_cast<unsigned char>(expanded[left - 1]))) left--;
                                    size_t right = concat_pos + 2;
                                    while (right < expanded.length() && isspace(static_cast<unsigned char>(expanded[right]))) right++;
                                    expanded.replace(left, right - left, "");
                                }

                                result += "(" + expanded + ")";
                                changed = true;
                            } else {
                                result += token;
                            }
                        } else {
                            result += token;
                        }
                    }
                } else {
                    result += token;
                }
            } else {
                result += text[i++];
            }
        }

        if (changed) return expand_macros(result, macros, depth + 1);
        return text;
    }

private:
    string src;
    size_t pos = 0;

    explicit ExpressionEvaluator(string s) : src(move(s)) {}

    void skip_whitespace() {
        while (pos < src.length() && isspace(static_cast<unsigned char>(src[pos]))) {
            pos++;
        }
    }

    char peek() {
        skip_whitespace();
        return (pos < src.length()) ? src[pos] : '\0';
    }

    char get() {
        skip_whitespace();
        return (pos < src.length()) ? src[pos++] : '\0';
    }

    static string process_defined(const string& input, const map<string, MacroDef>& macros) {
        string result;
        size_t i = 0;
        while (i < input.length()) {
            bool is_prev_alpha = (i > 0 && (isalnum(static_cast<unsigned char>(input[i - 1])) || input[i - 1] == '_'));

            if (!is_prev_alpha && input.substr(i, 7) == "defined" && 
                (i + 7 >= input.length() || (!isalnum(static_cast<unsigned char>(input[i + 7])) && input[i + 7] != '_'))) {
                
                i += 7;
                while (i < input.length() && isspace(static_cast<unsigned char>(input[i]))) i++;
                bool has_paren = (i < input.length() && input[i] == '(');
                if (has_paren) i++;

                while (i < input.length() && isspace(static_cast<unsigned char>(input[i]))) i++;
                size_t start = i;
                while (i < input.length() && (isalnum(static_cast<unsigned char>(input[i])) || input[i] == '_')) i++;
                string macro_name = input.substr(start, i - start);

                while (i < input.length() && isspace(static_cast<unsigned char>(input[i]))) i++;
                if (has_paren && i < input.length() && input[i] == ')') i++;

                result += (macros.find(macro_name) != macros.end()) ? "1" : "0";
            } else {
                result += input[i++];
            }
        }
        return result;
    }

    long long parse_primary() {
        skip_whitespace();
        char c = peek();
        if (c == '(') {
            get();
            long long val = parse_expr();
            if (peek() == ')') get();
            return val;
        }
        if (c == '!' || c == '~' || c == '+' || c == '-') {
            get();
            long long val = parse_primary();
            if (c == '!') return !val;
            if (c == '~') return ~val;
            if (c == '+') return +val;
            if (c == '-') return -val;
        }
        if (isdigit(static_cast<unsigned char>(c))) {
            size_t start = pos;
            if (c == '0' && (pos + 1 < src.length()) && (src[pos + 1] == 'x' || src[pos + 1] == 'X')) {
                pos += 2;
                while (pos < src.length() && isxdigit(static_cast<unsigned char>(src[pos]))) pos++;
            } else {
                while (pos < src.length() && isdigit(static_cast<unsigned char>(src[pos]))) pos++;
            }
            while (pos < src.length() && (src[pos] == 'u' || src[pos] == 'U' || src[pos] == 'l' || src[pos] == 'L')) pos++;
            string num_str = src.substr(start, pos - start);
            try {
                unsigned long long unsigned_val = strtoull(num_str.c_str(), nullptr, 0);
                return static_cast<long long>(unsigned_val);
            } catch (...) {
                return 0;
            }
        }
        return 0;
    }

    long long parse_mul() {
        long long left = parse_primary();
        while (true) {
            char c = peek();
            if (c == '*') { get(); left *= parse_primary(); }
            else if (c == '/') { get(); long long r = parse_primary(); left = r ? (left / r) : 0; }
            else if (c == '%') { get(); long long r = parse_primary(); left = r ? (left % r) : 0; }
            else break;
        }
        return left;
    }

    long long parse_add() {
        long long left = parse_mul();
        while (true) {
            char c = peek();
            if (c == '+') { get(); left += parse_mul(); }
            else if (c == '-') { get(); left -= parse_mul(); }
            else break;
        }
        return left;
    }

    long long parse_shift() {
        long long left = parse_add();
        while (true) {
            skip_whitespace();
            if (pos + 1 < src.length() && src[pos] == '<' && src[pos + 1] == '<') { pos += 2; left <<= parse_add(); }
            else if (pos + 1 < src.length() && src[pos] == '>' && src[pos + 1] == '>') { pos += 2; left >>= parse_add(); }
            else break;
        }
        return left;
    }

    long long parse_relational() {
        long long left = parse_shift();
        while (true) {
            skip_whitespace();
            if (pos + 1 < src.length() && src[pos] == '<' && src[pos + 1] == '=') { pos += 2; left = (left <= parse_shift()); }
            else if (pos + 1 < src.length() && src[pos] == '>' && src[pos + 1] == '=') { pos += 2; left = (left >= parse_shift()); }
            else if (peek() == '<') { get(); left = (left < parse_shift()); }
            else if (peek() == '>') { get(); left = (left > parse_shift()); }
            else break;
        }
        return left;
    }

    long long parse_equality() {
        long long left = parse_relational();
        while (true) {
            skip_whitespace();
            if (pos + 1 < src.length() && src[pos] == '=' && src[pos + 1] == '=') { pos += 2; left = (left == parse_relational()); }
            else if (pos + 1 < src.length() && src[pos] == '!' && src[pos + 1] == '=') { pos += 2; left = (left != parse_relational()); }
            else break;
        }
        return left;
    }

    long long parse_bit_and() {
        long long left = parse_equality();
        while (peek() == '&' && (pos + 1 >= src.length() || src[pos + 1] != '&')) {
            get(); left &= parse_equality();
        }
        return left;
    }

    long long parse_bit_xor() {
        long long left = parse_bit_and();
        while (peek() == '^') { get(); left ^= parse_bit_and(); }
        return left;
    }

    long long parse_bit_or() {
        long long left = parse_bit_xor();
        while (peek() == '|' && (pos + 1 >= src.length() || src[pos + 1] != '|')) {
            get(); left |= parse_bit_xor();
        }
        return left;
    }

    long long parse_log_and() {
        long long left = parse_bit_or();
        while (pos + 1 < src.length() && src[pos] == '&' && src[pos + 1] == '&') {
            pos += 2;
            long long right = parse_bit_or();
            left = (left && right);
        }
        return left;
    }

    long long parse_log_or() {
        long long left = parse_log_and();
        while (pos + 1 < src.length() && src[pos] == '|' && src[pos + 1] == '|') {
            pos += 2;
            long long right = parse_log_and();
            left = (left || right);
        }
        return left;
    }

    long long parse_expr() {
        long long condition = parse_log_or();
        if (peek() == '?') {
            get();
            long long true_val = parse_expr();
            if (peek() == ':') get();
            long long false_val = parse_expr();
            return condition ? true_val : false_val;
        }
        return condition;
    }
};

// ============================================================================
// Dependency Generator Context
// ============================================================================
struct CondBranch {
    bool active = true;
    bool parentActive = true;
    bool branchTaken = false;
};

class DependencyParser {
public:
    vector<string> includeDirs;
    map<string, MacroDef> macros;
    set<string> pragmaOnceFiles; // Normalized lower-case paths
    bool verbose = false;

    DependencyParser() {
        setup_msvc_defaults("1930"); // Default MSVC 2022
    }

    void setup_msvc_defaults(const string& msvc_ver) {
        add_object_macro("_WIN32", "1");
        add_object_macro("_WIN64", "1");
        add_object_macro("_MSC_VER", msvc_ver);
        add_object_macro("_MSC_FULL_VER", msvc_ver + "00000");
        add_object_macro("_MSVC_LANG", "202002L");
        add_object_macro("__cplusplus", "202002L");
        add_object_macro("_M_AMD64", "100");
        add_object_macro("_M_X64", "100");
        add_object_macro("_INTELLISENSE", "1");
    }

    void setup_gcc_defaults(const string& gcc_ver = "13") {
        macros.clear();
        // Parse version string (major[.minor[.patch]])
        int major = 13, minor = 2, patch = 0;
        sscanf(gcc_ver.c_str(), "%d.%d.%d", &major, &minor, &patch);
        add_object_macro("__GNUC__",           to_string(major));
        add_object_macro("__GNUC_MINOR__",     to_string(minor));
        add_object_macro("__GNUC_PATCHLEVEL__",to_string(patch));
        add_object_macro("__GNUG__",           to_string(major));
        add_object_macro("__cplusplus",        "202002L");
        add_object_macro("__x86_64__",         "1");
        add_object_macro("__LP64__",           "1");
        add_object_macro("__SIZE_TYPE__",      "long unsigned int");
        add_object_macro("__VERSION__",        "\"GCC " + gcc_ver + "\"");
        // Platform — leave _WIN32 if on Windows, otherwise define unix
#ifdef _WIN32
        add_object_macro("_WIN32", "1");
        add_object_macro("_WIN64", "1");
        add_object_macro("__MINGW32__", "1");
        add_object_macro("__MINGW64__", "1");
#else
        add_object_macro("__unix__",  "1");
        add_object_macro("__linux__", "1");
        add_object_macro("linux",     "1");
#endif
    }

    void setup_clang_defaults(const string& clang_ver = "17") {
        macros.clear();
        int major = 17, minor = 0, patch = 0;
        sscanf(clang_ver.c_str(), "%d.%d.%d", &major, &minor, &patch);
        add_object_macro("__clang__",              "1");
        add_object_macro("__clang_major__",        to_string(major));
        add_object_macro("__clang_minor__",        to_string(minor));
        add_object_macro("__clang_patchlevel__",   to_string(patch));
        // Clang exposes GCC-compatible version macros
        add_object_macro("__GNUC__",           "4");
        add_object_macro("__GNUC_MINOR__",     "2");
        add_object_macro("__GNUC_PATCHLEVEL__","1");
        add_object_macro("__cplusplus",        "202002L");
        add_object_macro("__x86_64__",         "1");
        add_object_macro("__LP64__",           "1");
        add_object_macro("__SIZE_TYPE__",      "long unsigned int");
        add_object_macro("__VERSION__",        "\"Clang " + clang_ver + "\"");
#ifdef _WIN32
        add_object_macro("_WIN32", "1");
        add_object_macro("_WIN64", "1");
#else
        add_object_macro("__unix__",  "1");
        add_object_macro("__linux__", "1");
#endif
    }

    void add_object_macro(const string& name, const string& body) {
        MacroDef def;
        def.name = name;
        def.isFunctionLike = false;
        def.body = body;
        macros[name] = def;
    }

    void load_env_includes() {
        const char* env_inc = getenv("INCLUDE");
        if (env_inc) {
            stringstream ss(env_inc);
            string item;
            while (getline(ss, item, ';')) {
                if (!item.empty()) {
                    includeDirs.push_back(item);
                }
            }
        }
    }

    // Probes a GCC-compatible compiler with -v to extract system include search paths.
    void probe_compiler_includes(const string& compiler) {
        // Read colon/semicolon-separated env vars first (no subprocess needed).
        for (const char* var : {"CPATH", "CPLUS_INCLUDE_PATH", "C_INCLUDE_PATH"}) {
            const char* v = getenv(var);
            if (!v) continue;
            stringstream ss(v);
            string item;
            char sep = ':'; // POSIX default
#ifdef _WIN32
            // On Windows these vars may use ';'
            if (string(v).find(';') != string::npos) sep = ';';
#endif
            while (getline(ss, item, sep))
                if (!item.empty()) includeDirs.push_back(item);
        }

        // Run the compiler with -v and parse the include search list from stderr.
        string cmd = compiler + " -xc++ -E -v -";
#ifdef _WIN32
        cmd += " < nul 2>&1";
#else
        cmd += " < /dev/null 2>&1";
#endif
        FILE* fp = _popen(cmd.c_str(), "r");
        if (!fp) return;
        char line[1024];
        bool in_includes = false;
        while (fgets(line, sizeof(line), fp)) {
            string s = line;
            // Trim trailing whitespace
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
            if (s.find("#include <...> search starts here:") != string::npos) { in_includes = true; continue; }
            if (s.find("End of search list.") != string::npos) { in_includes = false; continue; }
            if (in_includes) {
                // Lines are indented with a space
                size_t start = s.find_first_not_of(" \t");
                if (start != string::npos) includeDirs.push_back(s.substr(start));
            }
        }
        _pclose(fp);
    }

    void load_gcc_includes()   { probe_compiler_includes("g++");   }
    void load_clang_includes() { probe_compiler_includes("clang++"); }

    set<string, CaseInsensitivePathCompare> find_dependencies(const string& filepath) {
        set<string, CaseInsensitivePathCompare> dependencies;
        set<string> visitedFiles;
        map<string, MacroDef> localMacros = macros;
        
        parse_file(filepath, dependencies, visitedFiles, localMacros);
        return dependencies;
    }

private:
    string resolve_include(const string& header, const string& currentDir, bool isSystem) {
        if (!isSystem) {
            fs::path localPath = fs::path(currentDir) / header;
            if (fs::exists(localPath)) {
                return PathHelper::normalize(localPath.string());
            }
        }

        for (const auto& dir : includeDirs) {
            fs::path candidate = fs::path(dir) / header;
            if (fs::exists(candidate)) {
                return PathHelper::normalize(candidate.string());
            }
        }
        return "";
    }

    void parse_define(const string& line, map<string, MacroDef>& currentMacros) {
        stringstream ss(line);
        string rest;
        getline(ss, rest);
        size_t start = rest.find_first_not_of(" \t");
        if (start == string::npos) return;
        rest = rest.substr(start);

        size_t name_end = 0;
        while (name_end < rest.length() && (isalnum(static_cast<unsigned char>(rest[name_end])) || rest[name_end] == '_')) {
            name_end++;
        }
        if (name_end == 0) return;
        string macro_name = rest.substr(0, name_end);

        MacroDef def;
        def.name = macro_name;

        // Function-like macro: '(' must immediately follow macro_name
        if (name_end < rest.length() && rest[name_end] == '(') {
            def.isFunctionLike = true;
            size_t paren_close = rest.find(')', name_end);
            if (paren_close != string::npos) {
                string param_str = rest.substr(name_end + 1, paren_close - (name_end + 1));
                stringstream pss(param_str);
                string p;
                while (getline(pss, p, ',')) {
                    size_t ps = p.find_first_not_of(" \t");
                    size_t pe = p.find_last_not_of(" \t");
                    if (ps != string::npos && pe != string::npos) {
                        string param_name = p.substr(ps, pe - ps + 1);
                        if (param_name == "...") {
                            def.isVariadic = true;
                            def.params.push_back("__VA_ARGS__");
                        } else {
                            def.params.push_back(param_name);
                        }
                    }
                }
                size_t body_start = rest.find_first_not_of(" \t", paren_close + 1);
                def.body = (body_start != string::npos) ? rest.substr(body_start) : "";
            } else {
                def.body = "";
            }
        } else {
            def.isFunctionLike = false;
            size_t body_start = rest.find_first_not_of(" \t", name_end);
            def.body = (body_start != string::npos) ? rest.substr(body_start) : "1";
        }

        currentMacros[macro_name] = def;
    }

    void parse_file(const string& filepath, 
                    set<string, CaseInsensitivePathCompare>& dependencies, 
                    set<string>& visitedFiles,
                    map<string, MacroDef>& currentMacros) {
        
        string normPath = PathHelper::normalize(filepath);
        string lowerKey = PathHelper::lower_key(normPath);

        if (visitedFiles.count(lowerKey) || pragmaOnceFiles.count(lowerKey)) return;
        visitedFiles.insert(lowerKey);

        ifstream file(filepath);
        if (!file.is_open()) return;

        string currentDir = fs::path(filepath).parent_path().string();
        if (currentDir.empty()) currentDir = ".";

        vector<CondBranch> condStack;
        LineCleanerState cleanerState;

        auto is_active = [&]() {
            for (const auto& branch : condStack) {
                if (!branch.active || !branch.parentActive) return false;
            }
            return true;
        };

        string line;
        while (getline(file, line)) {
            // Handle line continuation backslash '\'
            while (!line.empty() && line.back() == '\\') {
                line.pop_back();
                string nextLine;
                if (getline(file, nextLine)) {
                    line += nextLine;
                }
            }

            // Strip comments and preserve C++11 raw strings
            string cleanLine = clean_line(line, cleanerState);

            size_t start = cleanLine.find_first_not_of(" \t");
            if (start == string::npos) continue;
            cleanLine = cleanLine.substr(start);

            if (cleanLine.empty() || cleanLine[0] != '#') continue;

            stringstream ss(cleanLine.substr(1));
            string directive;
            ss >> directive;

            if (directive == "pragma" && is_active()) {
                string sub;
                ss >> sub;
                if (sub == "once") {
                    pragmaOnceFiles.insert(lowerKey);
                }
            }
            else if (directive == "ifdef" || directive == "ifndef" || directive == "if") {
                bool parentAct = is_active();
                bool act = false;
                string rest;
                getline(ss, rest);

                if (parentAct) {
                    if (directive == "ifdef") {
                        stringstream rss(rest);
                        string sym; rss >> sym;
                        act = (currentMacros.find(sym) != currentMacros.end());
                    } else if (directive == "ifndef") {
                        stringstream rss(rest);
                        string sym; rss >> sym;
                        act = (currentMacros.find(sym) == currentMacros.end());
                    } else {
                        act = (ExpressionEvaluator::evaluate(rest, currentMacros) != 0);
                    }
                }
                condStack.push_back({ act, parentAct, act });
            } 
            else if (directive == "elif") {
                string rest;
                getline(ss, rest);
                if (!condStack.empty()) {
                    auto& top = condStack.back();
                    if (top.parentActive && !top.branchTaken) {
                        top.active = (ExpressionEvaluator::evaluate(rest, currentMacros) != 0);
                        if (top.active) top.branchTaken = true;
                    } else {
                        top.active = false;
                    }
                }
            } 
            else if (directive == "else") {
                if (!condStack.empty()) {
                    auto& top = condStack.back();
                    if (top.parentActive && !top.branchTaken) {
                        top.active = true;
                        top.branchTaken = true;
                    } else {
                        top.active = false;
                    }
                }
            } 
            else if (directive == "endif") {
                if (!condStack.empty()) {
                    condStack.pop_back();
                }
            } 
            else if (directive == "define" && is_active()) {
                string rest;
                getline(ss, rest);
                parse_define(rest, currentMacros);
            } 
            else if (directive == "undef" && is_active()) {
                string name;
                ss >> name;
                currentMacros.erase(name);
            } 
            else if (directive == "error" && is_active()) {
                if (verbose) {
                    cerr << "makedepend: Warning: #error encountered in active section of " 
                         << filepath << endl;
                }
            } 
            else if (directive == "include" && is_active()) {
                string rest;
                getline(ss, rest);

                // Handle computed includes (#include MACRO)
                size_t firstSymbol = rest.find_first_of("\"<");
                if (firstSymbol == string::npos) {
                    rest = ExpressionEvaluator::expand_macros(rest, currentMacros);
                    firstSymbol = rest.find_first_of("\"<");
                }

                if (firstSymbol != string::npos) {
                    char closeChar = (rest[firstSymbol] == '"') ? '"' : '>';
                    size_t lastSymbol = rest.find_first_of(closeChar, firstSymbol + 1);
                    if (lastSymbol != string::npos) {
                        string header = rest.substr(firstSymbol + 1, lastSymbol - firstSymbol - 1);
                        bool isSystem = (rest[firstSymbol] == '<');

                        string resolvedPath = resolve_include(header, currentDir, isSystem);
                        if (!resolvedPath.empty()) {
                            dependencies.insert(resolvedPath);
                            parse_file(resolvedPath, dependencies, visitedFiles, currentMacros);
                        }
                    }
                }
            }
        }
    }
};

// ============================================================================
// Makefile Updater
// ============================================================================
void update_makefile(const string& makefilePath, 
                     const string& delimiter, 
                     bool appendOnly, 
                     const map<string, set<string, CaseInsensitivePathCompare>>& allDeps) {
    
    vector<string> lines;
    bool foundDelimiter = false;

    if (fs::exists(makefilePath)) {
        ifstream in(makefilePath);
        string line;
        while (getline(in, line)) {
            if (line == delimiter) {
                foundDelimiter = true;
                if (!appendOnly) break; // Replace everything after delimiter
            }
            lines.push_back(line);
        }
    }

    ofstream out(makefilePath, ios::trunc);
    for (const auto& line : lines) {
        out << line << "\n";
    }

    if (!foundDelimiter) {
        out << "\n" << delimiter << "\n";
    }

    for (const auto& [target, deps] : allDeps) {
        if (deps.empty()) continue;
        
        string line = target + ":";
        for (const auto& dep : deps) {
            if (line.length() + dep.length() + 1 > 78) {
                out << line << " \\\n";
                line = "  " + dep;
            } else {
                line += " " + dep;
            }
        }
        out << line << "\n";
    }
}

// ============================================================================
// Help and Version Output
// ============================================================================
void print_version() {
    cout << PROGRAM_NAME << " version " << PROGRAM_VERSION << " (MSVC / GCC / Clang)\n"
         << "Dependency generator for MSVC, GCC, and Clang codebases.\n";
}

void print_help() {
    std::cout << R"(makedepend(1)           CrossShell for UNIX Reference Manual           makedepend(1)

    NAME
        makedepend - create dependencies in makefiles

    SYNOPSIS
        makedepend [OPTIONS] [--] SOURCEFILE...

    DESCRIPTION
        Parses C/C++ source code, evaluates preprocessor macros and conditional
        directives (#if, #ifdef, #elif, defined), and appends object-to-header
        dependencies to a Makefile.

    OPTIONS
        -I <dir>
            Add directory DIR to header search path.

        -D <symbol>[=<val>]
            Define preprocessor symbol (default value: 1).

        -U <symbol>
            Undefine preprocessor symbol.

        -f <file>
            Specify target Makefile (default: Makefile).

        -o <suffix>
            Set object file suffix (default: .obj).

        -p <prefix>
            Set object file prefix path.

        -s <string>
            Specify dependency delimiter string.

        -a
            Append dependencies instead of replacing existing section.

        -v, --verbose
            Enable verbose output during dependency scanning.

        --compiler <name>
            Select compiler preset: msvc (default), gcc, or clang.

        --detect-cl
            Auto-detect MSVC include paths from %INCLUDE%.

        --msvc-ver <ver>
            Set MSVC version simulation (default: 1930 for MSVC 2022).

        --detect-gcc
            Probe g++ to discover GCC system include paths.

        --gcc-ver <ver>
            Set GCC version for macro simulation (default: 13).

        --detect-clang
            Probe clang++ to discover Clang system include paths.

        --clang-ver <ver>
            Set Clang version for macro simulation (default: 17).

        -h, --help
            Display this reference manual and exit.

        -V, --version
            Display version information and exit.

    EXAMPLES
        makedepend -I include -f Makefile src/*.cpp
            Generate dependencies for all C++ source files and update Makefile.

        makedepend --compiler gcc -I /usr/include -f Makefile main.c
            Generate dependencies using GCC configuration.

    EXIT STATUS
        0
            Success.

        1
            An error occurred (e.g., missing arguments or file not found).

    CrossShell for UNIX                                                    makedepend(1)
)";
}

int main(int argc, char* argv[]) {
    DependencyParser parser;
    string makefilePath = "Makefile";
    string delimiter = DEFAULT_DELIMITER;
    string objSuffix = ".obj";
    string objPrefix = "";
    bool appendOnly = false;
    bool autoDetectCL = false;
    bool autoDetectGCC = false;
    bool autoDetectClang = false;
    vector<string> sourceFiles;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];

        if (arg == "-h" || arg == "--help" || arg == "/?") {
            print_help();
            return 0;
        } else if (arg == "-V" || arg == "--version") {
            print_version();
            return 0;
        } else if (arg == "-v" || arg == "--verbose") {
            parser.verbose = true;
        } else if (arg == "--msvc-ver" && i + 1 < argc) {
            parser.setup_msvc_defaults(argv[++i]);
        } else if (arg == "--compiler" && i + 1 < argc) {
            string compiler = argv[++i];
            if (compiler == "gcc") {
                parser.setup_gcc_defaults();
                autoDetectCL = false; autoDetectGCC = true;
                objSuffix = ".o";
            } else if (compiler == "clang") {
                parser.setup_clang_defaults();
                autoDetectCL = false; autoDetectClang = true;
                objSuffix = ".o";
            } else if (compiler == "msvc") {
                parser.setup_msvc_defaults("1930");
                autoDetectCL = true;
            } else {
                cerr << "makedepend: Unknown compiler '" << compiler << "'. Use msvc, gcc, or clang.\n";
                return 1;
            }
        } else if (arg == "--detect-gcc") {
            autoDetectGCC   = true;
            autoDetectCL    = false;
        } else if (arg == "--gcc-ver" && i + 1 < argc) {
            parser.setup_gcc_defaults(argv[++i]);
            autoDetectCL = false;
        } else if (arg == "--detect-clang") {
            autoDetectClang = true;
            autoDetectCL    = false;
        } else if (arg == "--clang-ver" && i + 1 < argc) {
            parser.setup_clang_defaults(argv[++i]);
            autoDetectCL = false;
        } else if (arg.rfind("-I", 0) == 0) {
            string dir = (arg.length() > 2) ? arg.substr(2) : argv[++i];
            parser.includeDirs.push_back(dir);
        } else if (arg.rfind("-D", 0) == 0) {
            string def = (arg.length() > 2) ? arg.substr(2) : argv[++i];
            size_t eq = def.find('=');
            if (eq != string::npos) {
                MacroDef md;
                md.name = def.substr(0, eq);
                md.isFunctionLike = false;
                md.body = def.substr(eq + 1);
                parser.macros[md.name] = md;
            } else {
                parser.add_object_macro(def, "1");
            }
        } else if (arg.rfind("-U", 0) == 0) {
            string sym = (arg.length() > 2) ? arg.substr(2) : argv[++i];
            parser.macros.erase(sym);
        } else if (arg.rfind("-f", 0) == 0) {
            makefilePath = (arg.length() > 2) ? arg.substr(2) : argv[++i];
        } else if (arg.rfind("-o", 0) == 0) {
            objSuffix = (arg.length() > 2) ? arg.substr(2) : argv[++i];
        } else if (arg.rfind("-p", 0) == 0) {
            objPrefix = (arg.length() > 2) ? arg.substr(2) : argv[++i];
        } else if (arg.rfind("-s", 0) == 0) {
            delimiter = (arg.length() > 2) ? arg.substr(2) : argv[++i];
        } else if (arg == "-a") {
            appendOnly = true;
        } else if (arg == "--") {
            while (++i < argc) sourceFiles.push_back(argv[i]);
        } else if (arg[0] != '-') {
            sourceFiles.push_back(arg);
        } else {
            cerr << "makedepend: Unknown option '" << arg << "'\n";
            return 1;
        }
    }

    if (autoDetectCL)    parser.load_env_includes();
    if (autoDetectGCC)   parser.load_gcc_includes();
    if (autoDetectClang) parser.load_clang_includes();

    if (sourceFiles.empty()) {
        cerr << "makedepend: No source files specified.\n";
        return 1;
    }

    map<string, set<string, CaseInsensitivePathCompare>> allDependencies;

    for (const auto& src : sourceFiles) {
        if (!fs::exists(src)) {
            cerr << "makedepend: Skipping non-existent file: " << src << "\n";
            continue;
        }

        fs::path p(src);
        string stem = p.stem().string();
        string target = objPrefix + stem + objSuffix;

        if (parser.verbose) {
            cout << "makedepend: Parsing " << src << " -> " << target << "\n";
        }

        allDependencies[target] = parser.find_dependencies(src);
    }

    update_makefile(makefilePath, delimiter, appendOnly, allDependencies);

    if (parser.verbose) {
        cout << "makedepend: Successfully updated " << makefilePath << "\n";
    }

    return 0;
}