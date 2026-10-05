// ============================================================================
// 4. PREDICATE & PATH QUERY ENGINE
// ============================================================================
class ObjectEvaluator {
public:
    [[nodiscard]] Value resolve_path(const Value& root, const std::string& path_str) const {
        if (path_str.empty()) return root;
        std::string path = path_str;
        if (path.front() == '.') path = path.substr(1);

        for (size_t i = 0; i < path.size(); ++i) {
            if (path[i] == '[') path[i] = '.';
            if (path[i] == ']') path.erase(i--, 1);
        }

        std::istringstream ss(path);
        std::string segment;
        Value curr = root;
        while (std::getline(ss, segment, '.')) {
            if (segment.empty()) continue;
            curr = curr.get_path(segment);
        }
        return curr;
    }

    [[nodiscard]] bool eval_expression(const std::string& expr, const Value& obj, const std::string& raw_line, bool ignore_case = false) const {
        if (expr.empty()) return true;

        std::string trimmed = GrepStringUtils::Trim(expr);
        while (trimmed.size() >= 2 && trimmed.front() == '(' && trimmed.back() == ')') {
            int depth = 0; bool balanced = true; char quote = '\0';
            for (size_t i = 0; i + 1 < trimmed.size(); ++i) {
                char ch = trimmed[i];
                if (quote != '\0') {
                    if (ch == quote) {
                        size_t slashes = 0, k = i;
                        while (k > 0 && trimmed[k - 1] == '\\') { ++slashes; --k; }
                        if ((slashes % 2) == 0) quote = '\0';
                    }
                    continue;
                }
                if (ch == '\'' || ch == '"') quote = ch;
                else if (ch == '(') ++depth;
                else if (ch == ')' && --depth == 0) { balanced = false; break; }
            }
            if (!balanced || depth != 1 || quote != '\0') break;
            trimmed = GrepStringUtils::Trim(trimmed.substr(1, trimmed.size() - 2));
        }

        size_t or_pos = GrepStringUtils::TopLevelOperator(trimmed, "||");
        if (or_pos != std::string::npos) {
            return eval_expression(trimmed.substr(0, or_pos), obj, raw_line, ignore_case) ||
                   eval_expression(trimmed.substr(or_pos + 2), obj, raw_line, ignore_case);
        }

        size_t and_pos = GrepStringUtils::TopLevelOperator(trimmed, "&&");
        if (and_pos != std::string::npos) {
            return eval_expression(trimmed.substr(0, and_pos), obj, raw_line, ignore_case) &&
                   eval_expression(trimmed.substr(and_pos + 2), obj, raw_line, ignore_case);
        }

        if (!trimmed.empty() && trimmed.front() == '!') {
            return !eval_expression(trimmed.substr(1), obj, raw_line, ignore_case);
        }

        std::regex op_re;
        std::smatch match;
        if (GrepSafe::CompileRegex(R"((.+?)\s*(==|!=|>=|<=|>|<|~|!~)\s*(.+))", op_re) && std::regex_match(trimmed, match, op_re)) {
            std::string lhs_str = GrepStringUtils::Trim(match[1].str());
            std::string op      = match[2].str();
            std::string rhs_str = GrepStringUtils::Trim(match[3].str());

            Value lhs = (lhs_str.front() == '.') ? resolve_path(obj, lhs_str) : Value(lhs_str);
            Value rhs = (rhs_str.front() == '.') ? resolve_path(obj, rhs_str) : Value(rhs_str);

            if (rhs_str.size() >= 2 && ((rhs_str.front() == '"' && rhs_str.back() == '"') || (rhs_str.front() == '\'' && rhs_str.back() == '\''))) {
                rhs = Value(rhs_str.substr(1, rhs_str.size() - 2));
            }

            if (op == "==") return lhs.to_string() == rhs.to_string();
            if (op == "!=") return lhs.to_string() != rhs.to_string();
            if (op == "~" || op == "!~") {
                static std::map<std::string, std::regex> regex_cache;
                std::regex::flag_type flags = std::regex::ECMAScript;
                if (ignore_case) flags |= std::regex::icase;
                std::string key = (ignore_case ? "i:" : "s:") + rhs.to_string();
                auto cached = regex_cache.find(key);
                if (cached == regex_cache.end()) {
                    std::regex new_re;
                    if (!GrepSafe::CompileRegex(rhs.to_string(), new_re, flags)) return false;
                    cached = regex_cache.emplace(key, std::move(new_re)).first;
                }
                bool matched = std::regex_search(lhs.to_string(), cached->second);
                return (op == "~") ? matched : !matched;
            }

            double l_num = lhs.to_number();
            double r_num = rhs.to_number();
            if (op == "<")  return l_num < r_num;
            if (op == "<=") return l_num <= r_num;
            if (op == ">")  return l_num > r_num;
            if (op == ">=") return l_num >= r_num;
        }

        if (trimmed.front() == '.') {
            return resolve_path(obj, trimmed).to_bool();
        }

        return !trimmed.empty();
    }
};

