/*
 * BSD 3-Clause License
 * Copyright (c) 2026, Roberto J Dohnert
 */

#include "driverctl_logger.hpp"
#include "driverctl_utilities.hpp"

#include <knownfolders.h>
#include <shlobj.h>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

ReportLogger::ReportLogger() {
    PWSTR profilePath = nullptr;
    fs::path baseDirectory;
    HRESULT profileResult = SHGetKnownFolderPath(
        FOLDERID_Profile, 0, nullptr, &profilePath);
    if (SUCCEEDED(profileResult) && profilePath) {
        baseDirectory = profilePath;
        CoTaskMemFree(profilePath);
    } else {
        if (profilePath) CoTaskMemFree(profilePath);
        std::error_code currentPathError;
        baseDirectory = fs::current_path(currentPathError);
        if (currentPathError) {
            std::cerr << "[driverctl] Warning: cannot resolve a log directory: "
                      << currentPathError.message() << '\n';
            return;
        }
        std::cerr << "[driverctl] Warning: could not resolve the user profile; "
                  << "using the current directory for logs.\n";
    }

    std::error_code directoryError;
    fs::path logDirectory = baseDirectory / "driverctl-logs";
    if (!fs::exists(logDirectory, directoryError)) {
        if (directoryError || !fs::create_directories(logDirectory, directoryError) ||
            directoryError) {
            std::cerr << "[driverctl] Warning: could not create log directory: "
                      << logDirectory.string() << " (" << directoryError.message()
                      << "); using the base directory.\n";
            logDirectory = baseDirectory;
        }
    } else if (directoryError) {
        std::cerr << "[driverctl] Warning: could not inspect log directory: "
                  << directoryError.message() << "; using the base directory.\n";
        logDirectory = baseDirectory;
    }

    auto now = std::chrono::system_clock::now();
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count() % 1000;
    std::time_t timestamp = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
    if (localtime_s(&localTime, &timestamp) != 0) {
        std::cerr << "[driverctl] Warning: could not format log timestamp.\n";
        return;
    }

    std::ostringstream timestampText;
    timestampText << std::put_time(&localTime, "%Y-%m-%d_%H-%M-%S")
                  << '-' << std::setw(3) << std::setfill('0') << milliseconds;
    DWORD processId = GetCurrentProcessId();
    for (unsigned int attempt = 0; attempt < 100; ++attempt) {
        std::wstring filename = L"driverctl_diag_";
        std::string timestampUtf8 = timestampText.str();
        filename.append(timestampUtf8.begin(), timestampUtf8.end());
        filename += L'_';
        filename += std::to_wstring(processId);
        filename += L'_';
        filename += std::to_wstring(attempt);
        filename += L".log";
        fs::path candidate = logDirectory / filename;
        HANDLE file = CreateFileW(candidate.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                                  nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE) {
            logPath_ = std::move(candidate);
            logHandle_ = file;
            logHealthy_ = true;
            return;
        }

        DWORD error = GetLastError();
        if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) {
            std::cerr << "[driverctl] Warning: failed to create log file: "
                      << FormatWin32Error(error) << '\n';
            return;
        }
    }
    std::cerr << "[driverctl] Warning: could not allocate a unique log filename.\n";
}

ReportLogger::~ReportLogger() {
    Finish();
}

bool ReportLogger::Finish() {
    if (logHandle_ != INVALID_HANDLE_VALUE) {
        HANDLE file = logHandle_;
        logHandle_ = INVALID_HANDLE_VALUE;
        if (!FlushFileBuffers(file)) {
            DWORD error = GetLastError();
            std::cerr << "[driverctl] Warning: failed to flush diagnostic log: "
                      << FormatWin32Error(error) << '\n';
            logHealthy_ = false;
        }
        if (!CloseHandle(file)) {
            DWORD error = GetLastError();
            std::cerr << "[driverctl] Warning: failed to close diagnostic log: "
                      << FormatWin32Error(error) << '\n';
            logHealthy_ = false;
        }
    }
    return logHealthy_;
}

void ReportLogger::WriteLine(std::string_view line) {
    std::cout.write(line.data(), static_cast<std::streamsize>(line.size()));
    std::cout.put('\n');
    if (!std::cout) {
        std::cerr << "[driverctl] Error: failed to write report to stdout.\n";
    }
    WriteLogLine(line);
}

void ReportLogger::WriteLogLine(std::string_view line) {
    if (logHandle_ == INVALID_HANDLE_VALUE) return;

    size_t offset = 0;
    while (offset < line.size()) {
        DWORD chunk = static_cast<DWORD>((std::min)(
            line.size() - offset, static_cast<size_t>((std::numeric_limits<DWORD>::max)())));
        DWORD written = 0;
        if (!WriteFile(logHandle_, line.data() + offset, chunk, &written, nullptr)) {
            DWORD error = GetLastError();
            std::cerr << "[driverctl] Warning: failed to write diagnostic log: "
                      << FormatWin32Error(error) << '\n';
            CloseHandle(logHandle_);
            logHandle_ = INVALID_HANDLE_VALUE;
            logHealthy_ = false;
            return;
        }
        if (written != chunk) {
            std::cerr << "[driverctl] Warning: diagnostic log write was incomplete.\n";
            CloseHandle(logHandle_);
            logHandle_ = INVALID_HANDLE_VALUE;
            logHealthy_ = false;
            return;
        }
        offset += written;
    }
    static constexpr char newline = '\n';
    DWORD written = 0;
    if (!WriteFile(logHandle_, &newline, 1, &written, nullptr)) {
        DWORD error = GetLastError();
        std::cerr << "[driverctl] Warning: failed to write diagnostic log: "
                  << FormatWin32Error(error) << '\n';
        CloseHandle(logHandle_);
        logHandle_ = INVALID_HANDLE_VALUE;
        logHealthy_ = false;
    } else if (written != 1) {
        std::cerr << "[driverctl] Warning: diagnostic log write was incomplete.\n";
        CloseHandle(logHandle_);
        logHandle_ = INVALID_HANDLE_VALUE;
        logHealthy_ = false;
    }
}

const fs::path& ReportLogger::GetPath() const {
    return logPath_;
}
