/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * ============================================================================
 * SINGLE FILE INDEX: rsync.cpp
 * ============================================================================
 * WinRsync - Object-Oriented Differential Stream & Directory Synchronizer
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [SYNC STATISTICS & OPTIONS] ............ SyncStatistics and RsyncOptions classes
 * 2. [DELTA & CHECKSUM ENGINES] ............. Adler32, Fnv1a64, BlockSignature, DeltaEngine
 * 3. [PIPING & STREAM HANDLER] .............. PipeStreamManager class (binary standard I/O)
 * 4. [CORE SYNCHRONIZATION ENGINE] .......... RsyncEngine class (file/directory sync & delta)
 * 5. [APPLICATION CONTROLLER] ............... RsyncApp class and main entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <fcntl.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <string_view>
#include <memory>
#include <chrono>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <cstdint>
#include <sstream>
#include <unordered_map>
#include <functional>

namespace fs = std::filesystem;

// ============================================================================
// 1. SYNC STATISTICS & OPTIONS
// ============================================================================

class SyncStatistics {
public:
    uint64_t totalFilesChecked{0};
    uint64_t filesTransferred{0};
    uint64_t filesDeleted{0};
    uint64_t bytesTransferred{0};
    uint64_t totalBytes{0};
    uint64_t deltaBytesSaved{0};
    std::chrono::duration<double> elapsedTime{0};

    void print() const {
        std::cout << "\n------------------ Rsync Transfer Statistics ------------------\n";
        std::cout << "Files Checked       : " << totalFilesChecked << "\n";
        std::cout << "Files Transferred   : " << filesTransferred << "\n";
        std::cout << "Files Deleted       : " << filesDeleted << "\n";
        std::cout << "Total File Size     : " << formatBytes(totalBytes) << "\n";
        std::cout << "Literal Data Sent   : " << formatBytes(bytesTransferred) << "\n";
        std::cout << "Delta Data Saved    : " << formatBytes(deltaBytesSaved) << "\n";
        std::cout << "Elapsed Time        : " << std::fixed << std::setprecision(3) << elapsedTime.count() << " seconds\n";
        double speed = (elapsedTime.count() > 0) ? (bytesTransferred / elapsedTime.count()) : 0;
        std::cout << "Transfer Speed      : " << formatBytes(static_cast<uint64_t>(speed)) << "/s\n";
        std::cout << "---------------------------------------------------------------\n";
    }

private:
    static std::string formatBytes(uint64_t bytes) {
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        int i = 0;
        double size = static_cast<double>(bytes);
        while (size >= 1024.0 && i < 4) {
            size /= 1024.0;
            i++;
        }
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << size << " " << units[i];
        return oss.str();
    }
};

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

    static RsyncOptions parse(int argc, char* argv[]) {
        RsyncOptions opts;
        std::vector<std::string> positional;

        for (int i = 1; i < argc; ++i) {
            std::string_view arg = argv[i];

            if (arg == "-h" || arg == "--help") {
                showHelp();
                std::exit(0);
            } else if (arg == "-v" || arg == "--verbose") {
                opts.verbose = true;
            } else if (arg == "-q" || arg == "--quiet") {
                opts.quiet = true;
            } else if (arg == "-r" || arg == "--recursive") {
                opts.recursive = true;
            } else if (arg == "-a" || arg == "--archive") {
                opts.archive = true;
                opts.recursive = true;
                opts.preserveTimes = true;
            } else if (arg == "-c" || arg == "--checksum") {
                opts.checksumOnly = true;
            } else if (arg == "-u" || arg == "--update") {
                opts.updateOnly = true;
            } else if (arg == "-n" || arg == "--dry-run") {
                opts.dryRun = true;
            } else if (arg == "--delete") {
                opts.deleteTarget = true;
            } else if (arg == "--stats") {
                opts.showStats = true;
            } else if (arg == "--progress") {
                opts.showProgress = true;
            } else if (arg == "-t" || arg == "--times") {
                opts.preserveTimes = true;
            } else if (arg.rfind("--block-size=", 0) == 0) {
                opts.blockSize = std::stoul(std::string(arg.substr(13)));
                if (opts.blockSize == 0) opts.blockSize = 4096;
            } else if (arg.length() > 1 && arg[0] == '-' && arg[1] != '-') {
                for (size_t j = 1; j < arg.length(); ++j) {
                    switch (arg[j]) {
                        case 'v': opts.verbose = true; break;
                        case 'q': opts.quiet = true; break;
                        case 'r': opts.recursive = true; break;
                        case 'a': opts.archive = true; opts.recursive = true; opts.preserveTimes = true; break;
                        case 'c': opts.checksumOnly = true; break;
                        case 'u': opts.updateOnly = true; break;
                        case 'n': opts.dryRun = true; break;
                        case 't': opts.preserveTimes = true; break;
                        default:
                            std::cerr << "rsync: unknown option -- '" << arg[j] << "'\n";
                            std::cerr << "Try 'rsync --help' for more information.\n";
                            std::exit(1);
                    }
                }
            } else {
                positional.push_back(std::string(arg));
            }
        }

        if (positional.size() < 2) {
            std::cerr << "rsync: missing destination file operand after '" 
                      << (positional.empty() ? "" : positional[0]) << "'\n";
            std::cerr << "Try 'rsync --help' for more information.\n";
            std::exit(1);
        }

        opts.sourcePath = positional[0];
        opts.destinationPath = positional[1];

        if (opts.sourcePath == "-") opts.pipeFromStdin = true;
        if (opts.destinationPath == "-") opts.pipeToStdout = true;

        return opts;
    }

    static void showHelp() {
        std::cout <<
R"(rsync(1)                   CrossShell for UNIX Reference Manual                  rsync(1)

NAME
    rsync - synchronize files, directories, and binary streams

SYNOPSIS
    rsync [OPTIONS] SRC DEST
    rsync [OPTIONS] - DEST
    rsync [OPTIONS] SRC -

DESCRIPTION
    Performs differential synchronization using size and modification
    time or checksum comparison. Directory synchronization requires
    -r or -a.

OPTIONS
    -a, --archive
        Archive mode; equivalent to -r -t (preserves timestamps).
    -r, --recursive
        Recurse into directories.
    -v, --verbose
        Increase verbosity and log transferred files.
    -q, --quiet
        Suppress non-error messages.
    -c, --checksum
        Skip based on checksum comparison instead of mod-time and size.
    -u, --update
        Skip files that are newer on the receiver.
    -t, --times
        Preserve modification times.
    -n, --dry-run
        Perform a trial run with no changes made.
    --delete
        Delete destination entries absent from the source.
    --block-size=SIZE
        Set delta chunk size for calculations (default: 4096).
    --progress
        Show transfer progress during synchronization.
    --stats
        Print transfer statistics upon completion.
    -h, --help
        Display this comprehensive reference manual and exit.

PIPING AND STREAMING
    A source of '-' reads binary data from standard input. A destination
    of '-' writes binary data to standard output.

EXAMPLES
    type archive.tar | rsync - C:\Backups\archive.tar
        Stream archive into destination file via standard input.

    rsync C:\Data\database.db - | downstream_processor
        Stream source file to standard output.

    rsync -av --delete C:\Projects D:\Backups\Projects
        Mirror directory tree and delete removed destination files.

    rsync -avn C:\Source C:\Destination
        Dry run preview of directory synchronization.

    rsync -vc C:\LargeVM.vhdx D:\Backups\LargeVM.vhdx
        Differential update using block checksums.

EXIT STATUS
    0   Successful synchronization or help displayed.
    1   Invalid options, missing operands, stream failure, or sync failure.

    CrossShell for UNIX                                                   rsync(1)
)";
    }
};

