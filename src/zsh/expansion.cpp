#include "expansion.hpp"
#include "engine.hpp"
#include "parser.hpp"
#include "scripting.hpp"

vector<string> g_process_subst_temp_files;
vector<ProcessSubstSink> g_process_subst_sinks;
int g_process_subst_eval_depth = 0;

static bool parse_ll_checked(const string& text, long long& out) {
    if (text.empty()) return false;
    size_t i = 0;
    if (text[0] == '+' || text[0] == '-') i = 1;
    if (i == text.size()) return false;
    for (; i < text.size(); ++i) {
        if (!isdigit((unsigned char)text[i])) return false;
    }
    try {
        out = stoll(text);
        return true;
    } catch (...) {
        return false;
    }
}

static bool checked_add(long long a, long long b, long long& out) {
    if ((b > 0 && a > std::numeric_limits<long long>::max() - b) ||
        (b < 0 && a < std::numeric_limits<long long>::min() - b)) {
        return false;
    }
    out = a + b;
    return true;
}

static bool checked_sub(long long a, long long b, long long& out) {
    if ((b < 0 && a > std::numeric_limits<long long>::max() + b) ||
        (b > 0 && a < std::numeric_limits<long long>::min() + b)) {
        return false;
    }
    out = a - b;
    return true;
}

static bool checked_mul(long long a, long long b, long long& out) {
    if (a == 0 || b == 0) {
        out = 0;
        return true;
    }
    if ((b > 0 && a > std::numeric_limits<long long>::max() / b) ||
        (b < 0 && a < std::numeric_limits<long long>::min() / b) ||
        (b > 0 && a < std::numeric_limits<long long>::min() / b) ||
        (b < 0 && a > std::numeric_limits<long long>::max() / b)) {
        return false;
    }
    out = a * b;
    return true;
}

static bool checked_div(long long a, long long b, long long& out) {
    if (b == 0) return false;
    // LLONG_MIN / -1 overflows signed division (undefined behavior).
    if (b == -1 && a == LLONG_MIN) return false;
    out = a / b;
    return true;
}

long long eval_math_expr(const string& expr) {
    struct Eval {
        const string& s;
        size_t i;
        int depth = 0;
        Eval(const string& e, size_t start=0) : s(e), i(start), depth(0) {}

        long long primary() {
            if (++depth > 200) return 0; // guard against deeply nested parentheses
            if (i < s.size() && s[i] == '(') {
                ++i; long long v = lorr(); if (i < s.size() && s[i] == ')') ++i; --depth; return v;
            }
            // hex literal
            if (i+1 < s.size() && s[i]=='0' && (s[i+1]=='x'||s[i+1]=='X')) {
                i += 2; long long v = 0;
                while (i < s.size() && isxdigit((unsigned char)s[i])) {
                    char c = (char)tolower((unsigned char)s[i++]);
                    v = v*16 + (isdigit(c) ? c-'0' : c-'a'+10);
                }
                --depth; return v;
            }
            // octal literal (0 followed by octal digits)
            if (i < s.size() && s[i]=='0' && i+1 < s.size() && s[i+1]>='0' && s[i+1]<='7') {
                ++i; long long v = 0;
                while (i < s.size() && s[i]>='0' && s[i]<='7') v = v*8 + (s[i++]-'0');
                --depth; return v;
            }
            long long v = 0;
            while (i < s.size() && isdigit((unsigned char)s[i])) v = v*10 + (s[i++]-'0');
            --depth; return v;
        }

        long long unary() {
            if (++depth > 200) return 0; // guard against stack overflow via deeply nested input
            long long v;
            if (i < s.size()) {
                if (s[i]=='-') { ++i; v = -unary(); --depth; return v; }
                if (s[i]=='+') { ++i; v =  unary(); --depth; return v; }
                if (s[i]=='~') { ++i; v = ~unary(); --depth; return v; }
                if (s[i]=='!') { ++i; v =  unary() ? 0LL : 1LL; --depth; return v; }
            }
            v = primary(); --depth; return v;
        }

        long long mul() {
            long long v = unary();
            while (i < s.size() && (s[i]=='*'||s[i]=='/'||s[i]=='%')) {
                char op = s[i++]; long long r = unary();
                long long tmp;
                if (op == '*') {
                    if (!checked_mul(v, r, tmp)) return 0;
                    v = tmp;
                } else if (op == '/') {
                    if (!checked_div(v, r, tmp)) return 0;
                    v = tmp;
                } else {
                    if (r == 0) return 0;
                    v = v % r;
                }
            }
            return v;
        }

        long long add() {
            long long v = mul();
            while (i < s.size() && (s[i]=='+'||s[i]=='-')) {
                char op = s[i++]; long long r = mul();
                long long tmp;
                if (op == '+') {
                    if (!checked_add(v, r, tmp)) return 0;
                    v = tmp;
                } else {
                    if (!checked_sub(v, r, tmp)) return 0;
                    v = tmp;
                }
            }
            return v;
        }

        long long shift() {
            long long v = add();
            while (i+1 < s.size() && ((s[i]=='<'&&s[i+1]=='<')||(s[i]=='>'&&s[i+1]=='>'))) {
                bool left = s[i]=='<'; i+=2; long long r = add();
                if (r < 0 || r >= 63) return 0;
                v = left ? (v << r) : (v >> r);
            }
            return v;
        }

        long long rel() {
            long long v = shift();
            while (i < s.size()) {
                if (i+1<s.size()&&s[i]=='<'&&s[i+1]=='=') { i+=2; v = v<=shift() ? 1:0; }
                else if (i+1<s.size()&&s[i]=='>'&&s[i+1]=='=') { i+=2; v = v>=shift() ? 1:0; }
                else if (s[i]=='<'&&(i+1>=s.size()||s[i+1]!='<')) { ++i; v = v<shift()  ? 1:0; }
                else if (s[i]=='>'&&(i+1>=s.size()||s[i+1]!='>')) { ++i; v = v>shift()  ? 1:0; }
                else break;
            }
            return v;
        }

        long long eq() {
            long long v = rel();
            while (i+1 < s.size() && ((s[i]=='='&&s[i+1]=='=')||(s[i]=='!'&&s[i+1]=='='))) {
                bool equ = s[i]=='='; i+=2; long long r = rel(); v = equ ? (v==r?1:0) : (v!=r?1:0);
            }
            return v;
        }

        long long band() {
            long long v = eq();
            while (i<s.size() && s[i]=='&' && (i+1>=s.size()||s[i+1]!='&')) { ++i; v &= eq(); }
            return v;
        }

        long long bxor() {
            long long v = band();
            while (i<s.size() && s[i]=='^') { ++i; v ^= band(); }
            return v;
        }

        long long borr() {
            long long v = bxor();
            while (i<s.size() && s[i]=='|' && (i+1>=s.size()||s[i+1]!='|')) { ++i; v |= bxor(); }
            return v;
        }

        long long land() {
            long long v = borr();
            while (i+1<s.size() && s[i]=='&' && s[i+1]=='&') { i+=2; long long r=borr(); v=(v&&r)?1:0; }
            return v;
        }

        long long lorr() {
            long long v = land();
            while (i+1<s.size() && s[i]=='|' && s[i+1]=='|') { i+=2; long long r=land(); v=(v||r)?1:0; }
            return v;
        }
    };

    string clean;
    clean.reserve(expr.size());
    for (char c : expr) if (!isspace((unsigned char)c)) clean += c;
    if (clean.empty()) return 0;
    return Eval(clean).lorr();
}



