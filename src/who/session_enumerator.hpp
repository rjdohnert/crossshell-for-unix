#pragma once

#include "user_session.hpp"
#include "who.hpp"

class SessionEnumerator {
public:
    static std::wstring FormatFileTime(const FILETIME& ft);

    static std::wstring GetBootTime();

    static std::wstring GetCurrentUserName();

    static std::vector<UserSession> GetLoggedOnUsers();
};
