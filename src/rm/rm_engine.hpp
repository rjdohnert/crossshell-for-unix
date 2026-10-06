#pragma once

#include "rm_options.hpp"
#include "rm.hpp"

class RmEngine {
private:
    RmOptions m_options;
    uint64_t m_removedCount{0};

public:
    explicit RmEngine(RmOptions opts);

    int Run();

private:
    bool ProcessTarget(const fs::path& target);

    bool ProcessFile(const fs::path& file);

    bool ProcessDirectory(const fs::path& dir);

    bool RemoveDirectoryRecursive(const fs::path& dir);
};
