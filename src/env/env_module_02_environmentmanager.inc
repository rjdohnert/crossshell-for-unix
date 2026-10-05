// ============================================================================
// Environment Manager (OOP Environment Map & Serialization)
// ============================================================================

class EnvironmentManager {
public:
    using EnvMap = std::map<std::wstring, std::wstring, CaseInsensitiveWStringCompare>;

    EnvironmentManager() = default;

    void loadSystemEnvironment() {
        m_envMap.clear();
        LPWCH envBlock = GetEnvironmentStringsW();
        if (!envBlock) return;

        LPWCH current = envBlock;
        while (*current) {
            std::wstring entry = current;
            // Skip Windows hidden/per-drive working directory variables (e.g. "=C:=")
            if (!entry.empty() && entry[0] != L'=') {
                size_t equalsPos = entry.find(L'=');
                if (equalsPos != std::wstring::npos) {
                    std::wstring key = entry.substr(0, equalsPos);
                    std::wstring val = entry.substr(equalsPos + 1);
                    m_envMap[key] = val;
                }
            }
            current += entry.length() + 1;
        }

        FreeEnvironmentStringsW(envBlock);
    }

    void clear() {
        m_envMap.clear();
    }

    void set(const std::wstring& name, const std::wstring& value) {
        m_envMap[name] = value;
    }

    void unset(const std::wstring& name) {
        m_envMap.erase(name);
    }

    const EnvMap& getMap() const {
        return m_envMap;
    }

    // Windows CreateProcessW requires a double-null-terminated block of sorted strings:
    // "VAR1=VAL1\0VAR2=VAL2\0\0"
    std::vector<wchar_t> createEnvironmentBlock() const {
        std::vector<wchar_t> block;
        for (const auto& [key, val] : m_envMap) {
            std::wstring entry = key + L"=" + val;
            block.insert(block.end(), entry.begin(), entry.end());
            block.push_back(L'\0');
        }
        block.push_back(L'\0'); // Final terminating null
        return block;
    }

    void printEnvironment(std::wostream& os, wchar_t delimiter) const {
        for (const auto& [key, val] : m_envMap) {
            os << key << L"=" << val << delimiter;
        }
        os.flush();
    }

private:
    EnvMap m_envMap;
};
