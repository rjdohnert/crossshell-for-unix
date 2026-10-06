#include "pid_filter.hpp"
#include "process_info.hpp"

PidFilter::PidFilter(DWORD pid) : m_pid(pid) {}

[[nodiscard]] bool PidFilter::matches(const ProcessInfo& proc) const  {
        return proc.pid == m_pid;
    }
