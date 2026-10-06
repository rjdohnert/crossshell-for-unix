#include "wildcard_expander.hpp"

bool WildcardExpander::matchesWildcard(std::string_view text, std::string_view pattern) {
        size_t t = 0, p = 0, starIdx = std::string_view::npos, match = 0;
        while (t < text.size()) {
            if (p < pattern.size() && (pattern[p] == '?' ||
                std::tolower(static_cast<unsigned char>(text[t])) ==
                std::tolower(static_cast<unsigned char>(pattern[p])))) {
                ++t;
                ++p;
            } else if (p < pattern.size() && pattern[p] == '*') {
                starIdx = p;
                match = t;
                ++p;
            } else if (starIdx != std::string_view::npos) {
                p = starIdx + 1;
                ++match;
                t = match;
            } else {
                return false;
            }
        }
        while (p < pattern.size() && pattern[p] == '*') {
            ++p;
        }
        return p == pattern.size();
    }

std::vector<fs::path> WildcardExpander::expand(const std::vector<std::string>& inputs) {
        std::vector<fs::path> resolved;

        for (const auto& raw : inputs) {
            if (raw.find_first_of("*?") == std::string::npos) {
                resolved.emplace_back(raw);
                continue;
            }

            fs::path p(raw);
            fs::path dir = p.has_parent_path() ? p.parent_path() : fs::current_path();
            std::string pattern = p.filename().string();

            std::error_code ec;
            if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
                continue;
            }

            bool matchedAny = false;
            for (const auto& entry : fs::directory_iterator(dir, ec)) {
                std::string entryName = entry.path().filename().string();
                if (matchesWildcard(entryName, pattern)) {
                    resolved.push_back(entry.path());
                    matchedAny = true;
                }
            }

            if (!matchedAny) {
                // If nothing matched, retain literal argument to let caller handle standard file error
                resolved.emplace_back(raw);
            }
        }
        return resolved;
    }
