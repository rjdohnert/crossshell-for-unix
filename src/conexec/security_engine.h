#ifndef CONEXEC_SECURITY_ENGINE_H
#define CONEXEC_SECURITY_ENGINE_H

#include "conexec.h"
#include "isolation_level.h"

// Windows Security & ACL Management for AppContainer sandboxing.
class SecurityEngine {
public:
    // Grant the AppContainer packages SID (S-1-15-2-1) `permissions` on a path.
    static bool GrantAppContainerAccess(const fs::path& targetPath, DWORD permissions);

    // Ensure the executable named in `command` is readable/executable by
    // AppContainer processes.
    static void EnsureBinaryReadable(const std::wstring& command);

    // Build an environment block with TEMP/TMP/LOCALAPPDATA redirected into the
    // sandbox workspace.
    static std::vector<wchar_t> CreateSandboxedEnvironmentBlock(const fs::path& workspacePath);

    // Create a Job Object with limits appropriate to the isolation level.
    // Returns nullptr for Normal level. For sandboxed levels, returns nullptr
    // ONLY on failure -- callers must treat nullptr-for-sandbox as fatal
    // (fail-closed) rather than launching without limits.
    static HANDLE CreateJobSandbox(IsolationLevel level);
};

#endif // CONEXEC_SECURITY_ENGINE_H
