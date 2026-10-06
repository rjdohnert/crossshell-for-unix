#pragma once

#include "recode.hpp"

class ITransformer {
public:
    virtual ~ITransformer() = default;
    [[nodiscard]] virtual std::vector<uint8_t> transform(const std::vector<uint8_t>& input) const = 0;
    [[nodiscard]] virtual std::string getName() const = 0;
};

// --- Windows Native Code Page / Charset Transformer ---
