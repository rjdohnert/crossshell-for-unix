#ifndef DSKCTL_REPORTER_HPP
#define DSKCTL_REPORTER_HPP

#include "dskctl_models.hpp"

#include <filesystem>
#include <vector>

void PrintHelp(const std::vector<CleanupTarget>& targets);
void PrintHumanPreview(const std::vector<TargetReport>& previews,
                       bool execute, bool useRecycleBin, bool verbose);
void PrintHumanSummary(const CleanSummary& summary,
                       const DskctlOptions& options,
                       const std::filesystem::path& logPath,
                       bool logSaved);
void PrintStructuredSummary(const CleanSummary& summary,
                            const DskctlOptions& options,
                            const std::filesystem::path& logPath,
                            bool logSaved);

#endif