string ZshEnvironment::expand_vars(const string& input) {
        string str = input;

        auto is_single_quoted_at = [&](size_t pos) {
            bool in_sq = false;
            bool in_dq = false;
            for (size_t k = 0; k < pos && k < str.size(); ++k) {
                if (str[k] == '"' && !in_sq) in_dq = !in_dq;
                else if (str[k] == '\'' && !in_dq) in_sq = !in_sq;
            }
            return in_sq;
        };

        static const regex nested_name_re(R"(\$\{\$\{([A-Za-z_][A-Za-z0-9_]*)\}\})");
        for (int pass = 0; pass < 16; ++pass) {
            smatch nested_match;
            if (!regex_search(str, nested_match, nested_name_re)) break;
            string indirect_name;
            auto inner = vars.find(nested_match[1].str());
            if (inner != vars.end()) indirect_name = inner->second;
            string value;
            auto indirect = vars.find(indirect_name);
            if (indirect != vars.end()) value = indirect->second;
            else if (!indirect_name.empty()) {
                char* environment_value = getenv(indirect_name.c_str());
                if (environment_value) value = environment_value;
            }
            str.replace(static_cast<size_t>(nested_match.position()), nested_match.length(), value);
        }

        // Tilde expansion: ~ or ~/path → $HOME
        if (!str.empty() && str[0] == '~') {
            if (str.size() == 1 || str[1] == '/' || str[1] == '\\')
                str = home_dir + str.substr(1);
        }

        // 1. Math Evaluation $(( ... ))
        size_t search_from = 0;
        while (search_from < str.size()) {
            size_t p1 = str.find("$(( ", search_from);
            size_t p2 = str.find("$(((", search_from);
            size_t p3 = str.find("$((", search_from);
            size_t math_pos = string::npos;
            for (size_t p : {p1, p2, p3}) {
                if (p != string::npos && (math_pos == string::npos || p < math_pos)) math_pos = p;
            }
            if (math_pos == string::npos) break;
            if (is_single_quoted_at(math_pos)) { search_from = math_pos + 1; continue; }
            size_t end_pos = str.find("))", math_pos);
            if (end_pos != string::npos) {
                string expr = expand_vars(str.substr(math_pos + 3, end_pos - (math_pos + 3)));
                string repl = to_string(eval_math_expr(expr));
                str.replace(math_pos, (end_pos + 2) - math_pos, repl);
                search_from = math_pos + repl.size();
            } else {
                search_from = math_pos + 3;
            }
        }

        // 1b. Command substitution $( ... ) and ` ... `
        auto find_matching_paren = [&](size_t open_pos) {
            bool q_sq = false, q_dq = false;
            int depth = 0;
            for (size_t j = open_pos; j < str.size(); ++j) {
                char ch = str[j];
                if (ch == '"' && !q_sq) { q_dq = !q_dq; continue; }
                if (ch == '\'' && !q_dq) { q_sq = !q_sq; continue; }
                if (q_sq || q_dq) continue;
                if (ch == '(') depth++;
                else if (ch == ')') {
                    depth--;
                    if (depth == 0) return j;
                }
            }
            return string::npos;
        };

        search_from = 0;
        while (search_from < str.size()) {
            size_t sub_pos = str.find("$(", search_from);
            if (sub_pos == string::npos) break;
            if (sub_pos + 2 < str.size() && str[sub_pos + 2] == '(') { search_from = sub_pos + 2; continue; }
            if (is_single_quoted_at(sub_pos)) { search_from = sub_pos + 2; continue; }

            size_t close_pos = find_matching_paren(sub_pos + 1);
            if (close_pos == string::npos) break;

            string inner_cmd = str.substr(sub_pos + 2, close_pos - (sub_pos + 2));
            string captured = capture_command_output(inner_cmd);
            str.replace(sub_pos, (close_pos + 1) - sub_pos, captured);
            search_from = sub_pos + captured.size();
        }

        search_from = 0;
        while (search_from < str.size()) {
            size_t bt_open = str.find('`', search_from);
            if (bt_open == string::npos) break;
            if (is_single_quoted_at(bt_open)) { search_from = bt_open + 1; continue; }

            size_t bt_close = str.find('`', bt_open + 1);
            if (bt_close == string::npos) break;

            string inner_cmd = str.substr(bt_open + 1, bt_close - (bt_open + 1));
            string captured = capture_command_output(inner_cmd);
            str.replace(bt_open, (bt_close + 1) - bt_open, captured);
            search_from = bt_open + captured.size();
        }

        // 1c. Process substitution: <(command), >(command), and =(command)
        search_from = 0;
        while (search_from < str.size()) {
            size_t in_pos = str.find("<(", search_from);
            size_t out_pos = str.find(">(", search_from);
            size_t eq_pos = str.find("=(", search_from);
            while (eq_pos != string::npos) {
                // Check if this is an array assignment like name=(...)
                size_t id_start = eq_pos;
                while (id_start > 0 && (isalnum((unsigned char)str[id_start - 1]) || str[id_start - 1] == '_')) {
                    --id_start;
                }
                bool is_identifier_assignment = (id_start < eq_pos) && (id_start == 0 || isspace((unsigned char)str[id_start - 1]));
                if (is_identifier_assignment) {
                    eq_pos = str.find("=(", eq_pos + 2);
                } else {
                    break;
                }
            }

            size_t sub_pos = string::npos;
            bool to_input_file = true;

            if (in_pos != string::npos && (sub_pos == string::npos || in_pos < sub_pos)) {
                sub_pos = in_pos;
                to_input_file = true;
            }
            if (eq_pos != string::npos && (sub_pos == string::npos || eq_pos < sub_pos)) {
                sub_pos = eq_pos;
                to_input_file = true;
            }
            if (out_pos != string::npos && (sub_pos == string::npos || out_pos < sub_pos)) {
                sub_pos = out_pos;
                to_input_file = false;
            }

            if (sub_pos == string::npos) break;
            if (is_single_quoted_at(sub_pos)) { search_from = sub_pos + 2; continue; }

            size_t close_pos = find_matching_paren(sub_pos + 1);
            if (close_pos == string::npos) break;

            string inner_cmd = trim_copy(str.substr(sub_pos + 2, close_pos - (sub_pos + 2)));
            string tmp_path = create_temp_process_subst_path();
            g_process_subst_temp_files.push_back(tmp_path);

            if (to_input_file) {
                // Produce data now so the caller can read from a normal file path.
                ++g_process_subst_eval_depth;
                parse_and_execute(inner_cmd + " > " + quote_for_shell_path(normalize_path_to_unix(tmp_path)));
                --g_process_subst_eval_depth;
            } else {
                // Defer sink execution until the current command finishes writing to this file.
                g_process_subst_sinks.push_back({tmp_path, inner_cmd});
            }

            string repl = normalize_path_to_win(tmp_path);
            str.replace(sub_pos, (close_pos + 1) - sub_pos, repl);
            search_from = sub_pos + repl.size();
        }

        // 2. Array Length Expansion $#arr
        if (str.length() > 2 && str[0] == '$' && str[1] == '#' && !is_single_quoted_at(0)) {
            string arr_name = str.substr(2);
            if (indexed_arrays.count(arr_name)) return to_string(indexed_arrays[arr_name].size());
            if (assoc_arrays.count(arr_name)) return to_string(assoc_arrays[arr_name].size());
        }

        // 3. Array Indexing Expansion $arr[1] or $map[key]
        static const regex arr_regex(R"(\$(\w+)\[([^\]]+)\])"); // static: avoid per-call construction
        if (str.size() <= 4096) {
            smatch match;
            string::const_iterator search_it = str.cbegin();
            while (regex_search(search_it, str.cend(), match, arr_regex)) {
                size_t abs_pos = static_cast<size_t>(match[0].first - str.cbegin());
                if (is_single_quoted_at(abs_pos)) {
                    search_it = match[0].second;
                    continue;
                }

                string var_name = match[1].str();
                string key = match[2].str();
                string val = "";

                if (indexed_arrays.count(var_name)) {
                    auto trim_ws = [](string s) {
                        size_t b = 0;
                        while (b < s.size() && isspace((unsigned char)s[b])) ++b;
                        size_t e = s.size();
                        while (e > b && isspace((unsigned char)s[e - 1])) --e;
                        return s.substr(b, e - b);
                    };

                    size_t comma = key.find(',');
                    if (comma != string::npos) {
                        long long a = 0, b = 0;
                        string left = trim_ws(key.substr(0, comma));
                        string right = trim_ws(key.substr(comma + 1));
                        if (parse_ll_checked(left, a) && parse_ll_checked(right, b) && a > 0 && b > 0) {
                            if (a > b) swap(a, b);
                            long long lo = max(1LL, a), hi = min(static_cast<long long>(indexed_arrays[var_name].size()), b);
                            for (long long k = lo; k <= hi; ++k) {
                                if (!val.empty()) val += " ";
                                val += indexed_arrays[var_name][static_cast<size_t>(k - 1)];
                            }
                        }
                    } else {
                        long long idx = 0;
                        if (parse_ll_checked(key, idx)) {
                            bool zero_based = options.count("ksharrays") && options.at("ksharrays");
                            if (idx < 0) idx = static_cast<long long>(indexed_arrays[var_name].size()) + idx + (zero_based ? 0 : 1);
                            if ((zero_based && idx >= 0) || (!zero_based && idx > 0)) {
                                size_t pos = static_cast<size_t>(zero_based ? idx : idx - 1);
                                if (pos < indexed_arrays[var_name].size()) {
                                    val = indexed_arrays[var_name][pos];
                                }
                            }
                        }
                    }
                } else if (assoc_arrays.count(var_name)) {
                    if (assoc_arrays[var_name].count(key)) {
                        val = assoc_arrays[var_name][key];
                    }
                }
                str.replace(abs_pos, match.length(0), val);
                search_it = str.cbegin() + static_cast<ptrdiff_t>(abs_pos + val.size());
            }
        }

        // 4. Standard Variable & Modifier Expansion
        string result;
        result.reserve(str.size());
        size_t i = 0;
        bool in_sq = false;
        bool in_dq = false;
        while (i < str.length()) {
            if (str[i] == '\'' && !in_dq) {
                in_sq = !in_sq;
                result += str[i++];
                continue;
            }
            if (str[i] == '"' && !in_sq) {
                in_dq = !in_dq;
                result += str[i++];
                continue;
            }

            if (str[i] == '$' && !in_sq) {
                i++;
                if (i < str.length() && str[i] == '$') { result += to_string(GetCurrentProcessId()); i++; continue; }
                if (i < str.length() && str[i] == '!') { result += jobs.empty() ? "" : to_string(jobs.back().pid); i++; continue; }
                if (i < str.length() && str[i] == '?') { result += to_string(last_exit_code); i++; continue; }
                if (i < str.length() && str[i] == '#') { result += to_string(positional_args.size()); i++; continue; }
                if (i < str.length() && str[i] == '0') { result += (vars.count("0") ? vars.at("0") : "zsh"); i++; continue; }
                if (i < str.length() && str[i] == '@') {
                    char ifs_sep = ' ';
                    if (vars.count("IFS") && !vars.at("IFS").empty()) ifs_sep = vars.at("IFS")[0];
                    else if (getenv("IFS") && getenv("IFS")[0] != '\0') ifs_sep = getenv("IFS")[0];
                    for (size_t k = 0; k < positional_args.size(); ++k) {
                        if (k > 0) result += ifs_sep;
                        result += positional_args[k];
                    }
                    i++;
                    continue;
                }
                if (i < str.length() && str[i] == '*') {
                    char ifs_sep = ' ';
                    if (vars.count("IFS") && !vars.at("IFS").empty()) ifs_sep = vars.at("IFS")[0];
                    else if (getenv("IFS") && getenv("IFS")[0] != '\0') ifs_sep = getenv("IFS")[0];
                    for (size_t k = 0; k < positional_args.size(); ++k) {
                        if (k > 0) result += ifs_sep;
                        result += positional_args[k];
                    }
                    i++;
                    continue;
                }
                if (i < str.length() && isdigit(str[i])) {
                    int idx = str[i] - '0';
                    if (idx >= 1 && idx <= (int)positional_args.size()) result += positional_args[idx - 1];
                    i++; continue;
                }

                bool braced = false;
                if (i < str.length() && str[i] == '{') { braced = true; i++; }

                string var_expr = "";
                if (braced) {
                    int nested_braces = 0;
                    while (i < str.length()) {
                        if (str[i] == '$' && i + 1 < str.length() && str[i + 1] == '{') {
                            ++nested_braces;
                            var_expr += "${";
                            i += 2;
                            continue;
                        }
                        if (str[i] == '}') {
                            if (nested_braces == 0) break;
                            --nested_braces;
                        }
                        var_expr += str[i++];
                    }
                } else {
                    while (i < str.length() && (isalnum(static_cast<unsigned char>(str[i])) || str[i] == '_'))
                        var_expr += str[i++];
                }
                if (braced && i < str.length() && str[i] == '}') i++;

                if (!braced) {
                    if (var_expr == "SECONDS") result += to_string(time(nullptr) - start_time);
                    else if (var_expr == "RANDOM") result += to_string(rand() % 32768);
                    else if (var_expr == "UID" || var_expr == "EUID" || var_expr == "GID" || var_expr == "EGID") {
                        result += vars.count(var_expr) ? vars.at(var_expr) : "1000";
                    } else if (var_expr == "pipestatus" || var_expr == "PIPESTATUS") {
                        if (indexed_arrays.count("pipestatus")) {
                            const auto& vals = indexed_arrays.at("pipestatus");
                            for (size_t pi = 0; pi < vals.size(); ++pi) {
                                if (pi > 0) result += ' ';
                                result += vals[pi];
                            }
                        } else if (vars.count(var_expr)) result += vars.at(var_expr);
                        else result += to_string(last_exit_code);
                    } else if (vars.count(var_expr)) result += vars[var_expr];
                    else { char* e = getenv(var_expr.c_str()); if (e) result += e; }
                } else {
                    // Length expansion: ${#var} or ${#arr}
                    if (!var_expr.empty() && var_expr[0] == '#') {
                        string vn = var_expr.substr(1);
                        if (indexed_arrays.count(vn)) result += to_string(indexed_arrays[vn].size());
                        else if (assoc_arrays.count(vn)) result += to_string(assoc_arrays[vn].size());
                        else {
                            string v = vars.count(vn) ? vars[vn] : "";
                            result += to_string(v.size());
                        }
                        continue;
                    }

                    // Flag handling: ${(flags)expr}
                    string flags;
                    string core_expr = var_expr;
                    if (var_expr.size() > 3 && var_expr[0] == '(') {
                        size_t fp = var_expr.find(')');
                        if (fp != string::npos && fp + 1 < var_expr.size()) {
                            flags = var_expr.substr(1, fp - 1);
                            core_expr = var_expr.substr(fp + 1);
                        }
                    }

                    auto get_flag_delim = [](const string& fl, char code, string& out_delim) -> bool {
                        string marker; marker += code; marker += ':';
                        size_t p = fl.find(marker);
                        if (p == string::npos) return false;
                        p += marker.size();
                        size_t e = fl.find(':', p);
                        if (e == string::npos) return false;
                        out_delim = fl.substr(p, e - p);
                        return true;
                    };

                    auto apply_flags_to_list = [&](vector<string>& list) {
                        for (auto& val : list) {
                            if (flags.find('L') != string::npos) {
                                transform(val.begin(), val.end(), val.begin(), [](unsigned char c){ return static_cast<char>(tolower(c)); });
                            }
                            if (flags.find('U') != string::npos) {
                                transform(val.begin(), val.end(), val.begin(), [](unsigned char c){ return static_cast<char>(toupper(c)); });
                            }
                            if (flags.find('C') != string::npos) {
                                bool cap_next = true;
                                for (char& c : val) {
                                    if (isspace((unsigned char)c) || c == '_' || c == '-') cap_next = true;
                                    else if (cap_next) { c = static_cast<char>(toupper((unsigned char)c)); cap_next = false; }
                                    else { c = static_cast<char>(tolower((unsigned char)c)); }
                                }
                            }
                            if (flags.find('q') != string::npos) {
                                string q = "'";
                                for (char c : val) { if (c == '\'') q += "'\\''"; else q += c; }
                                q += "'";
                                val = q;
                            }
                        }
                    };

                    // Type flag: ${(t)var}
                    if (flags.find('t') != string::npos) {
                        if (integer_vars.count(core_expr)) result += "integer";
                        else if (assoc_arrays.count(core_expr)) result += "association";
                        else if (indexed_arrays.count(core_expr)) result += "array";
                        else if (vars.count(core_expr)) result += "scalar";
                        else result += "";
                        continue;
                    }

                    // Keys flag: ${(k)assoc} or Values flag: ${(v)assoc}
                    if (flags.find('k') != string::npos && assoc_arrays.count(core_expr)) {
                        vector<string> keys;
                        for (const auto& [k, v] : assoc_arrays[core_expr]) keys.push_back(k);
                        apply_flags_to_list(keys);
                        string delim = " ";
                        get_flag_delim(flags, 'j', delim);
                        for (size_t ki = 0; ki < keys.size(); ++ki) {
                            if (ki > 0) result += delim;
                            result += keys[ki];
                        }
                        continue;
                    }
                    if (flags.find('v') != string::npos && assoc_arrays.count(core_expr)) {
                        vector<string> vals;
                        for (const auto& [k, v] : assoc_arrays[core_expr]) vals.push_back(v);
                        apply_flags_to_list(vals);
                        string delim = " ";
                        get_flag_delim(flags, 'j', delim);
                        for (size_t vi = 0; vi < vals.size(); ++vi) {
                            if (vi > 0) result += delim;
                            result += vals[vi];
                        }
                        continue;
                    }

                    // Check for parameter operators (:- , :+ , := , :? , etc.)
                    size_t name_length = 0;
                    if (!core_expr.empty() && (isalpha(static_cast<unsigned char>(core_expr[0])) || core_expr[0] == '_')) {
                        name_length = 1;
                        while (name_length < core_expr.size() &&
                               (isalnum(static_cast<unsigned char>(core_expr[name_length])) || core_expr[name_length] == '_'))
                            ++name_length;
                    }
                    if (name_length > 0 && name_length < core_expr.size()) {
                        string name = core_expr.substr(0, name_length);
                        string remainder = core_expr.substr(name_length);
                        static const vector<string> parameter_operators = {
                            ":-", ":+", ":=", ":?", "-", "+", "=", "?"
                        };
                        bool handled_op = false;
                        for (const auto& parameter_operator : parameter_operators) {
                            if (remainder.rfind(parameter_operator, 0) != 0) continue;

                            auto variable = vars.find(name);
                            bool is_set = variable != vars.end();
                            string value = is_set ? variable->second : "";
                            if (!is_set) {
                                if (const char* environment_value = getenv(name.c_str())) {
                                    value = environment_value;
                                    is_set = true;
                                }
                            }
                            bool colon_form = parameter_operator[0] == ':';
                            bool has_value = is_set && (!colon_form || !value.empty());
                            string word = remainder.substr(parameter_operator.size());
                            char operation = parameter_operator.back();

                            if (operation == '-') {
                                result += has_value ? value : expand_vars(word);
                            } else if (operation == '+') {
                                if (has_value) result += expand_vars(word);
                            } else if (operation == '=') {
                                if (!has_value) {
                                    if (readonly_vars.count(name)) {
                                        cerr << "zsh: read-only variable: " << name << "\n";
                                        last_exit_code = 1;
                                    } else {
                                        value = expand_vars(word);
                                        vars[name] = value;
                                    }
                                }
                                result += value;
                            } else {
                                if (!has_value) {
                                    cerr << "zsh: " << name << ": "
                                         << (word.empty() ? "parameter not set" : expand_vars(word)) << "\n";
                                    last_exit_code = 1;
                                } else {
                                    result += value;
                                }
                            }
                            handled_op = true;
                            break;
                        }
                        if (handled_op) continue;
                    }

                    // Array / string indexing & slicing: ${var[key]} or ${arr[start,end]}
                    size_t bracket_open = core_expr.find('[');
                    if (bracket_open != string::npos && core_expr.back() == ']') {
                        string vn = core_expr.substr(0, bracket_open);
                        string key = core_expr.substr(bracket_open + 1, core_expr.size() - bracket_open - 2);
                        if (indexed_arrays.count(vn)) {
                            const auto& values = indexed_arrays[vn];
                            if (key == "@" || key == "*") {
                                vector<string> vals = values;
                                apply_flags_to_list(vals);
                                string delim = " ";
                                get_flag_delim(flags, 'j', delim);
                                for (size_t value_index = 0; value_index < vals.size(); ++value_index) {
                                    if (value_index > 0) result += delim;
                                    result += vals[value_index];
                                }
                            } else if (key.find(',') != string::npos) {
                                size_t comma = key.find(',');
                                long long s_idx = 1, e_idx = static_cast<long long>(values.size());
                                parse_ll_checked(key.substr(0, comma), s_idx);
                                parse_ll_checked(key.substr(comma + 1), e_idx);
                                bool zero_based = options.count("ksharrays") && options.at("ksharrays");
                                long long n = static_cast<long long>(values.size());
                                if (s_idx < 0) s_idx = n + s_idx + (zero_based ? 0 : 1);
                                if (e_idx < 0) e_idx = n + e_idx + (zero_based ? 0 : 1);
                                long long start_pos = zero_based ? s_idx : s_idx - 1;
                                long long end_pos = zero_based ? e_idx : e_idx - 1;
                                if (start_pos < 0) start_pos = 0;
                                if (end_pos >= n) end_pos = n - 1;
                                vector<string> slice;
                                for (long long idx = start_pos; idx <= end_pos && idx < n; ++idx) {
                                    slice.push_back(values[static_cast<size_t>(idx)]);
                                }
                                apply_flags_to_list(slice);
                                string delim = " ";
                                get_flag_delim(flags, 'j', delim);
                                for (size_t sli = 0; sli < slice.size(); ++sli) {
                                    if (sli > 0) result += delim;
                                    result += slice[sli];
                                }
                            } else {
                                long long index = 0;
                                if (parse_ll_checked(key, index)) {
                                    bool zero_based = options.count("ksharrays") && options.at("ksharrays");
                                    if (index < 0) index = static_cast<long long>(values.size()) + index + (zero_based ? 0 : 1);
                                    if ((zero_based && index >= 0 && index < static_cast<long long>(values.size())) ||
                                        (!zero_based && index > 0 && index <= static_cast<long long>(values.size()))) {
                                        string v = values[static_cast<size_t>(zero_based ? index : index - 1)];
                                        vector<string> vals = {v};
                                        apply_flags_to_list(vals);
                                        result += vals[0];
                                    }
                                }
                            }
                        } else if (assoc_arrays.count(vn) && assoc_arrays[vn].count(key)) {
                            string v = assoc_arrays[vn][key];
                            vector<string> vals = {v};
                            apply_flags_to_list(vals);
                            result += vals[0];
                        } else if (vars.count(vn)) {
                            string val = vars[vn];
                            if (key.find(',') != string::npos) {
                                size_t comma = key.find(',');
                                long long s_idx = 1, e_idx = static_cast<long long>(val.size());
                                parse_ll_checked(key.substr(0, comma), s_idx);
                                parse_ll_checked(key.substr(comma + 1), e_idx);
                                bool zero_based = options.count("ksharrays") && options.at("ksharrays");
                                long long n = static_cast<long long>(val.size());
                                if (s_idx < 0) s_idx = n + s_idx + (zero_based ? 0 : 1);
                                if (e_idx < 0) e_idx = n + e_idx + (zero_based ? 0 : 1);
                                long long start_pos = zero_based ? s_idx : s_idx - 1;
                                long long end_pos = zero_based ? e_idx : e_idx - 1;
                                if (start_pos < 0) start_pos = 0;
                                if (end_pos >= n) end_pos = n - 1;
                                if (start_pos <= end_pos && start_pos < n) {
                                    string slice = val.substr(static_cast<size_t>(start_pos), static_cast<size_t>(end_pos - start_pos + 1));
                                    vector<string> vals = {slice};
                                    apply_flags_to_list(vals);
                                    result += vals[0];
                                }
                            }
                        }
                        continue;
                    }

                    // Modifiers and transformations: :h, :t, :r, :e, :l, :u, //, #, %
                    // Separate base var name from modifiers
                    string base_name = core_expr;
                    vector<string> mods;
                    size_t mod_pos = core_expr.find(':');
                    if (mod_pos != string::npos && (core_expr.find(":-") == string::npos && core_expr.find(":=") == string::npos && core_expr.find(":?") == string::npos && core_expr.find(":+") == string::npos)) {
                        base_name = core_expr.substr(0, mod_pos);
                        size_t cp = mod_pos;
                        while (cp < core_expr.size()) {
                            if (core_expr[cp] == ':') {
                                size_t next_col = core_expr.find(':', cp + 1);
                                if (next_col == string::npos) {
                                    mods.push_back(core_expr.substr(cp));
                                    break;
                                } else {
                                    mods.push_back(core_expr.substr(cp, next_col - cp));
                                    cp = next_col;
                                }
                            } else {
                                ++cp;
                            }
                        }
                    }

                    if (!mods.empty() && (vars.count(base_name) || indexed_arrays.count(base_name) || getenv(base_name.c_str()))) {
                        string val;
                        if (vars.count(base_name)) val = vars[base_name];
                        else if (indexed_arrays.count(base_name)) {
                            for (size_t vi = 0; vi < indexed_arrays[base_name].size(); ++vi) {
                                if (vi > 0) val += ' ';
                                val += indexed_arrays[base_name][vi];
                            }
                        } else if (const char* env_v = getenv(base_name.c_str())) val = env_v;

                        for (const auto& mod : mods) {
                            if (mod == ":h") {
                                string norm = normalize_path_to_unix(val);
                                fs::path p(norm);
                                string parent = p.parent_path().string();
                                val = parent.empty() ? "." : normalize_path_to_unix(parent);
                            } else if (mod == ":t") {
                                string norm = normalize_path_to_unix(val);
                                fs::path p(norm);
                                val = normalize_path_to_unix(p.filename().string());
                            } else if (mod == ":r") {
                                size_t dot = val.rfind('.');
                                if (dot != string::npos && (dot > val.find_last_of("/\\") || val.find_last_of("/\\") == string::npos))
                                    val = val.substr(0, dot);
                            } else if (mod == ":e") {
                                size_t dot = val.rfind('.');
                                if (dot != string::npos && (dot > val.find_last_of("/\\") || val.find_last_of("/\\") == string::npos))
                                    val = val.substr(dot + 1);
                                else val = "";
                            } else if (mod == ":l") {
                                transform(val.begin(), val.end(), val.begin(), [](unsigned char c){ return static_cast<char>(tolower(c)); });
                            } else if (mod == ":u") {
                                transform(val.begin(), val.end(), val.begin(), [](unsigned char c){ return static_cast<char>(toupper(c)); });
                            }
                        }
                        vector<string> vals = {val};
                        apply_flags_to_list(vals);
                        result += vals[0];
                        continue;
                    }

                    if (core_expr.find('#') != string::npos) {
                        size_t p = core_expr.find('#');
                        string vn = core_expr.substr(0, p), pref = core_expr.substr(p + 1);
                        string v = vars.count(vn) ? vars[vn] : "";
                        if (!pref.empty()) {
                            if (pref.find_first_of("*?") != string::npos) {
                                for (size_t cut = 0; cut <= v.size(); ++cut) {
                                    if (match_wildcard(pref, v.substr(0, cut))) {
                                        v = v.substr(cut);
                                        break;
                                    }
                                }
                            } else if (v.rfind(pref, 0) == 0) {
                                v = v.substr(pref.size());
                            }
                        }
                        vector<string> vals = {v};
                        apply_flags_to_list(vals);
                        result += vals[0];
                    } else if (core_expr.find('%') != string::npos) {
                        size_t p = core_expr.find('%');
                        string vn = core_expr.substr(0, p), suf = core_expr.substr(p + 1);
                        string v = vars.count(vn) ? vars[vn] : "";
                        if (!suf.empty()) {
                            if (suf.find_first_of("*?") != string::npos) {
                                for (size_t i2 = v.size(); ; ) {
                                    if (match_wildcard(suf, v.substr(i2))) {
                                        v.resize(i2);
                                        break;
                                    }
                                    if (i2 == 0) break;
                                    --i2;
                                }
                            } else if (v.size() >= suf.size() && v.compare(v.size() - suf.size(), suf.size(), suf) == 0) {
                                v.resize(v.size() - suf.size());
                            }
                        }
                        vector<string> vals = {v};
                        apply_flags_to_list(vals);
                        result += vals[0];
                    } else if (core_expr.find("//") != string::npos) {
                        size_t pos = core_expr.find("//");
                        string vn = core_expr.substr(0, pos), rest = core_expr.substr(pos + 2);
                        size_t slash = rest.find('/');
                        string search = rest.substr(0, slash), rep = (slash != string::npos) ? rest.substr(slash + 1) : "";
                        string v = vars.count(vn) ? vars[vn] : "";
                        if (!search.empty()) {
                            string out; size_t last = 0, n = search.size(), p = 0;
                            while ((p = v.find(search, last)) != string::npos) { out += v.substr(last, p - last) + rep; last = p + n; }
                            out += v.substr(last);
                            v = out;
                        }
                        vector<string> vals = {v};
                        apply_flags_to_list(vals);
                        result += vals[0];
                    } else {
                        auto split_by_delim = [](const string& src, const string& delim) {
                            vector<string> items;
                            if (delim.empty()) {
                                for (char c : src) items.push_back(string(1, c));
                                return items;
                            }
                            size_t start = 0;
                            while (start <= src.size()) {
                                size_t p = src.find(delim, start);
                                if (p == string::npos) {
                                    items.push_back(src.substr(start));
                                    break;
                                }
                                items.push_back(src.substr(start, p - start));
                                start = p + delim.size();
                            }
                            return items;
                        };

                        auto emit_vals = [&](vector<string> vals) {
                            string s_delim;
                            if (vals.size() == 1 && get_flag_delim(flags, 's', s_delim)) {
                                vals = split_by_delim(vals[0], s_delim);
                            } else if (vals.size() == 1 && flags.find('f') != string::npos) {
                                vals = split_by_delim(vals[0], "\n");
                            } else if (vals.size() == 1 && flags.find('w') != string::npos) {
                                vals = tokenize_words(vals[0]);
                            }
                            apply_flags_to_list(vals);
                            string delim = " ";
                            get_flag_delim(flags, 'j', delim);
                            for (size_t vi = 0; vi < vals.size(); ++vi) {
                                if (vi > 0) result += delim;
                                result += vals[vi];
                            }
                        };

                        if (core_expr.find('$') != string::npos) {
                            string expanded_core = expand_vars(core_expr);
                            emit_vals({expanded_core});
                        } else if (indexed_arrays.count(core_expr)) {
                            emit_vals(indexed_arrays[core_expr]);
                        } else {
                            string registry_value;
                            RegistryValueMetadata registry_metadata;
                            if (read_zsh_registry_property(core_expr, registry_value, registry_metadata)) {
                                this->registry_metadata[core_expr] = std::move(registry_metadata);
                                emit_vals({registry_value});
                            } else if (vars.count(core_expr)) {
                                emit_vals({vars[core_expr]});
                            } else {
                                char* e = getenv(core_expr.c_str());
                                if (e) emit_vals({string(e)});
                                else if (!flags.empty()) emit_vals({""});
                            }
                        }
                    }
                }
            } else {
                result += str[i]; i++;
            }
        }
        return result;
    }

