#pragma once

#include "watch.hpp"

class WatchApplication {
public:
    static BOOL WINAPI CtrlHandler(DWORD type);

    int Run(int argc, wchar_t* argv[]) const;
};
