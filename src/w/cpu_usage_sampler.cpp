#include "cpu_usage_sampler.hpp"

double GetCpuUsagePercentage() {
    FILETIME ftIdle1, ftKernel1, ftUser1;
    FILETIME ftIdle2, ftKernel2, ftUser2;

    if (!GetSystemTimes(&ftIdle1, &ftKernel1, &ftUser1)) return 0.0;
    Sleep(100);
    if (!GetSystemTimes(&ftIdle2, &ftKernel2, &ftUser2)) return 0.0;

    ULONGLONG idle1 = ((ULONGLONG)ftIdle1.dwHighDateTime << 32) | ftIdle1.dwLowDateTime;
    ULONGLONG idle2 = ((ULONGLONG)ftIdle2.dwHighDateTime << 32) | ftIdle2.dwLowDateTime;

    ULONGLONG kernel1 = ((ULONGLONG)ftKernel1.dwHighDateTime << 32) | ftKernel1.dwLowDateTime;
    ULONGLONG kernel2 = ((ULONGLONG)ftKernel2.dwHighDateTime << 32) | ftKernel2.dwLowDateTime;

    ULONGLONG user1 = ((ULONGLONG)ftUser1.dwHighDateTime << 32) | ftUser1.dwLowDateTime;
    ULONGLONG user2 = ((ULONGLONG)ftUser2.dwHighDateTime << 32) | ftUser2.dwLowDateTime;

    ULONGLONG usrDiff = user2 - user1;
    ULONGLONG kerDiff = kernel2 - kernel1;
    ULONGLONG idlDiff = idle2 - idle1;

    ULONGLONG sysTotal = usrDiff + kerDiff;
    if (sysTotal == 0 || sysTotal <= idlDiff) return 0.0;

    ULONGLONG busyTotal = sysTotal - idlDiff;
    return (busyTotal * 100.0) / sysTotal;
}