bool match_wildcard(const string& pattern, const string& str) {
    size_t alt_open = pattern.find("@(");
    if (alt_open != string::npos) {
        int depth = 1;
        size_t alt_close = string::npos;
        for (size_t i = alt_open + 2; i < pattern.size(); ++i) {
            if (pattern[i] == '(') ++depth;
            else if (pattern[i] == ')' && --depth == 0) { alt_close = i; break; }
        }
        if (alt_close != string::npos) {
            string prefix = pattern.substr(0, alt_open);
            string suffix = pattern.substr(alt_close + 1);
            string alternatives = pattern.substr(alt_open + 2, alt_close - alt_open - 2);
            string current;
            int nested_depth = 0;
            for (size_t i = 0; i <= alternatives.size(); ++i) {
                char ch = i < alternatives.size() ? alternatives[i] : '|';
                if (ch == '(') ++nested_depth;
                else if (ch == ')' && nested_depth > 0) --nested_depth;
                if (ch == '|' && nested_depth == 0) {
                    if (match_wildcard(prefix + current + suffix, str)) return true;
                    current.clear();
                } else {
                    current += ch;
                }
            }
            return false;
        }
    }

    size_t p = 0, s = 0;
    size_t star_p = string::npos, star_s = 0;

    while (s < str.length()) {
        if (p < pattern.length() && pattern[p] == '[') {
            size_t close = pattern.find(']', p + 1);
            if (close != string::npos) {
                bool negate = false;
                size_t class_start = p + 1;
                if (class_start < close && (pattern[class_start] == '!' || pattern[class_start] == '^')) {
                    negate = true;
                    class_start++;
                }
                char sc = static_cast<char>(tolower(static_cast<unsigned char>(str[s])));
                bool matched = false;
                for (size_t ci = class_start; ci < close; ++ci) {
                    if (ci + 2 < close && pattern[ci + 1] == '-') {
                        char start_c = static_cast<char>(tolower(static_cast<unsigned char>(pattern[ci])));
                        char end_c = static_cast<char>(tolower(static_cast<unsigned char>(pattern[ci + 2])));
                        if (start_c > end_c) std::swap(start_c, end_c);
                        if (sc >= start_c && sc <= end_c) { matched = true; break; }
                        ci += 2;
                    } else {
                        char pc = static_cast<char>(tolower(static_cast<unsigned char>(pattern[ci])));
                        if (sc == pc) { matched = true; break; }
                    }
                }
                if (matched != negate) {
                    p = close + 1;
                    s++;
                    continue;
                }
            }
        }

        if (p < pattern.length() && (pattern[p] == '?' || tolower(static_cast<unsigned char>(pattern[p])) == tolower(static_cast<unsigned char>(str[s])))) {
            p++;
            s++;
        } else if (p < pattern.length() && pattern[p] == '*') {
            star_p = p;
            star_s = s;
            p++;
        } else if (star_p != string::npos) {
            p = star_p + 1;
            star_s++;
            s = star_s;
        } else {
            return false;
        }
    }

    while (p < pattern.length() && pattern[p] == '*') p++;
    return p == pattern.length();
}

