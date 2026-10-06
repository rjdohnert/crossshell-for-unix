#ifndef RSYNC_OPTIONS_HPP
#define RSYNC_OPTIONS_HPP

#include "rsync.hpp"

class RsyncOptions {
public:
    bool recursive{false};
    bool archive{false};
    bool verbose{false};
    bool quiet{false};
    bool checksumOnly{false};
    bool updateOnly{false};
    bool dryRun{false};
    bool deleteTarget{false};
    bool showStats{false};
    bool preserveTimes{false};
    bool showProgress{false};
    bool pipeFromStdin{false};
    bool pipeToStdout{false};
    size_t blockSize{4096};

    std::string sourcePath;
    std::string destinationPath;

    static RsyncOptions parse(int argc, char* argv[]);
    static void showHelp();
};

#endif // RSYNC_OPTIONS_HPP
