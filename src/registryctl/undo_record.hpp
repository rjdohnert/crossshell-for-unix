#pragma once

#include "registryctl.hpp"

struct UndoRecord {
    std::string op;
    std::string path;
    std::string valueName;
    std::string typeStr;
    std::string rawDataHex;
    bool hadPreviousValue = false;
};
