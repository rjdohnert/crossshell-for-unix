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

