#include "common.h"

volatile bool g_Interrupted = false;

const wchar_t* LongPath(const wchar_t* path) {
    static wchar_t buf[PATH_BUFFER_CAPACITY];
    size_t len = wcslen(path);
    if (len < MAX_PATH || wcsncmp(path, L"\\\\?\\", 4) == 0) {
        return path;
    }
    static wchar_t full[PATH_BUFFER_CAPACITY];
    DWORD n = GetFullPathNameW(path, PATH_BUFFER_CAPACITY, full, NULL);
    if (n == 0 || n >= PATH_BUFFER_CAPACITY) {
        return path;
    }
    if (wcsncmp(full, L"\\\\", 2) == 0) {
        swprintf(buf, PATH_BUFFER_CAPACITY, L"\\\\?\\UNC\\%ls", full + 2);
    } else {
        swprintf(buf, PATH_BUFFER_CAPACITY, L"\\\\?\\%ls", full);
    }
    return buf;
}
