#include "registry_reader.hpp"
#include "scoped_hkey.hpp"
#include "software_item.hpp"

wstring RegistryReader::ReadString(HKEY hKey, const wchar_t* valueName) {
        DWORD dataType = 0;
        DWORD dataSize = 0;

        if (RegQueryValueExW(hKey, valueName, nullptr, &dataType, nullptr, &dataSize) != ERROR_SUCCESS || dataSize == 0) {
            return L"";
        }

        vector<wchar_t> buffer((dataSize / sizeof(wchar_t)) + 1, L'\0');
        if (RegQueryValueExW(hKey, valueName, nullptr, &dataType, 
                             reinterpret_cast<BYTE*>(buffer.data()), &dataSize) == ERROR_SUCCESS) {
            if (dataType == REG_SZ || dataType == REG_EXPAND_SZ) {
                return wstring(buffer.data());
            }
        }
        return L"";
    }

DWORD RegistryReader::ReadDword(HKEY hKey, const wchar_t* valueName) {
        DWORD val = 0;
        DWORD size = sizeof(DWORD);
        DWORD type = 0;
        if (RegQueryValueExW(hKey, valueName, nullptr, &type, 
                             reinterpret_cast<BYTE*>(&val), &size) == ERROR_SUCCESS) {
            if (type == REG_DWORD) return val;
        }
        return 0;
    }

void RegistryReader::EnumerateHive(HKEY hRoot, const wchar_t* subkeyPath, REGSAM samDesired, 
                              const wstring& archLabel, vector<SoftwareItem>& items) {
        ScopedHKey hKey;
        if (RegOpenKeyExW(hRoot, subkeyPath, 0, KEY_READ | samDesired, hKey.receive()) != ERROR_SUCCESS) {
            return;
        }

        DWORD index = 0;
        wchar_t keyName[256];
        DWORD keyNameSize = 256;

        while (RegEnumKeyExW(hKey.get(), index++, keyName, &keyNameSize, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
            keyNameSize = 256;

            ScopedHKey hSubKey;
            if (RegOpenKeyExW(hKey.get(), keyName, 0, KEY_READ | samDesired, hSubKey.receive()) == ERROR_SUCCESS) {
                wstring name = ReadString(hSubKey.get(), L"DisplayName");
                if (name.empty()) continue;

                SoftwareItem item;
                item.name = name;
                item.revision = ReadString(hSubKey.get(), L"DisplayVersion");
                if (item.revision.empty()) item.revision = L"N/A";

                item.vendor = ReadString(hSubKey.get(), L"Publisher");
                if (item.vendor.empty()) item.vendor = L"Unknown";

                item.installDate = ReadString(hSubKey.get(), L"InstallDate");
                if (item.installDate.empty()) item.installDate = L"N/A";

                item.location = ReadString(hSubKey.get(), L"InstallLocation");
                if (item.location.empty()) item.location = L"N/A";

                item.arch = archLabel;
                item.isSystemComponent = (ReadDword(hSubKey.get(), L"SystemComponent") == 1);

                items.push_back(item);
            }
        }
    }
