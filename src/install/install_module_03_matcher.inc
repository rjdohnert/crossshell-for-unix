// ============================================================================
// File Path Pattern Matcher
// ============================================================================

class WildcardMatcher {
public:
    static bool match(std::string_view pattern, std::string_view str) {
        if (pattern == "*" || pattern == "*.*") return true;
        size_t p = 0, s = 0, star_p = std::string_view::npos, star_s = 0;
        while (s < str.size()) {
            if (p < pattern.size() && (pattern[p] == '?' ||
                std::tolower(static_cast<unsigned char>(pattern[p])) ==
                std::tolower(static_cast<unsigned char>(str[s])))) {
                p++; s++;
            } else if (p < pattern.size() && pattern[p] == '*') {
                star_p = p++;
                star_s = s;
            } else if (star_p != std::string_view::npos) {
                p = star_p + 1;
                s = ++star_s;
            } else {
                return false;
            }
        }
        while (p < pattern.size() && pattern[p] == '*') p++;
        return p == pattern.size();
    }
};

