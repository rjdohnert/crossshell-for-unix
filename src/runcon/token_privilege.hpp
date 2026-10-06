#ifndef TOKEN_PRIVILEGE_HPP
#define TOKEN_PRIVILEGE_HPP

#include "runcon.hpp"

class TokenPrivilegeGuard {
public:
    static bool enablePrivilege(LPCWSTR lpszPrivilege);
};

class IntegrityLevelMapper {
public:
    static std::wstring mapLevelToSid(const std::wstring& levelStr);
};

#endif // TOKEN_PRIVILEGE_HPP
