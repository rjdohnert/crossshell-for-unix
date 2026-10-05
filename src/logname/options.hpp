/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 */

#ifndef LOGNAME_OPTIONS_HPP
#define LOGNAME_OPTIONS_HPP

#include <string>

class LognameOptions {
public:
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, char* argv[]);
    void PrintUsage(const char* progName) const;
    void PrintVersion() const;
};

#endif // LOGNAME_OPTIONS_HPP
