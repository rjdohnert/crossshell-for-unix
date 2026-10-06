#include "pipe_stream.hpp"

void PipeStreamManager::configureBinaryMode() {
    #ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    #endif
}

bool PipeStreamManager::syncStdinToFile(const fs::path& dstPath, SyncStatistics& stats, bool dryRun) {
    configureBinaryMode();
    std::ofstream outFile;
    if (!dryRun) {
        outFile.open(dstPath, std::ios::binary);
        if (!outFile) {
            std::cerr << "rsync: failed to create destination file " << dstPath << "\n";
            return false;
        }
    }

    char buffer[16384];
    uint64_t totalRead = 0;
    while (std::cin.read(buffer, sizeof(buffer)) || std::cin.gcount() > 0) {
        size_t bytes = static_cast<size_t>(std::cin.gcount());
        totalRead += bytes;
        if (!dryRun) {
            outFile.write(buffer, bytes);
        }
    }

    stats.bytesTransferred += totalRead;
    stats.totalBytes += totalRead;
    stats.filesTransferred++;
    return true;
}

bool PipeStreamManager::syncFileToStdout(const fs::path& srcPath, SyncStatistics& stats) {
    configureBinaryMode();
    std::ifstream srcFile(srcPath, std::ios::binary);
    if (!srcFile) {
        std::cerr << "rsync: failed to open source file " << srcPath << "\n";
        return false;
    }

    char buffer[16384];
    uint64_t totalRead = 0;
    while (srcFile.read(buffer, sizeof(buffer)) || srcFile.gcount() > 0) {
        size_t bytes = static_cast<size_t>(srcFile.gcount());
        totalRead += bytes;
        std::cout.write(buffer, bytes);
    }

    stats.bytesTransferred += totalRead;
    stats.totalBytes += totalRead;
    stats.filesTransferred++;
    return true;
}
