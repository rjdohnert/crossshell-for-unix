#include "hex_transformer.hpp"

HexTransformer::HexTransformer(bool isEncode) : encode(isEncode) {}

[[nodiscard]] std::string HexTransformer::getName() const  {
        return encode ? "Encode Hex" : "Decode Hex";
    }

[[nodiscard]] std::vector<uint8_t> HexTransformer::transform(const std::vector<uint8_t>& input) const  {
        if (encode) {
            std::vector<uint8_t> out;
            out.reserve(input.size() * 2);
            for (uint8_t b : input) {
                out.push_back(hexLookup[b >> 4]);
                out.push_back(hexLookup[b & 0x0F]);
            }
            return out;
        } else {
            std::vector<uint8_t> out;
            int high = -1;
            for (uint8_t c : input) {
                if (std::isspace(c)) continue;
                int val = -1;
                if (c >= '0' && c <= '9') val = c - '0';
                else if (c >= 'a' && c <= 'f') val = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F') val = c - 'A' + 10;
                if (val < 0) continue;

                if (high == -1) {
                    high = val;
                } else {
                    out.push_back(static_cast<uint8_t>((high << 4) | val));
                    high = -1;
                }
            }
            return out;
        }
    }
