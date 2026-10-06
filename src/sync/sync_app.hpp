#pragma once

#include "sync_options.hpp"
#include "sync.hpp"

class SyncApp {
private:
    CliOptions options;

public:
    int run(int argc, char* argv[]);
};
