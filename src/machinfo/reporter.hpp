#ifndef REPORTER_HPP
#define REPORTER_HPP

#include "machinfo.hpp"
#include "options.hpp"
#include <string>

class MachinfoReporter {
public:
    static std::wstring formatMemorySize(ULONGLONG bytes);
    static std::wstring jsonEscape(const std::wstring& value);
    static void report(const CpuTopology& cpu, const SystemFirmwareInfo& fw, const MemoryStatusInfo& mem,
                       const TpmInfo& tpm, const std::wstring& osVersion, const MachinfoOptions& opts);
};

#endif // REPORTER_HPP
