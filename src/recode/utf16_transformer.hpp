#pragma once

#include "recode.hpp"
#include "transformer.hpp"

class Utf16Transformer : public ITransformer {
private:
    bool toUtf16;

public:
    explicit Utf16Transformer(bool to16);

    [[nodiscard]] std::string getName() const override;

    [[nodiscard]] std::vector<uint8_t> transform(const std::vector<uint8_t>& input) const override;
};

// --- Base64 Transformer ---
