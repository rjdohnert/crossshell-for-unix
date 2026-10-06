#ifndef SYNC_ENGINE_HPP
#define SYNC_ENGINE_HPP

#include "rsync.hpp"
#include "rsync_options.hpp"
#include "delta_engine.hpp"
#include "pipe_stream.hpp"

class RsyncEngine {
private:
    RsyncOptions options;
    SyncStatistics stats;
    DeltaEngine deltaEngine;

    bool shouldUpdate(const fs::path& src, const fs::path& dst);
    void syncSingleFile(const fs::path& src, const fs::path& dst);
    void syncDirectory(const fs::path& srcDir, const fs::path& dstDir);
    void finalize(std::chrono::time_point<std::chrono::high_resolution_clock> startTime);

public:
    explicit RsyncEngine(const RsyncOptions& opts)
        : options(opts), deltaEngine(opts.blockSize) {}

    int execute();
};

#endif // SYNC_ENGINE_HPP
