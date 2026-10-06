#pragma once

#include "rpm.hpp"

struct FileRecord {
    std::string path;
    std::string sha256;
    uint32_t size = 0;
    uint32_t mode = 0;
};
