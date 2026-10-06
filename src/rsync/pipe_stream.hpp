#ifndef PIPE_STREAM_HPP
#define PIPE_STREAM_HPP

#include "rsync.hpp"

class PipeStreamManager {
public:
    static void configureBinaryMode();
    static bool syncStdinToFile(const fs::path& dstPath, SyncStatistics& stats, bool dryRun);
    static bool syncFileToStdout(const fs::path& srcPath, SyncStatistics& stats);
};

#endif // PIPE_STREAM_HPP
