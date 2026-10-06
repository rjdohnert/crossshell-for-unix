#pragma once

#include "wall_config.hpp"
#include "wall.hpp"

class WallEngine {
private:
    WallConfig config;

public:
    explicit WallEngine(WallConfig cfg);

    int execute();
};
