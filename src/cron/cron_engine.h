#ifndef CRON_ENGINE_H
#define CRON_ENGINE_H

#include "cron.h"
#include "cron_options.h"
#include "cron_job.h"

class CronEngine {
public:
    explicit CronEngine(CronOptions opts);
    int execute();

private:
    // Launch a command via cmd.exe on a detached thread. Commands come from the
    // crontab and are executed with the daemon's full privileges -- this is
    // inherent to a cron daemon. Overlap protection: a command whose previous
    // instance is still running is skipped (see m_runningCommands).
    static void executeCommandAsync(const std::string& cmd);

    CronOptions options;

    // Tracks commands currently executing to prevent overlapping instances of
    // the same job. Guarded by m_runningMutex.
    static std::set<std::string> m_runningCommands;
    static std::mutex m_runningMutex;
};

#endif // CRON_ENGINE_H
