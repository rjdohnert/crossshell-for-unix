#pragma once

#include "handles_types.hpp"

#include <string>
#include <vector>

class HandlesCollector {
public:
    static bool collect(std::vector<ProcessHandleInfo>& processes,
                        std::wstring& error);
};
