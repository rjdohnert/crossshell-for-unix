#include "rot13_transformer.hpp"

[[nodiscard]] std::string Rot13Transformer::getName() const  { return "ROT13"; }

[[nodiscard]] std::vector<uint8_t> Rot13Transformer::transform(const std::vector<uint8_t>& input) const  {
        std::vector<uint8_t> out = input;
        for (auto& c : out) {
            if (c >= 'a' && c <= 'z') c = static_cast<uint8_t>('a' + (c - 'a' + 13) % 26);
            else if (c >= 'A' && c <= 'Z') c = static_cast<uint8_t>('A' + (c - 'A' + 13) % 26);
        }
        return out;
    }
