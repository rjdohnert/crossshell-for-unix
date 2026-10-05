/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <netfw.h>
#include <comdef.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <limits>
#include <memory>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "advapi32.lib")

// ============================================================================
// 1. RAII COM LIFECYCLE & STRING UTILITIES
// ============================================================================
class ComInitializer {
private:
    HRESULT m_hr;

public:
    ComInitializer() {
        m_hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(m_hr)) {
            m_hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        }
    }

    ~ComInitializer() {
        if (SUCCEEDED(m_hr) && m_hr != S_FALSE) {
            CoUninitialize();
        }
    }

    bool Succeeded() const { return true; }
};

class StringUtils {
public:
    static std::string ToLower(const std::string& text) {
        std::string result = text;
        std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return result;
    }

    static std::wstring ToWide(const std::string& str) {
        if (str.empty()) return L"";
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0);
        std::wstring wstr(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), &wstr[0], size_needed);
        return wstr;
    }

    static std::string ToNarrow(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), NULL, 0, NULL, NULL);
        std::string str(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), &str[0], size_needed, NULL, NULL);
        return str;
    }

    static bool ParseRuleNumber(const std::string& text, int& value) {
        if (text.empty()) return false;
        char* end = nullptr;
        unsigned long parsed = std::strtoul(text.c_str(), &end, 10);
        if (end == text.c_str() || *end != '\0' || parsed > static_cast<unsigned long>(std::numeric_limits<int>::max())) {
            return false;
        }
        value = static_cast<int>(parsed);
        return true;
    }

    static bool ParsePortSpec(const std::string& spec, std::string& port, LONG& protocol) {
        port.clear();
        protocol = NET_FW_IP_PROTOCOL_ANY;

        if (spec.empty()) return true;

        const size_t slashPos = spec.find('/');
        if (slashPos == std::string::npos) {
            port = spec;
            return true;
        }

        std::string protoText = ToLower(spec.substr(slashPos + 1));
        if (protoText == "tcp") {
            protocol = NET_FW_IP_PROTOCOL_TCP;
        } else if (protoText == "udp") {
            protocol = NET_FW_IP_PROTOCOL_UDP;
        } else {
            return false;
        }

        std::string portText = spec.substr(0, slashPos);
        if (portText.empty() || portText == "any") {
            return true;
        }

        char* end = nullptr;
        unsigned long parsed = std::strtoul(portText.c_str(), &end, 10);
        if (end == portText.c_str() || *end != '\0' || parsed == 0 || parsed > 65535) {
            return false;
        }

        port = portText;
        return true;
    }
};

// ============================================================================
// 2. DOMAIN ENTITY MODEL
// ============================================================================
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

// ============================================================================
// 3. FIREWALL MANAGER (COM INetFwPolicy2 WRAPPER)
// ============================================================================
class FirewallManager {
private:
    INetFwPolicy2* m_policy = nullptr;

public:
    FirewallManager() = default;

    ~FirewallManager() {
        if (m_policy) {
            m_policy->Release();
            m_policy = nullptr;
        }
    }

    bool Initialize() {
        if (m_policy) return true;
        HRESULT hr = CoCreateInstance(
            __uuidof(NetFwPolicy2),
            nullptr,
            CLSCTX_INPROC_SERVER,
            __uuidof(INetFwPolicy2),
            reinterpret_cast<void**>(&m_policy)
        );
        if (FAILED(hr)) {
            hr = CoCreateInstance(
                __uuidof(NetFwPolicy2),
                nullptr,
                CLSCTX_ALL,
                __uuidof(INetFwPolicy2),
                reinterpret_cast<void**>(&m_policy)
            );
        }
        return SUCCEEDED(hr) && m_policy != nullptr;
    }

    bool GetProfileStatus(bool& isEnabled, std::string& activeProfiles) {
        if (!Initialize()) return false;

        long currentProfiles = 0;
        m_policy->get_CurrentProfileTypes(&currentProfiles);

        std::vector<std::string> names;
        if (currentProfiles & NET_FW_PROFILE2_DOMAIN) names.push_back("Domain");
        if (currentProfiles & NET_FW_PROFILE2_PRIVATE) names.push_back("Private");
        if (currentProfiles & NET_FW_PROFILE2_PUBLIC) names.push_back("Public");
        if (names.empty()) names.push_back("Standard");

        std::ostringstream oss;
        for (size_t i = 0; i < names.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << names[i];
        }
        activeProfiles = oss.str();

        VARIANT_BOOL domainEnabled = VARIANT_FALSE;
        VARIANT_BOOL privateEnabled = VARIANT_FALSE;
        VARIANT_BOOL publicEnabled = VARIANT_FALSE;

        m_policy->get_FirewallEnabled(NET_FW_PROFILE2_DOMAIN, &domainEnabled);
        m_policy->get_FirewallEnabled(NET_FW_PROFILE2_PRIVATE, &privateEnabled);
        m_policy->get_FirewallEnabled(NET_FW_PROFILE2_PUBLIC, &publicEnabled);

        isEnabled = (domainEnabled == VARIANT_TRUE || privateEnabled == VARIANT_TRUE || publicEnabled == VARIANT_TRUE);
        return true;
    }

