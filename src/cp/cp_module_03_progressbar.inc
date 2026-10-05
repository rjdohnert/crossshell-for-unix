class ProgressBar {
private:
    uint64_t totalBytes = 0;
    uint64_t transferredBytes = 0;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point lastRenderTime;
    int barWidth = 32;
    bool cursorHidden = false;

    static std::string formatSize(uint64_t bytes) {
        constexpr const char* suffixes[] = { "B", "KB", "MB", "GB", "TB" };
        int idx = 0;
        double size = static_cast<double>(bytes);
        while (size >= 1024.0 && idx < 4) {
            size /= 1024.0;
            idx++;
        }
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(idx == 0 ? 0 : 1) << size << " " << suffixes[idx];
        return oss.str();
    }

    static std::string formatTime(uint64_t seconds) {
        int m = static_cast<int>(seconds / 60);
        int s = static_cast<int>(seconds % 60);
        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(2) << m << ":" << std::setw(2) << s;
        return oss.str();
    }

    void hideCursor() {
        if (!cursorHidden) {
            std::cout << "\x1b[?25l" << std::flush;
            cursorHidden = true;
        }
    }

    void showCursor() {
        if (cursorHidden) {
            std::cout << "\x1b[?25h" << std::flush;
            cursorHidden = false;
        }
    }

public:
    ProgressBar() {
        startTime = std::chrono::steady_clock::now();
        lastRenderTime = startTime;
    }

    ~ProgressBar() {
        showCursor();
    }

    void reset(uint64_t fileBytes) {
        totalBytes = fileBytes;
        transferredBytes = 0;
        startTime = std::chrono::steady_clock::now();
        lastRenderTime = startTime;
    }

    void update(uint64_t chunkBytes, const std::string& currentFileName, bool forceRender = false) {
        transferredBytes += chunkBytes;
        auto now = std::chrono::steady_clock::now();
        auto msSinceLast = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastRenderTime).count();

        if (!forceRender && msSinceLast < 80 && transferredBytes < totalBytes) {
            return;
        }
        lastRenderTime = now;

        hideCursor();

        double progress = (totalBytes > 0) ? (static_cast<double>(transferredBytes) / totalBytes) : 1.0;
        progress = (std::min)(1.0, (std::max)(0.0, progress));

        auto elapsedSec = std::chrono::duration_cast<std::chrono::duration<double>>(now - startTime).count();
        double speed = (elapsedSec > 0.001) ? (transferredBytes / elapsedSec) : 0.0;
        uint64_t etaSec = (speed > 0.0 && transferredBytes < totalBytes) 
                          ? static_cast<uint64_t>((totalBytes - transferredBytes) / speed) : 0;

        int filled = static_cast<int>(progress * barWidth);
        int empty = barWidth - filled;

        std::string displayName = currentFileName;
        if (displayName.length() > 22) {
            displayName = displayName.substr(0, 19) + "...";
        }

        std::ostringstream oss;
        oss << "\r"
            << std::left << std::setw(22) << displayName << " "
            << "[" << std::string(filled, '=') 
            << (filled < barWidth ? ">" : "") 
            << std::string((empty > 0 ? empty - 1 : 0), ' ') << "] "
            << std::right << std::setw(3) << static_cast<int>(progress * 100.0) << "% "
            << std::setw(9) << formatSize(static_cast<uint64_t>(speed)) << "/s "
            << "ETA " << formatTime(etaSec) << "  \x1b[K";

        std::cout << oss.str() << std::flush;
    }

    void finish() {
        showCursor();
        std::cout << "\n" << std::flush;
    }
};

// ============================================================================
// Class: FileAttributePreserver
// Handles Windows timestamps and file security/attribute preservation.
// ============================================================================
