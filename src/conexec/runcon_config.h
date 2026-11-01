#ifndef CONEXEC_RUNCON_CONFIG_H
#define CONEXEC_RUNCON_CONFIG_H

#include "conexec.h"
#include "isolation_level.h"

struct RunconConfig {
    IsolationLevel level = IsolationLevel::Normal;
    std::wstring command;
    std::wstring workingDirectory;
    bool stageCurrentDir = false;
    bool verbose = false;
};

#endif // CONEXEC_RUNCON_CONFIG_H
