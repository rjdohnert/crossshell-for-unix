#pragma once

#include "rename.hpp"

class WildcardExpander {
private:
    static bool matchesWildcard(std::string_view text, std::string_view pattern);

public:
    static std::vector<fs::path> expand(const std::vector<std::string>& inputs);
};
