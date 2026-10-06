#pragma once

#include "swlist.hpp"

class SystemInfo {
public:
    static wstring GetHostNameString();

    static wstring ToUpper(wstring str);
};
