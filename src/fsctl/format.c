#include "format.h"

// --- Formatting Helpers ---
void format_size(unsigned long long bytes, wchar_t *out, size_t out_sz) {
    const wchar_t *units[] = { L"B", L"KB", L"MB", L"GB", L"TB" };
    int unit_idx = 0;
    double d_size = (double)bytes;

    while (d_size >= 1024.0 && unit_idx < 4) {
        d_size /= 1024.0;
        unit_idx++;
    }
    if (unit_idx == 0) {
        swprintf(out, out_sz, L"%llu B", bytes);
    } else {
        swprintf(out, out_sz, L"%.1f %ls", d_size, units[unit_idx]);
    }
}

void format_filetime(FILETIME ft, wchar_t *out, size_t out_sz) {
    SYSTEMTIME st_utc, st_local;
    FileTimeToSystemTime(&ft, &st_utc);
    SystemTimeToTzSpecificLocalTime(NULL, &st_utc, &st_local);
    swprintf(out, out_sz, L"%04d-%02d-%02d %02d:%02d",
             st_local.wYear, st_local.wMonth, st_local.wDay,
             st_local.wHour, st_local.wMinute);
}
