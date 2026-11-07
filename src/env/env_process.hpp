#ifndef ENV_PROCESS_HPP
#define ENV_PROCESS_HPP

#include "env_environment.hpp"

#include <string>
#include <vector>

class ProcessRunner {
public:
    static int Execute(
        const std::vector<std::wstring>& commandAndArgs,
        const EnvironmentManager::Map& environment,
        const std::vector<wchar_t>& environmentBlock,
        const std::wstring& workingDirectory,
        bool verbose);
};

#endif
