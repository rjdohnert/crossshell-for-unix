#pragma once

#include "supervisor_defaults.hpp"
#include "supervisord.hpp"

struct IpcRequest {
    std::string raw;
    std::vector<std::string> tokens;
    IpcAction action = IpcAction::Unknown;
    std::string actionText;
    std::string target;
    std::wstring wTarget;
    bool clientPrivileged = false;
};
