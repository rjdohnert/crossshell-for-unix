# conexec

## What it does
Runs a command under selectable Windows isolation policies using AppContainer security boundaries, temporary workspaces, Windows job objects, and controlled network capabilities.

## Usage
```text
conexec <level> [options] -- <command> [args...]
```

## Security levels
- `-1`, `TotalLockdown`: zero-capability AppContainer lockdown, no network access, discarded temporary workspace, four-process limit, and 512 MiB aggregate job-memory limit.
- `-2`, `Ephemeral`: network-enabled AppContainer, discarded temporary workspace, sixteen-process limit, and 2 GiB aggregate job-memory limit.
- `-3`, `Interactive`: network-enabled AppContainer with staged output and interactive confirmation before changes are copied back.
- `-4`, `Permissive`: network-enabled AppContainer with output preserved in a dedicated temporary workspace.
- `-5`, `Normal`: runs with the current user permissions and does not attach the process to a job object.

## Options
- `-s`, `--stage`: copy the current directory into the sandbox workspace before execution.
- `-d PATH`, `--dir PATH`: set the target working directory.
- `-v`, `--verbose`: print security and execution telemetry.
- `-h`, `--help`: display the help manual.
- `-V`, `--version`: display version information.

## Network behavior
Levels `-2`, `-3`, and `-4` receive the AppContainer `internetClient`, `internetClientServer`, and `privateNetworkClientServer` capabilities. Level `-1` receives no network capabilities.

## Output behavior
Level `-1` and `-2` discard sandbox changes after execution. Level `-3` presents detected changes for confirmation. Level `-4` preserves the temporary workspace path after execution.

## Process cleanup
Sandboxed processes are assigned to a Windows job object. On shutdown, the job is terminated and drained by polling its active-process count before the AppContainer profile and workspace are removed.

## Exit status
Returns the target command's exit code on normal completion and `1` when setup, process creation, job assignment, or security initialization fails.

## Windows notes
This is a Windows-native utility using AppContainer profiles, security capabilities, ACLs, temporary directories, and job objects. The isolation boundary does not protect against a Windows kernel or sandbox-escape vulnerability.
