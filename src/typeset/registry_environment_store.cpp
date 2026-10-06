#include "registry_environment_store.hpp"

bool RegistryEnvironmentStore::SetPersistentUserEnv(const std::wstring& var, const std::wstring& val) {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
            if (val.empty()) {
                RegDeleteValueW(hKey, var.c_str());
            } else {
                RegSetValueExW(
                    hKey, 
                    var.c_str(), 
                    0, 
                    REG_SZ, 
                    reinterpret_cast<const BYTE*>(val.c_str()), 
                    static_cast<DWORD>((val.length() + 1) * sizeof(wchar_t))
                );
            }
            RegCloseKey(hKey);

            DWORD_PTR dwResult;
            SendMessageTimeoutW(
                HWND_BROADCAST, 
                WM_SETTINGCHANGE, 
                0, 
                reinterpret_cast<LPARAM>(L"Environment"), 
                SMTO_ABORTIFHUNG, 
                5000, 
                &dwResult
            );
            return true;
        }
        return false;
    }
