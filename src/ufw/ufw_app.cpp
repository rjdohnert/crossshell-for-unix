#include "rule_formatter.hpp"
#include "string_utils.hpp"
#include "ufw_app.hpp"

int UfwApplication::Run(int argc, char* argv[]) {
        if (argc < 2) {
            RuleFormatter::DisplayHelp();
            return 0;
        }

        std::string cmd = StringUtils::ToLower(argv[1]);

        if (cmd == "--help" || cmd == "-h" || cmd == "help" || cmd == "/?" || cmd == "-?") {
            RuleFormatter::DisplayHelp();
            return 0;
        }

        if (cmd == "--version" || cmd == "-v" || cmd == "version") {
            RuleFormatter::DisplayVersion();
            return 0;
        }

        if (cmd == "enable") {
            if (m_fw.SetFirewallEnabled(true)) {
                std::cout << "Firewall is active and enabled on system startup\n";
                return 0;
            }
            std::cerr << "ufw: Failed to enable firewall. Administrator privileges required.\n";
            return 1;
        }

        if (cmd == "disable") {
            if (m_fw.SetFirewallEnabled(false)) {
                std::cout << "Firewall stopped and disabled on system startup\n";
                return 0;
            }
            std::cerr << "ufw: Failed to disable firewall. Administrator privileges required.\n";
            return 1;
        }

        if (cmd == "reset") {
            if (m_fw.ResetDefaults()) {
                std::cout << "Firewall reset to default configuration.\n";
                return 0;
            }
            std::cerr << "ufw: Failed to reset firewall.\n";
            return 1;
        }

        if (cmd == "status") {
            std::string mode = (argc >= 3) ? StringUtils::ToLower(argv[2]) : "";
            bool isEnabled = true;
            std::string activeProfiles;
            m_fw.GetProfileStatus(isEnabled, activeProfiles);
            auto rules = m_fw.EnumerateRules();
            RuleFormatter::DisplayStatus(mode, isEnabled, activeProfiles, rules);
            return 0;
        }

        if (cmd == "default") {
            if (argc < 4) {
                std::cerr << "ufw: Usage: ufw default <allow|deny> <incoming|outgoing>\n";
                return 1;
            }
            std::string actionStr = StringUtils::ToLower(argv[2]);
            std::string dirStr = StringUtils::ToLower(argv[3]);

            NET_FW_ACTION action = (actionStr == "allow") ? NET_FW_ACTION_ALLOW : NET_FW_ACTION_BLOCK;
            NET_FW_RULE_DIRECTION dir = (dirStr == "incoming" || dirStr == "in") ? NET_FW_RULE_DIR_IN : NET_FW_RULE_DIR_OUT;

            if (m_fw.SetDefaultPolicy(dir, action)) {
                std::cout << "Default " << dirStr << " policy changed to " << actionStr << "\n";
                return 0;
            }
            std::cerr << "ufw: Failed to update default policy.\n";
            return 1;
        }

        if (cmd == "delete") {
            if (argc < 3) {
                std::cerr << "ufw: Usage: ufw delete <RuleNumber>\n";
                return 1;
            }
            int ruleNum = 0;
            if (!StringUtils::ParseRuleNumber(argv[2], ruleNum)) {
                std::cerr << "ufw: Invalid rule number '" << argv[2] << "'.\n";
                return 1;
            }
            if (m_fw.DeleteRuleByNumber(ruleNum)) {
                std::cout << "Rule " << ruleNum << " deleted successfully.\n";
                return 0;
            }
            std::cerr << "ufw: Could not find or delete rule " << ruleNum << "\n";
            return 1;
        }

        if (cmd == "allow" || cmd == "deny" || cmd == "reject") {
            NET_FW_ACTION action = (cmd == "allow") ? NET_FW_ACTION_ALLOW : NET_FW_ACTION_BLOCK;
            NET_FW_RULE_DIRECTION dir = NET_FW_RULE_DIR_IN;
            std::string port = "";
            LONG protocol = NET_FW_IP_PROTOCOL_ANY;
            std::string remoteIp = "*";
            std::string appPath = "";

            int argIdx = 2;
            if (argIdx < argc && (StringUtils::ToLower(argv[argIdx]) == "out" || StringUtils::ToLower(argv[argIdx]) == "outgoing")) {
                dir = NET_FW_RULE_DIR_OUT;
                argIdx++;
            } else if (argIdx < argc && (StringUtils::ToLower(argv[argIdx]) == "in" || StringUtils::ToLower(argv[argIdx]) == "incoming")) {
                dir = NET_FW_RULE_DIR_IN;
                argIdx++;
            }

            if (argIdx < argc && StringUtils::ToLower(argv[argIdx]) == "app" && argIdx + 1 < argc) {
                appPath = argv[argIdx + 1];
            } else if (argIdx < argc && StringUtils::ToLower(argv[argIdx]) == "from" && argIdx + 1 < argc) {
                remoteIp = argv[argIdx + 1];
            } else if (argIdx < argc) {
                std::string spec = argv[argIdx];
                if (!StringUtils::ParsePortSpec(spec, port, protocol)) {
                    std::cerr << "ufw: Invalid port specification '" << spec << "'.\n";
                    return 1;
                }
            }

            if (m_fw.AddRule(action, dir, port, protocol, remoteIp, appPath)) {
                std::cout << "Rule added successfully.\n";
                return 0;
            }
            std::cerr << "ufw: Failed to add firewall rule.\n";
            return 1;
        }

        std::cerr << "ufw: Unknown command '" << cmd << "'. Type 'ufw help' for usage.\n";
        return 1;
    }
