#include "substring_strategy.hpp"

[[nodiscard]] bool SubstringStrategy::caseInsensitiveCompare(char a, char b) {
        return std::tolower(static_cast<unsigned char>(a)) ==
               std::tolower(static_cast<unsigned char>(b));
    }

[[nodiscard]] size_t SubstringStrategy::findMatch(const std::string& str, size_t pos) const {
        if (from_.empty()) return std::string::npos;
        if (!ignoreCase_) {
            return str.find(from_, pos);
        }
        auto it = std::search(str.begin() + pos, str.end(),
                              from_.begin(), from_.end(),
                              caseInsensitiveCompare);
        if (it != str.end()) {
            return std::distance(str.begin(), it);
        }
        return std::string::npos;
    }

[[nodiscard]] size_t SubstringStrategy::rfindMatch(const std::string& str) const {
        if (from_.empty()) return std::string::npos;
        if (!ignoreCase_) {
            return str.rfind(from_);
        }
        auto it = std::find_end(str.begin(), str.end(),
                                from_.begin(), from_.end(),
                                caseInsensitiveCompare);
        if (it != str.end()) {
            return std::distance(str.begin(), it);
        }
        return std::string::npos;
    }

SubstringStrategy::SubstringStrategy(std::string from, std::string to, bool all, bool last, bool ignoreCase)
        : from_(std::move(from)), to_(std::move(to)),
          replaceAll_(all), replaceLast_(last), ignoreCase_(ignoreCase) {}

std::string SubstringStrategy::transform(const std::string& input) const  {
        if (from_.empty()) return input;

        std::string result = input;
        if (replaceLast_) {
            size_t pos = rfindMatch(result);
            if (pos != std::string::npos) {
                result.replace(pos, from_.length(), to_);
            }
        } else if (replaceAll_) {
            size_t pos = 0;
            while ((pos = findMatch(result, pos)) != std::string::npos) {
                result.replace(pos, from_.length(), to_);
                pos += to_.length();
            }
        } else {
            size_t pos = findMatch(result, 0);
            if (pos != std::string::npos) {
                result.replace(pos, from_.length(), to_);
            }
        }
        return result;
    }
