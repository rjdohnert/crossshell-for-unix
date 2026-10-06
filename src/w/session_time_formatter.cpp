#include "session_time_formatter.hpp"
#include "string_encoding.hpp"

std::string FormatLoginTime(ULONGLONG timestamp) {
    if (timestamp == 0) return " - ";
    FILETIME ft;
    ft.dwLowDateTime = (DWORD)(timestamp & 0xFFFFFFFF);
    ft.dwHighDateTime = (DWORD)(timestamp >> 32);

    SYSTEMTIME stUTC, stLocal;
    FileTimeToSystemTime(&ft, &stUTC);
    SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);

    SYSTEMTIME stCurrent;
    GetLocalTime(&stCurrent);

    char buf[32];
    if (stLocal.wYear == stCurrent.wYear &&
        stLocal.wMonth == stCurrent.wMonth &&
        stLocal.wDay == stCurrent.wDay) {
        int hour = stLocal.wHour % 12;
        if (hour == 0) hour = 12;
        snprintf(buf, sizeof(buf), "%02d:%02d%s",
            hour, stLocal.wMinute, (stLocal.wHour >= 12) ? "pm" : "am");
    } else {
        static const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                       "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
        snprintf(buf, sizeof(buf), "%s%02d", months[(stLocal.wMonth - 1) % 12], stLocal.wDay);
    }
    return buf;
}

std::string FormatIdleTime(ULONGLONG idleMs) {
    ULONGLONG totalSec = idleMs / 1000;
    if (totalSec < 60) {
        return std::to_string(totalSec) + "s";
    }
    ULONGLONG totalMin = totalSec / 60;
    if (totalMin < 60) {
        return std::to_string(totalMin) + "m";
    }
    ULONGLONG hours = totalMin / 60;
    ULONGLONG mins = totalMin % 60;
    if (hours < 24) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%llu:%02llu", hours, mins);
        return buf;
    }
    ULONGLONG days = hours / 24;
    return std::to_string(days) + "days";
}

std::string FormatCpuTime(ULONGLONG cpu100ns) {
    double totalSec = (double)cpu100ns / 10000000.0;
    if (totalSec < 60.0) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2fs", totalSec);
        return buf;
    }
    ULONGLONG totalMin = (ULONGLONG)(totalSec / 60.0);
    double remSec = totalSec - (totalMin * 60.0);
    char buf[16];
    snprintf(buf, sizeof(buf), "%llu:%02.0fm", totalMin, remSec);
    return buf;
}
