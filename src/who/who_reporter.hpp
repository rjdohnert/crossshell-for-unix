#pragma once

#include "user_session.hpp"
#include "who.hpp"

class WhoReporter {
public:
    static void PrintHeader();

    static void PrintSession(const UserSession& s);

    static void PrintCount(const std::vector<UserSession>& sessions);
};
