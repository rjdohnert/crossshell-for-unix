#include "name_filter.hpp"
#include "process_info.hpp"

NameFilter::NameFilter(std::wstring name) : m_name(std::move(name)) {
        std::transform(m_name.begin(), m_name.end(), m_name.begin(), ::towlower);
    }

[[nodiscard]] bool NameFilter::matches(const ProcessInfo& proc) const  {
        std::wstring current = proc.name;
        std::transform(current.begin(), current.end(), current.begin(), ::towlower);
        return current == m_name;
    }
