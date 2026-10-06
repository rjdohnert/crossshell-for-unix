#pragma once

#include "rename.hpp"

class IRenameStrategy {
public:
    virtual ~IRenameStrategy() = default;
    [[nodiscard]] virtual std::string transform(const std::string& input) const = 0;
};