    bool SetFirewallEnabled(bool enable) {
        if (!Initialize()) return false;
        VARIANT_BOOL state = enable ? VARIANT_TRUE : VARIANT_FALSE;
        HRESULT hr1 = m_policy->put_FirewallEnabled(NET_FW_PROFILE2_DOMAIN, state);
        HRESULT hr2 = m_policy->put_FirewallEnabled(NET_FW_PROFILE2_PRIVATE, state);
        HRESULT hr3 = m_policy->put_FirewallEnabled(NET_FW_PROFILE2_PUBLIC, state);
        return SUCCEEDED(hr1) && SUCCEEDED(hr2) && SUCCEEDED(hr3);
    }

    bool SetDefaultPolicy(NET_FW_RULE_DIRECTION dir, NET_FW_ACTION action) {
        if (!Initialize()) return false;
        long profiles[] = { NET_FW_PROFILE2_DOMAIN, NET_FW_PROFILE2_PRIVATE, NET_FW_PROFILE2_PUBLIC };
        for (long p : profiles) {
            if (dir == NET_FW_RULE_DIR_IN) {
                if (FAILED(m_policy->put_DefaultInboundAction(static_cast<NET_FW_PROFILE_TYPE2>(p), action))) return false;
            } else {
                if (FAILED(m_policy->put_DefaultOutboundAction(static_cast<NET_FW_PROFILE_TYPE2>(p), action))) return false;
            }
        }
        return true;
    }

    std::vector<FirewallRule> EnumerateRules() {
        std::vector<FirewallRule> result;
        if (!Initialize()) return result;

        INetFwRules* rulesCol = nullptr;
        if (FAILED(m_policy->get_Rules(&rulesCol)) || !rulesCol) return result;

        IUnknown* enumUnknown = nullptr;
        if (FAILED(rulesCol->get__NewEnum(&enumUnknown)) || !enumUnknown) {
            rulesCol->Release();
            return result;
        }

        IEnumVARIANT* enumVariant = nullptr;
        if (FAILED(enumUnknown->QueryInterface(__uuidof(IEnumVARIANT), reinterpret_cast<void**>(&enumVariant))) || !enumVariant) {
            enumUnknown->Release();
            rulesCol->Release();
            return result;
        }

        VARIANT var;
        VariantInit(&var);
        ULONG fetched = 0;
        int nextId = 1;

        while (enumVariant->Next(1, &var, &fetched) == S_OK && fetched == 1) {
            if (V_VT(&var) == VT_DISPATCH && V_DISPATCH(&var)) {
                INetFwRule* rule = nullptr;
                if (SUCCEEDED(V_DISPATCH(&var)->QueryInterface(__uuidof(INetFwRule), reinterpret_cast<void**>(&rule))) && rule) {
                    BSTR bName = nullptr;
                    rule->get_Name(&bName);
                    std::string name = bName ? StringUtils::ToNarrow(bName) : "";
                    SysFreeString(bName);

                    if (name.rfind("UFW:", 0) == 0) {
                        FirewallRule fr;
                        fr.id = nextId++;
                        fr.name = name;

                        BSTR bDesc = nullptr;
                        rule->get_Description(&bDesc);
                        fr.description = bDesc ? StringUtils::ToNarrow(bDesc) : "";
                        SysFreeString(bDesc);

                        BSTR bApp = nullptr;
                        rule->get_ApplicationName(&bApp);
                        fr.appPath = bApp ? StringUtils::ToNarrow(bApp) : "";
                        SysFreeString(bApp);

                        BSTR bPorts = nullptr;
                        rule->get_LocalPorts(&bPorts);
                        fr.ports = bPorts ? StringUtils::ToNarrow(bPorts) : "Any";
                        SysFreeString(bPorts);

                        BSTR bRemote = nullptr;
                        rule->get_RemoteAddresses(&bRemote);
                        fr.remoteIp = bRemote ? StringUtils::ToNarrow(bRemote) : "*";
                        SysFreeString(bRemote);

                        LONG proto = 0;
                        rule->get_Protocol(&proto);
                        if (proto == NET_FW_IP_PROTOCOL_TCP) fr.protocol = "tcp";
                        else if (proto == NET_FW_IP_PROTOCOL_UDP) fr.protocol = "udp";
                        else fr.protocol = "any";

                        rule->get_Action(&fr.action);
                        rule->get_Direction(&fr.dir);

                        VARIANT_BOOL enabled = VARIANT_FALSE;
                        rule->get_Enabled(&enabled);
                        fr.enabled = (enabled == VARIANT_TRUE);

                        result.push_back(fr);
                    }
                    rule->Release();
                }
            }
            VariantClear(&var);
        }

        enumVariant->Release();
        enumUnknown->Release();
        rulesCol->Release();
        return result;
    }

