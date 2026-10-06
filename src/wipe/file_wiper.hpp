#pragma once

#include "pass_config.hpp"
#include "wipe.hpp"

class FileWiper {
public:
    static bool SecureRenameAndDelete(const std::string& filepath, bool verbose);

    static bool WipeFile(const std::string& filepath, const std::vector<PassConfig>& passes, bool force, bool verbose);
};
