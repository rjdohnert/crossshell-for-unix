#pragma once

#include "software_item.hpp"
#include "swlist.hpp"

class RegistryReader {
public:
    static wstring ReadString(HKEY hKey, const wchar_t* valueName);

    static DWORD ReadDword(HKEY hKey, const wchar_t* valueName);

    static void EnumerateHive(HKEY hRoot, const wchar_t* subkeyPath, REGSAM samDesired, 
                              const wstring& archLabel, vector<SoftwareItem>& items);
};
