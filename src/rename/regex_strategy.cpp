#include "regex_strategy.hpp"

RegexStrategy::RegexStrategy(const std::string& pattern, std::string replacement, bool all, bool ignoreCase)
        : replacement_(std::move(replacement)), replaceAll_(all) {
        auto flags = std::regex_constants::ECMAScript;
        if (ignoreCase) {
            flags |= std::regex_constants::icase;
        }
        regexEngine_ = std::regex(pattern, flags);
    }

std::string RegexStrategy::transform(const std::string& input) const  {
        if (replaceAll_) {
            return std::regex_replace(input, regexEngine_, replacement_);
        } else {
            return std::regex_replace(input, regexEngine_, replacement_,
                                      std::regex_constants::format_first_only);
        }
    }