bool parse_glob_qualifiers(string& pattern, GlobQualifiers& q) {
    if (pattern.size() < 3 || pattern.back() != ')') return false;
    size_t open = pattern.rfind('(');
    if (open == string::npos || open == 0) return false;
    if (pattern[open - 1] == '@' || pattern[open - 1] == '$' || pattern[open - 1] == '=') return false;

    string body = pattern.substr(open + 1, pattern.size() - open - 2);
    if (body.empty()) return false;

    GlobQualifiers parsed;
    parsed.has_qualifier = true;
    size_t idx = 0;
    while (idx < body.size()) {
        if (body[idx] == '.') { parsed.regular_only = true; idx++; }
        else if (body[idx] == '/') { parsed.directory_only = true; idx++; }
        else if (body[idx] == '@') { parsed.symlink_only = true; idx++; }
        else if (body[idx] == 'H' || body[idx] == 'h') { parsed.hidden_only = true; idx++; }
        else if (body[idx] == 'D') { /* dotfiles include */ idx++; }
        else if (body[idx] == '*') { parsed.executable_only = true; idx++; }
        else if (body[idx] == 'N') { parsed.null_glob = true; idx++; }
        else if (body.compare(idx, 2, "om") == 0) { parsed.order_mtime_desc = true; idx += 2; }
        else if (body.compare(idx, 2, "Om") == 0) { parsed.order_mtime_asc = true; idx += 2; }
        else if (body.compare(idx, 2, "ol") == 0) { parsed.order_size_desc = true; idx += 2; }
        else if (body.compare(idx, 2, "Ol") == 0) { parsed.order_size_asc = true; idx += 2; }
        else if (body.compare(idx, 2, "on") == 0) { parsed.order_name_asc = true; idx += 2; }
        else if (body.compare(idx, 2, "On") == 0) { parsed.order_name_desc = true; idx += 2; }
        else if (body[idx] == 'm') {
            idx++;
            int mode = 2; // exact
            if (idx < body.size() && body[idx] == '-') { mode = -1; idx++; }
            else if (idx < body.size() && body[idx] == '+') { mode = 1; idx++; }
            size_t num_start = idx;
            while (idx < body.size() && isdigit(static_cast<unsigned char>(body[idx]))) idx++;
            if (idx > num_start) {
                parsed.mtime_mode = mode;
                try { parsed.mtime_days = stod(body.substr(num_start, idx - num_start)); } catch (...) {}
            } else {
                return false;
            }
        }
        else if (body[idx] == '[') {
            size_t close_bracket = body.find(']', idx);
            if (close_bracket == string::npos) return false;
            string inner = body.substr(idx + 1, close_bracket - idx - 1);
            size_t comma = inner.find(',');
            if (comma != string::npos) {
                try {
                    parsed.select_index = static_cast<size_t>(stoull(inner.substr(0, comma)));
                    parsed.select_end = static_cast<size_t>(stoull(inner.substr(comma + 1)));
                } catch (...) {}
            } else {
                try {
                    parsed.select_index = static_cast<size_t>(stoull(inner));
                } catch (...) {}
            }
            idx = close_bracket + 1;
        }
        else {
            return false;
        }
    }

    q = parsed;
    pattern.erase(open);
    return true;
}

