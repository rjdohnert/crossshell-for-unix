    // Helper for string manipulations and UTF-8 <-> UTF-16 conversions
    class StringConverter {
    public:
        static std::wstring ToWide(std::string_view str) {
            if (str.empty()) return L"";
            int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
            std::wstring wstr(size_needed, 0);
            MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), &wstr[0], size_needed);
            return wstr;
        }

        static std::string ToNarrow(std::wstring_view wstr) {
            if (wstr.empty()) return "";
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
            std::string str(size_needed, 0);
            WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), &str[0], size_needed, nullptr, nullptr);
            return str;
        }

        static std::string Trim(const std::string& str) {
            const size_t first = str.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) return "";
            const size_t last = str.find_last_not_of(" \t\r\n");
            return str.substr(first, (last - first + 1));
        }

        static std::vector<std::string> SplitDelimited(const std::string& str, const std::string& delims = ",;\t") {
            std::vector<std::string> tokens;
            size_t start = 0, end = 0;
            while ((end = str.find_first_of(delims, start)) != std::string::npos) {
                tokens.emplace_back(Trim(str.substr(start, end - start)));
                start = end + 1;
            }
            tokens.emplace_back(Trim(str.substr(start)));
            return tokens;
        }
    };

