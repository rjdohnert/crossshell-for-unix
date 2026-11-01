#ifndef CONEXEC_H
#define CONEXEC_H

// Shared umbrella header for conexec: Win32 security headers, standard library,
// version constants, and the global signal-cleanup state.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define SECURITY_WIN32
#include <windows.h>
#include <sddl.h>
#include <userenv.h>
#include <aclapi.h>
#include <winioctl.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <chrono>
#include <memory>
#include <atomic>
#include <mutex>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "userenv.lib")

namespace fs = std::filesystem;

#define RUNCON_VERSION L"3.1.0"
#define RUNCON_AUTHOR  L"Roberto J Dohnert"

// --- Global State for Signal Cleanup ---
namespace GlobalState {
    extern std::atomic<bool> isTerminating;
    extern std::atomic<bool> signalCleanupStarted;
    extern std::mutex cleanupMutex;
    extern fs::path activeWorkspace;
    extern HANDLE activeJob;
    extern std::wstring appContainerProfileName;
}

#endif // CONEXEC_H
