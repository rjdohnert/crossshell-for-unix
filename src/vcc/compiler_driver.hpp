#pragma once

#include "build_options.hpp"
#include "toolchain_locator.hpp"
#include "vcc.hpp"

class CompilerDriver {
private:
    ToolchainLocator m_locator;

public:
    int Run(const BuildOptions& opts);
};
