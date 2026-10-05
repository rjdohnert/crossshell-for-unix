// ============================================================================
// Pattern / Byte Parser
// ============================================================================
class PatternParser {
public:
    static std::vector<uint8_t> parse(std::string_view input) {
        std::vector<uint8_t> bytes;
        bytes.reserve(input.size());

        for (size_t i = 0; i < input.size(); ++i) {
            if (input[i] == '\\' && i + 1 < input.size()) {
                ++i;
                switch (input[i]) {
                    case 'a':  bytes.push_back(0x07); break; // Bell
                    case 'b':  bytes.push_back(0x08); break; // Backspace
                    case 'e':  bytes.push_back(0x1B); break; // Escape
                    case 'f':  bytes.push_back(0x0C); break; // Form feed
                    case 'n':  bytes.push_back(0x0A); break; // Line feed
                    case 'r':  bytes.push_back(0x0D); break; // Carriage return
                    case 't':  bytes.push_back(0x09); break; // Horizontal tab
                    case 'v':  bytes.push_back(0x0B); break; // Vertical tab
                    case '\\': bytes.push_back('\\'); break;
                    case '0': { // Octal: \0OOO
                        size_t count = 0;
                        unsigned int val = 0;
                        while (i + 1 < input.size() && count < 3 && input[i + 1] >= '0' && input[i + 1] <= '7') {
                            val = (val * 8) + (input[++i] - '0');
                            count++;
                        }
                        bytes.push_back(static_cast<uint8_t>(val & 0xFF));
                        break;
                    }
                    case 'x': case 'X': { // Hex: \xHH
                        std::string hexStr;
                        while (i + 1 < input.size() && hexStr.size() < 2 && std::isxdigit(static_cast<unsigned char>(input[i + 1]))) {
                            hexStr.push_back(input[++i]);
                        }
                        if (!hexStr.empty()) {
                            bytes.push_back(static_cast<uint8_t>(std::stoul(hexStr, nullptr, 16)));
                        } else {
                            bytes.push_back(static_cast<uint8_t>('x'));
                        }
                        break;
                    }
                    case 'd': case 'D': { // Decimal: \dDDD
                        std::string decStr;
                        while (i + 1 < input.size() && decStr.size() < 3 && std::isdigit(static_cast<unsigned char>(input[i + 1]))) {
                            decStr.push_back(input[++i]);
                        }
                        if (!decStr.empty()) {
                            bytes.push_back(static_cast<uint8_t>(std::stoul(decStr, nullptr, 10)));
                        } else {
                            bytes.push_back(static_cast<uint8_t>('d'));
                        }
                        break;
                    }
                    default:
                        bytes.push_back(static_cast<uint8_t>(input[i]));
                        break;
                }
            } else {
                bytes.push_back(static_cast<uint8_t>(input[i]));
            }
        }
        return bytes;
    }

    static std::string toHexPreview(const std::vector<uint8_t>& data, size_t maxLen = 16) {
        std::ostringstream oss;
        size_t len = std::min(data.size(), maxLen);
        for (size_t i = 0; i < len; ++i) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]) << " ";
        }
        if (data.size() > maxLen) oss << "...";
        return oss.str();
    }
};

