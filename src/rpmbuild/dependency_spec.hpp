#pragma once

#include "rpmbuild.hpp"

struct DependencySpec {
    std::string name;
    std::string version;
    uint32_t flags = 0;
};
