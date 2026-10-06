#pragma once

#include "recode.hpp"
#include "transformer.hpp"

class UrlTransformer : public ITransformer {
private:
    bool encode;

public:
    explicit UrlTransformer(bool isEncode);

    [[nodiscard]] std::string getName() const override;

    [[nodiscard]] std::vector<uint8_t> transform(const std::vector<uint8_t>& input) const override;
};

// --- HTML Entities Transformer ---
