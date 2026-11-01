#include "common.h"

const wchar_t* LongPath(const wchar_t* path) {
    static wchar_t buf[BDF_PATH_CAP];
    size_t len = wcslen(path);
    if (len < MAX_PATH || wcsncmp(path, L"\\\\?\\", 4) == 0) {
        return path;
    }
    static wchar_t full[BDF_PATH_CAP];
    DWORD n = GetFullPathNameW(path, BDF_PATH_CAP, full, NULL);
    if (n == 0 || n >= BDF_PATH_CAP) {
        return path;
    }
    if (wcsncmp(full, L"\\\\", 2) == 0) {
        swprintf(buf, BDF_PATH_CAP, L"\\\\?\\UNC\\%ls", full + 2);
    } else {
        swprintf(buf, BDF_PATH_CAP, L"\\\\?\\%ls", full);
    }
    return buf;
}

char* SafeStrDup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* copy = (char*)malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy;
}

wchar_t* Utf8ToUtf16(const char* utf8) {
    if (!utf8) return NULL;
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (len <= 0) return NULL;
    wchar_t* wstr = (wchar_t*)malloc(len * sizeof(wchar_t));
    if (!wstr) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wstr, len);
    return wstr;
}

char* Utf16ToUtf8(const wchar_t* utf16) {
    if (!utf16) return NULL;
    int len = WideCharToMultiByte(CP_UTF8, 0, utf16, -1, NULL, 0, NULL, NULL);
    if (len <= 0) return NULL;
    char* str = (char*)malloc(len);
    if (!str) return NULL;
    WideCharToMultiByte(CP_UTF8, 0, utf16, -1, str, len, NULL, NULL);
    return str;
}

size_t CountDigits(uint64_t n) {
    size_t count = 1;
    while (n >= 10) {
        n /= 10;
        count++;
    }
    return count;
}