vector<DirEntryInfo> read_dir_entries_win32(const string& dir_path, const FILETIME& now_ft) {
    vector<DirEntryInfo> entries;
    string search_path = (dir_path.empty() || dir_path == ".") ? "*" : dir_path + "\\*";
    wstring wsearch = string_to_wstring(normalize_path_to_win(search_path));
    WIN32_FIND_DATAW fd = {};
    HANDLE hFind = FindFirstFileW(wsearch.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return entries;

    ULARGE_INTEGER now_u;
    now_u.LowPart = now_ft.dwLowDateTime;
    now_u.HighPart = now_ft.dwHighDateTime;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;

        int req = WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, NULL, 0, NULL, NULL);
        string name(req > 1 ? req - 1 : 0, '\0');
        if (!name.empty()) {
            WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, &name[0], req, NULL, NULL);
        }

        DirEntryInfo info;
        info.name = name;
        info.attributes = fd.dwFileAttributes;
        info.is_directory = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        info.is_symlink = (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
        info.is_hidden = ((fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0) || (!name.empty() && name[0] == '.');
        info.is_regular = !info.is_directory && !info.is_symlink;

        ULARGE_INTEGER sz;
        sz.LowPart = fd.nFileSizeLow;
        sz.HighPart = fd.nFileSizeHigh;
        info.size = sz.QuadPart;
        info.mtime = fd.ftLastWriteTime;

        ULARGE_INTEGER file_u;
        file_u.LowPart = fd.ftLastWriteTime.dwLowDateTime;
        file_u.HighPart = fd.ftLastWriteTime.dwHighDateTime;
        if (now_u.QuadPart >= file_u.QuadPart) {
            unsigned long long diff_100ns = now_u.QuadPart - file_u.QuadPart;
            info.age_days = static_cast<double>(diff_100ns) / (86400.0 * 10000000.0);
        } else {
            info.age_days = 0.0;
        }

        string ext;
        size_t dot_pos = name.rfind('.');
        if (dot_pos != string::npos) ext = name.substr(dot_pos);
        transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return static_cast<char>(tolower(c)); });
        info.is_executable = (ext == ".exe" || ext == ".bat" || ext == ".cmd" || ext == ".com" || ext == ".ps1" || ext == ".vbs" || ext == ".wsf");

        entries.push_back(std::move(info));
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
    return entries;
}

