#pragma once

#include "stdbuf_options.hpp"
#include "stdbuf.hpp"

class StdbufProcessLauncher {
public:
    static int launch(const StdbufOptions& opts);
};
