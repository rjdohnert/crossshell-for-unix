#include "strutil.h"

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

bool FormatIntegerBase(int64_t val, int base, char* out, size_t out_len) {
    static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    if (base < 2 || base > 36 || out_len < 128) return false;

    if (base == 10) {
        snprintf(out, out_len, "%lld", (long long)val);
        return true;
    }

    char buf[128];
    size_t idx = sizeof(buf) - 1;
    buf[idx] = '\0';

    uint64_t uval = (val < 0) ? (uint64_t)(-val) : (uint64_t)val;
    if (uval == 0) {
        buf[--idx] = '0';
    } else {
        while (uval > 0 && idx > 0) {
            buf[--idx] = digits[uval % (unsigned int)base];
            uval /= (unsigned int)base;
        }
    }

    if (val < 0) {
        snprintf(out, out_len, "-%d#%s", base, &buf[idx]);
    } else {
        snprintf(out, out_len, "%d#%s", base, &buf[idx]);
    }
    return true;
}

bool ParseIntegerBase(const char* str, int64_t* out_val) {
    if (!str || !*str) return false;

    char* endptr = NULL;
    errno = 0;

    const char* hash = strchr(str, '#');
    if (hash) {
        long base = strtol(str, &endptr, 10);
        if (errno != 0 || endptr != hash || base < 2 || base > 36) return false;

        const char* num_part = hash + 1;
        uint64_t res = 0;
        bool neg = (*num_part == '-');
        if (neg) num_part++;

        while (*num_part) {
            char c = (char)tolower((unsigned char)*num_part);
            int digit = -1;
            if (c >= '0' && c <= '9') digit = c - '0';
            else if (c >= 'a' && c <= 'z') digit = c - 'a' + 10;
            if (digit < 0 || digit >= base) return false;

            res = res * (uint64_t)base + (uint64_t)digit;
            num_part++;
        }
        *out_val = neg ? -(int64_t)res : (int64_t)res;
        return true;
    }

    long long val = strtoll(str, &endptr, 0);
    if (errno != 0 || endptr == str) return false;
    *out_val = (int64_t)val;
    return true;
}
