#pragma once

#include "ufw.hpp"

struct FirewallRule {
    int id = 0;
    std::string name;
    std::string description;
    std::string appPath;
    std::string ports;
    std::string protocol;
    std::string remoteIp;
    NET_FW_ACTION action = NET_FW_ACTION_ALLOW;
    NET_FW_RULE_DIRECTION dir = NET_FW_RULE_DIR_IN;
    bool enabled = true;
};
