/*
 * BSD 3-Clause License
 * Copyright (c) 2026, Roberto J Dohnert
 */

#include "du_scanner.hpp"

#include "du_path_filter.hpp"

#include <algorithm>
#include <cerrno>
#include <limits>
#include <set>
#include <iostream>
#include <string>
#include <system_error>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif

namespace fs = std::filesystem;

namespace du {
namespace {

struct FileDetails {
    std::uint64_t bytes = 0;
    std::string identity;
};

struct DirectoryFrame {
    fs::path path;
    fs::directory_iterator iterator;
    std::size_t depth = 0;
    std::uint64_t bytes = 0;
    std::string identity;
};

void AddError(ScanSummary& summary, const std::string& message) {
    if (summary.errorCount < (std::numeric_limits<std::size_t>::max)()) {
        ++summary.errorCount;
    }
    std::cerr << "du: " << message << '\n';
}

void AddSkip(ScanSummary& summary) {
    if (summary.skippedCount < (std::numeric_limits<std::size_t>::max)()) {
        ++summary.skippedCount;
    }
}

bool AddBytes(std::uint64_t& total, std::uint64_t amount) {
    const std::uint64_t maximum =
        (std::numeric_limits<std::uint64_t>::max)();
    if (amount > maximum - total) {
        total = maximum;
        return false;
    }
    total += amount;
    return true;
}

bool AddFileCount(ScanSummary& summary) {
    if (summary.fileCount == (std::numeric_limits<std::size_t>::max)()) {
        return false;
    }
    ++summary.fileCount;
    return true;
}

std::string PathText(const fs::path& path) {
    const std::string utf8 = path.u8string();
    return utf8;
}

std::string SystemErrorMessage(int error) {
    return std::error_code(error, std::system_category()).message();
}

bool IsReparsePoint(const fs::path& path, fs::file_status linkStatus) {
    if (fs::is_symlink(linkStatus)) return true;
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
    (void)path;
    return false;
#endif
}

bool GetFileDetails(const fs::path& path, bool apparentSize,
                    FileDetails& details, std::string& error) {
#ifdef _WIN32
    HANDLE handle = CreateFileW(
        path.c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        error = SystemErrorMessage(static_cast<int>(GetLastError()));
        return false;
    }

    BY_HANDLE_FILE_INFORMATION basicInfo{};
    FILE_STANDARD_INFO standardInfo{};
    const bool gotBasicInfo =
        GetFileInformationByHandle(handle, &basicInfo) != 0;
    const DWORD basicError = gotBasicInfo ? ERROR_SUCCESS : GetLastError();
    const bool gotStandardInfo =
        GetFileInformationByHandleEx(handle, FileStandardInfo,
                                     &standardInfo, sizeof(standardInfo)) != 0;
    const DWORD standardError =
        gotStandardInfo ? ERROR_SUCCESS : GetLastError();
    CloseHandle(handle);
    if (!gotBasicInfo || !gotStandardInfo) {
        const DWORD queryError = gotBasicInfo ? standardError : basicError;
        error = SystemErrorMessage(static_cast<int>(queryError));
        return false;
    }

    const LONGLONG rawBytes = apparentSize
        ? standardInfo.EndOfFile.QuadPart
        : standardInfo.AllocationSize.QuadPart;
    if (rawBytes < 0) {
        error = "filesystem returned a negative file size";
        return false;
    }
    details.bytes = static_cast<std::uint64_t>(rawBytes);
    if (basicInfo.nFileIndexHigh != 0 || basicInfo.nFileIndexLow != 0) {
        details.identity = "win:" +
            std::to_string(basicInfo.dwVolumeSerialNumber) + ":" +
            std::to_string(basicInfo.nFileIndexHigh) + ":" +
            std::to_string(basicInfo.nFileIndexLow);
    }
#else
    struct stat status {};
    if (::stat(path.c_str(), &status) != 0) {
        error = SystemErrorMessage(errno);
        return false;
    }
    if (status.st_size < 0 || status.st_blocks < 0) {
        error = "filesystem returned a negative file size";
        return false;
    }
    details.bytes = static_cast<std::uint64_t>(status.st_size);
    if (!apparentSize) {
        const std::uint64_t blocks = static_cast<std::uint64_t>(status.st_blocks);
        if (blocks > (std::numeric_limits<std::uint64_t>::max)() / 512u) {
            error = "allocated file size exceeds the supported range";
            return false;
        }
        details.bytes = blocks * 512u;
    }
    if (status.st_ino != 0) {
        details.identity = "posix:" + std::to_string(status.st_dev) + ":" +
                           std::to_string(status.st_ino);
    }
#endif
    return true;
}

bool GetDirectoryIdentity(const fs::path& path, std::string& identity,
                          std::string& error) {
#ifdef _WIN32
    HANDLE handle = CreateFileW(
        path.c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        error = SystemErrorMessage(static_cast<int>(GetLastError()));
        return false;
    }

    BY_HANDLE_FILE_INFORMATION info{};
    const BOOL success = GetFileInformationByHandle(handle, &info);
    const DWORD queryError = success ? ERROR_SUCCESS : GetLastError();
    CloseHandle(handle);
    if (!success) {
        error = SystemErrorMessage(static_cast<int>(queryError));
        return false;
    }
    if (info.nFileIndexHigh != 0 || info.nFileIndexLow != 0) {
        identity = "win:" + std::to_string(info.dwVolumeSerialNumber) + ":" +
            std::to_string(info.nFileIndexHigh) + ":" +
            std::to_string(info.nFileIndexLow);
    } else {
        std::error_code canonicalError;
        const fs::path canonical = fs::weakly_canonical(path, canonicalError);
        if (canonicalError) {
            error = canonicalError.message();
            return false;
        }
        identity = "path:" + PathText(canonical);
    }
#else
    struct stat status {};
    if (::stat(path.c_str(), &status) != 0) {
        error = SystemErrorMessage(errno);
        return false;
    }
    if (status.st_ino != 0) {
        identity = "posix:" + std::to_string(status.st_dev) + ":" +
                   std::to_string(status.st_ino);
    } else {
        std::error_code canonicalError;
        const fs::path canonical = fs::weakly_canonical(path, canonicalError);
        if (canonicalError) {
            error = canonicalError.message();
            return false;
        }
        identity = "path:" + PathText(canonical);
    }
#endif
    return true;
}

bool ShouldPrintFile(const Options& options, std::size_t depth) {
    return options.allFiles &&
        (!options.maxDepth || depth <= *options.maxDepth);
}

bool ShouldPrintDirectory(const Options& options, std::size_t depth) {
    return !options.maxDepth || depth <= *options.maxDepth;
}

} // namespace

DiskUsageScanner::DiskUsageScanner(Options options)
    : options_(std::move(options)) {
    if (options_.summarizeOnly) options_.maxDepth = 0;
}

ScanSummary DiskUsageScanner::Scan(
    const std::vector<fs::path>& paths, const EntryCallback& onEntry) {
    ScanSummary summary;
    std::set<std::string> activeDirectories;
    std::set<std::string> countedFiles;

    auto emit = [&onEntry](const fs::path& path, std::uint64_t bytes) {
        if (onEntry) onEntry(UsageEntry{path, bytes});
    };

    auto addToDirectory = [&](std::vector<DirectoryFrame>& stack,
                              std::uint64_t amount) {
        if (stack.empty()) {
            if (!AddBytes(summary.totalBytes, amount)) {
                AddError(summary, "total size exceeds the supported range.");
            }
        } else if (!AddBytes(stack.back().bytes, amount)) {
            AddError(summary, "directory size exceeds the supported range.");
        }
    };

    for (const fs::path& target : paths) {
        std::error_code statusError;
        const fs::file_status linkStatus = fs::symlink_status(target, statusError);
        if (statusError || !fs::exists(linkStatus)) {
            AddError(summary, "cannot access '" + PathText(target) + "': " +
                (statusError ? statusError.message() : "path does not exist"));
            continue;
        }

        const bool reparsePoint = IsReparsePoint(target, linkStatus);
        if (reparsePoint && !options_.dereference) {
            AddSkip(summary);
            continue;
        }

        std::error_code targetStatusError;
        const fs::file_status targetStatus = options_.dereference
            ? fs::status(target, targetStatusError) : linkStatus;
        if (targetStatusError || !fs::exists(targetStatus)) {
            AddError(summary, "cannot inspect '" + PathText(target) + "': " +
                (targetStatusError ? targetStatusError.message()
                                   : "target does not exist"));
            continue;
        }

        if (!fs::is_directory(targetStatus)) {
            FileDetails details;
            std::string detailError;
            if (!GetFileDetails(target, options_.apparentSize, details,
                                detailError)) {
                AddError(summary, "cannot read '" + PathText(target) +
                                  "': " + detailError);
                continue;
            }
            if (!details.identity.empty() &&
                !countedFiles.insert(details.identity).second) {
                AddSkip(summary);
                continue;
            }
            if (!AddFileCount(summary)) {
                AddError(summary, "file count exceeds the supported range.");
            }
            if (!AddBytes(summary.totalBytes, details.bytes)) {
                AddError(summary, "total size exceeds the supported range.");
            }
            emit(target, details.bytes);
            continue;
        }

        std::string rootIdentity;
        std::string identityError;
        if (!GetDirectoryIdentity(target, rootIdentity, identityError)) {
            AddError(summary, "cannot inspect directory '" + PathText(target) +
                              "': " + identityError);
            continue;
        }

        std::error_code iteratorError;
        fs::directory_iterator rootIterator(
            target, fs::directory_options::none, iteratorError);
        if (iteratorError) {
            AddError(summary, "cannot enumerate '" + PathText(target) +
                              "': " + iteratorError.message());
            continue;
        }

        activeDirectories.insert(rootIdentity);
        std::vector<DirectoryFrame> stack;
        stack.push_back({target, std::move(rootIterator), 0, 0,
                         std::move(rootIdentity)});

        while (!stack.empty()) {
            DirectoryFrame& frame = stack.back();
            if (frame.iterator == fs::directory_iterator()) {
                const fs::path completedPath = frame.path;
                const std::size_t completedDepth = frame.depth;
                std::uint64_t completedBytes = frame.bytes;
                FileDetails directoryDetails;
                std::string directoryDetailsError;
                if (GetFileDetails(completedPath, options_.apparentSize,
                                   directoryDetails, directoryDetailsError)) {
                    if (!AddBytes(completedBytes, directoryDetails.bytes)) {
                        AddError(summary,
                                 "directory size exceeds the supported range.");
                    }
                } else {
                    AddError(summary, "cannot read directory size for '" +
                                      PathText(completedPath) + "': " +
                                      directoryDetailsError);
                }
                activeDirectories.erase(frame.identity);
                stack.pop_back();
                if (ShouldPrintDirectory(options_, completedDepth)) {
                    emit(completedPath, completedBytes);
                }
                addToDirectory(stack, completedBytes);
                continue;
            }

            const fs::directory_entry entry = *frame.iterator;
            const fs::path childPath = entry.path();
            const std::size_t childDepth = frame.depth + 1;
            std::error_code incrementError;
            frame.iterator.increment(incrementError);
            if (incrementError) {
                AddError(summary, "cannot continue enumerating '" +
                                  PathText(frame.path) + "': " +
                                  incrementError.message());
                frame.iterator = fs::directory_iterator();
            }

            if (IsPathExcluded(childPath.filename().u8string(),
                               options_.excludePatterns)) {
                continue;
            }

            std::error_code childLinkError;
            const fs::file_status childLinkStatus =
                fs::symlink_status(childPath, childLinkError);
            if (childLinkError || !fs::exists(childLinkStatus)) {
                AddError(summary, "cannot inspect '" + PathText(childPath) +
                    "': " + (childLinkError ? childLinkError.message()
                                             : "path disappeared"));
                continue;
            }

            const bool childReparse = IsReparsePoint(childPath, childLinkStatus);
            if (childReparse && !options_.dereference) {
                AddSkip(summary);
                continue;
            }

            std::error_code childStatusError;
            const fs::file_status childStatus = options_.dereference
                ? fs::status(childPath, childStatusError) : childLinkStatus;
            if (childStatusError || !fs::exists(childStatus)) {
                AddError(summary, "cannot inspect '" + PathText(childPath) +
                    "': " + (childStatusError ? childStatusError.message()
                                               : "target does not exist"));
                continue;
            }

            if (fs::is_directory(childStatus)) {
                std::string childIdentity;
                std::string childIdentityError;
                if (!GetDirectoryIdentity(childPath, childIdentity,
                                          childIdentityError)) {
                    AddError(summary, "cannot inspect directory '" +
                        PathText(childPath) + "': " + childIdentityError);
                    continue;
                }
                if (activeDirectories.find(childIdentity) !=
                    activeDirectories.end()) {
                    AddSkip(summary);
                    continue;
                }

                std::error_code childIteratorError;
                fs::directory_iterator childIterator(
                    childPath, fs::directory_options::none,
                    childIteratorError);
                if (childIteratorError) {
                    AddError(summary, "cannot enumerate '" +
                        PathText(childPath) + "': " +
                        childIteratorError.message());
                    continue;
                }

                activeDirectories.insert(childIdentity);
                stack.push_back({childPath, std::move(childIterator),
                                 childDepth, 0, std::move(childIdentity)});
                continue;
            }

            FileDetails details;
            std::string detailError;
            if (!GetFileDetails(childPath, options_.apparentSize, details,
                                detailError)) {
                AddError(summary, "cannot read '" + PathText(childPath) +
                                  "': " + detailError);
                continue;
            }
            if (!details.identity.empty() &&
                !countedFiles.insert(details.identity).second) {
                AddSkip(summary);
                continue;
            }
            if (!AddFileCount(summary)) {
                AddError(summary, "file count exceeds the supported range.");
            }
            addToDirectory(stack, details.bytes);
            if (ShouldPrintFile(options_, childDepth)) {
                emit(childPath, details.bytes);
            }
        }
    }

    if (options_.total) emit(fs::path("total"), summary.totalBytes);
    return summary;
}

} // namespace du
