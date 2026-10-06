#ifndef NFSCTL_OPTIONS_HPP
#define NFSCTL_OPTIONS_HPP

#include "nfsctl.hpp"
#include "engine.hpp"

class NfsctlHelpSystem {
public:
    static void PrintMainHelp();
    static bool HasHelpFlag(const std::vector<std::wstring>& args);
};

class NfsctlOptionsParser {
public:
    static DWORD ParseGlobalOptions(std::vector<std::wstring>& args, GlobalOptions& options);
};

#endif // NFSCTL_OPTIONS_HPP
