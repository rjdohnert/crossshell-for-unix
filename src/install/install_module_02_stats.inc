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

