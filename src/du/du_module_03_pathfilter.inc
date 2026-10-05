    // =========================================================================
    // Wildcard Filter (Handles --exclude=PATTERN)
    // =========================================================================
    class PathFilter {
    public:
        static bool is_excluded(const std::string& filename, const std::vector<std::string>& patterns) {
            for (const auto& pattern : patterns) {
                if (wildcard_match(pattern, filename)) {
                    return true;
                }
            }
            return false;
        }

    private:
        static bool wildcard_match(std::string_view pattern, std::string_view str) {
            size_t p_idx = 0, s_idx = 0;
            size_t star_idx = std::string_view::npos, match_idx = 0;

            while (s_idx < str.length()) {
                if (p_idx < pattern.length() && (pattern[p_idx] == '?' || pattern[p_idx] == str[s_idx])) {
                    p_idx++;
                    s_idx++;
                } else if (p_idx < pattern.length() && pattern[p_idx] == '*') {
                    star_idx = p_idx;
                    match_idx = s_idx;
                    p_idx++;
                } else if (star_idx != std::string_view::npos) {
                    p_idx = star_idx + 1;
                    match_idx++;
                    s_idx = match_idx;
                } else {
                    return false;
                }
            }

            while (p_idx < pattern.length() && pattern[p_idx] == '*') {
                p_idx++;
            }

            return p_idx == pattern.length();
        }
    };
