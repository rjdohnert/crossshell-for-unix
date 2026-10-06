#include "sync_engine.hpp"

bool RsyncEngine::shouldUpdate(const fs::path& src, const fs::path& dst) {
    if (!fs::exists(dst)) return true;

    if (options.checksumOnly) {
        return Fnv1a64StrongHash::computeFile(src) != Fnv1a64StrongHash::computeFile(dst);
    }

    auto srcSize = fs::file_size(src);
    auto dstSize = fs::file_size(dst);
    if (srcSize != dstSize) return true;

    auto srcTime = fs::last_write_time(src);
    auto dstTime = fs::last_write_time(dst);

    if (options.updateOnly) {
        return srcTime > dstTime;
    }

    return srcTime != dstTime;
}

void RsyncEngine::syncSingleFile(const fs::path& src, const fs::path& dst) {
    stats.totalFilesChecked++;
    fs::path targetFile = dst;
    if (fs::is_directory(dst)) {
        targetFile = dst / src.filename();
    }

    if (shouldUpdate(src, targetFile)) {
        if (options.verbose && !options.quiet) {
            std::cout << src.filename().string() << "\n";
        }

        deltaEngine.applyDeltaSync(src, targetFile, stats, options.dryRun);

        if (options.preserveTimes && !options.dryRun && fs::exists(targetFile)) {
            fs::last_write_time(targetFile, fs::last_write_time(src));
        }
    }
}

void RsyncEngine::syncDirectory(const fs::path& srcDir, const fs::path& dstDir) {
    if (!options.dryRun && !fs::exists(dstDir)) {
        fs::create_directories(dstDir);
    }

    for (const auto& entry : fs::recursive_directory_iterator(srcDir)) {
        const auto& srcItem = entry.path();
        auto relPath = fs::relative(srcItem, srcDir);
        auto dstItem = dstDir / relPath;

        stats.totalFilesChecked++;

        if (fs::is_directory(srcItem)) {
            if (!options.dryRun && !fs::exists(dstItem)) {
                fs::create_directories(dstItem);
            }
        } else if (fs::is_regular_file(srcItem)) {
            if (shouldUpdate(srcItem, dstItem)) {
                if (options.verbose && !options.quiet) {
                    std::cout << relPath.string() << "\n";
                }

                deltaEngine.applyDeltaSync(srcItem, dstItem, stats, options.dryRun);

                if (options.preserveTimes && !options.dryRun && fs::exists(dstItem)) {
                    fs::last_write_time(dstItem, fs::last_write_time(srcItem));
                }
            }
        }
    }

    if (options.deleteTarget && fs::exists(dstDir)) {
        for (const auto& entry : fs::recursive_directory_iterator(dstDir)) {
            const auto& dstItem = entry.path();
            auto relPath = fs::relative(dstItem, dstDir);
            auto srcItem = srcDir / relPath;

            if (!fs::exists(srcItem)) {
                stats.filesDeleted++;
                if (options.verbose && !options.quiet) {
                    std::cout << "deleting " << relPath.string() << "\n";
                }
                if (!options.dryRun) {
                    fs::remove_all(dstItem);
                }
            }
        }
    }
}

void RsyncEngine::finalize(std::chrono::time_point<std::chrono::high_resolution_clock> startTime) {
    auto endTime = std::chrono::high_resolution_clock::now();
    stats.elapsedTime = endTime - startTime;
    if (options.showStats && !options.quiet) {
        stats.print();
    }
}

int RsyncEngine::execute() {
    auto startTime = std::chrono::high_resolution_clock::now();

    if (options.pipeFromStdin) {
        if (options.verbose) std::cerr << "receiving file list and payload from STDIN...\n";
        bool ok = PipeStreamManager::syncStdinToFile(options.destinationPath, stats, options.dryRun);
        finalize(startTime);
        return ok ? 0 : 1;
    }

    if (options.pipeToStdout) {
        bool ok = PipeStreamManager::syncFileToStdout(options.sourcePath, stats);
        finalize(startTime);
        return ok ? 0 : 1;
    }

    fs::path src(options.sourcePath);
    fs::path dst(options.destinationPath);

    if (!fs::exists(src)) {
        std::cerr << "rsync: change_dir " << src << " failed: No such file or directory\n";
        return 1;
    }

    if (fs::is_directory(src)) {
        if (!options.recursive) {
            std::cerr << "skipping directory " << src.string() << "\n";
            return 0;
        }
        syncDirectory(src, dst);
    } else {
        syncSingleFile(src, dst);
    }

    finalize(startTime);
    return 0;
}
