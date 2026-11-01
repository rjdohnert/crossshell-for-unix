#ifndef CRON_JOB_H
#define CRON_JOB_H

#include "cron.h"

struct CronJob {
    std::string minutePattern;
    std::string hourPattern;
    std::string domPattern;
    std::string monthPattern;
    std::string dowPattern;
    std::string command;
    std::string rawLine;
};

#endif // CRON_JOB_H
