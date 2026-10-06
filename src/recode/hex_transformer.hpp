#pragma once

#include "recode.hpp"
#include "transformer.hpp"

class HexTransformer : public ITransformer {
private:
    bool encode;
    static constexpr char hexLookup[] = "0123456789abcdef";

public:
    explicit HexTransformer(bool isEncode);

    [[nodiscard]] std::string getName() const override;

    [[nodiscard]] std::vector<uint8_t> transform(const std::vector<uint8_t>& input) const override;
};

// --- URL / Percent-Encoding Transformer ---
