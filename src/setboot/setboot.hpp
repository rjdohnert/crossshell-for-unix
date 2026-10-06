#ifndef SETBOOT_HPP
#define SETBOOT_HPP

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fcntl.h>
#include <io.h>

#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <cwctype>
#include <memory>

#pragma comment(lib, "Advapi32.lib")

struct BootEntry {
    WORD id{0};
    std::wstring name;         // e.g. Boot0001
    std::wstring description;  // e.g. Windows Boot Manager
    std::wstring devicePath;   // e.g. \EFI\Microsoft\Boot\bootmgfw.efi
    DWORD attributes{0};
    bool isValid{false};
};

struct BootEnvironment {
    std::wstring firmwareType{L"Unknown"};
    bool isUefi{false};
    WORD currentBootId{0};
    bool hasCurrentBoot{false};
    WORD nextBootId{0};
    bool hasNextBoot{false};
    WORD timeoutSeconds{0};
    bool hasTimeout{false};
    bool bootOrderReadOk{false};
    DWORD bootOrderReadError{ERROR_SUCCESS};
    std::vector<WORD> bootOrder;
    std::vector<BootEntry> bootEntries;
};

#endif // SETBOOT_HPP
