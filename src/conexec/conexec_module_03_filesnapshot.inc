struct FileSnapshot {
    fs::file_time_type lastWrite;
    uintmax_t size;
};

// --- RAII Resource Wrappers ---
