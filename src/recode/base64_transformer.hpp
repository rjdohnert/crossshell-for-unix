#pragma once

#include "recode.hpp"
#include "transformer.hpp"

class Base64Transformer : public ITransformer {
private:
    bool encode;
    static constexpr char b64Table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

public:
    explicit Base64Transformer(bool isEncode);

    [[nodiscard]] std::string getName() const override;

    [[nodiscard]] std::vector<uint8_t> transform(const std::vector<uint8_t>& input) const override;

private:
    static std::vector<uint8_t> encodeBase64(const std::vector<uint8_t>& data);

    static std::vector<uint8_t> decodeBase64(const std::vector<uint8_t>& data);
};

// --- Hexadecimal Transformer ---
