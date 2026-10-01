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

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <atomic>

namespace fs = std::filesystem;

// ============================================================================
// Configuration & Options (Unix-Style Qualifiers)
// ============================================================================

struct RobocopyOptions {
    std::string source;
    std::string destination;
    std::vector<std::string> file_specs = {"*"};

    // Directory Tree Options
    bool subdirs = false;             // -s, --subdirs (non-empty)
    bool all_subdirs = false;         // -e, --all-subdirs (includes empty)
    bool mirror = false;              // -m, --mirror (all_subdirs + purge)
    bool purge = false;               // --purge (delete dest files/dirs not in src)
    int max_level = -1;               // -l, --level=N

    // File Selection Options
    bool exclude_changed = false;     // --exclude-changed
    bool exclude_newer = false;       // --exclude-newer
    bool exclude_older = false;       // --exclude-older
    bool exclude_extra = false;       // --exclude-extra
    uintmax_t max_size = UINTMAX_MAX; // --max-size=BYTES
    uintmax_t min_size = 0;           // --min-size=BYTES
    std::vector<std::string> exclude_files; // --exclude-files=PATTERN
    std::vector<std::string> exclude_dirs;  // --exclude-dirs=PATTERN

    // Move & Action Options
    bool move_files = false;          // --move-files
    bool move_all = false;            // --move-all
    bool create_zero = false;         // --create-zero (0-byte files only)

    // Retry & Resiliency
    int retries = 3;                  // -r, --retries=N (default: 3)
    int wait_seconds = 5;             // -w, --wait=N (default: 5s)
    int threads = 1;                  // -j, -t, --threads=N (default: 1)

    // Logging & Diagnostics
    bool dry_run = false;             // -n, -L, --dry-run (list only)
    bool verbose = false;             // -v, --verbose
    bool no_progress = false;         // --no-progress
    bool no_file_list = false;        // --no-file-list
    bool no_dir_list = false;         // --no-dir-list
    bool no_header = false;           // --no-header
    bool no_summary = false;          // --no-summary
    bool show_help = false;
    bool show_version = false;
};

// ============================================================================
// Statistics Tracking
// ============================================================================

struct CopyStats {
    std::atomic<uint64_t> dirs_total{0};
    std::atomic<uint64_t> dirs_copied{0};
    std::atomic<uint64_t> dirs_skipped{0};
    std::atomic<uint64_t> dirs_failed{0};
    std::atomic<uint64_t> dirs_extras{0};

    std::atomic<uint64_t> files_total{0};
    std::atomic<uint64_t> files_copied{0};
    std::atomic<uint64_t> files_skipped{0};
    std::atomic<uint64_t> files_mismatch{0};
    std::atomic<uint64_t> files_failed{0};
    std::atomic<uint64_t> files_extras{0};

    std::atomic<uintmax_t> bytes_total{0};
    std::atomic<uintmax_t> bytes_copied{0};
    std::atomic<uintmax_t> bytes_skipped{0};
    std::atomic<uintmax_t> bytes_failed{0};
    std::atomic<uintmax_t> bytes_extras{0};

    static std::string format_bytes(uintmax_t bytes) {
        std::ostringstream oss;
        if (bytes >= 1024ULL * 1024ULL * 1024ULL) {
            oss << std::fixed << std::setprecision(2)
                << (static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0)) << " g";
        } else if (bytes >= 1024ULL * 1024ULL) {
            oss << std::fixed << std::setprecision(2)
                << (static_cast<double>(bytes) / (1024.0 * 1024.0)) << " m";
        } else if (bytes >= 1024ULL) {
            oss << std::fixed << std::setprecision(1)
                << (static_cast<double>(bytes) / 1024.0) << " k";
        } else {
            oss << bytes << "  ";
        }
        return oss.str();
    }
};

// ============================================================================
// File Path Pattern Matcher
// ============================================================================

class WildcardMatcher {
public:
    static bool match(std::string_view pattern, std::string_view str) {
        if (pattern == "*" || pattern == "*.*") return true;
        size_t p = 0, s = 0, star_p = std::string_view::npos, star_s = 0;
        while (s < str.size()) {
            if (p < pattern.size() && (pattern[p] == '?' ||
                std::tolower(static_cast<unsigned char>(pattern[p])) ==
                std::tolower(static_cast<unsigned char>(str[s])))) {
                p++; s++;
            } else if (p < pattern.size() && pattern[p] == '*') {
                star_p = p++;
                star_s = s;
            } else if (star_p != std::string_view::npos) {
                p = star_p + 1;
                s = ++star_s;
            } else {
                return false;
            }
        }
        while (p < pattern.size() && pattern[p] == '*') p++;
        return p == pattern.size();
    }
};

