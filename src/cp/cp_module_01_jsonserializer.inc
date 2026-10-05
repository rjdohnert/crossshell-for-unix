class JsonSerializer {
public:
    static std::string escape(const std::string& str) {
        std::ostringstream oss;
        for (unsigned char c : str) {
            switch (c) {
                case '"': oss << "\\\""; break;
                case '\\': oss << "\\\\"; break;
                case '\b': oss << "\\b"; break;
                case '\f': oss << "\\f"; break;
                case '\n': oss << "\\n"; break;
                case '\r': oss << "\\r"; break;
                case '\t': oss << "\\t"; break;
                default:
                    if (c < 32) oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                    else oss.put(c);
            }
        }
        return oss.str();
    }
};

// ============================================================================
// Class: CopyOptions
// Encapsulates configuration corresponding to HP-UX cp flags.
// ============================================================================
