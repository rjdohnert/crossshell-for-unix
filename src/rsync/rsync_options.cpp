#include "rsync_options.hpp"

std::string SyncStatistics::formatBytes(uint64_t bytes) {
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

void SyncStatistics::print() const {
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

RsyncOptions RsyncOptions::parse(int argc, char* argv[]) {
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

void RsyncOptions::showHelp() {
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
