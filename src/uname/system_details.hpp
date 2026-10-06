#pragma once

#include "uname.hpp"

struct SystemDetails {
    std::wstring sysname;          // -s
    std::wstring nodename;         // -n
    std::wstring release;          // -r
    std::wstring version;          // -v
    std::wstring machine;          // -m
    std::wstring machineId;        // -i
    std::wstring licenseId;        // -l
    std::wstring model;            // -M
    std::wstring marketingEdition;
    DWORD buildNumber{0};
    DWORD ubr{0};
    DWORD numProcessors{0};
};
