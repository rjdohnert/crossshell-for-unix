#include "system_metrics_sampler.hpp"

unsigned long long SystemMetricsSampler::fileTimeToULL(const FILETIME& ft) {
        return (unsigned long long)ft.dwLowDateTime | ((unsigned long long)ft.dwHighDateTime << 32);
    }

double SystemMetricsSampler::getCpuUtilization() {
        FILETIME idleTime1, kernelTime1, userTime1;
        FILETIME idleTime2, kernelTime2, userTime2;

        if (!GetSystemTimes(&idleTime1, &kernelTime1, &userTime1)) return 0.0;
        Sleep(100);
        if (!GetSystemTimes(&idleTime2, &kernelTime2, &userTime2)) return 0.0;

        unsigned long long idle1 = fileTimeToULL(idleTime1);
        unsigned long long kernel1 = fileTimeToULL(kernelTime1);
        unsigned long long user1 = fileTimeToULL(userTime1);

        unsigned long long idle2 = fileTimeToULL(idleTime2);
        unsigned long long kernel2 = fileTimeToULL(kernelTime2);
        unsigned long long user2 = fileTimeToULL(userTime2);

        unsigned long long idleDiff = idle2 - idle1;
        unsigned long long kernelDiff = kernel2 - kernel1;
        unsigned long long userDiff = user2 - user1;

        unsigned long long totalSys = kernelDiff + userDiff;
        if (totalSys == 0) return 0.0;

        unsigned long long activeSys = totalSys - idleDiff;
        return static_cast<double>(activeSys) / totalSys;
    }

int SystemMetricsSampler::getActiveUserCount() {
        PWTS_SESSION_INFOW pSessionInfo = NULL;
        DWORD dwCount = 0;
        int activeUsers = 0;

        if (WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessionInfo, &dwCount)) {
            for (DWORD i = 0; i < dwCount; ++i) {
                if (pSessionInfo[i].SessionId != 0 && pSessionInfo[i].State == WTSActive) {
                    activeUsers++;
                }
            }
            WTSFreeMemory(pSessionInfo);
        }
        return activeUsers > 0 ? activeUsers : 1;
    }
