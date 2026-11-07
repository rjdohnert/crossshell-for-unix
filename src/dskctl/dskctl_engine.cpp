/*
 * BSD 3-Clause License
 * Copyright (c) 2026, Roberto J Dohnert
 */

#include "dskctl_engine.hpp"

#include "dskctl_helpers.hpp"
#include "dskctl_security.hpp"

#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>

namespace fs = std::filesystem;

namespace {

constexpr std::size_t kMaximumPreviewSamples = 5;
constexpr std::size_t kMaximumReportedIssues = 100;
constexpr std::size_t kMaximumVisitedEntries = 250000;
constexpr int kMaximumDirectoryDepth = 64;

struct TraversalState {
    std::size_t visited = 0;
    bool limitReached = false;
};

void AddIssue(TargetReport& report, std::string issue) {
    if (report.issues.size() < kMaximumReportedIssues) {
        report.issues.push_back(std::move(issue));
    }
}

void AddError(TargetReport& report, std::string issue = "Cleanup operation failed.") {
    ++report.errors;
    AddIssue(report, std::move(issue));
}

void AddSkip(TargetReport& report, std::string issue) {
    ++report.skipped;
    AddIssue(report, std::move(issue));
}

void AddSample(TargetReport& report, const fs::path& path) {
    if (report.samplePaths.size() < kMaximumPreviewSamples) {
        report.samplePaths.push_back(PathToUtf8(path));
    }
}

bool InspectEntry(const fs::path& path,
                  const std::vector<fs::path>& approvedRoots,
                  TargetReport& report, TraversalState& traversal, int depth) {
    if (traversal.limitReached) return false;
    if (traversal.visited >= kMaximumVisitedEntries ||
        depth > kMaximumDirectoryDepth) {
        traversal.limitReached = true;
        AddError(report, "Preview stopped at the traversal depth or entry limit.");
        return false;
    }
    ++traversal.visited;

    std::string allowlistError;
    if (!IsPathWithinApprovedRoots(path, approvedRoots, allowlistError)) {
        AddSkip(report, allowlistError);
        return false;
    }

    DWORD attributes = 0;
    std::string attributesError;
    if (!QueryPathAttributes(path, attributes, attributesError)) {
        AddSkip(report, attributesError);
        return false;
    }
    if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) {
        AddSkip(report, "Skipped reparse point: " + PathToUtf8(path));
        return false;
    }
    AddSample(report, path);

    if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
        std::error_code iteratorError;
        fs::directory_iterator iterator(path, fs::directory_options::none,
                                        iteratorError);
        if (iteratorError) {
            AddError(report, "Cannot enumerate " + PathToUtf8(path) + ": " +
                             iteratorError.message());
            return false;
        }
        for (const fs::directory_iterator end; iterator != end;) {
            const fs::path child = iterator->path();
            InspectEntry(child, approvedRoots, report, traversal, depth + 1);
            if (traversal.limitReached) break;
            iterator.increment(iteratorError);
            if (iteratorError) {
                AddError(report, "Cannot continue enumerating " +
                                 PathToUtf8(path) + ": " +
                                 iteratorError.message());
                break;
            }
        }
        return true;
    }

    if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        std::error_code sizeError;
        std::uintmax_t size = fs::file_size(path, sizeError);
        if (sizeError) {
            AddError(report, "Cannot read file size for " + PathToUtf8(path) +
                             ": " + sizeError.message());
            return false;
        }
        report.bytes += static_cast<std::uint64_t>(size);
        ++report.files;
    }
    return true;
}

bool MoveToRecycleBin(const fs::path& path, std::string& error) {
    std::wstring source = path.native();
    source.push_back(L'\0');
    source.push_back(L'\0');

    SHFILEOPSTRUCTW operation{};
    operation.wFunc = FO_DELETE;
    operation.pFrom = source.c_str();
    operation.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION |
                       FOF_SILENT | FOF_NOERRORUI;
    int result = SHFileOperationW(&operation);
    if (result != 0 || operation.fAnyOperationsAborted) {
        error = result != 0
            ? "SHFileOperationW failed with code " + std::to_string(result) +
              " for " + PathToUtf8(path) + "."
            : "Recycle Bin operation was aborted for " + PathToUtf8(path) + ".";
        return false;
    }
    return true;
}