    bool AddRule(NET_FW_ACTION action, NET_FW_RULE_DIRECTION dir, const std::string& port, LONG protocol, const std::string& remoteIp, const std::string& appPath) {
        if (!Initialize()) return false;

        INetFwRules* rulesCol = nullptr;
        if (FAILED(m_policy->get_Rules(&rulesCol)) || !rulesCol) return false;

        INetFwRule* rule = nullptr;
        HRESULT hr = CoCreateInstance(__uuidof(NetFwRule), nullptr, CLSCTX_ALL, __uuidof(INetFwRule), reinterpret_cast<void**>(&rule));
        if (FAILED(hr) || !rule) {
            rulesCol->Release();
            return false;
        }

        std::ostringstream nameStream;
        nameStream << "UFW: " << (action == NET_FW_ACTION_ALLOW ? "ALLOW " : "DENY ")
                   << (dir == NET_FW_RULE_DIR_IN ? "IN " : "OUT ");
        if (!appPath.empty()) nameStream << "APP " << appPath;
        else if (!port.empty()) nameStream << "PORT " << port << "/" << (protocol == NET_FW_IP_PROTOCOL_TCP ? "tcp" : (protocol == NET_FW_IP_PROTOCOL_UDP ? "udp" : "any"));
        if (!remoteIp.empty() && remoteIp != "*") nameStream << " FROM " << remoteIp;

        std::wstring wName = StringUtils::ToWide(nameStream.str());
        BSTR bName = SysAllocString(wName.c_str());
        rule->put_Name(bName);
        SysFreeString(bName);

        rule->put_Action(action);
        rule->put_Direction(dir);
        rule->put_Enabled(VARIANT_TRUE);
        rule->put_Profiles(NET_FW_PROFILE2_ALL);

        if (!appPath.empty()) {
            std::wstring wApp = StringUtils::ToWide(appPath);
            BSTR bApp = SysAllocString(wApp.c_str());
            rule->put_ApplicationName(bApp);
            SysFreeString(bApp);
        }

        if (protocol != NET_FW_IP_PROTOCOL_ANY) {
            rule->put_Protocol(protocol);
        }

        if (!port.empty()) {
            std::wstring wPort = StringUtils::ToWide(port);
            BSTR bPort = SysAllocString(wPort.c_str());
            rule->put_LocalPorts(bPort);
            SysFreeString(bPort);
        }

        if (!remoteIp.empty()) {
            std::wstring wRemote = StringUtils::ToWide(remoteIp);
            BSTR bRemote = SysAllocString(wRemote.c_str());
            rule->put_RemoteAddresses(bRemote);
            SysFreeString(bRemote);
        }

        hr = rulesCol->Add(rule);
        rule->Release();
        rulesCol->Release();
        return SUCCEEDED(hr);
    }

    bool DeleteRuleByNumber(int ruleNum) {
        auto rules = EnumerateRules();
        for (const auto& r : rules) {
            if (r.id == ruleNum) {
                return DeleteRuleByName(r.name);
            }
        }
        return false;
    }

    bool DeleteRuleByName(const std::string& name) {
        if (!Initialize()) return false;
        INetFwRules* rulesCol = nullptr;
        if (FAILED(m_policy->get_Rules(&rulesCol)) || !rulesCol) return false;

        std::wstring wName = StringUtils::ToWide(name);
        BSTR bName = SysAllocString(wName.c_str());
        HRESULT hr = rulesCol->Remove(bName);
        SysFreeString(bName);
        rulesCol->Release();
        return SUCCEEDED(hr);
    }

    bool ResetDefaults() {
        if (!Initialize()) return false;
        HRESULT hr = m_policy->RestoreLocalFirewallDefaults();
        return SUCCEEDED(hr);
    }
};

// ============================================================================
// 4. FORMATTER & PRESENTATION LAYER
// ============================================================================
class RuleFormatter {
public:
    static void DisplayHelp() {
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

    static void DisplayVersion() {
        std::cout << "ufw 0.36.2 (Windows Advanced Security Edition)\n"
                  << "Copyright 2026 Roberto J. Dohnert\n";
    }

    static void DisplayStatus(const std::string& mode, bool isEnabled, const std::string& activeProfiles, const std::vector<FirewallRule>& rules) {
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
};

// ============================================================================
// 5. COMMAND LINE PARSER & APPLICATION CONTROLLER
// ============================================================================
class UfwApplication {
private:
    FirewallManager m_fw;

public:
    int Run(int argc, char* argv[]) {
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
};

int main(int argc, char* argv[]) {
    ComInitializer com;
    UfwApplication app;
    return app.Run(argc, argv);
}
