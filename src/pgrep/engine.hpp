#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "pgrep.hpp"
#include "options.hpp"

class ProcessMatcher {
public:
    static std::wstring ToLowerCopy(const std::wstring& input);
    static bool IsMatch(const std::wstring& name, const PgrepOptions& options);
};

#endif // ENGINE_HPP
