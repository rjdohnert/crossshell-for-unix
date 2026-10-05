// ============================================================================
// 5. CORE SEARCH ENGINE
// ============================================================================
struct MatchResult {
    bool matched = false;
    std::string text_to_display;
    std::vector<std::pair<size_t, size_t>> match_spans;
};

class GrepEngine {
    GrepOptions opts;
    std::vector<std::regex> compiled_regexes;
    ObjectEvaluator object_evaluator;

public:
    explicit GrepEngine(GrepOptions o) : opts(std::move(o)) {
        std::regex::flag_type flags = std::regex::ECMAScript;
        if (opts.ignore_case) flags |= std::regex::icase;

        for (auto& pat : opts.patterns) {
            std::string final_pat = pat;
            if (opts.fixed_strings) {
                std::string escaped;
                for (char ch : final_pat) {
                    if (std::string("\\.^$|()[]{}*+?").find(ch) != std::string::npos) escaped += '\\';
                    escaped += ch;
                }
                final_pat = std::move(escaped);
            }
            if (opts.word_regexp) final_pat = "\\b(?:" + final_pat + ")\\b";
            if (opts.line_regexp) final_pat = "^(?:" + final_pat + ")$";
            std::regex compiled;
            if (GrepSafe::CompileRegex(final_pat, compiled, flags)) {
                compiled_regexes.push_back(std::move(compiled));
            }
        }
    }

    MatchResult evaluate_line(const std::string& line) {
        MatchResult res;
        Value parsed_obj;
        bool is_json = false;

        if (!opts.text_only) {
            std::string trimmed = line;
            trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](unsigned char ch) { return !std::isspace(ch); }));
            if (!trimmed.empty() && (trimmed.front() == '{' || trimmed.front() == '[')) {
                MiniJsonParser parser(trimmed);
                parsed_obj = parser.parse_value();
                is_json = (parsed_obj.type == Value::OBJECT || parsed_obj.type == Value::ARRAY);
            }
        }

        if (opts.json_mode && !is_json) {
            res.matched = false;
            return res;
        }

        if (!opts.where_expr.empty()) {
            if (!object_evaluator.eval_expression(opts.where_expr, parsed_obj, line, opts.ignore_case)) {
                res.matched = opts.invert_match;
                res.text_to_display = line;
                return res;
            }
            if (compiled_regexes.empty()) {
                res.matched = !opts.invert_match;
                res.text_to_display = resolve_output_text(parsed_obj, line);
                return res;
            }
        }

        std::string target_string = line;
        if (!opts.key_path.empty() && is_json) {
            Value val = object_evaluator.resolve_path(parsed_obj, opts.key_path);
            target_string = val.to_string();
        }

        bool found = false;
        if (compiled_regexes.empty()) {
            found = true;
        } else {
            for (auto& re : compiled_regexes) {
                std::smatch sm;
                std::string::const_iterator search_start(target_string.cbegin());
                while (std::regex_search(search_start, target_string.cend(), sm, re)) {
                    found = true;
                    size_t match_idx = std::distance(target_string.cbegin(), sm[0].first);
                    size_t match_len = sm[0].length();
                    res.match_spans.push_back({match_idx, match_len});
                    if (match_len == 0) {
                        if (search_start == target_string.cend()) break;
                        ++search_start;
                    } else {
                        search_start = sm[0].second;
                    }
                }
            }
        }

        res.matched = opts.invert_match ? !found : found;
        res.text_to_display = resolve_output_text(parsed_obj, line);
        return res;
    }

private:
    std::string resolve_output_text(const Value& obj, const std::string& raw) {
        if (!opts.extract_path.empty() && (obj.type == Value::OBJECT || obj.type == Value::ARRAY)) {
            return object_evaluator.resolve_path(obj, opts.extract_path).to_string();
        }
        return raw;
    }
};

