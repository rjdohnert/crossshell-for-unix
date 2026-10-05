/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 */

#ifndef LOGOUT_ENGINE_HPP
#define LOGOUT_ENGINE_HPP

class SessionManager {
public:
    static bool TerminateSession(bool force);
};

#endif // LOGOUT_ENGINE_HPP
