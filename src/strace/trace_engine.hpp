#pragma once

#include "strace.hpp"
#include "trace_options.hpp"

class TraceEngine {
public:
    static int Execute(TraceOptions& options);
};
