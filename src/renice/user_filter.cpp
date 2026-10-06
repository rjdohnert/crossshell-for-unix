#include "process_info.hpp"
#include "user_filter.hpp"

UserFilter::UserFilter(std::wstring user) : m_user(std::move(user)) {
        std::transform(m_user.begin(), m_user.end(), m_user.begin(), ::towlower);
    }

[[nodiscard]] bool UserFilter::matches(const ProcessInfo& proc) const  {
        std::wstring current = proc.owner;
        std::transform(current.begin(), current.end(), current.begin(), ::towlower);
        return current == m_user;
    }
