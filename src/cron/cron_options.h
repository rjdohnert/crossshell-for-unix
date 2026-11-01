#ifndef CRON_OPTIONS_H
#define CRON_OPTIONS_H

#include "cron.h"

class CronOptions {
public:
    std::string crontabPath{"crontab.txt"};
    bool forceSystemCrontab{false};
    int outputFormat{0};
    std::string pipeCommand;

    static void printUsage(const char* prog = nullptr);
    static void printVersion();
    static bool parse(int argc, char* argv[], CronOptions& opts);
};

#endif // CRON_OPTIONS_H
