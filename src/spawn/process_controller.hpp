#ifndef PROCESS_CONTROLLER_HPP
#define PROCESS_CONTROLLER_HPP

#include "spawn.hpp"
#include "spawn_options.hpp"
#include "vms_status.hpp"

class ProcessController {
public:
    static bool launch(const std::wstring& cmd,
                       const SpawnOptions& opts,
                       const VmsStatusReporter& reporter,
                       DWORD& outExitCode);
};

#endif // PROCESS_CONTROLLER_HPP