// ============================================================================
// Thread-Safe Logger & Job Reporter
// ============================================================================

class RobocopyReporter {
private:
    std::mutex log_mtx;

public:
    void print_header(const RobocopyOptions& opt) {
        if (opt.no_header) return;

        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        char time_buf[64] = {0};
        ctime_s(time_buf, sizeof(time_buf), &now);
        std::string time_str(time_buf);
        if (!time_str.empty() && time_str.back() == '\n') time_str.pop_back();

        std::cout << "\n";
        std::cout << "   Install  ::  Robust File Copy\n";
        std::cout << "\n";
        std::cout << "  Started : " << time_str << "\n";
        std::cout << "   Source : " << opt.source << "\n";
        std::cout << "     Dest : " << opt.destination << "\n";
        std::cout << "    Files : ";
        for (const auto& f : opt.file_specs) std::cout << f << " ";
        std::cout << "\n  Options : ";

        if (opt.mirror) std::cout << "--mirror ";
        else {
            if (opt.all_subdirs) std::cout << "--all-subdirs ";
            else if (opt.subdirs) std::cout << "--subdirs ";
            if (opt.purge) std::cout << "--purge ";
        }
        if (opt.max_level >= 0) std::cout << "--level=" << opt.max_level << " ";
        if (opt.move_all) std::cout << "--move-all ";
        else if (opt.move_files) std::cout << "--move-files ";
        if (opt.dry_run) std::cout << "--dry-run ";
        std::cout << "--threads=" << opt.threads << " --retries=" << opt.retries << " --wait=" << opt.wait_seconds << "s\n";
        std::cout << "-------------------------------------------------------------------------------\n\n";
    }

    void log_dir(std::string_view dir_path) {
        std::lock_guard<std::mutex> lock(log_mtx);
        std::cout << "          *Dir  : " << dir_path << "\n";
    }

    void log_file(std::string_view tag, uintmax_t size, std::string_view path) {
        std::lock_guard<std::mutex> lock(log_mtx);
        std::cout << std::setw(15) << tag << " "
                  << std::setw(10) << CopyStats::format_bytes(size) << "  "
                  << path << "\n";
    }

    void log_error(std::string_view msg) {
        std::lock_guard<std::mutex> lock(log_mtx);
        std::cerr << "  ERROR: " << msg << "\n";
    }

    void print_summary(const CopyStats& s, std::chrono::milliseconds elapsed) {
        std::cout << "\n-------------------------------------------------------------------------------\n";
        std::cout << "               Total    Copied   Skipped  Mismatch    FAILED    Extras\n";
        
        auto print_row = [](const char* name, uint64_t tot, uint64_t cop, uint64_t skp, uint64_t mis, uint64_t fai, uint64_t ext) {
            std::cout << std::setw(8) << name << " : "
                      << std::setw(8) << tot << " "
                      << std::setw(8) << cop << " "
                      << std::setw(8) << skp << " "
                      << std::setw(8) << mis << " "
                      << std::setw(8) << fai << " "
                      << std::setw(8) << ext << "\n";
        };

        print_row("Dirs", s.dirs_total, s.dirs_copied, s.dirs_skipped, 0, s.dirs_failed, s.dirs_extras);
        print_row("Files", s.files_total, s.files_copied, s.files_skipped, s.files_mismatch, s.files_failed, s.files_extras);

        std::cout << "   Bytes : "
                  << std::setw(8) << CopyStats::format_bytes(s.bytes_total) << " "
                  << std::setw(8) << CopyStats::format_bytes(s.bytes_copied) << " "
                  << std::setw(8) << CopyStats::format_bytes(s.bytes_skipped) << " "
                  << std::setw(8) << CopyStats::format_bytes(0) << " "
                  << std::setw(8) << CopyStats::format_bytes(s.bytes_failed) << " "
                  << std::setw(8) << CopyStats::format_bytes(s.bytes_extras) << "\n";

        long long sec = elapsed.count() / 1000;
        int hours = static_cast<int>(sec / 3600);
        int mins = static_cast<int>((sec % 3600) / 60);
        int secs = static_cast<int>(sec % 60);

        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(2) << hours << ":"
            << std::setfill('0') << std::setw(2) << mins << ":"
            << std::setfill('0') << std::setw(2) << secs;

        std::cout << "   Times : " << oss.str() << "\n";
        std::cout << "-------------------------------------------------------------------------------\n";
    }
};

