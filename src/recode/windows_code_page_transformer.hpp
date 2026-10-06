#pragma once

#include "recode.hpp"
#include "transformer.hpp"

class WindowsCodePageTransformer : public ITransformer {
private:
    uint32_t fromCodePage;
    uint32_t toCodePage;
    std::string name;

public:
    WindowsCodePageTransformer(uint32_t fromCP, uint32_t toCP, std::string transformerName);

    [[nodiscard]] std::string getName() const override;

    [[nodiscard]] std::vector<uint8_t> transform(const std::vector<uint8_t>& input) const override;
};

// --- UTF-16LE / UTF-8 Converter ---
