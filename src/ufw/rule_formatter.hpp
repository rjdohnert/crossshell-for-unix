#pragma once

#include "firewall_rule.hpp"
#include "ufw.hpp"

class RuleFormatter {
public:
    static void DisplayHelp();

    static void DisplayVersion();

    static void DisplayStatus(const std::string& mode, bool isEnabled, const std::string& activeProfiles, const std::vector<FirewallRule>& rules);
};