bool RemoveEntry(const fs::path& path,
                 const std::vector<fs::path>& approvedRoots,
                 TargetReport& report, TraversalState& traversal, int depth) {
    if (traversal.limitReached) return false;
    if (traversal.visited >= kMaximumVisitedEntries ||
        depth > kMaximumDirectoryDepth) {
        traversal.limitReached = true;
        AddError(report, "Deletion stopped at the traversal depth or entry limit.");
        return false;
    }
    ++traversal.visited;

    std::string allowlistError;
    if (!IsPathWithinApprovedRoots(path, approvedRoots, allowlistError)) {
        AddSkip(report, allowlistError);
        return false;
    }

    DWORD attributes = 0;
    std::string attributesError;
    if (!QueryPathAttributes(path, attributes, attributesError)) {
        if (!attributesError.empty()) AddError(report, attributesError);
        return false;
    }
    if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) {
        AddSkip(report, "Skipped reparse point during deletion: " +
                        PathToUtf8(path));
        return false;
    }

    if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
        std::error_code iteratorError;
        fs::directory_iterator iterator(path, fs::directory_options::none,
                                        iteratorError);
        if (iteratorError) {
            AddError(report, "Cannot enumerate for deletion " +
                             PathToUtf8(path) + ": " +
                             iteratorError.message());
            return false;
        }
        for (const fs::directory_iterator end; iterator != end;) {
            const fs::path child = iterator->path();
            RemoveEntry(child, approvedRoots, report, traversal, depth + 1);
            if (traversal.limitReached) return false;
            iterator.increment(iteratorError);
            if (iteratorError) {
                AddError(report, "Cannot continue deletion enumeration " +
                                 PathToUtf8(path) + ": " +
                                 iteratorError.message());
                return false;
            }
        }

        if (!RemoveDirectoryW(path.c_str())) {
            DWORD error = GetLastError();
            AddError(report, "Cannot remove directory " + PathToUtf8(path) +
                             ": " + FormatWin32Error(error));
            return false;
        }
        return true;
    }

    std::error_code sizeError;
    std::uintmax_t size = fs::file_size(path, sizeError);
    if (sizeError) {
        AddError(report, "Cannot read file size for " + PathToUtf8(path) +
                         ": " + sizeError.message());
        size = 0;
    }
    if (!DeleteFileW(path.c_str())) {
        DWORD error = GetLastError();
        AddError(report, "Cannot delete " + PathToUtf8(path) + ": " +
                         FormatWin32Error(error));
        return false;
    }
    report.bytes += static_cast<std::uint64_t>(size);
    ++report.files;
    return true;
}

TargetReport QueryRecycleBin() {
    TargetReport report;
    report.targetName = "Recycle Bin";
    SHQUERYRBINFO info{};
    info.cbSize = sizeof(info);
    HRESULT result = SHQueryRecycleBinW(nullptr, &info);
    if (FAILED(result) || info.i64Size < 0 || info.i64NumItems < 0) {
        std::ostringstream error;
        error << "SHQueryRecycleBinW failed or returned invalid statistics (HRESULT 0x"
              << std::hex << static_cast<unsigned long>(result) << ").";
        AddError(report, error.str());
        return report;
    }
    report.bytes = static_cast<std::uint64_t>(info.i64Size);
    report.files = static_cast<std::size_t>(info.i64NumItems);
    return report;
}

