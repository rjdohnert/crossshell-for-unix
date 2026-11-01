#include "cron_time_matcher.h"

namespace {

// Named months and days accepted in cron fields (case-insensitive).
const char* kMonthNames[] = {"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
const char* kDayNames[]   = {"SUN","MON","TUE","WED","THU","FRI","SAT"};

std::string toUpper(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}

} // namespace

std::vector<std::string> CronTimeMatcher::split(const std::string& str, char delim) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delim)) {
        if (!token.empty()) tokens.push_back(token);
    }
    return tokens;
}

// Parse a numeric or named token to a value within [minVal, maxVal].
// Returns false if the token is neither a valid number in range nor a known name.
// (File-local helper; does the name lookup directly to avoid private access.)
static bool tokenToValue(const std::string& token, int minVal, int maxVal, int& out) {
    if (token.empty()) return false;

    // Try a named month/day first.
    std::string up = toUpper(token);
    if (minVal == 1 && maxVal == 12) {
        for (int i = 0; i < 12; ++i) {
            if (up == kMonthNames[i]) { out = i + 1; return true; }
        }
    }
    if (minVal == 0 && maxVal == 7) {
        for (int i = 0; i < 7; ++i) {
            if (up == kDayNames[i]) { out = i; return true; }
        }
    }

    // Otherwise require a plain non-negative integer.
    for (char c : token) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    try {
        size_t pos = 0;
        int v = std::stoi(token, &pos);
        if (pos != token.size()) return false;
        if (v < minVal || v > maxVal) return false;
        out = v;
        return true;
    } catch (...) {
        return false;
    }
}

bool CronTimeMatcher::parseSubfield(const std::string& sub, int minVal, int maxVal,
                                    int& rangeStart, int& rangeEnd, int& step) {
    step = 1;
    std::string expr = sub;

    size_t slashPos = sub.find('/');
    if (slashPos != std::string::npos) {
        expr = sub.substr(0, slashPos);
        std::string stepStr = sub.substr(slashPos + 1);
        if (stepStr.empty()) return false;
        for (char c : stepStr) {
            if (!std::isdigit(static_cast<unsigned char>(c))) return false;
        }
        try {
            step = std::stoi(stepStr);
        } catch (...) {
            return false;
        }
        if (step <= 0) step = 1;
    }

    if (expr == "*" || expr.empty()) {
        rangeStart = minVal;
        rangeEnd = maxVal;
        return true;
    }

    size_t dashPos = expr.find('-');
    if (dashPos != std::string::npos) {
        if (!tokenToValue(expr.substr(0, dashPos), minVal, maxVal, rangeStart)) return false;
        if (!tokenToValue(expr.substr(dashPos + 1), minVal, maxVal, rangeEnd)) return false;
        if (rangeEnd < rangeStart) return false;
        return true;
    }

    if (!tokenToValue(expr, minVal, maxVal, rangeStart)) return false;
    rangeEnd = rangeStart;
    return true;
}

bool CronTimeMatcher::matchSubfield(const std::string& sub, int val, int minVal, int maxVal) {
    int rangeStart, rangeEnd, step;
    if (!parseSubfield(sub, minVal, maxVal, rangeStart, rangeEnd, step)) {
        return false; // malformed subfield never matches
    }
    if (val < rangeStart || val > rangeEnd) return false;
    return ((val - rangeStart) % step) == 0;
}

bool CronTimeMatcher::matchField(const std::string& pattern, int value, int minVal, int maxVal) {
    if (pattern == "*") return true;

    auto subfields = split(pattern, ',');
    for (const auto& sub : subfields) {
        if (matchSubfield(sub, value, minVal, maxVal)) {
            return true;
        }
    }
    return false;
}

bool CronTimeMatcher::isWildcard(const std::string& pattern) {
    return pattern == "*";
}

bool CronTimeMatcher::isValidField(const std::string& pattern, int minVal, int maxVal) {
    if (pattern.empty()) return false;
    if (pattern == "*") return true;
    auto subfields = split(pattern, ',');
    if (subfields.empty()) return false;
    int rs, re, st;
    for (const auto& sub : subfields) {
        if (!parseSubfield(sub, minVal, maxVal, rs, re, st)) return false;
    }
    return true;
}

bool CronTimeMatcher::isJobDue(const CronJob& job, const struct tm& localTime) {
    int min = localTime.tm_min;
    int hour = localTime.tm_hour;
    int dom = localTime.tm_mday;
    int mon = localTime.tm_mon + 1;
    int dow = localTime.tm_wday; // 0 = Sunday

    bool minMatch = matchField(job.minutePattern, min, 0, 59);
    bool hourMatch = matchField(job.hourPattern, hour, 0, 23);
    bool monMatch = matchField(job.monthPattern, mon, 1, 12);

    bool domRestricted = !isWildcard(job.domPattern);
    bool dowRestricted = !isWildcard(job.dowPattern);

    bool domMatch = matchField(job.domPattern, dom, 1, 31);
    // Day-of-week: accept 0-6, and also 7 as Sunday (0).
    bool dowMatch = matchField(job.dowPattern, dow, 0, 6) ||
                    (dow == 0 && matchField(job.dowPattern, 7, 0, 7));

    // Standard cron rule: if BOTH dom and dow are restricted, a job is due when
    // EITHER matches (OR). If only one (or neither) is restricted, both must
    // match (AND) -- a wildcard field always matches, so this degrades correctly.
    bool dayMatch;
    if (domRestricted && dowRestricted) {
        dayMatch = domMatch || dowMatch;
    } else {
        dayMatch = domMatch && dowMatch;
    }

    return minMatch && hourMatch && monMatch && dayMatch;
}