// ============================================================================
// 2. DELTA & CHECKSUM ENGINES
// ============================================================================

class Adler32RollingChecksum {
private:
    static constexpr uint32_t MOD_ADLER = 65521;
    uint32_t a{1};
    uint32_t b{0};

public:
    Adler32RollingChecksum() = default;

    void compute(const uint8_t* data, size_t len) {
        a = 1;
        b = 0;
        for (size_t i = 0; i < len; ++i) {
            a = (a + data[i]) % MOD_ADLER;
            b = (b + a) % MOD_ADLER;
        }
    }

    void roll(uint8_t outChar, uint8_t inChar, size_t blockLen) {
        a = (a + MOD_ADLER + inChar - outChar) % MOD_ADLER;
        b = (b + MOD_ADLER + a - (static_cast<uint32_t>(blockLen) * outChar) - 1) % MOD_ADLER;
    }

    [[nodiscard]] uint32_t getValue() const {
        return (b << 16) | a;
    }
};

class Fnv1a64StrongHash {
public:
    static uint64_t compute(const uint8_t* data, size_t len) {
        uint64_t hash = 0xcbf29ce484222325ULL;
        for (size_t i = 0; i < len; ++i) {
            hash ^= static_cast<uint64_t>(data[i]);
            hash *= 0x100000001b3ULL;
        }
        return hash;
    }

    static uint64_t computeFile(const fs::path& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) return 0;

        uint64_t hash = 0xcbf29ce484222325ULL;
        std::vector<uint8_t> buffer(65536);
        while (file.read(reinterpret_cast<char*>(buffer.data()), buffer.size()) || file.gcount() > 0) {
            size_t bytes = static_cast<size_t>(file.gcount());
            for (size_t i = 0; i < bytes; ++i) {
                hash ^= static_cast<uint64_t>(buffer[i]);
                hash *= 0x100000001b3ULL;
            }
        }
        return hash;
    }
};

struct BlockSignature {
    size_t index{0};
    uint32_t weakHash{0};
    uint64_t strongHash{0};
};

class DeltaEngine {
private:
    size_t blockSize;

public:
    explicit DeltaEngine(size_t chunkSize) : blockSize(chunkSize) {}