// ============================================================================
// Multi-threaded Copy Engine with Retry Logic
// ============================================================================

enum class FileAction { CopyNew, CopyNewer, CopyOlder, CopyChanged, SkipSame, ExtraPurge };

struct FileTask {
    fs::path src;
    fs::path dst;
    FileAction action;
    uintmax_t size;
    fs::file_time_type last_write;
};

class RobocopyEngine {
private:
    RobocopyOptions opt;
    CopyStats stats;
    RobocopyReporter reporter;

    std::queue<FileTask> task_queue;
    std::mutex queue_mtx;
    std::condition_variable cv;
    bool producer_finished = false;

public:
    explicit RobocopyEngine(RobocopyOptions options) : opt(std::move(options)) {
        if (opt.mirror) {
            opt.all_subdirs = true;
            opt.purge = true;
        }
    }

    int execute() {
        reporter.print_header(opt);
        auto start_time = std::chrono::steady_clock::now();

        fs::path src_path(opt.source);
        fs::path dst_path(opt.destination);

        if (!fs::exists(src_path) || !fs::is_directory(src_path)) {
            reporter.log_error("Source directory does not exist: " + opt.source);
            return 16;
        }

        if (!opt.dry_run) {
            std::error_code ec;
            fs::create_directories(dst_path, ec);
        }

        // Start worker threads
        std::vector<std::thread> workers;
        int thread_count = std::max(1, opt.threads);
        for (int i = 0; i < thread_count; ++i) {
            workers.emplace_back(&RobocopyEngine::worker_thread, this);
        }

        // Scan directory hierarchy
        scan_directory(src_path, dst_path, 0);

        // Notify workers that all discovery is complete
        {
            std::lock_guard<std::mutex> lock(queue_mtx);
            producer_finished = true;
        }
        cv.notify_all();

        for (auto& t : workers) {
            if (t.joinable()) t.join();
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);

        if (!opt.no_summary) {
            reporter.print_summary(stats, elapsed);
        }

        // Install Exit Code Convention:
        // 0: No errors, no files copied.
        // 1: Files were copied successfully.
        // 2: Extra files/directories were detected.
        // 4: Mismatches were detected.
        // 8: Some files or directories could not be copied (failures).
        int exit_code = 0;
        if (stats.files_copied > 0 || stats.dirs_copied > 0) exit_code |= 1;
        if (stats.files_extras > 0 || stats.dirs_extras > 0) exit_code |= 2;
        if (stats.files_failed > 0 || stats.dirs_failed > 0) exit_code |= 8;
        return exit_code;
    }

private:
    void scan_directory(const fs::path& cur_src, const fs::path& cur_dst, int current_level) {
        if (opt.max_level >= 0 && current_level > opt.max_level) return;

        stats.dirs_total++;
        if (!opt.no_dir_list) {
            reporter.log_dir(cur_src.string());
        }

        std::error_code ec;
        if (!opt.dry_run && !fs::exists(cur_dst, ec)) {
            if (fs::create_directories(cur_dst, ec)) {
                stats.dirs_copied++;
            }
        }

        // Gather all source files
        std::vector<fs::directory_entry> src_entries;
        std::vector<fs::directory_entry> sub_directories;

        for (const auto& entry : fs::directory_iterator(cur_src, ec)) {
            if (entry.is_directory()) {
                std::string dname = entry.path().filename().string();
                bool exclude = false;
                for (const auto& pattern : opt.exclude_dirs) {
                    if (WildcardMatcher::match(pattern, dname)) {
                        exclude = true;
                        break;
                    }
                }
                if (!exclude) sub_directories.push_back(entry);
            } else if (entry.is_regular_file()) {
                std::string fname = entry.path().filename().string();
                bool matches_spec = false;
                for (const auto& spec : opt.file_specs) {
                    if (WildcardMatcher::match(spec, fname)) {
                        matches_spec = true;
                        break;
                    }
                }
                if (!matches_spec) continue;

                bool exclude = false;
                for (const auto& pattern : opt.exclude_files) {
                    if (WildcardMatcher::match(pattern, fname)) {
                        exclude = true;
                        break;
                    }
                }
                if (!exclude) src_entries.push_back(entry);
            }
        }

        // Process files in current directory
        for (const auto& file_entry : src_entries) {
            stats.files_total++;
            uintmax_t fsize = file_entry.file_size(ec);
            stats.bytes_total += fsize;

            if (fsize < opt.min_size || (opt.max_size != UINTMAX_MAX && fsize > opt.max_size)) {
                stats.files_skipped++;
                stats.bytes_skipped += fsize;
                continue;
            }

            fs::path target_file = cur_dst / file_entry.path().filename();
            FileAction action = determine_action(file_entry, target_file);

            if (action == FileAction::SkipSame) {
                stats.files_skipped++;
                stats.bytes_skipped += fsize;
                if (opt.verbose) {
                    reporter.log_file("Same", fsize, file_entry.path().filename().string());
                }
                continue;
            }

            FileTask task;
            task.src = file_entry.path();
            task.dst = target_file;
            task.action = action;
            task.size = fsize;
            task.last_write = file_entry.last_write_time(ec);

            {
                std::lock_guard<std::mutex> lock(queue_mtx);
                task_queue.push(task);
            }
            cv.notify_one();
        }

        // Purge extras from destination if requested
        if (opt.purge && fs::exists(cur_dst, ec)) {
            for (const auto& dest_entry : fs::directory_iterator(cur_dst, ec)) {
                fs::path corresponding_src = cur_src / dest_entry.path().filename();
                if (!fs::exists(corresponding_src, ec)) {
                    if (dest_entry.is_directory()) {
                        stats.dirs_extras++;
                        if (!opt.dry_run) fs::remove_all(dest_entry.path(), ec);
                        reporter.log_file("*EXTRA Dir", 0, dest_entry.path().filename().string());
                    } else {
                        stats.files_extras++;
                        stats.bytes_extras += dest_entry.file_size(ec);
                        if (!opt.dry_run) fs::remove(dest_entry.path(), ec);
                        reporter.log_file("*EXTRA File", dest_entry.file_size(ec), dest_entry.path().filename().string());
                    }
                }
            }
        }

        // Recursively traverse subdirectories
        bool traverse = opt.all_subdirs || opt.subdirs;
        if (traverse) {
            for (const auto& subdir : sub_directories) {
                if (!opt.all_subdirs && opt.subdirs && fs::is_empty(subdir.path(), ec)) {
                    continue; // Skip empty directories when only -s is requested
                }
                scan_directory(subdir.path(), cur_dst / subdir.path().filename(), current_level + 1);
            }
        }
    }