static bool match_qualifiers(const DirEntryInfo& st, const GlobQualifiers& q) {
    if (q.regular_only && !st.is_regular) return false;
    if (q.directory_only && !st.is_directory) return false;
    if (q.symlink_only && !st.is_symlink) return false;
    if (q.hidden_only && !st.is_hidden) return false;
    if (q.executable_only && !st.is_executable) return false;
    if (q.mtime_mode == -1) {
        if (st.age_days > q.mtime_days) return false;
    } else if (q.mtime_mode == 1) {
        if (st.age_days < q.mtime_days) return false;
    } else if (q.mtime_mode == 2) {
        if (static_cast<long long>(st.age_days) != static_cast<long long>(q.mtime_days)) return false;
    }
    return true;
}

struct GlobResultItem {
    string display_path;
    DirEntryInfo info;
};

static void glob_traverse(
    size_t seg_idx,
    const string& current_fs_dir,
    const string& current_display,
    const vector<string>& segments,
    vector<GlobResultItem>& results,
    const GlobQualifiers& q,
    const FILETIME& now_ft,
    char sep_char,
    int depth_limit = 0)
{
    if (depth_limit > 64) return;
    if (seg_idx >= segments.size()) return;

    const string& seg = segments[seg_idx];
    bool is_last = (seg_idx == segments.size() - 1);

    if (seg == "**") {
        if (!is_last) {
            glob_traverse(seg_idx + 1, current_fs_dir, current_display, segments, results, q, now_ft, sep_char, depth_limit);
        } else {
            auto entries = read_dir_entries_win32(current_fs_dir, now_ft);
            for (const auto& entry : entries) {
                if ((!entry.name.empty() && entry.name[0] == '.') || entry.is_hidden) {
                    if (!q.hidden_only) continue;
                }
                string display_path = current_display + entry.name;
                if (match_qualifiers(entry, q)) {
                    results.push_back({display_path, entry});
                }
            }
        }

        auto entries = read_dir_entries_win32(current_fs_dir, now_ft);
        for (const auto& entry : entries) {
            if (!entry.is_directory) continue;
            if ((!entry.name.empty() && entry.name[0] == '.') || entry.is_hidden) {
                if (!q.hidden_only) continue;
            }
            string next_fs = (current_fs_dir.empty() || current_fs_dir == ".") ? entry.name : current_fs_dir + "/" + entry.name;
            string next_disp = current_display + entry.name + sep_char;
            glob_traverse(seg_idx, next_fs, next_disp, segments, results, q, now_ft, sep_char, depth_limit + 1);
        }
        return;
    }

    bool has_wildcard = (seg.find_first_of("*?[") != string::npos || seg.find("@(") != string::npos);

    if (has_wildcard) {
        auto entries = read_dir_entries_win32(current_fs_dir, now_ft);
        for (const auto& entry : entries) {
            if ((!entry.name.empty() && entry.name[0] == '.') || entry.is_hidden) {
                if (!q.hidden_only && (seg.empty() || seg[0] != '.')) continue;
            }

            if (!match_wildcard(seg, entry.name)) continue;

            if (is_last) {
                string display_path = current_display + entry.name;
                if (match_qualifiers(entry, q)) {
                    results.push_back({display_path, entry});
                }
            } else {
                if (entry.is_directory) {
                    string next_fs = (current_fs_dir.empty() || current_fs_dir == ".") ? entry.name : current_fs_dir + "/" + entry.name;
                    string next_disp = current_display + entry.name + sep_char;
                    glob_traverse(seg_idx + 1, next_fs, next_disp, segments, results, q, now_ft, sep_char, depth_limit + 1);
                }
            }
        }
    } else {
        string next_fs = (current_fs_dir.empty() || current_fs_dir == ".") ? seg : current_fs_dir + "/" + seg;
        string next_disp = current_display + seg + (is_last ? "" : string(1, sep_char));

        if (is_last) {
            wstring wpath = string_to_wstring(normalize_path_to_win(next_fs));
            WIN32_FILE_ATTRIBUTE_DATA data = {};
            if (GetFileAttributesExW(wpath.c_str(), GetFileExInfoStandard, &data)) {
                DirEntryInfo entry_info;
                entry_info.name = seg;
                entry_info.attributes = data.dwFileAttributes;
                entry_info.is_directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                entry_info.is_symlink = (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
                entry_info.is_hidden = ((data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0) || (!seg.empty() && seg[0] == '.');
                entry_info.is_regular = !entry_info.is_directory && !entry_info.is_symlink;
                ULARGE_INTEGER sz; sz.LowPart = data.nFileSizeLow; sz.HighPart = data.nFileSizeHigh;
                entry_info.size = sz.QuadPart;
                entry_info.mtime = data.ftLastWriteTime;
                ULARGE_INTEGER now_u, file_u;
                now_u.LowPart = now_ft.dwLowDateTime; now_u.HighPart = now_ft.dwHighDateTime;
                file_u.LowPart = data.ftLastWriteTime.dwLowDateTime; file_u.HighPart = data.ftLastWriteTime.dwHighDateTime;
                if (now_u.QuadPart >= file_u.QuadPart) {
                    entry_info.age_days = static_cast<double>(now_u.QuadPart - file_u.QuadPart) / (86400.0 * 10000000.0);
                }
                string ext;
                size_t dot_pos = seg.rfind('.');
                if (dot_pos != string::npos) ext = seg.substr(dot_pos);
                transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return static_cast<char>(tolower(c)); });
                entry_info.is_executable = (ext == ".exe" || ext == ".bat" || ext == ".cmd" || ext == ".com" || ext == ".ps1" || ext == ".vbs" || ext == ".wsf");

                if (match_qualifiers(entry_info, q)) {
                    results.push_back({next_disp, entry_info});
                }
            }
        } else {
            wstring wpath = string_to_wstring(normalize_path_to_win(next_fs));
            DWORD attr = GetFileAttributesW(wpath.c_str());
            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
                glob_traverse(seg_idx + 1, next_fs, next_disp, segments, results, q, now_ft, sep_char, depth_limit + 1);
            }
        }
    }
}

