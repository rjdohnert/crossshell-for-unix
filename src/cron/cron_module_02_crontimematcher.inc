class CronTimeMatcher {
private:
    static std::vector<std::string> split(const std::string& str, char delim) {
        std::vector<std::string> tokens;
        std::stringstream ss(str);
        std::string token;
        while (std::getline(ss, token, delim)) {
            if (!token.empty()) tokens.push_back(token);
        }
        return tokens;
    }

    static bool matchSubfield(const std::string& sub, int val, int minVal, int maxVal) {
        std::string expr = sub;
        int step = 1;

        size_t slashPos = sub.find('/');
        if (slashPos != std::string::npos) {
            expr = sub.substr(0, slashPos);
            step = std::stoi(sub.substr(slashPos + 1));
            if (step <= 0) step = 1;
        }

        int rangeStart = minVal;
        int rangeEnd = maxVal;

        if (expr == "*") {
            rangeStart = minVal;
            rangeEnd = maxVal;
        } else {
            size_t dashPos = expr.find('-');
            if (dashPos != std::string::npos) {
                rangeStart = std::stoi(expr.substr(0, dashPos));
                rangeEnd = std::stoi(expr.substr(dashPos + 1));
            } else {
                rangeStart = std::stoi(expr);
                rangeEnd = rangeStart;
            }
        }

        if (val < rangeStart || val > rangeEnd) return false;
        return ((val - rangeStart) % step) == 0;
    }

public:
    static bool matchField(const std::string& pattern, int value, int minVal, int maxVal) {
        if (pattern == "*") return true;

        auto subfields = split(pattern, ',');
        for (const auto& sub : subfields) {
            if (matchSubfield(sub, value, minVal, maxVal)) {
                return true;
            }
        }
        return false;
    }

    static bool isJobDue(const CronJob& job, const struct tm& localTime) {
        int min = localTime.tm_min;
        int hour = localTime.tm_hour;
        int dom = localTime.tm_mday;
        int mon = localTime.tm_mon + 1;
        int dow = localTime.tm_wday; // 0 = Sunday

        bool minMatch = matchField(job.minutePattern, min, 0, 59);
        bool hourMatch = matchField(job.hourPattern, hour, 0, 23);
        bool domMatch = matchField(job.domPattern, dom, 1, 31);
        bool monMatch = matchField(job.monthPattern, mon, 1, 12);
        bool dowMatch = matchField(job.dowPattern, dow, 0, 6) ||
                        (dow == 0 && matchField(job.dowPattern, 7, 0, 7));

        return minMatch && hourMatch && domMatch && monMatch && dowMatch;
    }
};

// ============================================================================
// 2. OPTIONS & CONFIGURATION
// ============================================================================

