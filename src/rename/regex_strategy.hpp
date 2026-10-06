#pragma once

#include "rename_strategy.hpp"
#include "rename.hpp"

class RegexStrategy : public IRenameStrategy {
private:
    std::regex regexEngine_;
    std::string replacement_;
    bool replaceAll_;

public:
    RegexStrategy(const std::string& pattern, std::string replacement, bool all, bool ignoreCase);

    std::string transform(const std::string& input) const override;
};
