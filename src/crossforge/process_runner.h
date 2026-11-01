#ifndef CROSSFORGE_PROCESS_RUNNER_H
#define CROSSFORGE_PROCESS_RUNNER_H

#include "crossforge.h"

// Runs an external command through the Windows shell, capturing combined
// stdout/stderr, with a configurable timeout, ESC cancellation, and a
// best-effort Windows Job Object that limits child-process count and kills the
// tree on close.
//
// NOTE: This is a process *runner* with resource limits, not a security
// sandbox. The job object restricts process count and lifetime only; it does
// NOT isolate the filesystem, registry, network, or privileges. Commands are
// executed with the calling user's full rights via cmd.exe.
class ProcessRunner {
public:
    static DWORD GetProcessTimeoutMs();
    static DWORD GetActiveProcessLimit();
    static std::string ResolveShellPath();
    static std::string StripCmdPrefix(const std::string& command);
    static bool CheckEscapeCancel(HANDLE inputHandle);

    // Execute `command`, appending combined stdout/stderr to `outputText`.
    // Returns true only if the process launched and exited with code 0.
    static bool RunProcess(const std::string& command, std::string& outputText);
};

#endif // CROSSFORGE_PROCESS_RUNNER_H
