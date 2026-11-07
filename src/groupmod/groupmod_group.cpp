#define WIN32_LEAN_AND_MEAN
#include "groupmod_group.hpp"

#include <windows.h>
#include <lm.h>

namespace {
bool ContainsControlCharacter(const std::wstring& value) {
    for (wchar_t character : value) {
        if (character < 0x20 || (character >= 0x7f && character <= 0x9f)) {
            return true;
        }
    }
    return false;
}

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
}

GroupModResult ModifyLocalGroup(
    const std::wstring& groupName,
    const std::optional<std::wstring>& requestedName,
    const std::optional<std::wstring>& comment) {
    if (groupName.empty() || groupName.find(L'\0') != std::wstring::npos ||
        ContainsControlCharacter(groupName)) {
        return {ERROR_INVALID_NAME, false, false, false};
    }
    if (requestedName &&
        (requestedName->empty() ||
         requestedName->find(L'\0') != std::wstring::npos ||
         ContainsControlCharacter(*requestedName))) {
        return {ERROR_INVALID_NAME, false, false, false};
    }
    if (comment && comment->find(L'\0') != std::wstring::npos) {
        return {ERROR_INVALID_PARAMETER, false, false, false};
    }

    std::optional<std::wstring> newName = requestedName;
    if (newName && *newName == groupName) newName.reset();
    if (!newName && !comment) return {NERR_Success, true, false, false};

    NET_API_STATUS status = NERR_Success;
    DWORD parameterError = 0;
    if (newName && comment) {
        std::wstring mutableName = *newName;
        std::wstring mutableComment = *comment;
        LOCALGROUP_INFO_1 info{};
        info.lgrpi1_name = mutableName.data();
        info.lgrpi1_comment = mutableComment.data();
        status = NetLocalGroupSetInfo(
            nullptr, groupName.c_str(), 1,
            reinterpret_cast<LPBYTE>(&info), &parameterError);
    } else if (newName) {
        std::wstring mutableName = *newName;
        LOCALGROUP_INFO_0 info{};
        info.lgrpi0_name = mutableName.data();
        status = NetLocalGroupSetInfo(
            nullptr, groupName.c_str(), 0,
            reinterpret_cast<LPBYTE>(&info), &parameterError);
    } else {
        std::wstring mutableComment = *comment;
        LOCALGROUP_INFO_1002 info{};
        info.lgrpi1002_comment = mutableComment.data();
        status = NetLocalGroupSetInfo(
            nullptr, groupName.c_str(), 1002,
            reinterpret_cast<LPBYTE>(&info), &parameterError);
    }

    if (status == NERR_Success) return {NERR_Success, true, true, false};
    if (status == NERR_GroupNotFound || status == ERROR_NO_SUCH_ALIAS) {
        return {status, false, false, true};
    }
    if (status == NERR_GroupExists) {
        return {status, false, false, false};
    }
    return {status, false, false, false};
}

std::wstring GroupModErrorMessage(unsigned long status) {
    if (status == ERROR_ACCESS_DENIED) {
        return L"access denied; run from an elevated terminal";
    }
    if (status == ERROR_INVALID_NAME ||
        status == ERROR_INVALID_PARAMETER) {
        return L"the group name or modification is invalid";
    }
    if (status == NERR_GroupNotFound || status == ERROR_NO_SUCH_ALIAS) {
        return L"the local group does not exist";
    }
    if (status == NERR_GroupExists) {
        return L"a local group with the requested new name already exists";
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
