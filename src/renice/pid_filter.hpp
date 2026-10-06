#pragma once

#include "process_filter.hpp"
#include "process_info.hpp"
#include "renice.hpp"

class PidFilter : public IProcessFilter {
public:
    explicit PidFilter(DWORD pid);
    [[nodiscard]] bool matches(const ProcessInfo& proc) const override;
private:
    DWORD m_pid;
};
