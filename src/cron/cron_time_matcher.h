#ifndef CRON_TIME_MATCHER_H
#define CRON_TIME_MATCHER_H

#include "cron.h"
#include "cron_job.h"

// Matches a cron schedule against the current local time. Supports wildcards,
// lists, ranges, steps, and named months/days (JAN-DEC, SUN-SAT).
class CronTimeMatcher {
public:
    // Returns true if `pattern` matches `value`. Pattern is a comma-separated
    // list of subfields, each: "*", "n", "a-b", "*/s", "a-b/s", or "a/s".
    // Invalid subfields never match (they are rejected at parse time).
    static bool matchField(const std::string& pattern, int value, int minVal, int maxVal);

    // True if `pattern` is exactly "*" (unrestricted).
    static bool isWildcard(const std::string& pattern);

    // Standard cron semantics: when BOTH day-of-month and day-of-week are
    // restricted (not "*"), a job is due if EITHER matches. Otherwise both
    // must match.
    static bool isJobDue(const CronJob& job, const struct tm& localTime);

    // Validate a single field pattern; returns true if it parses cleanly.
    static bool isValidField(const std::string& pattern, int minVal, int maxVal);

private:
    static std::vector<std::string> split(const std::string& str, char delim);
    static bool matchSubfield(const std::string& sub, int val, int minVal, int maxVal);
    // Parse a subfield into rangeStart/rangeEnd/step. Returns false on malformed input.
    static bool parseSubfield(const std::string& sub, int minVal, int maxVal,
                              int& rangeStart, int& rangeEnd, int& step);
};

#endif // CRON_TIME_MATCHER_H
