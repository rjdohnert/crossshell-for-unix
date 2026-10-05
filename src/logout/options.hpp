/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 */

#ifndef LOGOUT_OPTIONS_HPP
#define LOGOUT_OPTIONS_HPP

class LogoutOptions {
public:
    bool force = false;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, char* argv[]);
    void PrintUsage(const char* progName) const;
    void PrintVersion() const;
};

#endif // LOGOUT_OPTIONS_HPP
