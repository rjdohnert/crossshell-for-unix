#pragma once

#include "process_thread_info.hpp"
#include "threads.hpp"

class IOutputFormatter {
public:
    virtual ~IOutputFormatter() = default;
    virtual void format(std::ostream& os, const std::vector<ProcessThreadInfo>& data) = 0;
};