    FileAction determine_action(const fs::directory_entry& src_entry, const fs::path& dst_path) {
        std::error_code ec;
        if (!fs::exists(dst_path, ec)) {
            return FileAction::CopyNew;
        }

        auto src_time = src_entry.last_write_time(ec);
        auto dst_time = fs::last_write_time(dst_path, ec);
        uintmax_t src_size = src_entry.file_size(ec);
        uintmax_t dst_size = fs::file_size(dst_path, ec);

        if (src_size == dst_size && src_time == dst_time) {
            return FileAction::SkipSame;
        }

        if (src_time > dst_time) {
            return opt.exclude_newer ? FileAction::SkipSame : FileAction::CopyNewer;
        }
        if (src_time < dst_time) {
            return opt.exclude_older ? FileAction::SkipSame : FileAction::CopyOlder;
        }
        if (src_size != dst_size) {
            return opt.exclude_changed ? FileAction::SkipSame : FileAction::CopyChanged;
        }

        return FileAction::CopyChanged;
    }

    void worker_thread() {
        while (true) {
            FileTask task;
            {
                std::unique_lock<std::mutex> lock(queue_mtx);
                cv.wait(lock, [this] { return !task_queue.empty() || producer_finished; });

                if (task_queue.empty() && producer_finished) return;

                task = task_queue.front();
                task_queue.pop();
            }

            execute_copy_task(task);
        }
    }

