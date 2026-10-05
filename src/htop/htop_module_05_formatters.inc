// ============================================================================
// FORMATTING HELPERS
// ============================================================================
static std::wstring FormatBytes(SIZE_T bytes) {
    double db = static_cast<double>(bytes);
    wchar_t buf[32];
    if (db >= 1024.0 * 1024.0 * 1024.0) {
        swprintf_s(buf, L"%.2fG", db / (1024.0 * 1024.0 * 1024.0));
    } else if (db >= 1024.0 * 1024.0) {
        swprintf_s(buf, L"%.1fM", db / (1024.0 * 1024.0));
    } else if (db >= 1024.0) {
        swprintf_s(buf, L"%.0fK", db / 1024.0);
    } else {
        swprintf_s(buf, L"%zudB", bytes);
    }
    return buf;
}

static std::wstring FormatCpuTime(ULONGLONG time100ns) {
    ULONGLONG totalSeconds = time100ns / 10000000ULL;
    ULONGLONG hours = totalSeconds / 3600;
    ULONGLONG minutes = (totalSeconds % 3600) / 60;
    ULONGLONG seconds = totalSeconds % 60;
    ULONGLONG hundredths = (time100ns / 100000ULL) % 100;

    wchar_t buf[32];
    if (hours > 0) {
        swprintf_s(buf, L"%lluh%02llu:%02llu", hours, minutes, seconds);
    } else {
        swprintf_s(buf, L"%02llu:%02llu.%02llu", minutes, seconds, hundredths);
    }
    return buf;
}

static std::wstring FormatUptime(ULONGLONG seconds) {
    ULONGLONG days = seconds / 86400;
    ULONGLONG hours = (seconds % 86400) / 3600;
    ULONGLONG minutes = (seconds % 3600) / 60;
    ULONGLONG secs = seconds % 60;

    wchar_t buf[64];
    if (days > 0) {
        swprintf_s(buf, L"%llud %02llu:%02llu:%02llu", days, hours, minutes, secs);
    } else {
        swprintf_s(buf, L"%02llu:%02llu:%02llu", hours, minutes, secs);
    }
    return buf;
}

