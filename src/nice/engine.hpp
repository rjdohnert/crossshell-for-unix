#ifndef NICE_ENGINE_HPP
#define NICE_ENGINE_HPP

#include "nice.hpp"
#include "options.hpp"

class PriorityMapper {
public:
    static PriorityInfo GetCurrentNiceLevel();
    static DWORD MapNiceToPriorityClass(int niceVal);
};

class ProcessPriorityEngine {
public:
    static int ExecuteWithPriority(int argc, wchar_t* argv[], const NiceOptions& options);
};

#endif // NICE_ENGINE_HPP
