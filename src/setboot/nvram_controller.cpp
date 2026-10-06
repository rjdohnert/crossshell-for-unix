#include "nvram_controller.hpp"
#include "setboot_options.hpp"

const wchar_t* UefiNvramController::EFI_GLOBAL_GUID = L"{8BE4DF61-93CA-11D2-AA0D-00E098032B8C}";

bool TokenPrivilegeGuard::enablePrivilege(const wchar_t* privName) {
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return false;
    }

    TOKEN_PRIVILEGES tp = { 0 };
    LUID luid;
    if (!LookupPrivilegeValueW(nullptr, privName, &luid)) {
        CloseHandle(hToken);
        return false;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    BOOL result = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), nullptr, nullptr);
    DWORD err = GetLastError();
    CloseHandle(hToken);

    return (result && err != ERROR_NOT_ALL_ASSIGNED);
}

BootEntry UefiNvramController::readBootOption(WORD id) {
    BootEntry entry;
    entry.id = id;

    wchar_t varName[16];
    swprintf_s(varName, 16, L"Boot%04X", id);
    entry.name = varName;

    BYTE buffer[2048] = { 0 };
    DWORD readBytes = GetFirmwareEnvironmentVariableW(
        varName,
        EFI_GLOBAL_GUID,
        buffer,
        sizeof(buffer)
    );

    if (readBytes < 6) {
        return entry;
    }

    entry.attributes = *reinterpret_cast<DWORD*>(buffer);
    const wchar_t* descPtr = reinterpret_cast<const wchar_t*>(buffer + 6);

    size_t maxDescChars = (readBytes - 6) / sizeof(wchar_t);
    size_t descLen = wcsnlen(descPtr, maxDescChars);
    entry.description = std::wstring(descPtr, descLen);
    entry.isValid = true;

    size_t descByteSize = (descLen + 1) * sizeof(wchar_t);
    size_t pathOffset = 6 + descByteSize;
    if (pathOffset < readBytes) {
        const BYTE* pathBytes = buffer + pathOffset;
        size_t pathSize = readBytes - pathOffset;

        std::wstring rawPath;
        for (size_t i = 0; i + 1 < pathSize; i += 2) {
            wchar_t ch = *reinterpret_cast<const wchar_t*>(pathBytes + i);
            if ((ch >= 32 && ch <= 126) || ch == L'\\' || ch == L'/') {
                rawPath += ch;
            }
        }
        entry.devicePath = rawPath.empty() ? L"EFI Device Path" : rawPath;
    } else {
        entry.devicePath = L"EFI Device Path";
    }

    return entry;
}

bool UefiNvramController::readWord(const wchar_t* varName, WORD& outVal) {
    WORD val = 0;
    DWORD bytes = GetFirmwareEnvironmentVariableW(
        varName,
        EFI_GLOBAL_GUID,
        &val,
        sizeof(WORD)
    );
    if (bytes == sizeof(WORD)) {
        outVal = val;
        return true;
    }
    return false;
}

std::vector<WORD> UefiNvramController::readBootOrder(bool& readOk, DWORD& readError) {
    std::vector<WORD> order;
    readOk = false;
    readError = ERROR_SUCCESS;

    BYTE buffer[512] = { 0 };
    DWORD bytes = GetFirmwareEnvironmentVariableW(
        L"BootOrder",
        EFI_GLOBAL_GUID,
        buffer,
        sizeof(buffer)
    );

    if (bytes == 0) {
        readError = GetLastError();
        return order;
    }

    readOk = true;
    size_t count = bytes / sizeof(WORD);
    const WORD* pOrder = reinterpret_cast<const WORD*>(buffer);
    for (size_t i = 0; i < count; ++i) {
        order.push_back(pOrder[i]);
    }
    return order;
}

BootEnvironment UefiNvramController::queryEnvironment() {
    BootEnvironment env;

    FIRMWARE_TYPE fwType;
    if (GetFirmwareType(&fwType)) {
        if (fwType == FirmwareTypeUefi) {
            env.firmwareType = L"UEFI (Unified Extensible Firmware Interface)";
            env.isUefi = true;
        } else if (fwType == FirmwareTypeBios) {
            env.firmwareType = L"Legacy BIOS";
            env.isUefi = false;
        }
    }

    if (!env.isUefi) return env;

    env.hasCurrentBoot = readWord(L"BootCurrent", env.currentBootId);
    env.hasNextBoot = readWord(L"BootNext", env.nextBootId);
    env.hasTimeout = readWord(L"Timeout", env.timeoutSeconds);
    env.bootOrder = readBootOrder(env.bootOrderReadOk, env.bootOrderReadError);

    for (WORD id : env.bootOrder) {
        BootEntry entry = readBootOption(id);
        if (entry.isValid) {
            env.bootEntries.push_back(entry);
        }
    }

    return env;
}

bool UefiNvramController::writeBootOrder(const std::vector<WORD>& order) {
    DWORD byteSize = static_cast<DWORD>(order.size() * sizeof(WORD));
    return SetFirmwareEnvironmentVariableW(
        L"BootOrder",
        EFI_GLOBAL_GUID,
        const_cast<WORD*>(order.data()),
        byteSize
    ) != 0;
}

bool UefiNvramController::writeBootNext(WORD bootId) {
    return SetFirmwareEnvironmentVariableW(
        L"BootNext",
        EFI_GLOBAL_GUID,
        &bootId,
        sizeof(WORD)
    ) != 0;
}

bool UefiNvramController::writeTimeout(WORD seconds) {
    return SetFirmwareEnvironmentVariableW(
        L"Timeout",
        EFI_GLOBAL_GUID,
        &seconds,
        sizeof(WORD)
    ) != 0;
}

bool UefiNvramController::parseBootId(const std::wstring& input, WORD& outId) {
    std::wstring str = SetbootOptions::toUpper(input);
    if (str.rfind(L"BOOT", 0) == 0) {
        str = str.substr(4);
    }
    try {
        outId = static_cast<WORD>(wcstoul(str.c_str(), nullptr, 16));
        return true;
    } catch (...) {
        return false;
    }
}
