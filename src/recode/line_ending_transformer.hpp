#pragma once

#include "recode.hpp"
#include "transformer.hpp"

class LineEndingTransformer : public ITransformer {
private:
    bool toCrlf;

public:
    explicit LineEndingTransformer(bool crlf);

    [[nodiscard]] std::string getName() const override;

    [[nodiscard]] std::vector<uint8_t> transform(const std::vector<uint8_t>& input) const override;
};
