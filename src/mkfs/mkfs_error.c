#include "mkfs_error.h"

#include <wchar.h>

void MkfsErrorClear(MkfsError* error) {
    if (error == NULL) return;
    error->code = ERROR_SUCCESS;
    error->message[0] = L'\0';
}

void MkfsErrorSet(MkfsError* error, DWORD code, const wchar_t* message) {
    if (error == NULL) return;
    error->code = code;
    if (message == NULL) {
        error->message[0] = L'\0';
        return;
    }
    (void)wcsncpy_s(error->message, _countof(error->message), message, _TRUNCATE);
}

void MkfsErrorSetWindows(MkfsError* error, const wchar_t* operation, DWORD code) {
    if (error == NULL) return;
    wchar_t system_message[256] = {0};
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, code, 0, system_message, (DWORD)_countof(system_message), NULL);
    if (length == 0) {
        (void)swprintf_s(error->message, _countof(error->message),
            L"%ls failed (Windows error %lu)", operation, (unsigned long)code);
        error->code = code;
        return;
    }

    size_t message_length = wcslen(system_message);
    while (message_length != 0 &&
        (system_message[message_length - 1] == L'\r' ||
         system_message[message_length - 1] == L'\n')) {
        system_message[--message_length] = L'\0';
    }
    (void)swprintf_s(error->message, _countof(error->message),
        L"%ls: %ls (Windows error %lu)", operation, system_message, (unsigned long)code);
    error->code = code;
}

const wchar_t* MkfsFileSystemName(MkfsFileSystem file_system) {
    switch (file_system) {
        case MKFS_FILESYSTEM_NTFS: return L"NTFS";
        case MKFS_FILESYSTEM_FAT32: return L"FAT32";
        case MKFS_FILESYSTEM_EXFAT: return L"exFAT";
        default: return L"unknown";
    }
}
