#ifndef NVRAM_CONTROLLER_HPP
#define NVRAM_CONTROLLER_HPP

#include "setboot.hpp"

class TokenPrivilegeGuard {
public:
    static bool enablePrivilege(const wchar_t* privName);
};

class UefiNvramController {
private:
    static const wchar_t* EFI_GLOBAL_GUID;

public:
    static BootEntry readBootOption(WORD id);
    static bool readWord(const wchar_t* varName, WORD& outVal);
    static std::vector<WORD> readBootOrder(bool& readOk, DWORD& readError);
    static BootEnvironment queryEnvironment();
    static bool writeBootOrder(const std::vector<WORD>& order);
    static bool writeBootNext(WORD bootId);
    static bool writeTimeout(WORD seconds);
    static bool parseBootId(const std::wstring& input, WORD& outId);
};

#endif // NVRAM_CONTROLLER_HPP
