#pragma once

#include "recode.hpp"
#include "transformer.hpp"

class Rot13Transformer : public ITransformer {
public:
    [[nodiscard]] std::string getName() const override;

    [[nodiscard]] std::vector<uint8_t> transform(const std::vector<uint8_t>& input) const override;
};

// --- Line-Ending Transformer (CRLF <-> LF) ---
