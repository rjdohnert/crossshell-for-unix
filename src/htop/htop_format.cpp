#include "htop_format.hpp"

#include <cwchar>

std::wstring FormatBytes(SIZE_T bytes) {
    const double value = static_cast<double>(bytes);
    wchar_t buffer[32];
    if (value >= 1024.0 * 1024.0 * 1024.0) {
        swprintf_s(buffer, L"%.2fG", value / (1024.0 * 1024.0 * 1024.0));
    } else if (value >= 1024.0 * 1024.0) {
        swprintf_s(buffer, L"%.1fM", value / (1024.0 * 1024.0));
    } else if (value >= 1024.0) {
        swprintf_s(buffer, L"%.0fK", value / 1024.0);
    } else {
        swprintf_s(buffer, L"%zuB", static_cast<size_t>(bytes));
    }
    return buffer;
}

std::wstring FormatCpuTime(ULONGLONG time100ns) {
    const ULONGLONG totalSeconds = time100ns / 10000000ULL;
    const ULONGLONG hours = totalSeconds / 3600;
    const ULONGLONG minutes = (totalSeconds % 3600) / 60;
    const ULONGLONG seconds = totalSeconds % 60;
    const ULONGLONG hundredths = (time100ns / 100000ULL) % 100;

    wchar_t buffer[32];
    if (hours > 0) {
        swprintf_s(buffer, L"%lluh%02llu:%02llu", hours, minutes, seconds);
    } else {
        swprintf_s(buffer, L"%02llu:%02llu.%02llu", minutes, seconds, hundredths);
    }
    return buffer;
}

std::wstring FormatUptime(ULONGLONG seconds) {
    const ULONGLONG days = seconds / 86400;
    const ULONGLONG hours = (seconds % 86400) / 3600;
    const ULONGLONG minutes = (seconds % 3600) / 60;
    const ULONGLONG remainingSeconds = seconds % 60;

    wchar_t buffer[64];
    if (days > 0) {
        swprintf_s(buffer, L"%llud %02llu:%02llu:%02llu",
                   days, hours, minutes, remainingSeconds);
    } else {
        swprintf_s(buffer, L"%02llu:%02llu:%02llu",
                   hours, minutes, remainingSeconds);
    }
    return buffer;
}
