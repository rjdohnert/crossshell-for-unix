#pragma once

#include "xargs_options.hpp"
#include "xargs.hpp"

class XargsEngine {
private:
    XargsOptions options;

public:
    explicit XargsEngine(XargsOptions opts);

    int execute();
};
