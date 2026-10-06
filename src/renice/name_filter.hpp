#pragma once

#include "process_filter.hpp"
#include "process_info.hpp"
#include "renice.hpp"

class NameFilter : public IProcessFilter {
public:
    explicit NameFilter(std::wstring name);
    [[nodiscard]] bool matches(const ProcessInfo& proc) const override;
private:
    std::wstring m_name;
};
