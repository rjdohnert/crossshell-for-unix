#pragma once

#include "pass_config.hpp"
#include "wipe.hpp"

class PassGenerator {
public:
    static std::vector<PassConfig> GetPassConfigs(int passes, bool dod_mode, bool quick_mode);

    static void FillBuffer(std::vector<char>& buf, const PassConfig& cfg);
};
