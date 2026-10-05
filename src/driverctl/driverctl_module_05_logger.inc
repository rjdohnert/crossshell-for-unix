// Handles file logging to the user's home folder
class ReportLogger {
private:
    fs::path logPath;
    std::ofstream logFile;

public:
    ReportLogger() {
        PWSTR userProfile = nullptr;
        fs::path baseDir;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Profile, 0, nullptr, &userProfile))) {
            baseDir = userProfile;
            CoTaskMemFree(userProfile);
        } else {
            // Fall back to the current directory if the profile path is unavailable.
            std::error_code ec;
            baseDir = fs::current_path(ec);
            std::cerr << "[driverctl] Warning: could not resolve user profile folder; "
                      << "writing log to current directory.\n";
        }

        // Store logs in a dedicated "driverctl-logs" directory under the base dir.
        std::error_code ec;
        fs::path logDir = baseDir / "driverctl-logs";
        if (!fs::exists(logDir, ec)) {
            if (!fs::create_directories(logDir, ec) || ec) {
                std::cerr << "[driverctl] Warning: could not create log directory: "
                          << logDir.string() << " (" << ec.message() << "); using base directory.\n";
                logDir = baseDir;
            }
        }

        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
        localtime_s(&tm, &t);
        std::ostringstream ts;
        ts << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");
        std::string filename = "driverctl_diag_" + ts.str() + ".log";
        // Note: opening truncates any existing file of the same name; the
        // timestamp makes collisions unlikely (same-second re-runs only).
        logPath = logDir / filename;
        logFile.open(logPath);
        if (logFile.fail()) {
            std::cerr << "[driverctl] Warning: failed to open log file: "
                      << logPath.string() << "\n";
        }
    }

    void WriteLine(std::string_view line) {
        std::cout << line << "\n";
        if (logFile.is_open()) {
            logFile << line << "\n";
        }
    }

    fs::path GetPath() const { return logPath; }
};
