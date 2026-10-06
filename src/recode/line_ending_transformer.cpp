#include "line_ending_transformer.hpp"

LineEndingTransformer::LineEndingTransformer(bool crlf) : toCrlf(crlf) {}

[[nodiscard]] std::string LineEndingTransformer::getName() const  {
        return toCrlf ? "Convert to CRLF" : "Convert to LF";
    }

[[nodiscard]] std::vector<uint8_t> LineEndingTransformer::transform(const std::vector<uint8_t>& input) const  {
        std::vector<uint8_t> out;
        out.reserve(input.size());

        for (size_t i = 0; i < input.size(); ++i) {
            if (input[i] == '\r') {
                if (i + 1 < input.size() && input[i + 1] == '\n') {
                    if (toCrlf) {
                        out.push_back('\r');
                        out.push_back('\n');
                    } else {
                        out.push_back('\n');
                    }
                    i++;
                } else {
                    if (toCrlf) {
                        out.push_back('\r');
                        out.push_back('\n');
                    } else {
                        out.push_back('\n');
                    }
                }
            } else if (input[i] == '\n') {
                if (toCrlf) {
                    out.push_back('\r');
                    out.push_back('\n');
                } else {
                    out.push_back('\n');
                }
            } else {
                out.push_back(input[i]);
            }
        }
        return out;
    }
