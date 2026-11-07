#include "fdisk_error.h"

#include <wchar.h>

void FdiskErrorClear(FdiskError* error) {
    if (error == NULL) return;
    error->code = ERROR_SUCCESS;
    error->message[0] = L'\0';
}

void FdiskErrorSet(FdiskError* error, DWORD code, const wchar_t* message) {
    if (error == NULL) return;
    error->code = code;
    if (message == NULL) {
        error->message[0] = L'\0';
        return;
    }
    wcsncpy_s(error->message, FDISK_ERROR_CAPACITY, message, _TRUNCATE);
}

void FdiskErrorSetWindows(FdiskError* error, const wchar_t* operation, DWORD code) {
    if (error == NULL) return;
    wchar_t system_message[256] = {0};
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, code, 0, system_message, (DWORD)(sizeof(system_message) / sizeof(system_message[0])), NULL);
    if (length == 0) {
        (void)swprintf_s(error->message, FDISK_ERROR_CAPACITY,
            L"%ls failed (Windows error %lu)", operation, (unsigned long)code);
        error->code = code;
        return;
    }

    while (system_message[0] != L'\0') {
        const size_t last = wcslen(system_message);
        if (last == 0 || (system_message[last - 1] != L'\r' && system_message[last - 1] != L'\n')) break;
        system_message[last - 1] = L'\0';
    }
    (void)swprintf_s(error->message, FDISK_ERROR_CAPACITY,
        L"%ls: %ls (Windows error %lu)", operation, system_message, (unsigned long)code);
    error->code = code;
}
