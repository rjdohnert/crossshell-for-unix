#pragma once

#include "recycle.hpp"

class RecycleEngine {
public:
    static bool RecycleItem(const fs::path& targetPath, bool quiet);
};
