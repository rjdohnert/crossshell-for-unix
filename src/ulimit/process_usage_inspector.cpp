#include "process_snapshot.hpp"
#include "process_usage_inspector.hpp"

bool ProcessUsageInspector::SnapshotProcess(HANDLE process_handle, ProcessSnapshot& snapshot) {
        PROCESS_MEMORY_COUNTERS_EX counters = {};
        counters.cb = sizeof(counters);
        if (!GetProcessMemoryInfo(process_handle, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters))) {
            return false;
        }

        FILETIME creation_time = {}, exit_time = {}, kernel_time = {}, user_time = {};
        if (!GetProcessTimes(process_handle, &creation_time, &exit_time, &kernel_time, &user_time)) {
            return false;
        }

        DWORD handle_cnt = 0;
        GetProcessHandleCount(process_handle, &handle_cnt);

        snapshot.working_set = counters.WorkingSetSize;
        snapshot.peak_working_set = counters.PeakWorkingSetSize;
        snapshot.private_usage = static_cast<SIZE_T>(counters.PrivateUsage);
        snapshot.kernel_time_100ns = (static_cast<ULONGLONG>(kernel_time.dwHighDateTime) << 32) | kernel_time.dwLowDateTime;
        snapshot.user_time_100ns = (static_cast<ULONGLONG>(user_time.dwHighDateTime) << 32) | user_time.dwLowDateTime;
        snapshot.handle_count = handle_cnt;
        return true;
    }
