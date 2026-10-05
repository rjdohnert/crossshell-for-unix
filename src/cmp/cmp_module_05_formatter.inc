    class Formatter {
    public:
        static std::string byte_representation(uint8_t b) {
            std::ostringstream oss;
            if (b >= 32 && b <= 126) {
                // Printable character
                oss << std::setw(3) << std::oct << static_cast<int>(b) << " " << static_cast<char>(b);
            } else if (b == '\b') {
                oss << "\\b";
            } else if (b == '\f') {
                oss << "\\f";
            } else if (b == '\n') {
                oss << "\\n";
            } else if (b == '\r') {
                oss << "\\r";
            } else if (b == '\t') {
                oss << "\\t";
            } else if (b == '\v') {
                oss << "\\v";
            } else {
                // Non-printable control char (e.g. ^A) or raw octal
                if (b < 32) {
                    oss << "^" << static_cast<char>(b + '@');
                } else {
                    oss << std::setw(3) << std::oct << static_cast<int>(b);
                }
            }
            return oss.str();
        }
    };

    // =========================================================================
    // Comparison Engine (Core Logic)
    // =========================================================================
