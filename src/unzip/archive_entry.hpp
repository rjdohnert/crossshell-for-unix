#pragma once

#include "unzip.hpp"

struct ArchiveEntry {
    uint64_t size = 0;
    std::string date;
    std::string name;
};
