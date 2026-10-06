#include "engine.hpp"

// ============================================================================
// Comment & Raw String Cleanup Engine
// ============================================================================
std::string clean_line(const std::string& line, LineCleanerState& state) {
    std::string result;
    bool inString = false;
    bool inChar = false;

    for (size_t i = 0; i < line.length(); ++i) {
        char c = line[i];
        char nextC = (i + 1 < line.length()) ? line[i + 1] : '\0';

        // 1. Inside C++11 Raw String Literal R"delim(...)delim"
        if (state.inRawString) {
            result += c;
            std::string endMatch = ")" + state.rawDelimiter + "\"";
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
            if (parenPos != std::string::npos) {
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
// Preprocessor Expression Evaluator & Macro Processor
// ============================================================================
ExpressionEvaluator::ExpressionEvaluator(std::string s) : src(std::move(s)) {}

void ExpressionEvaluator::skip_whitespace() {
    while (pos < src.length() && isspace(static_cast<unsigned char>(src[pos]))) {
        pos++;
    }
}

char ExpressionEvaluator::peek() {
    skip_whitespace();
    return (pos < src.length()) ? src[pos] : '\0';
}

char ExpressionEvaluator::get() {
    skip_whitespace();
    return (pos < src.length()) ? src[pos++] : '\0';
}

long long ExpressionEvaluator::evaluate(std::string expr, const std::map<std::string, MacroDef>& macros) {
    expr = process_defined(expr, macros);
    expr = expand_macros(expr, macros);

    try {
        ExpressionEvaluator eval(expr);
        return eval.parse_expr();
    } catch (...) {
        return 0; // Fallback for unparseable preprocessor expressions
    }
}

std::string ExpressionEvaluator::expand_macros(std::string text, const std::map<std::string, MacroDef>& macros, int depth) {
    if (depth > 10) return text;
    bool changed = false;
    std::string result;
    size_t i = 0;

    while (i < text.length()) {
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
        } else if (isalpha(static_cast<unsigned char>(text[i])) || text[i] == '_') {
            size_t start = i;
            while (i < text.length() && (isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_')) i++;
            std::string token = text.substr(start, i - start);

            auto it = macros.find(token);
            if (it != macros.end()) {
                const MacroDef& def = it->second;
                
                if (!def.isFunctionLike) {
                    result += "(" + def.body + ")";
                    changed = true;
                } else {
                    size_t p = i;
                    while (p < text.length() && isspace(static_cast<unsigned char>(text[p]))) p++;
                    
                    if (p < text.length() && text[p] == '(') {
                        size_t arg_start = p + 1;
                        int paren_depth = 1;
                        std::vector<std::string> args;
                        std::string cur_arg;
                        size_t curr = arg_start;

                        while (curr < text.length() && paren_depth > 0) {
                            char c = text[curr];
                            if (c == '(') paren_depth++;
                            else if (c == ')') paren_depth--;

                            if ((c == ',' && paren_depth == 1) || (paren_depth == 0)) {
                                size_t s = cur_arg.find_first_not_of(" \t\n\r");
                                size_t e = cur_arg.find_last_not_of(" \t\n\r");
                                args.push_back((s != std::string::npos) ? cur_arg.substr(s, e - s + 1) : "");
                                cur_arg.clear();
                            } else {
                                cur_arg += c;
                            }
                            curr++;
                        }

                        if (paren_depth == 0) {
                            i = curr;
                            std::string expanded = def.body;

                            for (size_t k = 0; k < def.params.size(); ++k) {
                                std::string param = def.params[k];
                                std::string val = "";
                                
                                if (def.isVariadic && param == "__VA_ARGS__") {
                                    for (size_t v = k; v < args.size(); ++v) {
                                        if (v > k) val += ", ";
                                        val += args[v];
                                    }
                                } else if (k < args.size()) {
                                    val = args[k];
                                }

                                std::string hash_param = "#" + param;
                                size_t hpos = 0;
                                while ((hpos = expanded.find(hash_param, hpos)) != std::string::npos) {
                                    expanded.replace(hpos, hash_param.length(), "\"" + val + "\"");
                                    hpos += val.length() + 2;
                                }

                                size_t ppos = 0;
                                while ((ppos = expanded.find(param, ppos)) != std::string::npos) {
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

                            size_t concat_pos = 0;
                            while ((concat_pos = expanded.find("##")) != std::string::npos) {
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

std::string ExpressionEvaluator::process_defined(const std::string& input, const std::map<std::string, MacroDef>& macros) {
    std::string result;
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
            std::string macro_name = input.substr(start, i - start);

            while (i < input.length() && isspace(static_cast<unsigned char>(input[i]))) i++;
            if (has_paren && i < input.length() && input[i] == ')') i++;

            result += (macros.find(macro_name) != macros.end()) ? "1" : "0";
        } else {
            result += input[i++];
        }
    }
    return result;
}

long long ExpressionEvaluator::parse_primary() {
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
        std::string num_str = src.substr(start, pos - start);
        try {
            unsigned long long unsigned_val = strtoull(num_str.c_str(), nullptr, 0);
            return static_cast<long long>(unsigned_val);
        } catch (...) {
            return 0;
        }
    }
    return 0;
}

long long ExpressionEvaluator::parse_mul() {
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

long long ExpressionEvaluator::parse_add() {
    long long left = parse_mul();
    while (true) {
        char c = peek();
        if (c == '+') { get(); left += parse_mul(); }
        else if (c == '-') { get(); left -= parse_mul(); }
        else break;
    }
    return left;
}

long long ExpressionEvaluator::parse_shift() {
    long long left = parse_add();
    while (true) {
        skip_whitespace();
        if (pos + 1 < src.length() && src[pos] == '<' && src[pos + 1] == '<') { pos += 2; left <<= parse_add(); }
        else if (pos + 1 < src.length() && src[pos] == '>' && src[pos + 1] == '>') { pos += 2; left >>= parse_add(); }
        else break;
    }
    return left;
}

long long ExpressionEvaluator::parse_relational() {
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

long long ExpressionEvaluator::parse_equality() {
    long long left = parse_relational();
    while (true) {
        skip_whitespace();
        if (pos + 1 < src.length() && src[pos] == '=' && src[pos + 1] == '=') { pos += 2; left = (left == parse_relational()); }
        else if (pos + 1 < src.length() && src[pos] == '!' && src[pos + 1] == '=') { pos += 2; left = (left != parse_relational()); }
        else break;
    }
    return left;
}

long long ExpressionEvaluator::parse_bit_and() {
    long long left = parse_equality();
    while (peek() == '&' && (pos + 1 >= src.length() || src[pos + 1] != '&')) {
        get(); left &= parse_equality();
    }
    return left;
}

long long ExpressionEvaluator::parse_bit_xor() {
    long long left = parse_bit_and();
    while (peek() == '^') { get(); left ^= parse_bit_and(); }
    return left;
}

long long ExpressionEvaluator::parse_bit_or() {
    long long left = parse_bit_xor();
    while (peek() == '|' && (pos + 1 >= src.length() || src[pos + 1] != '|')) {
        get(); left |= parse_bit_xor();
    }
    return left;
}

long long ExpressionEvaluator::parse_log_and() {
    long long left = parse_bit_or();
    while (pos + 1 < src.length() && src[pos] == '&' && src[pos + 1] == '&') {
        pos += 2;
        long long right = parse_bit_or();
        left = (left && right);
    }
    return left;
}

long long ExpressionEvaluator::parse_log_or() {
    long long left = parse_log_and();
    while (pos + 1 < src.length() && src[pos] == '|' && src[pos + 1] == '|') {
        pos += 2;
        long long right = parse_log_and();
        left = (left || right);
    }
    return left;
}

long long ExpressionEvaluator::parse_expr() {
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

// ============================================================================
// Dependency Generator Context
// ============================================================================
DependencyParser::DependencyParser() {
    setup_msvc_defaults("1930");
}

void DependencyParser::setup_msvc_defaults(const std::string& msvc_ver) {
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

void DependencyParser::setup_gcc_defaults(const std::string& gcc_ver) {
    macros.clear();
    int major = 13, minor = 2, patch = 0;
    sscanf(gcc_ver.c_str(), "%d.%d.%d", &major, &minor, &patch);
    add_object_macro("__GNUC__",           std::to_string(major));
    add_object_macro("__GNUC_MINOR__",     std::to_string(minor));
    add_object_macro("__GNUC_PATCHLEVEL__",std::to_string(patch));
    add_object_macro("__GNUG__",           std::to_string(major));
    add_object_macro("__cplusplus",        "202002L");
    add_object_macro("__x86_64__",         "1");
    add_object_macro("__LP64__",           "1");
    add_object_macro("__SIZE_TYPE__",      "long unsigned int");
    add_object_macro("__VERSION__",        "\"GCC " + gcc_ver + "\"");
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

void DependencyParser::setup_clang_defaults(const std::string& clang_ver) {
    macros.clear();
    int major = 17, minor = 0, patch = 0;
    sscanf(clang_ver.c_str(), "%d.%d.%d", &major, &minor, &patch);
    add_object_macro("__clang__",              "1");
    add_object_macro("__clang_major__",        std::to_string(major));
    add_object_macro("__clang_minor__",        std::to_string(minor));
    add_object_macro("__clang_patchlevel__",   std::to_string(patch));
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

void DependencyParser::add_object_macro(const std::string& name, const std::string& body) {
    MacroDef def;
    def.name = name;
    def.isFunctionLike = false;
    def.body = body;
    macros[name] = def;
}

void DependencyParser::load_env_includes() {
    const char* env_inc = getenv("INCLUDE");
    if (env_inc) {
        std::stringstream ss(env_inc);
        std::string item;
        while (std::getline(ss, item, ';')) {
            if (!item.empty()) {
                includeDirs.push_back(item);
            }
        }
    }
}

void DependencyParser::probe_compiler_includes(const std::string& compiler) {
    for (const char* var : {"CPATH", "CPLUS_INCLUDE_PATH", "C_INCLUDE_PATH"}) {
        const char* v = getenv(var);
        if (!v) continue;
        std::stringstream ss(v);
        std::string item;
        char sep = ':';
#ifdef _WIN32
        if (std::string(v).find(';') != std::string::npos) sep = ';';
#endif
        while (std::getline(ss, item, sep)) {
            if (!item.empty()) includeDirs.push_back(item);
        }
    }

    std::string cmd = compiler + " -xc++ -E -v -";
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
        std::string s = line;
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
        if (s.find("#include <...> search starts here:") != std::string::npos) { in_includes = true; continue; }
        if (s.find("End of search list.") != std::string::npos) { in_includes = false; continue; }
        if (in_includes) {
            size_t start = s.find_first_not_of(" \t");
            if (start != std::string::npos) includeDirs.push_back(s.substr(start));
        }
    }
    _pclose(fp);
}

void DependencyParser::load_gcc_includes()   { probe_compiler_includes("g++"); }
void DependencyParser::load_clang_includes() { probe_compiler_includes("clang++"); }

std::set<std::string, CaseInsensitivePathCompare> DependencyParser::find_dependencies(const std::string& filepath) {
    std::set<std::string, CaseInsensitivePathCompare> dependencies;
    std::set<std::string> visitedFiles;
    std::map<std::string, MacroDef> localMacros = macros;
    
    parse_file(filepath, dependencies, visitedFiles, localMacros);
    return dependencies;
}

std::string DependencyParser::resolve_include(const std::string& header, const std::string& currentDir, bool isSystem) {
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

void DependencyParser::parse_define(const std::string& line, std::map<std::string, MacroDef>& currentMacros) {
    std::stringstream ss(line);
    std::string rest;
    std::getline(ss, rest);
    size_t start = rest.find_first_not_of(" \t");
    if (start == std::string::npos) return;
    rest = rest.substr(start);

    size_t name_end = 0;
    while (name_end < rest.length() && (isalnum(static_cast<unsigned char>(rest[name_end])) || rest[name_end] == '_')) {
        name_end++;
    }
    if (name_end == 0) return;
    std::string macro_name = rest.substr(0, name_end);

    MacroDef def;
    def.name = macro_name;

    if (name_end < rest.length() && rest[name_end] == '(') {
        def.isFunctionLike = true;
        size_t paren_close = rest.find(')', name_end);
        if (paren_close != std::string::npos) {
            std::string param_str = rest.substr(name_end + 1, paren_close - (name_end + 1));
            std::stringstream pss(param_str);
            std::string p;
            while (std::getline(pss, p, ',')) {
                size_t ps = p.find_first_not_of(" \t");
                size_t pe = p.find_last_not_of(" \t");
                if (ps != std::string::npos && pe != std::string::npos) {
                    std::string param_name = p.substr(ps, pe - ps + 1);
                    if (param_name == "...") {
                        def.isVariadic = true;
                        def.params.push_back("__VA_ARGS__");
                    } else {
                        def.params.push_back(param_name);
                    }
                }
            }
            size_t body_start = rest.find_first_not_of(" \t", paren_close + 1);
            def.body = (body_start != std::string::npos) ? rest.substr(body_start) : "";
        } else {
            def.body = "";
        }
    } else {
        def.isFunctionLike = false;
        size_t body_start = rest.find_first_not_of(" \t", name_end);
        def.body = (body_start != std::string::npos) ? rest.substr(body_start) : "1";
    }

    currentMacros[macro_name] = def;
}

void DependencyParser::parse_file(const std::string& filepath, 
                                  std::set<std::string, CaseInsensitivePathCompare>& dependencies, 
                                  std::set<std::string>& visitedFiles,
                                  std::map<std::string, MacroDef>& currentMacros) {
    
    std::string normPath = PathHelper::normalize(filepath);
    std::string lowerKey = PathHelper::lower_key(normPath);

    if (visitedFiles.count(lowerKey) || pragmaOnceFiles.count(lowerKey)) return;
    visitedFiles.insert(lowerKey);

    std::ifstream file(filepath);
    if (!file.is_open()) return;

    std::string currentDir = fs::path(filepath).parent_path().string();
    if (currentDir.empty()) currentDir = ".";

    std::vector<CondBranch> condStack;
    LineCleanerState cleanerState;

    auto is_active = [&]() {
        for (const auto& branch : condStack) {
            if (!branch.active || !branch.parentActive) return false;
        }
        return true;
    };

    std::string line;
    while (std::getline(file, line)) {
        while (!line.empty() && line.back() == '\\') {
            line.pop_back();
            std::string nextLine;
            if (std::getline(file, nextLine)) {
                line += nextLine;
            }
        }

        std::string cleanLine = clean_line(line, cleanerState);

        size_t start = cleanLine.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        cleanLine = cleanLine.substr(start);

        if (cleanLine.empty() || cleanLine[0] != '#') continue;

        std::stringstream ss(cleanLine.substr(1));
        std::string directive;
        ss >> directive;

        if (directive == "pragma" && is_active()) {
            std::string sub;
            ss >> sub;
            if (sub == "once") {
                pragmaOnceFiles.insert(lowerKey);
            }
        } else if (directive == "ifdef" || directive == "ifndef" || directive == "if") {
            bool parentAct = is_active();
            bool act = false;
            std::string rest;
            std::getline(ss, rest);

            if (parentAct) {
                if (directive == "ifdef") {
                    std::stringstream rss(rest);
                    std::string sym; rss >> sym;
                    act = (currentMacros.find(sym) != currentMacros.end());
                } else if (directive == "ifndef") {
                    std::stringstream rss(rest);
                    std::string sym; rss >> sym;
                    act = (currentMacros.find(sym) == currentMacros.end());
                } else {
                    act = (ExpressionEvaluator::evaluate(rest, currentMacros) != 0);
                }
            }
            condStack.push_back({ act, parentAct, act });
        } else if (directive == "elif") {
            std::string rest;
            std::getline(ss, rest);
            if (!condStack.empty()) {
                auto& top = condStack.back();
                if (top.parentActive && !top.branchTaken) {
                    top.active = (ExpressionEvaluator::evaluate(rest, currentMacros) != 0);
                    if (top.active) top.branchTaken = true;
                } else {
                    top.active = false;
                }
            }
        } else if (directive == "else") {
            if (!condStack.empty()) {
                auto& top = condStack.back();
                if (top.parentActive && !top.branchTaken) {
                    top.active = true;
                    top.branchTaken = true;
                } else {
                    top.active = false;
                }
            }
        } else if (directive == "endif") {
            if (!condStack.empty()) {
                condStack.pop_back();
            }
        } else if (directive == "define" && is_active()) {
            std::string rest;
            std::getline(ss, rest);
            parse_define(rest, currentMacros);
        } else if (directive == "undef" && is_active()) {
            std::string name;
            ss >> name;
            currentMacros.erase(name);
        } else if (directive == "error" && is_active()) {
            if (verbose) {
                std::cerr << "makedepend: Warning: #error encountered in active section of " 
                          << filepath << "\n";
            }
        } else if (directive == "include" && is_active()) {
            std::string rest;
            std::getline(ss, rest);

            size_t firstSymbol = rest.find_first_of("\"<");
            if (firstSymbol == std::string::npos) {
                rest = ExpressionEvaluator::expand_macros(rest, currentMacros);
                firstSymbol = rest.find_first_of("\"<");
            }

            if (firstSymbol != std::string::npos) {
                char closeChar = (rest[firstSymbol] == '"') ? '"' : '>';
                size_t lastSymbol = rest.find_first_of(closeChar, firstSymbol + 1);
                if (lastSymbol != std::string::npos) {
                    std::string header = rest.substr(firstSymbol + 1, lastSymbol - firstSymbol - 1);
                    bool isSystem = (rest[firstSymbol] == '<');

                    std::string resolvedPath = resolve_include(header, currentDir, isSystem);
                    if (!resolvedPath.empty()) {
                        dependencies.insert(resolvedPath);
                        parse_file(resolvedPath, dependencies, visitedFiles, currentMacros);
                    }
                }
            }
        }
    }
}

// ============================================================================
// Makefile Updater
// ============================================================================
void update_makefile(const std::string& makefilePath, 
                     const std::string& delimiter, 
                     bool appendOnly, 
                     const std::map<std::string, std::set<std::string, CaseInsensitivePathCompare>>& allDeps) {
    
    std::vector<std::string> lines;
    bool foundDelimiter = false;

    if (fs::exists(makefilePath)) {
        std::ifstream in(makefilePath);
        std::string line;
        while (std::getline(in, line)) {
            if (line == delimiter) {
                foundDelimiter = true;
                if (!appendOnly) break;
            }
            lines.push_back(line);
        }
    }

    std::ofstream out(makefilePath, std::ios::trunc);
    for (const auto& line : lines) {
        out << line << "\n";
    }

    if (!foundDelimiter) {
        out << "\n" << delimiter << "\n";
    }

    for (const auto& [target, deps] : allDeps) {
        if (deps.empty()) continue;
        
        std::string line = target + ":";
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
