#ifndef PRINCIPAL_RESOLVER_HPP
#define PRINCIPAL_RESOLVER_HPP

#include "setfacl.hpp"

class PrincipalResolver {
public:
    static std::wstring ToLower(std::wstring value);
    static DWORD RightsToMask(const std::wstring& rights);
    static std::wstring SidToName(PSID sid);
    static PSID NameToSid(const std::wstring& name, std::vector<BYTE>& sidBuffer, std::vector<wchar_t>& domainBuffer);
    static PSID AllocateEveryoneSid();
};

#endif // PRINCIPAL_RESOLVER_HPP