vector<string> expand_globs(const vector<string>& args, const vector<bool>* glob_allowed) {
    vector<string> expanded;
    bool global_null_glob = g_env.options.count("nullglob") && g_env.options.at("nullglob");

    FILETIME now_ft;
    GetSystemTimeAsFileTime(&now_ft);

    for (size_t arg_index = 0; arg_index < args.size(); ++arg_index) {
        const auto& arg = args[arg_index];
        if (glob_allowed && arg_index < glob_allowed->size() && !(*glob_allowed)[arg_index]) {
            expanded.push_back(arg);
            continue;
        }

        string pattern_str = arg;
        GlobQualifiers qualifiers;
        parse_glob_qualifiers(pattern_str, qualifiers);
        bool null_glob = global_null_glob || qualifiers.null_glob;

        char sep_char = (pattern_str.find('\\') != string::npos && pattern_str.find('/') == string::npos) ? '\\' : '/';

        string drive_prefix;
        size_t offset = 0;
        if (pattern_str.size() >= 2 && isalpha(static_cast<unsigned char>(pattern_str[0])) && pattern_str[1] == ':') {
            drive_prefix = pattern_str.substr(0, 2);
            offset = 2;
            if (offset < pattern_str.size() && (pattern_str[offset] == '/' || pattern_str[offset] == '\\')) {
                drive_prefix += pattern_str[offset];
                offset++;
            }
        } else if (!pattern_str.empty() && (pattern_str[0] == '/' || pattern_str[0] == '\\')) {
            drive_prefix = pattern_str.substr(0, 1);
            offset = 1;
        }

        vector<string> raw_segments;
        string cur;
        for (size_t i = offset; i < pattern_str.size(); ++i) {
            if (pattern_str[i] == '/' || pattern_str[i] == '\\') {
                if (!cur.empty()) {
                    raw_segments.push_back(cur);
                    cur.clear();
                }
            } else {
                cur += pattern_str[i];
            }
        }
        if (!cur.empty()) raw_segments.push_back(cur);

        size_t first_wc = raw_segments.size();
        for (size_t i = 0; i < raw_segments.size(); ++i) {
            const string& s = raw_segments[i];
            if (s == "**" || s.find_first_of("*?[") != string::npos || s.find("@(") != string::npos) {
                first_wc = i;
                break;
            }
        }

        if (first_wc == raw_segments.size() && !qualifiers.has_qualifier) {
            expanded.push_back(arg);
            continue;
        }

        string prefix_fs_dir = drive_prefix;
        string display_base = drive_prefix;

        for (size_t i = 0; i < first_wc; ++i) {
            if (!prefix_fs_dir.empty() && prefix_fs_dir.back() != '/' && prefix_fs_dir.back() != '\\' && prefix_fs_dir.back() != ':') {
                prefix_fs_dir += "/";
                display_base += sep_char;
            }
            prefix_fs_dir += raw_segments[i];
            display_base += raw_segments[i];
        }
        if (first_wc > 0 || !drive_prefix.empty()) {
            if (!display_base.empty() && display_base.back() != '/' && display_base.back() != '\\' && display_base.back() != ':') {
                display_base += sep_char;
            }
        }

        if (prefix_fs_dir.empty()) {
            prefix_fs_dir = ".";
        }

        vector<string> glob_segments;
        if (first_wc < raw_segments.size()) {
            glob_segments.assign(raw_segments.begin() + first_wc, raw_segments.end());
        }

        vector<GlobResultItem> results;
        if (!glob_segments.empty()) {
            glob_traverse(0, prefix_fs_dir, display_base, glob_segments, results, qualifiers, now_ft, sep_char);
        } else if (qualifiers.has_qualifier) {
            wstring wpath = string_to_wstring(normalize_path_to_win(prefix_fs_dir));
            WIN32_FILE_ATTRIBUTE_DATA data = {};
            if (GetFileAttributesExW(wpath.c_str(), GetFileExInfoStandard, &data)) {
                DirEntryInfo entry_info;
                entry_info.name = pattern_str;
                entry_info.attributes = data.dwFileAttributes;
                entry_info.is_directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                entry_info.is_symlink = (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
                entry_info.is_hidden = ((data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0);
                entry_info.is_regular = !entry_info.is_directory && !entry_info.is_symlink;
                ULARGE_INTEGER sz; sz.LowPart = data.nFileSizeLow; sz.HighPart = data.nFileSizeHigh;
                entry_info.size = sz.QuadPart;
                entry_info.mtime = data.ftLastWriteTime;
                ULARGE_INTEGER now_u, file_u;
                now_u.LowPart = now_ft.dwLowDateTime; now_u.HighPart = now_ft.dwHighDateTime;
                file_u.LowPart = data.ftLastWriteTime.dwLowDateTime; file_u.HighPart = data.ftLastWriteTime.dwHighDateTime;
                if (now_u.QuadPart >= file_u.QuadPart) {
                    entry_info.age_days = static_cast<double>(now_u.QuadPart - file_u.QuadPart) / (86400.0 * 10000000.0);
                }
                string ext = fs::path(normalize_path_to_win(pattern_str)).extension().string();
                transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return static_cast<char>(tolower(c)); });
                entry_info.is_executable = (ext == ".exe" || ext == ".bat" || ext == ".cmd" || ext == ".com" || ext == ".ps1" || ext == ".vbs" || ext == ".wsf");

                if (match_qualifiers(entry_info, qualifiers)) {
                    results.push_back({pattern_str, entry_info});
                }
            }
        }

        if (qualifiers.order_mtime_desc) {
            stable_sort(results.begin(), results.end(), [](const GlobResultItem& a, const GlobResultItem& b) {
                ULARGE_INTEGER ua, ub;
                ua.LowPart = a.info.mtime.dwLowDateTime; ua.HighPart = a.info.mtime.dwHighDateTime;
                ub.LowPart = b.info.mtime.dwLowDateTime; ub.HighPart = b.info.mtime.dwHighDateTime;
                if (ua.QuadPart != ub.QuadPart) return ua.QuadPart > ub.QuadPart;
                return a.display_path < b.display_path;
            });
        } else if (qualifiers.order_mtime_asc) {
            stable_sort(results.begin(), results.end(), [](const GlobResultItem& a, const GlobResultItem& b) {
                ULARGE_INTEGER ua, ub;
                ua.LowPart = a.info.mtime.dwLowDateTime; ua.HighPart = a.info.mtime.dwHighDateTime;
                ub.LowPart = b.info.mtime.dwLowDateTime; ub.HighPart = b.info.mtime.dwHighDateTime;
                if (ua.QuadPart != ub.QuadPart) return ua.QuadPart < ub.QuadPart;
                return a.display_path < b.display_path;
            });
        } else if (qualifiers.order_size_desc) {
            stable_sort(results.begin(), results.end(), [](const GlobResultItem& a, const GlobResultItem& b) {
                if (a.info.size != b.info.size) return a.info.size > b.info.size;
                return a.display_path < b.display_path;
            });
        } else if (qualifiers.order_size_asc) {
            stable_sort(results.begin(), results.end(), [](const GlobResultItem& a, const GlobResultItem& b) {
                if (a.info.size != b.info.size) return a.info.size < b.info.size;
                return a.display_path < b.display_path;
            });
        } else if (qualifiers.order_name_desc) {
            stable_sort(results.begin(), results.end(), [](const GlobResultItem& a, const GlobResultItem& b) {
                return a.display_path > b.display_path;
            });
        } else {
            stable_sort(results.begin(), results.end(), [](const GlobResultItem& a, const GlobResultItem& b) {
                string sa = a.display_path, sb = b.display_path;
                transform(sa.begin(), sa.end(), sa.begin(), [](unsigned char c){ return static_cast<char>(tolower(c)); });
                transform(sb.begin(), sb.end(), sb.begin(), [](unsigned char c){ return static_cast<char>(tolower(c)); });
                if (sa != sb) return sa < sb;
                return a.display_path < b.display_path;
            });
        }

        if (qualifiers.select_index > 0) {
            size_t start = qualifiers.select_index;
            size_t end = qualifiers.select_end > 0 ? qualifiers.select_end : start;
            if (start <= results.size()) {
                for (size_t k = start - 1; k < end && k < results.size(); ++k) {
                    expanded.push_back(results[k].display_path);
                }
            } else if (!null_glob) {
                expanded.push_back(arg);
            }
        } else if (!results.empty()) {
            for (const auto& item : results) {
                expanded.push_back(item.display_path);
            }
        } else if (!null_glob) {
            expanded.push_back(arg);
        }
    }
    return expanded;
}

