#ifndef CRONTAB_PARSER_H
#define CRONTAB_PARSER_H

#include "cron.h"
#include "cron_job.h"

class CrontabParser {
public:
    // Parse one crontab line into `job`. Returns false for blank lines,
    // comments, malformed lines, or lines with invalid time fields.
    static bool parseLine(const std::string& line, CronJob& job);

    // Load all valid jobs from a crontab file. Missing file -> empty vector.
    static std::vector<CronJob> load(const std::string& filepath);

    static FILETIME getFileLastWriteTime(const std::string& filepath);
};

#endif // CRONTAB_PARSER_H
