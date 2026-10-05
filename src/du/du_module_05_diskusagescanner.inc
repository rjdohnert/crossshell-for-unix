    // =========================================================================
    // Recursive Disk Usage Scanner
    // =========================================================================
    class DiskUsageScanner {
    private:
        Options opts_;
        uint64_t grand_total_bytes_ = 0;

    public:
        explicit DiskUsageScanner(Options opts) : opts_(std::move(opts)) {
            if (opts_.summarize_only) {
                opts_.max_depth = 0;
            }
        }

        bool scan_all(const std::vector<std::string>& targets) {
            bool all_success = true;

            for (const auto& target_str : targets) {
                fs::path target(target_str);
                std::error_code ec;

                if (!fs::exists(target, ec)) {
                    std::cerr << "du: cannot access '" << target_str << "': No such file or directory\n";
                    all_success = false;
                    continue;
                }

                uint64_t path_bytes = 0;
                if (fs::is_directory(target, ec)) {
                    path_bytes = scan_directory(target, 0);
                } else {
                    path_bytes = calculate_item_size(target);
                    print_entry(path_bytes, target.string());
                }
                grand_total_bytes_ += path_bytes;
            }

            if (opts_.total) {
                print_entry(grand_total_bytes_, "total");
            }

            return all_success;
        }

    private:
        uint64_t calculate_item_size(const fs::path& p) {
            std::error_code ec;
            if (fs::is_symlink(p, ec) && !opts_.dereference) {
                return 0;
            }

            uint64_t size = fs::file_size(p, ec);
            if (ec) return 0;

            if (opts_.apparent_size) {
                return size;
            }

            // Calculate disk allocation size rounded up to cluster size
            if (size == 0) return 0;
            return ((size + opts_.cluster_size - 1) / opts_.cluster_size) * opts_.cluster_size;
        }

        uint64_t scan_directory(const fs::path& dir, int current_depth) {
            std::error_code ec;
            uint64_t dir_total = 0;

            if (PathFilter::is_excluded(dir.filename().string(), opts_.exclude_patterns)) {
                return 0;
            }

            auto iter_opts = fs::directory_options::skip_permission_denied;
            if (opts_.dereference) {
                iter_opts |= fs::directory_options::follow_directory_symlink;
            }

            for (const auto& entry : fs::directory_iterator(dir, iter_opts, ec)) {
                const auto& item_path = entry.path();
                std::string item_name = item_path.filename().string();

                if (PathFilter::is_excluded(item_name, opts_.exclude_patterns)) {
                    continue;
                }

                if (entry.is_directory(ec)) {
                    uint64_t sub_size = scan_directory(item_path, current_depth + 1);
                    dir_total += sub_size;
                } else {
                    uint64_t file_size = calculate_item_size(item_path);
                    dir_total += file_size;

                    if (opts_.all_files) {
                        if (opts_.max_depth == -1 || current_depth + 1 <= opts_.max_depth) {
                            print_entry(file_size, item_path.string());
                        }
                    }
                }
            }

            if (opts_.max_depth == -1 || current_depth <= opts_.max_depth) {
                print_entry(dir_total, dir.string());
            }

            return dir_total;
        }

        void print_entry(uint64_t bytes, const std::string& name) const {
            std::cout << SizeFormatter::format(bytes, opts_) << "\t" << name
                      << (opts_.null_terminated ? '\0' : '\n');
        }
    };
