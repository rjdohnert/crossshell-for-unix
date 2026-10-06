#pragma once

#include "sync.hpp"
#include "volume_target.hpp"

class StorageManager {
public:
    static std::vector<std::unique_ptr<VolumeTarget>> discover_volumes(bool removable_only);
};
