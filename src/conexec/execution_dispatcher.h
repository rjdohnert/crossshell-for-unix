#ifndef CONEXEC_EXECUTION_DISPATCHER_H
#define CONEXEC_EXECUTION_DISPATCHER_H

#include "conexec.h"
#include "runcon_config.h"
#include "file_snapshot.h"

// Launches a process under the configured isolation policy and handles
// post-execution output disposition (discard / preserve / interactive commit).
class ExecutionDispatcher {
public:
    static int Execute(const RunconConfig& cfg);

private:
    static void ProcessOutputDispositions(
        const RunconConfig& cfg,
        const fs::path& stageDir,
        const fs::path& destDir,
        const std::map<fs::path, FileSnapshot>& initialSnapshot);
};

#endif // CONEXEC_EXECUTION_DISPATCHER_H
