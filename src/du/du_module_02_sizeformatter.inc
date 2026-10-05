    // =========================================================================
    // Size Formatter Utility (Strategy Pattern)
    // =========================================================================
    class SizeFormatter {
    public:
        static std::string format(uint64_t raw_bytes, const Options& opts) {
            if (opts.format_mode == SizeFormatMode::Bytes) {
                return std::to_string(raw_bytes);
            }

            if (opts.format_mode == SizeFormatMode::HumanReadable) {
                return to_human_readable(raw_bytes);
            }

            // Block size divisions (-k, -m, default 1K)
            uint64_t blocks = (raw_bytes + opts.block_size - 1) / opts.block_size;
            return std::to_string(blocks);
        }

    private:
        static std::string to_human_readable(uint64_t bytes) {
            constexpr double unit = 1024.0;
            if (bytes < 1024) {
                return std::to_string(bytes) + "B";
            }

            const char* suffixes[] = { "K", "M", "G", "T", "P", "E" };
            double count = static_cast<double>(bytes);
            int exp = 0;

            while (count >= unit && exp < 6) {
                count /= unit;
                exp++;
            }

            std::ostringstream oss;
            if (count >= 10.0 || exp == 0) {
                oss << static_cast<uint64_t>(std::round(count)) << suffixes[exp - 1];
            } else {
                oss << std::fixed << std::setprecision(1) << count << suffixes[exp - 1];
            }
            return oss.str();
        }
    };
