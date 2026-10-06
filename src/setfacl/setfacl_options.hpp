#ifndef SETFACL_OPTIONS_HPP
#define SETFACL_OPTIONS_HPP

#include "setfacl.hpp"

class SetfaclOptionParser {
public:
    static void PrintUsage(const wchar_t* progName);
    static void PrintVersion();
    bool Parse(int argc, wchar_t* argv[], SetfaclOptions& opts) const;
};

#endif // SETFACL_OPTIONS_HPP
