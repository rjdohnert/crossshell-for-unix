class CpioHeaderCodec {
public:
    static uint32_t ParseHex8(const char* buf) {
        char tmp[9] = {0};
        std::memcpy(tmp, buf, 8);
        return static_cast<uint32_t>(std::strtoul(tmp, nullptr, 16));
    }

    static void FormatHex8(char* buf, uint32_t val) {
        snprintf(buf, 9, "%08X", val);
    }

    static void WritePadding(std::ostream& out, size_t count) {
        static const char zeros[4] = {0, 0, 0, 0};
        if (count > 0 && count <= 4) {
            out.write(zeros, count);
        }
    }

    static void SkipBytes(std::istream& in, size_t count) {
        char buf[8192];
        while (count > 0 && in) {
            size_t chunk = (std::min)(count, sizeof(buf));
            in.read(buf, chunk);
            count -= in.gcount();
            if (in.gcount() == 0) break;
        }
    }

    static uint32_t GetMtimeSec(const fs::path& p) {
        std::error_code ec;
        auto ftime = fs::last_write_time(p, ec);
        if (ec) return 0;

        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        return static_cast<uint32_t>(std::chrono::system_clock::to_time_t(sctp));
    }
};

// ============================================================================
// 3. ARCHIVE PROCESSING ENGINE
// ============================================================================

