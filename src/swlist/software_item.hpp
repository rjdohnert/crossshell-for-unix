#pragma once

#include "swlist.hpp"

struct SoftwareItem {
    wstring name;            // DisplayName / Product Name
    wstring revision;        // DisplayVersion
    wstring vendor;          // Publisher
    wstring installDate;     // InstallDate
    wstring location;        // InstallLocation
    wstring arch;            // x64, x86, or User
    bool isSystemComponent = false;
};
