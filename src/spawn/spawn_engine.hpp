#ifndef SPAWN_ENGINE_HPP
#define SPAWN_ENGINE_HPP

#include "spawn.hpp"
#include "spawn_options.hpp"
#include "vms_status.hpp"

class SpawnEngine {
private:
    SpawnOptions options;
    VmsStatusReporter reporter;

public:
    explicit SpawnEngine(SpawnOptions opts);
    int execute();
};

#endif // SPAWN_ENGINE_HPP