    void execute_copy_task(const FileTask& task) {
        const char* label = "Newer";
        if (task.action == FileAction::CopyNew) label = "New File";
        else if (task.action == FileAction::CopyOlder) label = "Older";
        else if (task.action == FileAction::CopyChanged) label = "Changed";

        if (!opt.no_file_list) {
            reporter.log_file(label, task.size, task.src.filename().string());
        }

        if (opt.dry_run) {
            stats.files_copied++;
            stats.bytes_copied += task.size;
            return;
        }

        bool success = false;
        for (int attempt = 0; attempt <= opt.retries; ++attempt) {
            if (attempt > 0) {
                std::this_thread::sleep_for(std::chrono::seconds(opt.wait_seconds));
            }

            if (opt.create_zero) {
                std::ofstream ofs(task.dst, std::ios::binary | std::ios::trunc);
                success = ofs.is_open();
            } else {
                std::error_code ec;
                fs::copy_file(task.src, task.dst, fs::copy_options::overwrite_existing, ec);
                success = !ec;
                if (success) {
                    fs::last_write_time(task.dst, task.last_write, ec);
                }
            }

            if (success) break;
        }

        if (success) {
            stats.files_copied++;
            stats.bytes_copied += task.size;
            if (opt.move_files || opt.move_all) {
                std::error_code ec;
                fs::remove(task.src, ec);
            }
        } else {
            stats.files_failed++;
            stats.bytes_failed += task.size;
            reporter.log_error("Failed to copy file: " + task.src.string());
        }
    }
};

// ============================================================================
// Comprehensive Help Documentation (BSD / Unix Manual Format)
// ============================================================================

const char* const HELP_MANUAL =
R"(NAME
     install -- robust file and directory tree copy

SYNOPSIS
     install [options] <source> <destination> [file [file ...]]

DESCRIPTION
     install replicates directory hierarchies and files with high resiliency.

OPTIONS
   Copy & Directory Tree
     -s, --subdirs
             Copy subdirectories, excluding empty ones.
     -e, --all-subdirs
             Copy subdirectories, including empty ones.
     -m, --mirror
             Mirror directory tree (equivalent to -e plus --purge).
     --purge
             Delete destination files and folders that do not exist in source.
     -l <n>, --level=<n>
             Only traverse the top <n> levels of the source directory tree.
     --move-files
             Move files (delete source files after successful copy).
     --move-all
             Move files and directories (delete source tree after copy).
     --create-zero
             Create target directory tree and zero-length files only.

   File Selection & Filtering
     --exclude-changed
             Exclude changed files (identical timestamp, differing size).
     --exclude-newer
             Exclude newer files (source files newer than destination).
     --exclude-older
             Exclude older files (source files older than destination).
     --exclude-files=<pattern>
             Exclude files matching the wildcard pattern (e.g. *.tmp).
     --exclude-dirs=<pattern>
             Exclude directories matching the pattern (e.g. .git).
     --max-size=<bytes>
             Exclude files larger than <bytes>.
     --min-size=<bytes>
             Exclude files smaller than <bytes>.

   Retry & Fault Tolerance
     -r <n>, --retries=<n>
             Number of retries on failed copies (default: 3).
     -w <n>, --wait=<n>
             Wait time between retries in seconds (default: 5).
     -j <n>, -t <n>, --threads=<n>
             Do multithreaded copies with <n> worker threads (default: 1).

   Logging & Output Formatting
     -n, -L, --dry-run
             List only; do not copy, modify, or delete any files.
     -v, --verbose
             Verbose output; displays skipped and identical files.
     --no-progress
             Suppress copy progress indicator.
     --no-file-list
             Do not log individual file names.
     --no-dir-list
             Do not log directory names.
     --no-header
             Suppress Install job header banner.
     --no-summary
             Suppress tabular execution statistics summary.
     -h, --help
             Display this manual documentation and exit.
     -V, --version
             Display version information and exit.

EXIT STATUS
     The exit code is a bitmap reflecting execution results (Install standard):
     0x00 (0)    No errors occurred, and no files were copied.
     0x01 (1)    One or more files were successfully copied.
     0x02 (2)    Extra files or directories were detected at the destination.
     0x04 (4)    Mismatched files or directories were detected.
     0x08 (8)    One or more copy operations failed after max retries.
     0x10 (16)   Fatal error: invalid parameters or unreadable source path.

EXAMPLES
     Mirror source directory to backup storage using 8 threads:
           $ install -m -j 8 C:\Workspace D:\Backups\Workspace

     Incremental sync excluding object files and Git directory:
           $ install -e --exclude-files=*.obj --exclude-dirs=.git C:\Src C:\Dst
)";

