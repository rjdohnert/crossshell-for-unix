#pragma once

#include "registryctl.hpp"

struct RegistryRecord {
    std::string rootKey;
    std::string subKey;
    std::string valueName;
    DWORD type = REG_NONE;
    std::vector<uint8_t> rawData;
    bool exists = false;

    std::string GetTypeString() const;

    std::string GetFormattedData() const;
};
