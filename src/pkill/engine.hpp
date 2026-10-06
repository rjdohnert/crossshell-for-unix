#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "pkill.hpp"
#include "options.hpp"

class ProcessMatcherEngine {
public:
    static std::wstring ToLowerCopy(const std::wstring& input);
    static bool CollectProcesses(std::vector<ProcessInfo>& out);
    static bool IsMatch(const std::wstring& name, const PkillOptions& options);
    static int TerminateMatchingProcesses(const PkillOptions& options);
};

#endif // ENGINE_HPP
