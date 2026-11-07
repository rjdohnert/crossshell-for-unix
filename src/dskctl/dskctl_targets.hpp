#ifndef DSKCTL_TARGETS_HPP
#define DSKCTL_TARGETS_HPP

#include "dskctl_models.hpp"

#include <string>
#include <vector>

bool BuildCleanupTargets(std::vector<CleanupTarget>& targets,
                         std::string& error);
std::vector<std::filesystem::path> GetApprovedCleanupRoots(
    const std::vector<CleanupTarget>& targets);

#endif
