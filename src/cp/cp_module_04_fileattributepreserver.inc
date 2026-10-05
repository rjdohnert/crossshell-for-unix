class FileAttributePreserver {
public:
    static void preserveAttributes(const fs::path& src, const fs::path& dest, bool preserveTimestamps, bool preserveExtended) {
        std::error_code ec;

        if (preserveTimestamps) {
            auto lwt = fs::last_write_time(src, ec);
            if (!ec) {
                fs::last_write_time(dest, lwt, ec);
            }
        }

        if (preserveExtended) {
            DWORD attr = GetFileAttributesW(src.c_str());
            if (attr != INVALID_FILE_ATTRIBUTES) {
                SetFileAttributesW(dest.c_str(), attr & ~(FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_DIRECTORY));
            }
        }
    }
};

// ============================================================================
// Class: CopyEngine
// Core executor that manages directory traversal, conflict handling, and I/O.
// ============================================================================