    std::vector<BlockSignature> generateSignatures(const fs::path& filePath) const {
        std::vector<BlockSignature> signatures;
        std::ifstream file(filePath, std::ios::binary);
        if (!file) return signatures;

        std::vector<uint8_t> buffer(blockSize);
        size_t blockIdx = 0;

        while (file.read(reinterpret_cast<char*>(buffer.data()), blockSize) || file.gcount() > 0) {
            size_t bytesRead = static_cast<size_t>(file.gcount());
            Adler32RollingChecksum adler;
            adler.compute(buffer.data(), bytesRead);
            uint64_t strong = Fnv1a64StrongHash::compute(buffer.data(), bytesRead);

            signatures.push_back({blockIdx++, adler.getValue(), strong});
        }
        return signatures;
    }

    bool applyDeltaSync(const fs::path& src, const fs::path& dst, SyncStatistics& stats, bool dryRun) {
        if (!fs::exists(dst)) {
            uint64_t srcSize = fs::file_size(src);
            stats.bytesTransferred += srcSize;
            stats.totalBytes += srcSize;
            stats.filesTransferred++;

            if (!dryRun) {
                fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
            }
            return true;
        }

        uint64_t srcSize = fs::file_size(src);
        stats.totalBytes += srcSize;

        auto signatures = generateSignatures(dst);
        std::unordered_map<uint32_t, std::vector<BlockSignature>> sigLookup;
        for (const auto& sig : signatures) {
            sigLookup[sig.weakHash].push_back(sig);
        }

        std::ifstream srcFile(src, std::ios::binary);
        if (!srcFile) return false;

        fs::path tempTarget = dst.string() + ".rsync_tmp";
        std::ofstream outFile;
        if (!dryRun) {
            outFile.open(tempTarget, std::ios::binary);
            if (!outFile) return false;
        }

        std::ifstream dstReader(dst, std::ios::binary);
        std::vector<uint8_t> window(blockSize);
        uint64_t matchedBytes = 0;
        uint64_t literalBytes = 0;

        while (srcFile.read(reinterpret_cast<char*>(window.data()), blockSize) || srcFile.gcount() > 0) {
            size_t bytesRead = static_cast<size_t>(srcFile.gcount());
            Adler32RollingChecksum adler;
            adler.compute(window.data(), bytesRead);
            uint32_t weak = adler.getValue();

            bool blockMatched = false;
            if (sigLookup.find(weak) != sigLookup.end()) {
                uint64_t strong = Fnv1a64StrongHash::compute(window.data(), bytesRead);
                for (const auto& matchSig : sigLookup[weak]) {
                    if (matchSig.strongHash == strong) {
                        blockMatched = true;
                        matchedBytes += bytesRead;

                        if (!dryRun) {
                            dstReader.seekg(matchSig.index * blockSize);
                            std::vector<uint8_t> dstBlock(bytesRead);
                            dstReader.read(reinterpret_cast<char*>(dstBlock.data()), bytesRead);
                            outFile.write(reinterpret_cast<char*>(dstBlock.data()), bytesRead);
                        }
                        break;
                    }
                }
            }

            if (!blockMatched) {
                literalBytes += bytesRead;
                if (!dryRun) {
                    outFile.write(reinterpret_cast<char*>(window.data()), bytesRead);
                }
            }
        }

        if (!dryRun) {
            outFile.close();
            dstReader.close();
            srcFile.close();
            fs::rename(tempTarget, dst);
        }

        stats.bytesTransferred += literalBytes;
        stats.deltaBytesSaved += matchedBytes;
        stats.filesTransferred++;
        return true;
    }
};

// ============================================================================
// 3. PIPING & STREAM HANDLER
// ============================================================================

class PipeStreamManager {
public:
    static void configureBinaryMode() {
        #ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
        #endif
    }

    static bool syncStdinToFile(const fs::path& dstPath, SyncStatistics& stats, bool dryRun) {
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

    static bool syncFileToStdout(const fs::path& srcPath, SyncStatistics& stats) {
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
};

// ============================================================================
// 4. CORE SYNCHRONIZATION ENGINE
// ============================================================================

class RsyncEngine {
private:
    RsyncOptions options;
    SyncStatistics stats;
    DeltaEngine deltaEngine;

public:
    explicit RsyncEngine(const RsyncOptions& opts)
        : options(opts), deltaEngine(opts.blockSize) {}

    int execute() {
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

private:
    bool shouldUpdate(const fs::path& src, const fs::path& dst) {
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

    void syncSingleFile(const fs::path& src, const fs::path& dst) {
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

    void syncDirectory(const fs::path& srcDir, const fs::path& dstDir) {
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

    void finalize(std::chrono::time_point<std::chrono::high_resolution_clock> startTime) {
        auto endTime = std::chrono::high_resolution_clock::now();
        stats.elapsedTime = endTime - startTime;
        if (options.showStats && !options.quiet) {
            stats.print();
        }
    }
};

// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================

class RsyncApp {
public:
    static int run(int argc, char* argv[]) {
        SetConsoleOutputCP(CP_UTF8);

        try {
            RsyncOptions options = RsyncOptions::parse(argc, argv);
            RsyncEngine engine(options);
            return engine.execute();
        } catch (const std::exception& ex) {
            std::cerr << "rsync error: " << ex.what() << "\n";
            return 1;
        }
    }
};

int main(int argc, char* argv[]) {
    return RsyncApp::run(argc, argv);
}