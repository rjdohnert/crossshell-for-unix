#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <lm.h>
#include <ntsecapi.h>
#include <string>
#include <vector>

struct AccountRecord {
    std::wstring name;
    std::wstring fullName;
    std::wstring homeDir;
    std::wstring shell;
    std::wstring comment;
    std::wstring groups;
    std::wstring loginDuration;
    std::wstring status;
    unsigned long uid = 0;
    bool disabled = false;
};

enum class LsuserFormat { Table, Csv, Json };
