#include "engine.hpp"

DWORD ProcessorCountResolver::calculateCount(const NprocOptions& opts) {
    DWORD logicalCount = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    if (logicalCount == 0) logicalCount = 1;

    DWORD result = logicalCount;
    if (opts.ignore >= logicalCount) {
        result = 1;
    } else {
        result = logicalCount - static_cast<DWORD>(opts.ignore);
    }

    if (opts.includeAll) {
        SYSTEM_INFO si = {};
        GetSystemInfo(&si);
        DWORD allCount = si.dwNumberOfProcessors ? si.dwNumberOfProcessors : logicalCount;
        if (opts.ignore >= allCount) {
            result = 1;
        } else {
            result = allCount - static_cast<DWORD>(opts.ignore);
        }
    }

    return result;
}

NprocEngine::NprocEngine(NprocOptions opts) : options(std::move(opts)) {}

int NprocEngine::execute() {
    DWORD count = ProcessorCountResolver::calculateCount(options);
    std::cout << count << "\n";
    return 0;
}
