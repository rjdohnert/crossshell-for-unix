#pragma once

#include "rpm.hpp"

struct Dependency {
    std::string name;
    std::string version;
    uint32_t flags = 0;
};
