class ExprParser {
public:
    explicit ExprParser(std::vector<std::wstring> tokens) : tokens_(std::move(tokens)) {}

    bool parse(std::wstring& out, std::wstring& err) {
        if (tokens_.empty()) {
            err = L"syntax error";
            return false;
        }
        pos_ = 0;
        out = parse_or(err);
        if (!err.empty()) return false;
        if (pos_ != tokens_.size()) {
            err = L"syntax error";
            return false;
        }
        return true;
    }

private:
    static bool is_int(const std::wstring& s) {
        if (s.empty()) return false;
        size_t i = (s[0] == L'-' || s[0] == L'+') ? 1 : 0;
        if (i == s.size()) return false;
        for (; i < s.size(); ++i) {
            if (!iswdigit(s[i])) return false;
        }
        return true;
    }

    static long long to_int(const std::wstring& s, bool& ok) {
        ok = false;
        try {
            size_t idx = 0;
            long long v = std::stoll(s, &idx, 10);
            if (idx != s.size()) return 0;
            ok = true;
            return v;
        } catch (...) {
            return 0;
        }
    }

    static bool truthy(const std::wstring& s) {
        return !(s.empty() || s == L"0");
    }

    static std::wstring as_num(long long v) {
        return std::to_wstring(v);
    }

    std::wstring parse_or(std::wstring& err) {
        std::wstring left = parse_and(err);
        while (err.empty() && match(L"|")) {
            std::wstring right = parse_and(err);
            if (!truthy(left)) left = right;
        }
        return left;
    }

    std::wstring parse_and(std::wstring& err) {
        std::wstring left = parse_cmp(err);
        while (err.empty() && match(L"&")) {
            std::wstring right = parse_cmp(err);
            if (!truthy(left) || !truthy(right)) left = L"0";
        }
        return left;
    }

    std::wstring parse_cmp(std::wstring& err) {
        std::wstring left = parse_add(err);
        while (err.empty() && has_more()) {
            std::wstring op = peek();
            if (op != L"=" && op != L"!=" && op != L"<" && op != L"<=" && op != L">" && op != L">=") {
                break;
            }
            next();
            std::wstring right = parse_add(err);
            if (!err.empty()) break;

            bool li = is_int(left), ri = is_int(right);
            bool result = false;
            if (li && ri) {
                bool okL = false, okR = false;
                long long lv = to_int(left, okL), rv = to_int(right, okR);
                if (!okL || !okR) {
                    err = L"non-integer argument";
                    return L"";
                }
                if (op == L"=") result = (lv == rv);
                else if (op == L"!=") result = (lv != rv);
                else if (op == L"<") result = (lv < rv);
                else if (op == L"<=") result = (lv <= rv);
                else if (op == L">") result = (lv > rv);
                else if (op == L">=") result = (lv >= rv);
            } else {
                if (op == L"=") result = (left == right);
                else if (op == L"!=") result = (left != right);
                else if (op == L"<") result = (left < right);
                else if (op == L"<=") result = (left <= right);
                else if (op == L">") result = (left > right);
                else if (op == L">=") result = (left >= right);
            }
            left = result ? L"1" : L"0";
        }
        return left;
    }

    std::wstring parse_add(std::wstring& err) {
        std::wstring left = parse_mul(err);
        while (err.empty() && has_more()) {
            std::wstring op = peek();
            if (op != L"+" && op != L"-") break;
            next();
            std::wstring right = parse_mul(err);
            if (!err.empty()) break;

            bool okL = false, okR = false;
            long long lv = to_int(left, okL), rv = to_int(right, okR);
            if (!okL || !okR) {
                err = L"non-integer argument";
                return L"";
            }
            left = as_num(op == L"+" ? lv + rv : lv - rv);
        }
        return left;
    }

    std::wstring parse_mul(std::wstring& err) {
        std::wstring left = parse_primary(err);
        while (err.empty() && has_more()) {
            std::wstring op = peek();
            if (op != L"*" && op != L"/" && op != L"%") break;
            next();
            std::wstring right = parse_primary(err);
            if (!err.empty()) break;

            bool okL = false, okR = false;
            long long lv = to_int(left, okL), rv = to_int(right, okR);
            if (!okL || !okR) {
                err = L"non-integer argument";
                return L"";
            }
            if ((op == L"/" || op == L"%") && rv == 0) {
                err = L"division by zero";
                return L"";
            }
            if (op == L"*") left = as_num(lv * rv);
            else if (op == L"/") left = as_num(lv / rv);
            else left = as_num(lv % rv);
        }
        return left;
    }

    std::wstring parse_primary(std::wstring& err) {
        if (!has_more()) {
            err = L"syntax error";
            return L"";
        }

        if (match(L"(")) {
            std::wstring v = parse_or(err);
            if (!err.empty()) return L"";
            if (!match(L")")) {
                err = L"missing ')'";
                return L"";
            }
            return v;
        }

        return next();
    }

    bool has_more() const { return pos_ < tokens_.size(); }
    std::wstring peek() const { return tokens_[pos_]; }
    std::wstring next() { return tokens_[pos_++]; }
    bool match(const std::wstring& tok) {
        if (has_more() && tokens_[pos_] == tok) {
            ++pos_;
            return true;
        }
        return false;
    }

    std::vector<std::wstring> tokens_;
    size_t pos_ = 0;
};
