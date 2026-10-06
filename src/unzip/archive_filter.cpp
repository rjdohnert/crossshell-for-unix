#include "archive_filter.hpp"

bool simple_match(const std::string& pattern, const std::string& str) {
    if (pattern == str) return true;

    size_t star = pattern.find('*');
    if (star != std::string::npos) {
        std::string prefix = pattern.substr(0, star);
        std::string suffix = pattern.substr(star + 1);
        if (str.length() >= prefix.length() + suffix.length()) {
            return str.compare(0, prefix.length(), prefix) == 0 &&
                   str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
        }
    }

    return false;
}

bool should_extract(const std::string& filename, const std::vector<std::string>& filters) {
    if (filters.empty()) return true;
    for (const auto& filter : filters) {
        if (simple_match(filter, filename)) return true;
    }
    return false;
}
