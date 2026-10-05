class StringHelper {
public:
    static std::string ToLowerCopy(const std::string& value) {
        std::string out = value;
        std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return out;
    }

    static std::string TrimCopy(const std::string& value) {
        const char* ws = " \t\r\n";
        size_t start = value.find_first_not_of(ws);
        if (start == std::string::npos) {
            return std::string();
        }
        size_t end = value.find_last_not_of(ws);
        return value.substr(start, end - start + 1);
    }

    static std::string WideToUtf8(const std::wstring& value) {
        if (value.empty()) {
            return std::string();
        }

        int required = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (required <= 0) {
            return std::string();
        }

        std::string output(static_cast<size_t>(required), '\0');
        int written = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, &output[0], required, nullptr, nullptr);
        if (written <= 0) {
            return std::string();
        }
        output.resize(static_cast<size_t>(written - 1));
        return output;
    }

    static fs::path ExpandEnvPath(const std::wstring& rawPath) {
        wchar_t buffer[MAX_PATH];
        DWORD size = ExpandEnvironmentStringsW(rawPath.c_str(), buffer, MAX_PATH);
        if (size == 0 || size > MAX_PATH) {
            return fs::path();
        }
        return fs::path(buffer);
    }

    static std::string FormatBytes(uint64_t bytes) {
        const char* units[] = { "B", "KB", "MB", "GB", "TB" };
        int i = 0;
        double count = static_cast<double>(bytes);
        while (count >= 1024 && i < 4) {
            count /= 1024;
            i++;
        }
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(2) << count << " " << units[i];
        return stream.str();
    }
};
