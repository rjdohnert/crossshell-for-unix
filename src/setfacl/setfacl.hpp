#ifndef SETFACL_HPP
#define SETFACL_HPP

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#pragma comment(lib, "Advapi32.lib")

struct FaclEntry {
    std::wstring kind;
    std::wstring name;
    std::wstring rights;
};

struct SetfaclOptions {
    bool recursive = false;
    bool removeDacl = false;
    bool showHelp = false;
    bool showVersion = false;
    std::wstring spec;
    std::vector<std::wstring> paths;
};

#endif // SETFACL_HPP
