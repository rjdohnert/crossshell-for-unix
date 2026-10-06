#pragma once

#include "umask.hpp"

class SystemUmaskManager {
public:
    static unsigned int GetSystemUmask();

    static void SetSystemUmask(unsigned int mask);

    static int ExecuteSubcommand(const std::wstring& cmdline);
};
