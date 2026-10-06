#pragma once

#include "supervisord.hpp"

struct StatusSnapshotEntry {
    std::string name;
    std::string state;
};
