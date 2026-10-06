#pragma once

#include "rename_strategy.hpp"
#include "rename.hpp"

class SubstringStrategy : public IRenameStrategy {
private:
    std::string from_;
    std::string to_;
    bool replaceAll_;
    bool replaceLast_;
    bool ignoreCase_;

    [[nodiscard]] static bool caseInsensitiveCompare(char a, char b);

    [[nodiscard]] size_t findMatch(const std::string& str, size_t pos) const;

    [[nodiscard]] size_t rfindMatch(const std::string& str) const;

public:
    SubstringStrategy(std::string from, std::string to, bool all, bool last, bool ignoreCase);

    std::string transform(const std::string& input) const override;
};
