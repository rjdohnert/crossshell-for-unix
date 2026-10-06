#pragma once

#include "process_info.hpp"
#include "renice.hpp"

class IProcessFilter {
public:
    virtual ~IProcessFilter() = default;
    [[nodiscard]] virtual bool matches(const ProcessInfo& proc) const = 0;
};
