#include "url_transformer.hpp"

UrlTransformer::UrlTransformer(bool isEncode) : encode(isEncode) {}

[[nodiscard]] std::string UrlTransformer::getName() const  {
        return encode ? "Encode URL" : "Decode URL";
    }

[[nodiscard]] std::vector<uint8_t> UrlTransformer::transform(const std::vector<uint8_t>& input) const  {
        if (encode) {
            std::ostringstream escaped;
            for (uint8_t c : input) {
                if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                    escaped << static_cast<char>(c);
                } else {
                    escaped << '%' << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
                }
            }
            std::string str = escaped.str();
            return {str.begin(), str.end()};
        } else {
            std::vector<uint8_t> out;
            for (size_t i = 0; i < input.size(); ++i) {
                if (input[i] == '%' && i + 2 < input.size()) {
                    std::string hexStr = {static_cast<char>(input[i + 1]), static_cast<char>(input[i + 2])};
                    try {
                        auto val = static_cast<uint8_t>(std::stoul(hexStr, nullptr, 16));
                        out.push_back(val);
                        i += 2;
                        continue;
                    } catch (...) {}
                } else if (input[i] == '+') {
                    out.push_back(' ');
                    continue;
                }
                out.push_back(input[i]);
            }
            return out;
        }
    }
