#pragma once

#include "process_filter.hpp"
#include "process_info.hpp"
#include "renice.hpp"

class UserFilter : public IProcessFilter {
public:
    explicit UserFilter(std::wstring user);
    [[nodiscard]] bool matches(const ProcessInfo& proc) const override;
private:
    std::wstring m_user;
};
