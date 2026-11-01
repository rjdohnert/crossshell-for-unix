#include "crontab_parser.h"
#include "cron_time_matcher.h"

namespace {

// Expand a leading @macro into its five-field equivalent. Returns true and
// fills the fields if `first` is a recognized macro; otherwise returns false.
// @reboot is handled by the caller (it has no time fields).
bool expandMacro(const std::string& first, std::string fields[5]) {
    if (first == "@yearly" || first == "@annually") {
        fields[0] = "0"; fields[1] = "0"; fields[2] = "1"; fields[3] = "1"; fields[4] = "*";
        return true;
    }
    if (first == "@monthly") {
        fields[0] = "0"; fields[1] = "0"; fields[2] = "1"; fields[3] = "*"; fields[4] = "*";
        return true;
    }
    if (first == "@weekly") {
        fields[0] = "0"; fields[1] = "0"; fields[2] = "*"; fields[3] = "*"; fields[4] = "0";
        return true;
    }
    if (first == "@daily" || first == "@midnight") {
        fields[0] = "0"; fields[1] = "0"; fields[2] = "*"; fields[3] = "*"; fields[4] = "*";
        return true;
    }
    if (first == "@hourly") {
        fields[0] = "0"; fields[1] = "*"; fields[2] = "*"; fields[3] = "*"; fields[4] = "*";
        return true;
    }
    return false;
}

} // namespace

bool CrontabParser::parseLine(const std::string& line, CronJob& job) {
    std::string trimmed = line;
    size_t start = trimmed.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return false;
    trimmed = trimmed.substr(start);

    if (trimmed.empty() || trimmed[0] == '#') return false;

    std::istringstream iss(trimmed);
    std::string first;
    if (!(iss >> first)) return false;

    // @macro form: "@daily command..." or "@reboot command..."
    if (!first.empty() && first[0] == '@') {
        if (first == "@reboot") {
            // @reboot jobs run at daemon startup; represent with a sentinel that
            // never matches a wall-clock time. The engine handles the trigger.
            job.minutePattern = "@reboot";
            job.hourPattern = job.domPattern = job.monthPattern = job.dowPattern = "*";
        } else {
            std::string fields[5];
            if (!expandMacro(first, fields)) {
                return false; // unknown @macro
            }
            job.minutePattern = fields[0];
            job.hourPattern   = fields[1];
            job.domPattern    = fields[2];
            job.monthPattern  = fields[3];
            job.dowPattern    = fields[4];
        }
    } else {
        // Standard five-field form: first token is the minute field.
        job.minutePattern = first;
        if (!(iss >> job.hourPattern >> job.domPattern >> job.monthPattern >> job.dowPattern)) {
            return false;
        }
    }

    std::string remainder;
    std::getline(iss, remainder);
    size_t cmdStart = remainder.find_first_not_of(" \t");
    if (cmdStart != std::string::npos) {
        job.command = remainder.substr(cmdStart);
    } else {
        return false; // no command
    }

    // Validate time fields (skip for @reboot sentinel). Malformed fields are
    // rejected here so a bad line can never crash the scheduler later.
    if (job.minutePattern != "@reboot") {
        if (!CronTimeMatcher::isValidField(job.minutePattern, 0, 59)) return false;
        if (!CronTimeMatcher::isValidField(job.hourPattern, 0, 23)) return false;
        if (!CronTimeMatcher::isValidField(job.domPattern, 1, 31)) return false;
        if (!CronTimeMatcher::isValidField(job.monthPattern, 1, 12)) return false;
        if (!CronTimeMatcher::isValidField(job.dowPattern, 0, 7)) return false;
    }

    job.rawLine = line;
    return true;
}

std::vector<CronJob> CrontabParser::load(const std::string& filepath) {
    std::vector<CronJob> jobs;
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return jobs;
    }

    std::string line;
    while (std::getline(file, line)) {
        CronJob job;
        if (parseLine(line, job)) {
            jobs.push_back(job);
        }
    }
    return jobs;
}

FILETIME CrontabParser::getFileLastWriteTime(const std::string& filepath) {
    FILETIME ft = { 0 };
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (GetFileAttributesExA(filepath.c_str(), GetFileExInfoStandard, &fad)) {
        ft = fad.ftLastWriteTime;
    }
    return ft;
}
