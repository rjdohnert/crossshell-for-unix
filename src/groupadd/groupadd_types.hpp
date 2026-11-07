#pragma once

#include <windows.h>

#include <string>

struct CommandOptions {
    std::wstring groupName;
    std::wstring comment;
    bool force = false;
    bool systemGroup = false;
    bool quiet = false;
    bool verbose = false;
    bool showHelp = false;
    bool showVersion = false;
    bool readFromStdin = false;
};

struct GroupRequest {
    std::wstring name;
    std::wstring comment;
};

struct GroupAddResult {
    DWORD status = ERROR_SUCCESS;
    DWORD parameterError = 0;
    bool created = false;
    bool alreadyExists = false;
};
