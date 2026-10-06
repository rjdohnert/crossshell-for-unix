#pragma once

#include "registry_record.hpp"
#include "registryctl.hpp"

class OutputFormatter {
public:
    static void PrintDiff(const std::string& path, const std::string& value, const RegistryRecord& oldRec, const std::string& newType, const std::string& newData, bool willDelete = false);

    static void PrintJson(const RegistryRecord& rec);
};
