#pragma once

#include "pass_config.hpp"
#include "wipe.hpp"

class DirectoryTraverser {
public:
    static bool WipeDirectory(const std::string& dirpath, const std::vector<PassConfig>& passes,
                              bool recursive, bool force, bool verbose, bool interactive);
};
