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

