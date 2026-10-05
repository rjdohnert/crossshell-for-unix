/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 */

#ifndef LOGNAME_ENGINE_HPP
#define LOGNAME_ENGINE_HPP

#include <string>

class LoginNameProvider {
public:
    static std::string GetLoginName();
};

#endif // LOGNAME_ENGINE_HPP
