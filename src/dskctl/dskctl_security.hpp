#ifndef DSKCTL_SECURITY_HPP
#define DSKCTL_SECURITY_HPP

#include <filesystem>
#include <string>
#include <vector>

bool IsProcessElevated(std::string& error);
bool IsPathWithinApprovedRoots(
    const std::filesystem::path& path,
    const std::vector<std::filesystem::path>& approvedRoots,
    std::string& error);

#endif
