#ifndef CONEXEC_HANDLE_CLOSER_H
#define CONEXEC_HANDLE_CLOSER_H

#include "conexec.h"

// RAII wrapper for Win32 handles.
struct HandleCloser {
    void operator()(HANDLE h) const noexcept {
        if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
};
using ScopedHandle = std::unique_ptr<void, HandleCloser>;

// Wait (up to timeoutMs) for all processes in a job to exit. Returns true if
// the job drained to zero active processes, false on timeout or query failure.
bool DrainJobObject(HANDLE hJob, DWORD timeoutMs = 2000);

// UI / version / help output.
void PrintVersion();
void PrintHeader();
void PrintHelp();

// Recursively clear read-only attributes so deletion can proceed.
void ClearReadOnlyAttributes(const fs::path& path);

// Delete a directory tree. Reparse points (symlinks/junctions) are unlinked,
// NOT followed, so deletion never escapes the intended tree.
void SafeRemoveAll(const fs::path& path);

// Escape a single argument per Windows CRT command-line rules.
std::wstring EscapeArgument(const std::wstring& arg);

// Resolve an App Execution Alias reparse point to its real target. Returns ""
// if the path is not a valid AppExecLink alias. Validates the reparse buffer
// before reading to avoid out-of-bounds access on malformed data.
std::wstring ResolveAppExecutionAlias(const std::wstring& aliasPath);

// Resolve the executable portion of a command line to an absolute path,
// following App Execution Aliases, and re-escape it. Returns the rewritten
// command line.
std::wstring ResolveExecutableInCommand(const std::wstring& command);

// Console control handler: terminates the active job, removes the workspace,
// and deletes the AppContainer profile on Ctrl+C/Break/Close.
BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType);

#endif // CONEXEC_HANDLE_CLOSER_H
