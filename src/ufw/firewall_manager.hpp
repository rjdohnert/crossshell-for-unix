#pragma once

#include "firewall_rule.hpp"
#include "ufw.hpp"

class FirewallManager {
private:
    INetFwPolicy2* m_policy = nullptr;

public:
    FirewallManager() = default;

    ~FirewallManager();

    bool Initialize();

    bool GetProfileStatus(bool& isEnabled, std::string& activeProfiles);

    bool SetFirewallEnabled(bool enable);

    bool SetDefaultPolicy(NET_FW_RULE_DIRECTION dir, NET_FW_ACTION action);

    std::vector<FirewallRule> EnumerateRules();

    bool AddRule(NET_FW_ACTION action, NET_FW_RULE_DIRECTION dir, const std::string& port, LONG protocol, const std::string& remoteIp, const std::string& appPath);

    bool DeleteRuleByNumber(int ruleNum);

    bool DeleteRuleByName(const std::string& name);

    bool ResetDefaults();
};
