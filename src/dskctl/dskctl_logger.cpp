/*
 * BSD 3-Clause License
 * Copyright (c) 2026, Roberto J Dohnert
 */

#include "dskctl_logger.hpp"

#include "dskctl_helpers.hpp"

#include <knownfolders.h>
#include <shlobj.h>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <limits>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

DskctlLogger::DskctlLogger() {
    std::string folderError;
    fs::path localAppData = GetKnownFolderPath(FOLDERID_LocalAppData, folderError);
    if (localAppData.empty()) {
        std::cerr << "[dskctl] Warning: cannot resolve the log directory: "
                  << folderError << '\n';
        return;
    }

    fs::path directory = localAppData / L"CrossShell" / L"dskctl" / L"logs";
    std::error_code directoryError;
    fs::create_directories(directory, directoryError);
    if (directoryError) {
        std::cerr << "[dskctl] Warning: cannot create log directory "
                  << PathToUtf8(directory) << ": "
                  << directoryError.message() << '\n';
        return;
    }

    const auto now = std::chrono::system_clock::now();
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count() % 1000;
    const std::time_t timestamp = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
    if (localtime_s(&localTime, &timestamp) != 0) {
        std::cerr << "[dskctl] Warning: cannot format the log timestamp.\n";
        return;
    }
    std::ostringstream timestampText;
    timestampText << std::put_time(&localTime, "%Y-%m-%d_%H-%M-%S")
                  << '-' << std::setw(3) << std::setfill('0') << milliseconds;

    const std::string timestampTextUtf8 = timestampText.str();
    const std::wstring timestampWide(timestampTextUtf8.begin(),
                                     timestampTextUtf8.end());
    for (unsigned int attempt = 0; attempt < 100; ++attempt) {
        std::wstring name = L"dskctl_" + timestampWide;
        name += L'_';
        name += std::to_wstring(GetCurrentProcessId());
        name += L'_';
        name += std::to_wstring(attempt);
        name += L".log";
        fs::path candidate = directory / name;
        HANDLE file = CreateFileW(candidate.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                                  nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE) {
            path_ = std::move(candidate);
            handle_ = file;
            healthy_ = true;
            return;
        }

        DWORD error = GetLastError();
        if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) {
            std::cerr << "[dskctl] Warning: cannot create log file: "
                      << FormatWin32Error(error) << '\n';
            return;
        }
    }
    std::cerr << "[dskctl] Warning: cannot reserve a unique log file.\n";
}

DskctlLogger::~DskctlLogger() {
    Finish();
}

bool DskctlLogger::WriteLine(std::string_view line) {
    if (handle_ == INVALID_HANDLE_VALUE || !healthy_) return false;
    std::string_view remaining = line;
    while (!remaining.empty()) {
        DWORD chunk = static_cast<DWORD>((std::min)(
            remaining.size(), static_cast<std::size_t>(
                (std::numeric_limits<DWORD>::max)())));
        DWORD written = 0;
        if (!WriteFile(handle_, remaining.data(), chunk, &written, nullptr)) {
            DWORD error = GetLastError();
            std::cerr << "[dskctl] Warning: log write failed: "
                      << FormatWin32Error(error) << '\n';
            healthy_ = false;
            return false;
        }
        if (written != chunk) {
            std::cerr << "[dskctl] Warning: log write was incomplete.\n";
            healthy_ = false;
            return false;
        }
        remaining.remove_prefix(written);
    }

    static constexpr char newline = '\n';
    DWORD written = 0;
    if (!WriteFile(handle_, &newline, 1, &written, nullptr) || written != 1) {
        std::cerr << "[dskctl] Warning: log line terminator could not be written.\n";
        healthy_ = false;
        return false;
    }
    return true;
}

bool DskctlLogger::Finish() {
    if (handle_ == INVALID_HANDLE_VALUE) return healthy_;
    HANDLE file = handle_;
    handle_ = INVALID_HANDLE_VALUE;
    if (!FlushFileBuffers(file)) {
        DWORD error = GetLastError();
        std::cerr << "[dskctl] Warning: log flush failed: "
                  << FormatWin32Error(error) << '\n';
        healthy_ = false;
    }
    if (!CloseHandle(file)) {
        DWORD error = GetLastError();
        std::cerr << "[dskctl] Warning: log close failed: "
                  << FormatWin32Error(error) << '\n';
        healthy_ = false;
    }
    return healthy_;
}

const fs::path& DskctlLogger::GetPath() const {
    return path_;
}
