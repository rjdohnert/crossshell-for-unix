#pragma once

#include "vcc.hpp"

enum class TargetArch {
    X64,
    X86,
    ARM64
};

class ToolchainLocator {
public:
    bool IsClInPath() const;

    std::string FindVsInstallation() const;

    std::string GetVcvarsBatchPath(const std::string& vsPath, TargetArch arch) const;
};