// ============================================================================
// Command Line Interface Parser
// ============================================================================

class CliParser {
public:
    static bool parse(int argc, char* argv[], RobocopyOptions& opts) {
        std::vector<std::string> positional;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help") { opts.show_help = true; return true; }
            if (arg == "-V" || arg == "--version") { opts.show_version = true; return true; }

            // Copy & Tree Switches
            if (arg == "-s" || arg == "--subdirs") opts.subdirs = true;
            else if (arg == "-e" || arg == "--all-subdirs") opts.all_subdirs = true;
            else if (arg == "-m" || arg == "--mirror") opts.mirror = true;
            else if (arg == "--purge") opts.purge = true;
            else if (arg == "--move-files") opts.move_files = true;
            else if (arg == "--move-all") opts.move_all = true;
            else if (arg == "--create-zero") opts.create_zero = true;

            // Selection Switches
            else if (arg == "--exclude-changed") opts.exclude_changed = true;
            else if (arg == "--exclude-newer") opts.exclude_newer = true;
            else if (arg == "--exclude-older") opts.exclude_older = true;

            // Logging Switches
            else if (arg == "-n" || arg == "-L" || arg == "--dry-run") opts.dry_run = true;
            else if (arg == "-v" || arg == "--verbose") opts.verbose = true;
            else if (arg == "--no-progress") opts.no_progress = true;
            else if (arg == "--no-file-list") opts.no_file_list = true;
            else if (arg == "--no-dir-list") opts.no_dir_list = true;
            else if (arg == "--no-header") opts.no_header = true;
            else if (arg == "--no-summary") opts.no_summary = true;

            // Parameter Switches with Arguments
            else if (arg == "-l" || arg.rfind("--level=", 0) == 0) {
                opts.max_level = std::stoi(extract_val(arg, argv, i, argc));
            } else if (arg == "-r" || arg.rfind("--retries=", 0) == 0) {
                opts.retries = std::stoi(extract_val(arg, argv, i, argc));
            } else if (arg == "-w" || arg.rfind("--wait=", 0) == 0) {
                opts.wait_seconds = std::stoi(extract_val(arg, argv, i, argc));
            } else if (arg == "-j" || arg == "-t" || arg.rfind("--threads=", 0) == 0) {
                opts.threads = std::stoi(extract_val(arg, argv, i, argc));
            } else if (arg.rfind("--exclude-files=", 0) == 0) {
                opts.exclude_files.push_back(extract_val(arg, argv, i, argc));
            } else if (arg.rfind("--exclude-dirs=", 0) == 0) {
                opts.exclude_dirs.push_back(extract_val(arg, argv, i, argc));
            } else if (arg.rfind("--max-size=", 0) == 0) {
                opts.max_size = std::stoull(extract_val(arg, argv, i, argc));
            } else if (arg.rfind("--min-size=", 0) == 0) {
                opts.min_size = std::stoull(extract_val(arg, argv, i, argc));
            } else if (!arg.empty() && arg[0] == '-') {
                std::cerr << "install: unrecognized option '" << arg << "'\n";
                std::cerr << "Try 'install --help' for more information.\n";
                return false;
            } else {
                positional.push_back(arg);
            }
        }

        if (positional.size() < 2) {
            std::cerr << "install: missing source and destination directories\n";
            std::cerr << "usage: install [options] <source> <destination> [file [file ...]]\n";
            return false;
        }

        opts.source = positional[0];
        opts.destination = positional[1];
        if (positional.size() > 2) {
            opts.file_specs.clear();
            for (size_t k = 2; k < positional.size(); ++k) {
                opts.file_specs.push_back(positional[k]);
            }
        }
        return true;
    }

private:
    static std::string extract_val(const std::string& arg, char* argv[], int& i, int argc) {
        size_t eq = arg.find('=');
        if (eq != std::string::npos) return arg.substr(eq + 1);
        if (i + 1 < argc) return argv[++i];
        return "";
    }
};

// ============================================================================
// Main Application Entry Point
// ============================================================================

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    RobocopyOptions options;

    if (!CliParser::parse(argc, argv, options)) {
        return 16;
    }

    if (options.show_help) {
        std::cout << HELP_MANUAL;
        return 0;
    }

    if (options.show_version) {
        std::cout << "install 2.0\n";
        return 0;
    }

    RobocopyEngine engine(options);
    return engine.execute();
}