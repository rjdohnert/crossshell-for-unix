#ifndef DSKCTL_ENGINE_HPP
#define DSKCTL_ENGINE_HPP

#include "dskctl_models.hpp"

#include <filesystem>
#include <vector>

TargetReport PreviewCleanupTarget(
    const CleanupTarget& target,
    const std::vector<std::filesystem::path>& approvedRoots);
TargetReport ExecuteCleanupTarget(
    const CleanupTarget& target, bool useRecycleBin,
    const std::vector<std::filesystem::path>& approvedRoots);

#endif
