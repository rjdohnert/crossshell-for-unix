#pragma once

#include "session_data.hpp"
#include "w.hpp"

std::vector<SessionData> collectUserSessions(PWTS_SESSION_INFOW pSessions, DWORD sessionCount, const std::wstring& targetUser);
