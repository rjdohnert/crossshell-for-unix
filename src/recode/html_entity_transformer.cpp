#include "html_entity_transformer.hpp"

HtmlEntityTransformer::HtmlEntityTransformer(bool isEncode) : encode(isEncode) {}

[[nodiscard]] std::string HtmlEntityTransformer::getName() const  {
        return encode ? "Encode HTML Entities" : "Decode HTML Entities";
    }

[[nodiscard]] std::vector<uint8_t> HtmlEntityTransformer::transform(const std::vector<uint8_t>& input) const  {
        std::string str(input.begin(), input.end());
        if (encode) {
            std::string out;
            for (char c : str) {
                switch (c) {
                    case '&':  out.append("&amp;"); break;
                    case '\"': out.append("&quot;"); break;
                    case '\'': out.append("&#39;"); break;
                    case '<':  out.append("&lt;"); break;
                    case '>':  out.append("&gt;"); break;
                    default:   out.push_back(c); break;
                }
            }
            return {out.begin(), out.end()};
        } else {
            static const std::unordered_map<std::string, char> entities = {
                {"&amp;", '&'}, {"&quot;", '\"'}, {"&#39;", '\''}, {"&apos;", '\''},
                {"&lt;", '<'}, {"&gt;", '>'}, {"&nbsp;", ' '}
            };
            std::string out;
            for (size_t i = 0; i < str.size(); ++i) {
                if (str[i] == '&') {
                    size_t semi = str.find(';', i);
                    if (semi != std::string::npos && semi - i <= 7) {
                        std::string entity = str.substr(i, semi - i + 1);
                        auto it = entities.find(entity);
                        if (it != entities.end()) {
                            out.push_back(it->second);
                            i = semi;
                            continue;
                        }
                    }
                }
                out.push_back(str[i]);
            }
            return {out.begin(), out.end()};
        }
    }
