#pragma once

#include "rpmbuild.hpp"

struct FileSpec {
    std::string path;
    uint32_t mode = 0100644;
    uint32_t flags = 0;
};
