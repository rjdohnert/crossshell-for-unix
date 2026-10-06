#include "cpu_usage_sampler.hpp"
#include "summary_header.hpp"

void PrintHeader(size_t activeUsers) {
    SYSTEMTIME st;
    GetLocalTime(&st);

    int hour = st.wHour % 12;
    if (hour == 0) hour = 12;
    char timeBuf[16];
    snprintf(timeBuf, sizeof(timeBuf), "%2d:%02d%s",
        hour, st.wMinute, (st.wHour >= 12) ? "pm" : "am");

    ULONGLONG uptimeMs = GetTickCount64();
    ULONGLONG totalSec = uptimeMs / 1000;
    ULONGLONG days = totalSec / 86400;
    ULONGLONG hours = (totalSec % 86400) / 3600;
    ULONGLONG mins = (totalSec % 3600) / 60;

    double cpuUsage = GetCpuUsagePercentage();

    std::cout << "  " << timeBuf << "  up ";
    if (days > 0) {
        std::cout << days << " day(s), ";
    }
    std::cout << hours << ":" << std::setw(2) << std::setfill('0') << mins << std::setfill(' ') << ",  "
              << activeUsers << " user" << (activeUsers == 1 ? "" : "s") << ",  "
              << "cpu usage: " << std::fixed << std::setprecision(1) << cpuUsage << "%\n";
}
