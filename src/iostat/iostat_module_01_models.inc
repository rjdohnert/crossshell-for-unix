// ============================================================================
// 1. DATA STRUCTURES & CONFIGURATION
// ============================================================================

struct DiskStats {
    double reads_sec = 0.0;
    double writes_sec = 0.0;
    double read_bytes_sec = 0.0;
    double write_bytes_sec = 0.0;
    double pct_disk_time = 0.0;
    double queue_length = 0.0;
};

struct IostatOptions {
    int interval = 0;
    int count = -1;
    double scale = 1024.0;
    std::string unit = "kB";
    bool saw_interval = false;
    bool saw_count = false;
};

