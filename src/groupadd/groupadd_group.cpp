#define WIN32_LEAN_AND_MEAN
#include "groupadd_group.hpp"

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
}

GroupAddResult CreateLocalGroup(const std::wstring& name,
                                const std::wstring& comment) {
    if (name.empty() || name.find(L'\0') != std::wstring::npos ||
        comment.find(L'\0') != std::wstring::npos) {
        return {ERROR_INVALID_PARAMETER, 0, false, false};
    }
    for (wchar_t character : name) {
        if (character < 0x20 || (character >= 0x7f && character <= 0x9f)) {
            return {ERROR_INVALID_NAME, 0, false, false};
        }
    }

    LOCALGROUP_INFO_1 info{};
    info.lgrpi1_name = const_cast<LPWSTR>(name.c_str());
    info.lgrpi1_comment = comment.empty()
        ? nullptr
        : const_cast<LPWSTR>(comment.c_str());
    DWORD parameterError = 0;
    const NET_API_STATUS status = NetLocalGroupAdd(
        nullptr, 1, reinterpret_cast<LPBYTE>(&info), &parameterError);
    if (status == NERR_Success) {
        return {ERROR_SUCCESS, 0, true, false};
    }
    return {status, parameterError, false, status == NERR_GroupExists};
}

std::wstring GroupAddErrorMessage(const GroupAddResult& result) {
    if (result.alreadyExists) return L"the local group already exists";
    if (result.status == ERROR_ACCESS_DENIED) {
        return L"access denied; run from an elevated terminal";
    }
    if (result.status == ERROR_INVALID_NAME ||
        result.status == ERROR_INVALID_PARAMETER) {
        return L"the group name or comment is invalid";
    }

    std::wstring netMessagesPath(32768, L'\0');
    const UINT systemDirectoryLength = GetSystemDirectoryW(
        netMessagesPath.data(), static_cast<UINT>(netMessagesPath.size()));
    if (systemDirectoryLength == 0 ||
        systemDirectoryLength >= netMessagesPath.size()) {
        netMessagesPath.clear();
    } else {
        netMessagesPath.resize(systemDirectoryLength);
        netMessagesPath += L"\\netmsg.dll";
    }

    HMODULE netMessages = netMessagesPath.empty()
        ? nullptr
        : LoadLibraryW(netMessagesPath.c_str());
    if (netMessages != nullptr) {
        std::wstring message = FormatMessageForModule(
            result.status, FORMAT_MESSAGE_FROM_HMODULE, netMessages);
        FreeLibrary(netMessages);
        if (!message.empty()) return message;
    }

    std::wstring message = FormatMessageForModule(
        result.status, FORMAT_MESSAGE_FROM_SYSTEM, nullptr);
    if (!message.empty()) return message;
    return L"Windows account-management error " +
           std::to_wstring(result.status);
}
