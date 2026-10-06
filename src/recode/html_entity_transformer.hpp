#pragma once

#include "recode.hpp"
#include "transformer.hpp"

class HtmlEntityTransformer : public ITransformer {
private:
    bool encode;

public:
    explicit HtmlEntityTransformer(bool isEncode);

    [[nodiscard]] std::string getName() const override;

    [[nodiscard]] std::vector<uint8_t> transform(const std::vector<uint8_t>& input) const override;
};

// --- ROT13 Transformer ---
