#include "registry_engine.hpp"
#include "registry_record.hpp"
#include "scoped_hkey.hpp"
#include "string_conversion.hpp"
#include "undo_record.hpp"

bool RegistryEngine::DeleteTreeTransactedInternal(HKEY hKey, const std::wstring& subKey, REGSAM viewSam, HANDLE hTx) {
        ScopedHKey curKey;
        LSTATUS status = RegOpenKeyTransactedW(hKey, subKey.c_str(), 0, KEY_READ | KEY_WRITE | viewSam, &curKey.handle, hTx, nullptr);
        if (status != ERROR_SUCCESS) return false;

        WCHAR childName[512];
        DWORD cchName = 512;
        while (RegEnumKeyExW(curKey, 0, childName, &cchName, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
            if (!DeleteTreeTransactedInternal(curKey, childName, viewSam, hTx)) return false;
            cchName = 512;
        }

        curKey = ScopedHKey();
        return RegDeleteKeyTransactedW(hKey, subKey.c_str(), viewSam, 0, hTx, nullptr) == ERROR_SUCCESS;
    }

bool RegistryEngine::QueryValue(HKEY root, const std::string& rootStr, const std::string& subKey, const std::string& valueName, RegistryRecord& outRecord, REGSAM viewSam , HANDLE hTx ) {
        outRecord.rootKey = rootStr;
        outRecord.subKey = subKey;
        outRecord.valueName = valueName;
        outRecord.exists = false;

        ScopedHKey key;
        LSTATUS status = ERROR_SUCCESS;
        std::wstring wSubKey = Utils::ToWString(subKey);
        std::wstring wValueName = Utils::ToWString(valueName);

        REGSAM sam = KEY_READ | viewSam;
        if (hTx != INVALID_HANDLE_VALUE) {
            status = RegOpenKeyTransactedW(root, wSubKey.c_str(), 0, sam, &key.handle, hTx, nullptr);
        } else {
            status = RegOpenKeyExW(root, wSubKey.c_str(), 0, sam, &key.handle);
        }

        if (status != ERROR_SUCCESS) return false;

        DWORD type = 0;
        DWORD cbData = 0;
        for (int retry = 0; retry < 5; ++retry) {
            status = RegQueryValueExW(key, wValueName.c_str(), nullptr, &type, nullptr, &cbData);
            if (status != ERROR_SUCCESS) return false;

            outRecord.rawData.resize(cbData);
            status = RegQueryValueExW(key, wValueName.c_str(), nullptr, &type, outRecord.rawData.data(), &cbData);
            if (status == ERROR_SUCCESS) {
                outRecord.type = type;
                outRecord.exists = true;
                return true;
            }
            if (status != ERROR_MORE_DATA) break;
        }
        return false;
    }

bool RegistryEngine::SetValueRaw(HKEY root, const std::string& subKey, const std::string& valueName, DWORD type, const std::vector<uint8_t>& data, std::string& err, REGSAM viewSam , HANDLE hTx ) {
        ScopedHKey key;
        LSTATUS status = ERROR_SUCCESS;
        std::wstring wSubKey = Utils::ToWString(subKey);
        std::wstring wValueName = Utils::ToWString(valueName);
        REGSAM sam = KEY_WRITE | KEY_READ | viewSam;

        if (hTx != INVALID_HANDLE_VALUE) {
            status = RegCreateKeyTransactedW(root, wSubKey.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, sam, nullptr, &key.handle, nullptr, hTx, nullptr);
        } else {
            status = RegCreateKeyExW(root, wSubKey.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, sam, nullptr, &key.handle, nullptr);
        }

        if (status != ERROR_SUCCESS) {
            err = "Failed to open/create registry key: " + Utils::FormatWin32Error(status);
            return false;
        }

        status = RegSetValueExW(key, wValueName.c_str(), 0, type, data.data(), static_cast<DWORD>(data.size()));
        if (status != ERROR_SUCCESS) {
            err = "Failed to write registry value: " + Utils::FormatWin32Error(status);
            return false;
        }
        return true;
    }

bool RegistryEngine::DeleteValue(HKEY root, const std::string& subKey, const std::string& valueName, std::string& err, REGSAM viewSam , HANDLE hTx ) {
        ScopedHKey key;
        LSTATUS status = ERROR_SUCCESS;
        std::wstring wSubKey = Utils::ToWString(subKey);
        std::wstring wValueName = Utils::ToWString(valueName);
        REGSAM sam = KEY_SET_VALUE | viewSam;

        if (hTx != INVALID_HANDLE_VALUE) {
            status = RegOpenKeyTransactedW(root, wSubKey.c_str(), 0, sam, &key.handle, hTx, nullptr);
        } else {
            status = RegOpenKeyExW(root, wSubKey.c_str(), 0, sam, &key.handle);
        }

        if (status != ERROR_SUCCESS) {
            err = "Key not found or access denied: " + Utils::FormatWin32Error(status);
            return false;
        }

        status = RegDeleteValueW(key, wValueName.c_str());
        if (status != ERROR_SUCCESS) {
            err = "Failed to delete value: " + Utils::FormatWin32Error(status);
            return false;
        }
        return true;
    }

void RegistryEngine::BackupKeyRecursive(HKEY root, const std::string& rootStr, const std::string& subKey, std::vector<UndoRecord>& journal, REGSAM viewSam, HANDLE hTx) {
        ScopedHKey key;
        std::wstring wSub = Utils::ToWString(subKey);
        REGSAM sam = KEY_READ | viewSam;
        LSTATUS st = (hTx != INVALID_HANDLE_VALUE) ?
            RegOpenKeyTransactedW(root, wSub.c_str(), 0, sam, &key.handle, hTx, nullptr) :
            RegOpenKeyExW(root, wSub.c_str(), 0, sam, &key.handle);
        if (st != ERROR_SUCCESS) return;

        DWORD valIndex = 0;
        WCHAR valName[16384];
        DWORD cchVal = 16384;
        DWORD type = 0;
        bool hasValues = false;

        while (RegEnumValueW(key, valIndex++, valName, &cchVal, nullptr, &type, nullptr, nullptr) == ERROR_SUCCESS) {
            hasValues = true;
            RegistryRecord rec;
            std::string vName = Utils::ToString(valName);
            if (QueryValue(root, rootStr, subKey, vName, rec, viewSam, hTx)) {
                UndoRecord u;
                u.op = "set";
                u.path = rootStr + "\\" + subKey;
                u.valueName = vName;
                u.hadPreviousValue = true;
                u.typeStr = rec.GetTypeString();
                u.rawDataHex = Utils::BytesToHex(rec.rawData);
                journal.push_back(u);
            }
            cchVal = 16384;
        }

        // If the key is an empty leaf, record a default record to ensure its path is recreated on undo
        if (!hasValues) {
            UndoRecord u;
            u.op = "set";
            u.path = rootStr + "\\" + subKey;
            u.valueName = "";
            u.hadPreviousValue = false;
            u.typeStr = "REG_SZ";
            u.rawDataHex = "";
            journal.push_back(u);
        }

        DWORD subIndex = 0;
        WCHAR childName[512];
        DWORD cchChild = 512;
        while (RegEnumKeyExW(key, subIndex++, childName, &cchChild, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
            std::string nextSub = subKey.empty() ? Utils::ToString(childName) : (subKey + "\\" + Utils::ToString(childName));
            BackupKeyRecursive(root, rootStr, nextSub, journal, viewSam, hTx);
            cchChild = 512;
        }
    }

bool RegistryEngine::DeleteKeyRecursive(HKEY root, const std::string& subKey, std::string& err, REGSAM viewSam , HANDLE hTx ) {
        std::wstring wSubKey = Utils::ToWString(subKey);
        if (hTx != INVALID_HANDLE_VALUE) {
            if (!DeleteTreeTransactedInternal(root, wSubKey, viewSam, hTx)) {
                err = "Failed transacted recursive key deletion: " + Utils::FormatWin32Error(GetLastError());
                return false;
            }
            return true;
        } else {
            LSTATUS st = RegDeleteTreeW(root, wSubKey.c_str());
            if (st != ERROR_SUCCESS) {
                err = "Failed recursive tree deletion: " + Utils::FormatWin32Error(st);
                return false;
            }
            return true;
        }
    }