vector<string> expand_brace_word_impl(const string& word, int recursion_depth) {
    if (recursion_depth > 16) return {word};

    for (size_t open = 0; open < word.size(); ++open) {
        if (word[open] != '{' || (open > 0 && word[open - 1] == '$')) continue;

        int depth = 1;
        size_t close = string::npos;
        for (size_t i = open + 1; i < word.size(); ++i) {
            if (word[i] == '{') ++depth;
            else if (word[i] == '}' && --depth == 0) { close = i; break; }
        }
        if (close == string::npos) continue;

        string body = word.substr(open + 1, close - open - 1);
        vector<string> alternatives;
        string current;
        int nested_depth = 0;
        bool has_comma = false;
        for (size_t i = 0; i <= body.size(); ++i) {
            char ch = i < body.size() ? body[i] : ',';
            if (ch == '{') ++nested_depth;
            else if (ch == '}' && nested_depth > 0) --nested_depth;
            if (ch == ',' && nested_depth == 0) {
                alternatives.push_back(current);
                current.clear();
                if (i < body.size()) has_comma = true;
            } else {
                current += ch;
            }
        }

        if (!has_comma) {
            smatch sequence;
            static const regex numeric_sequence(R"(^(-?[0-9]+)\.\.(-?[0-9]+)(?:\.\.(-?[0-9]+))?$)");
            static const regex character_sequence(R"(^([A-Za-z])\.\.([A-Za-z])(?:\.\.(-?[0-9]+))?$)");
            alternatives.clear();
            if (regex_match(body, sequence, numeric_sequence)) {
                long long first = 0, last = 0, step = 0;
                if (!parse_ll_checked(sequence[1].str(), first) || !parse_ll_checked(sequence[2].str(), last))
                    continue;
                if (sequence[3].matched && !parse_ll_checked(sequence[3].str(), step)) continue;
                if (step == 0) step = first <= last ? 1 : -1;
                if ((first < last && step < 0) || (first > last && step > 0)) continue;
                for (long long value = first;
                     alternatives.size() < 10000 && (step > 0 ? value <= last : value >= last);
                     value += step) {
                    alternatives.push_back(to_string(value));
                    if ((step > 0 && value > LLONG_MAX - step) || (step < 0 && value < LLONG_MIN - step)) break;
                }
            } else if (regex_match(body, sequence, character_sequence)) {
                int first = static_cast<unsigned char>(sequence[1].str()[0]);
                int last = static_cast<unsigned char>(sequence[2].str()[0]);
                long long parsed_step = first <= last ? 1 : -1;
                if (sequence[3].matched && !parse_ll_checked(sequence[3].str(), parsed_step)) continue;
                if (parsed_step == 0 || (first < last && parsed_step < 0) || (first > last && parsed_step > 0)) continue;
                for (long long value = first;
                     alternatives.size() < 256 && (parsed_step > 0 ? value <= last : value >= last);
                     value += parsed_step) {
                    alternatives.push_back(string(1, static_cast<char>(value)));
                }
            } else {
                continue;
            }
        }

        vector<string> expanded;
        const string prefix = word.substr(0, open);
        const string suffix = word.substr(close + 1);
        for (const auto& alternative : alternatives) {
            vector<string> nested = expand_brace_word_impl(prefix + alternative + suffix, recursion_depth + 1);
            expanded.insert(expanded.end(), nested.begin(), nested.end());
            if (expanded.size() >= 10000) break;
        }
        return expanded;
    }
    return {word};
}

vector<string> expand_brace_word(const string& word) {
    return expand_brace_word_impl(word, 0);
}

vector<string> split_ifs_words(const string& value, const string& separators) {
    if (separators.empty()) return {value};
    vector<string> fields;
    string current;
    bool last_was_nonwhitespace_separator = false;
    for (size_t i = 0; i < value.size();) {
        char ch = value[i];
        bool is_separator = separators.find(ch) != string::npos;
        if (!is_separator) {
            current += ch;
            last_was_nonwhitespace_separator = false;
            ++i;
            continue;
        }

        bool whitespace_separator = isspace(static_cast<unsigned char>(ch)) != 0;
        if (whitespace_separator) {
            if (!current.empty()) {
                fields.push_back(std::move(current));
                current.clear();
            }
            while (i < value.size() && separators.find(value[i]) != string::npos &&
                   isspace(static_cast<unsigned char>(value[i]))) ++i;
            last_was_nonwhitespace_separator = false;
        } else {
            fields.push_back(std::move(current));
            current.clear();
            ++i;
            while (i < value.size() && separators.find(value[i]) != string::npos &&
                   isspace(static_cast<unsigned char>(value[i]))) ++i;
            last_was_nonwhitespace_separator = true;
        }
    }
    if (!current.empty() || last_was_nonwhitespace_separator) fields.push_back(std::move(current));
    return fields;
}