TargetReport ProcessRecycleBin(bool execute) {
    TargetReport report = QueryRecycleBin();
    if (!execute || report.errors != 0) return report;

    HRESULT result = SHEmptyRecycleBinW(
        nullptr, nullptr,
        SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
    if (FAILED(result)) {
        std::ostringstream error;
        error << "SHEmptyRecycleBinW failed (HRESULT 0x"
              << std::hex << static_cast<unsigned long>(result) << ").";
        AddError(report, error.str());
        report.bytes = 0;
        report.files = 0;
    }
    return report;
}

void ProcessPath(const fs::path& path, bool execute, bool useRecycleBin,
                 const std::vector<fs::path>& approvedRoots,
                 TargetReport& report) {
    std::error_code existsError;
    bool exists = fs::exists(path, existsError);
    if (existsError) {
        AddError(report, "Cannot inspect " + PathToUtf8(path) + ": " +
                         existsError.message());
        return;
    }
    if (!exists) {
        AddSkip(report, "Path does not exist: " + PathToUtf8(path));
        return;
    }

    TargetReport measured;
    measured.targetName = report.targetName;
    TraversalState measuredTraversal;
    InspectEntry(path, approvedRoots, measured, measuredTraversal, 0);
    if (!execute) {
        report.skipped += measured.skipped;
        report.errors += measured.errors;
        report.bytes += measured.bytes;
        report.files += measured.files;
        for (const std::string& sample : measured.samplePaths) {
            if (report.samplePaths.size() < kMaximumPreviewSamples) {
                report.samplePaths.push_back(sample);
            }
        }
        for (const std::string& issue : measured.issues) {
            AddIssue(report, issue);
        }
        return;
    }

    if (measured.errors != 0) {
        report.errors += measured.errors;
        for (const std::string& issue : measured.issues) AddIssue(report, issue);
        ++report.skipped;
        AddIssue(report, "Skipped deletion because preview encountered errors: " +
                         PathToUtf8(path));
        return;
    }
    if (useRecycleBin) {
        if (measured.skipped != 0) {
            report.skipped += measured.skipped;
            for (const std::string& issue : measured.issues) AddIssue(report, issue);
            AddIssue(report, "Skipped Recycle Bin operation because the target "
                             "contains an unapproved or reparse path: " +
                             PathToUtf8(path));
            return;
        }
        std::string moveError;
        if (!MoveToRecycleBin(path, moveError)) {
            AddError(report, std::move(moveError));
            return;
        }
        report.bytes += measured.bytes;
        report.files += measured.files;
        return;
    }

    TraversalState removalTraversal;
    RemoveEntry(path, approvedRoots, report, removalTraversal, 0);
}

} // namespace

TargetReport PreviewCleanupTarget(
    const CleanupTarget& target, const std::vector<fs::path>& approvedRoots) {
    if (target.recycleBin) return QueryRecycleBin();

    TargetReport report;
    report.targetName = target.name;
    for (const fs::path& path : target.paths) {
        ProcessPath(path, false, false, approvedRoots, report);
    }
    return report;
}

TargetReport ExecuteCleanupTarget(
    const CleanupTarget& target, bool useRecycleBin,
    const std::vector<fs::path>& approvedRoots) {
    if (target.recycleBin) return ProcessRecycleBin(true);

    TargetReport report;
    report.targetName = target.name;
    for (const fs::path& path : target.paths) {
        std::string allowlistError;
        if (!IsPathWithinApprovedRoots(path, approvedRoots, allowlistError)) {
            AddError(report, allowlistError);
            continue;
        }
        std::error_code existsError;
        bool exists = fs::exists(path, existsError);
        if (existsError) {
            AddError(report, "Cannot inspect " + PathToUtf8(path) + ": " +
                             existsError.message());
            continue;
        }
        if (!exists) {
            AddSkip(report, "Path does not exist: " + PathToUtf8(path));
            continue;
        }

        DWORD attributes = 0;
        std::string attributesError;
        if (!QueryPathAttributes(path, attributes, attributesError)) {
            AddError(report, attributesError);
            continue;
        }
        if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) {
            AddSkip(report, "Skipped reparse-point target: " + PathToUtf8(path));
            continue;
        }
        if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
            ProcessPath(path, true, useRecycleBin, approvedRoots, report);
            continue;
        }

        std::error_code iteratorError;
        fs::directory_iterator iterator(path, fs::directory_options::none,
                                        iteratorError);
        if (iteratorError) {
            AddError(report, "Cannot enumerate target " + PathToUtf8(path) +
                             ": " + iteratorError.message());
            continue;
        }
        std::size_t topLevelEntries = 0;
        for (const fs::directory_iterator end; iterator != end;) {
            const fs::path child = iterator->path();
            if (++topLevelEntries > kMaximumVisitedEntries) {
                AddError(report, "Target contains more entries than the safety limit.");
                break;
            }
            ProcessPath(child, true, useRecycleBin, approvedRoots, report);
            iterator.increment(iteratorError);
            if (iteratorError) {
                AddError(report, "Cannot continue target enumeration " +
                                 PathToUtf8(path) + ": " +
                                 iteratorError.message());
                break;
            }
        }
    }
    return report;
}
