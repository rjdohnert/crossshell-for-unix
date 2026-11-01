#ifndef CRON_REPORTER_H
#define CRON_REPORTER_H

#include "cron.h"

// Structured Output Reporter (JSON / CSV / Table / Pipe)
class CronReporter {
public:
    static int dispatchStatus(size_t jobCount, int format, const std::string& pipeCommand);
};

#endif // CRON_REPORTER_H
