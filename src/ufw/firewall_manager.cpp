#include "firewall_manager.hpp"
#include "firewall_rule.hpp"
#include "string_utils.hpp"

FirewallManager::~FirewallManager() {
        if (m_policy) {
            m_policy->Release();
            m_policy = nullptr;
        }
    }

bool FirewallManager::Initialize() {
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

bool FirewallManager::GetProfileStatus(bool& isEnabled, std::string& activeProfiles) {
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

bool FirewallManager::SetFirewallEnabled(bool enable) {
        if (!Initialize()) return false;
        VARIANT_BOOL state = enable ? VARIANT_TRUE : VARIANT_FALSE;
        HRESULT hr1 = m_policy->put_FirewallEnabled(NET_FW_PROFILE2_DOMAIN, state);
        HRESULT hr2 = m_policy->put_FirewallEnabled(NET_FW_PROFILE2_PRIVATE, state);
        HRESULT hr3 = m_policy->put_FirewallEnabled(NET_FW_PROFILE2_PUBLIC, state);
        return SUCCEEDED(hr1) && SUCCEEDED(hr2) && SUCCEEDED(hr3);
    }

bool FirewallManager::SetDefaultPolicy(NET_FW_RULE_DIRECTION dir, NET_FW_ACTION action) {
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

std::vector<FirewallRule> FirewallManager::EnumerateRules() {
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

bool FirewallManager::AddRule(NET_FW_ACTION action, NET_FW_RULE_DIRECTION dir, const std::string& port, LONG protocol, const std::string& remoteIp, const std::string& appPath) {
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

bool FirewallManager::DeleteRuleByNumber(int ruleNum) {
        auto rules = EnumerateRules();
        for (const auto& r : rules) {
            if (r.id == ruleNum) {
                return DeleteRuleByName(r.name);
            }
        }
        return false;
    }

bool FirewallManager::DeleteRuleByName(const std::string& name) {
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

bool FirewallManager::ResetDefaults() {
        if (!Initialize()) return false;
        HRESULT hr = m_policy->RestoreLocalFirewallDefaults();
        return SUCCEEDED(hr);
    }
