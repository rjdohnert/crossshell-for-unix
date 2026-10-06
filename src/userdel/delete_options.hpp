#pragma once

#include "userdel.hpp"

struct DeleteOptions {
    std::string username;
    bool removeHomeAndProfile = false;
    bool force = false;
    bool verbose = false;
    bool pipeMode = false;
    std::string rootDir = "";
};
