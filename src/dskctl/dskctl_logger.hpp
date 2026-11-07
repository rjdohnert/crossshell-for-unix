#ifndef DSKCTL_LOGGER_HPP
#define DSKCTL_LOGGER_HPP

#include <windows.h>

#include <filesystem>
#include <string_view>

class DskctlLogger {
public:
    DskctlLogger();
    ~DskctlLogger();

    DskctlLogger(const DskctlLogger&) = delete;
    DskctlLogger& operator=(const DskctlLogger&) = delete;

    bool WriteLine(std::string_view line);
    bool Finish();
    const std::filesystem::path& GetPath() const;

private:
    std::filesystem::path path_;
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    bool healthy_ = false;
};

#endif
