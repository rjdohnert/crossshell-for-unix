/*
 * BSD 3-Clause License
 * Copyright (c) 2026, Roberto J Dohnert
 */

#include "dskctl_app.hpp"

#include "dskctl_engine.hpp"
#include "dskctl_logger.hpp"
#include "dskctl_options.hpp"
#include "dskctl_reporter.hpp"
#include "dskctl_security.hpp"
#include "dskctl_targets.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

bool PromptYesNo() {
    std::cerr << "Continue with cleanup? [y/N]: " << std::flush;
    std::string response;
    if (!std::getline(std::cin, response)) return false;
    std::transform(response.begin(), response.end(), response.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    return response == "y" || response == "yes";
}

bool PromptDeleteConfirmation() {
    std::cerr << "This operation may permanently remove files. "
                 "Type CONFIRM to continue: " << std::flush;
    std::string response;
    if (!std::getline(std::cin, response)) return false;
    std::transform(response.begin(), response.end(), response.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::toupper(character));
        });
    return response == "CONFIRM";
}

void AddToSummary(CleanSummary& summary, TargetReport report) {
    const std::uint64_t maxBytes =
        (std::numeric_limits<std::uint64_t>::max)();
    if (report.bytes > maxBytes - summary.totalBytes) {
        summary.totalBytes = maxBytes;
        ++report.errors;
    } else {
        summary.totalBytes += report.bytes;
    }

    const std::size_t maxFiles = (std::numeric_limits<std::size_t>::max)();
    if (report.files > maxFiles - summary.totalFiles) {
        summary.totalFiles = maxFiles;
        ++report.errors;
    } else {
        summary.totalFiles += report.files;
    }
    summary.totalSkipped += report.skipped;
    summary.totalErrors += report.errors;
    summary.targets.push_back(std::move(report));
}

std::vector<const CleanupTarget*> GetSelectedTargets(
    const std::vector<CleanupTarget>& targets) {
    std::vector<const CleanupTarget*> selected;
    for (const CleanupTarget& target : targets) {
        if (target.selected) selected.push_back(&target);
    }
    return selected;
}

} // namespace

int DskctlApplication::Run(int argc, char* argv[]) const {
    ParseResult parsed = ParseCommandLine(argc, argv);
    if (!parsed.success) {
        std::cerr << "[dskctl] Error: " << parsed.error << '\n';
        return 2;
    }
    if (parsed.options.showHelp) {
        PrintHelp(parsed.options.targets);
        return 0;
    }

    DskctlOptions& options = parsed.options;
    const std::vector<const CleanupTarget*> selected =
        GetSelectedTargets(options.targets);
    if (!options.dryRun) {
        std::string elevationError;
        if (!IsProcessElevated(elevationError)) {
            if (!elevationError.empty()) {
                std::cerr << "[dskctl] Error: " << elevationError << '\n';
            } else {
                std::cerr << "[dskctl] Error: --execute requires an elevated "
                             "Administrator process.\n";
            }
            return 1;
        }
    }

    const std::vector<std::filesystem::path> approvedRoots =
        GetApprovedCleanupRoots(options.targets);
    DskctlLogger logger;
    logger.WriteLine("dskctl session started");

    std::vector<TargetReport> previews;
    previews.reserve(selected.size());
    for (const CleanupTarget* target : selected) {
        TargetReport preview = PreviewCleanupTarget(*target, approvedRoots);
        logger.WriteLine("Previewed target: " + target->name);
        previews.push_back(std::move(preview));
    }

    if (options.outputFormat == OutputFormat::Human) {
        PrintHumanPreview(previews, !options.dryRun, options.useRecycleBin,
                          options.verbose);
    }

    CleanSummary summary;
    if (options.dryRun) {
        for (TargetReport& preview : previews) {
            AddToSummary(summary, std::move(preview));
        }
    } else if (!options.force &&
               (!PromptYesNo() || !PromptDeleteConfirmation())) {
        summary.cancelled = true;
        for (TargetReport& preview : previews) {
            AddToSummary(summary, std::move(preview));
        }
        logger.WriteLine("Cleanup cancelled; no files were changed.");
    } else {
        for (const CleanupTarget* target : selected) {
            TargetReport report = ExecuteCleanupTarget(
                *target, options.useRecycleBin, approvedRoots);
            logger.WriteLine("Processed target: " + target->name +
                ", files=" + std::to_string(report.files) +
                ", errors=" + std::to_string(report.errors));
            AddToSummary(summary, std::move(report));
        }
    }

    logger.WriteLine("Summary: files=" + std::to_string(summary.totalFiles) +
        ", skipped=" + std::to_string(summary.totalSkipped) +
        ", errors=" + std::to_string(summary.totalErrors));
    const bool logSaved = logger.Finish();

    if (options.outputFormat == OutputFormat::Human) {
        PrintHumanSummary(summary, options, logger.GetPath(), logSaved);
    } else {
        PrintStructuredSummary(summary, options, logger.GetPath(), logSaved);
    }

    if (!std::cout) return 1;
    if (summary.cancelled) return 0;
    return summary.totalErrors == 0 ? 0 : 1;
}
