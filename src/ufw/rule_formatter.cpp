#include "firewall_rule.hpp"
#include "rule_formatter.hpp"

void RuleFormatter::DisplayHelp() {
        std::cout << R"(ufw(1)                  CrossShell for UNIX Reference Manual                 ufw(1)

    NAME
        ufw - program for managing a netfilter firewall

    SYNOPSIS
        ufw [OPTIONS] COMMAND [ARGUMENTS...]

    DESCRIPTION
        ufw provides a user-friendly interface for managing Windows Firewall
        rules and security policies.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -h, --help
            Display this reference manual and exit.

        -v, --version
            Display version information and exit.

    COMMANDS
        enable
            Enable the firewall.

        disable
            Disable the firewall.

        default allow|deny incoming|outgoing
            Set default policy.

        status
            Show firewall status.

        status numbered
            Show firewall status as numbered list of rules.

        reset
            Reset firewall to default state.

        allow <port>[/<protocol>]
            Allow traffic to port.

        deny <port>[/<protocol>]
            Deny traffic to port.

        reject <port>[/<protocol>]
            Reject traffic to port.

        delete <number>
            Delete rule by its assigned number.

        allow app <ApplicationPath>
            Allow application path.

        deny app <ApplicationPath>
            Deny application path.

        allow from <ip> [to <port>]
            Allow traffic from specific IP.

    EXAMPLES
        ufw status
            Display active firewall status and rules.

        ufw allow 80/tcp
            Allow incoming TCP traffic on port 80.

        ufw enable
            Enable the firewall.

    CrossShell for UNIX                                                      ufw(1)
)";
    }

void RuleFormatter::DisplayVersion() {
        std::cout << "ufw 0.36.2 (Windows Advanced Security Edition)\n"
                  << "Copyright 2026 Roberto J. Dohnert\n";
    }

void RuleFormatter::DisplayStatus(const std::string& mode, bool isEnabled, const std::string& activeProfiles, const std::vector<FirewallRule>& rules) {
        std::cout << "Status: " << (isEnabled ? "active" : "inactive") << "\n";
        if (!activeProfiles.empty()) {
            std::cout << "Active Profiles: " << activeProfiles << "\n";
        }
        std::cout << "\n";

        if (rules.empty()) {
            std::cout << "No active UFW rules.\n";
            return;
        }

        std::cout << std::left << std::setw(mode == "numbered" ? 8 : 4);
        if (mode == "numbered") std::cout << "  [ #]";

        std::cout << std::setw(25) << "To" << std::setw(15) << "Action" << std::setw(25) << "From" << "\n";
        std::cout << std::setw(mode == "numbered" ? 8 : 4);
        if (mode == "numbered") std::cout << "  ---";

        std::cout << std::setw(25) << "--" << std::setw(15) << "------" << std::setw(25) << "----" << "\n";

        for (const auto& r : rules) {
            if (mode == "numbered") {
                std::cout << "  [" << std::right << std::setw(2) << r.id << "] ";
            }

            std::string toSpec = (r.appPath.empty()) ? (r.ports + "/" + r.protocol) : r.appPath;
            std::string actionSpec = (r.action == NET_FW_ACTION_ALLOW ? "ALLOW" : "DENY");
            if (r.dir == NET_FW_RULE_DIR_OUT) actionSpec += " OUT";
            else actionSpec += " IN";

            std::cout << std::left << std::setw(25) << toSpec
                      << std::setw(15) << actionSpec
                      << std::setw(25) << r.remoteIp << "\n";
        }
    }
