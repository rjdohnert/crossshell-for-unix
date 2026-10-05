class FileSplitManager {
public:
    static std::string formatFilename(const std::string& prefix, int index, int digits, const std::string& customFmt) {
        if (!customFmt.empty()) {
            char buf[256];
            snprintf(buf, sizeof(buf), customFmt.c_str(), index);
            return prefix + buf;
        } else {
            std::ostringstream ss;
            ss << prefix << std::setfill('0') << std::setw(digits) << index;
            return ss.str();
        }
    }

    static void cleanup(const std::vector<std::string>& files) {
        for (const auto& f : files) {
            std::remove(f.c_str());
        }
    }
};

