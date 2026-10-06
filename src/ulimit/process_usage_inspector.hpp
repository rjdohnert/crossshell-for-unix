#pragma once

#include "process_snapshot.hpp"
#include "ulimit.hpp"

class ProcessUsageInspector {
public:
    static bool SnapshotProcess(HANDLE process_handle, ProcessSnapshot& snapshot);
};
