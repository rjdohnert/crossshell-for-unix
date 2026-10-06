#include "base64_transformer.hpp"

Base64Transformer::Base64Transformer(bool isEncode) : encode(isEncode) {}

[[nodiscard]] std::string Base64Transformer::getName() const  {
        return encode ? "Encode Base64" : "Decode Base64";
    }

[[nodiscard]] std::vector<uint8_t> Base64Transformer::transform(const std::vector<uint8_t>& input) const  {
        return encode ? encodeBase64(input) : decodeBase64(input);
    }

std::vector<uint8_t> Base64Transformer::encodeBase64(const std::vector<uint8_t>& data) {
        std::vector<uint8_t> out;
        int val = 0, valb = -6;
        for (uint8_t c : data) {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0) {
                out.push_back(static_cast<uint8_t>(b64Table[(val >> valb) & 0x3F]));
                valb -= 6;
            }
        }
        if (valb > -6) out.push_back(static_cast<uint8_t>(b64Table[((val << 8) >> (valb + 8)) & 0x3F]));
        while (out.size() % 4) out.push_back('=');
        return out;
    }

std::vector<uint8_t> Base64Transformer::decodeBase64(const std::vector<uint8_t>& data) {
        std::vector<int> T(256, -1);
        for (int i = 0; i < 64; i++) T[static_cast<uint8_t>(b64Table[i])] = i;

        std::vector<uint8_t> out;
        int val = 0, valb = -8;
        for (uint8_t c : data) {
            if (T[c] == -1) continue;
            val = (val << 6) + T[c];
            valb += 6;
            if (valb >= 0) {
                out.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
                valb -= 8;
            }
        }
        return out;
    }
