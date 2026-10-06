#pragma once

#include "registry_record.hpp"
#include "registryctl.hpp"
#include "undo_record.hpp"

class RegistryEngine {
private:
    static bool DeleteTreeTransactedInternal(HKEY hKey, const std::wstring& subKey, REGSAM viewSam, HANDLE hTx);

public:
    static bool QueryValue(HKEY root, const std::string& rootStr, const std::string& subKey, const std::string& valueName, RegistryRecord& outRecord, REGSAM viewSam = 0, HANDLE hTx = INVALID_HANDLE_VALUE);

    static bool SetValueRaw(HKEY root, const std::string& subKey, const std::string& valueName, DWORD type, const std::vector<uint8_t>& data, std::string& err, REGSAM viewSam = 0, HANDLE hTx = INVALID_HANDLE_VALUE);

    static bool DeleteValue(HKEY root, const std::string& subKey, const std::string& valueName, std::string& err, REGSAM viewSam = 0, HANDLE hTx = INVALID_HANDLE_VALUE);

    static void BackupKeyRecursive(HKEY root, const std::string& rootStr, const std::string& subKey, std::vector<UndoRecord>& journal, REGSAM viewSam, HANDLE hTx);

    static bool DeleteKeyRecursive(HKEY root, const std::string& subKey, std::string& err, REGSAM viewSam = 0, HANDLE hTx = INVALID_HANDLE_VALUE);
};
