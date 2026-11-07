#define WIN32_LEAN_AND_MEAN
#include "groupdel_group.hpp"

#include <windows.h>
#include <lm.h>

namespace {
std::wstring FormatMessageForModule(DWORD code, DWORD flags, HMODULE module) {
    wchar_t* buffer = nullptr;
    const DWORD length = FormatMessageW(
        flags | FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        module, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    std::wstring message = length != 0 && buffer != nullptr
        ? std::wstring(buffer, length)
        : std::wstring();
    if (buffer != nullptr) LocalFree(buffer);
    while (!message.empty() &&
           (message.back() == L'\r' || message.back() == L'\n' ||
            message.back() == L' ')) {
        message.pop_back();
    }
    return message;
}

bool ContainsControlCharacter(const std::wstring& value) {
    for (wchar_t character : value) {
        if (character < 0x20 || (character >= 0x7f && character <= 0x9f)) {
            return true;
        }
    }
    return false;
}
}

GroupDelResult DeleteLocalGroup(const std::wstring& name) {
    if (name.empty() || name.find(L'\0') != std::wstring::npos ||
        ContainsControlCharacter(name)) {
        return {ERROR_INVALID_NAME, false, false};
    }

    const NET_API_STATUS status = NetLocalGroupDel(nullptr, name.c_str());
    if (status == NERR_Success) return {NERR_Success, true, false};
    if (status == NERR_GroupNotFound || status == ERROR_NO_SUCH_ALIAS) {
        return {status, false, true};
    }
    return {status, false, false};
}

std::wstring GroupDelErrorMessage(unsigned long status) {
    if (status == ERROR_ACCESS_DENIED) {
        return L"access denied; run from an elevated terminal";
    }
    if (status == ERROR_INVALID_NAME ||
        status == ERROR_INVALID_PARAMETER) {
        return L"the group name is invalid";
    }
    if (status == NERR_GroupNotFound || status == ERROR_NO_SUCH_ALIAS) {
        return L"the local group does not exist";
    }
    if (status == NERR_SpeGroupOp) {
        return L"Windows protects this built-in or special local group";
    }

    std::wstring netMessagesPath(32768, L'\0');
    const UINT systemDirectoryLength = GetSystemDirectoryW(
        netMessagesPath.data(), static_cast<UINT>(netMessagesPath.size()));
    if (systemDirectoryLength != 0 &&
        systemDirectoryLength < netMessagesPath.size()) {
        netMessagesPath.resize(systemDirectoryLength);
        netMessagesPath += L"\\netmsg.dll";
        HMODULE netMessages = LoadLibraryW(netMessagesPath.c_str());
        if (netMessages != nullptr) {
            std::wstring message = FormatMessageForModule(
                static_cast<DWORD>(status), FORMAT_MESSAGE_FROM_HMODULE,
                netMessages);
            FreeLibrary(netMessages);
            if (!message.empty()) return message;
        }
    }

    const std::wstring systemMessage = FormatMessageForModule(
        static_cast<DWORD>(status), FORMAT_MESSAGE_FROM_SYSTEM, nullptr);
    if (!systemMessage.empty()) return systemMessage;
    return L"Windows account-management error " + std::to_wstring(status);
}
