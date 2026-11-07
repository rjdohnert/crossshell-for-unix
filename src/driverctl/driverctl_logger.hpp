#ifndef DRIVERCTL_LOGGER_HPP
#define DRIVERCTL_LOGGER_HPP

#include <windows.h>

#include <filesystem>
#include <string_view>

class ReportLogger {
public:
    ReportLogger();
    ~ReportLogger();

    ReportLogger(const ReportLogger&) = delete;
    ReportLogger& operator=(const ReportLogger&) = delete;

    /* Write one report line to stdout and, when available, the unique log file. */
    void WriteLine(std::string_view line);
    bool Finish();

    const std::filesystem::path& GetPath() const;

private:
    void WriteLogLine(std::string_view line);

    std::filesystem::path logPath_;
    HANDLE logHandle_ = INVALID_HANDLE_VALUE;
    bool logHealthy_ = false;
};

#endif
