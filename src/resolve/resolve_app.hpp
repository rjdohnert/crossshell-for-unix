#pragma once

#include "resolve.hpp"

class ResolveApplication {
public:
        void PrintHelp(const wchar_t* progName) const;

    int Run(int argc, wchar_t* argv[]);
};
