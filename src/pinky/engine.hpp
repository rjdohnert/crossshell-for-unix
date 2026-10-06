#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "pinky.hpp"

class FingerClient {
public:
    static bool QueryRemote(const std::string& user, const std::string& host);
};

class PinkyProfileService {
public:
    static std::wstring FormatTime(DWORD timeSecs);
    static std::string WideToUtf8(const std::wstring& value);
    static std::wstring GetUserFullName(const std::wstring& username);
    static std::vector<PinkySessionSummary> QueryActiveSessions();
    static PinkyUserProfile QueryUserProfile(const std::wstring& username, bool printPlan);
};

#endif // ENGINE_HPP
