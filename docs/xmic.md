# xmic

## What it does

Queries Windows system information through WMI, with preset inventory and monitoring commands, custom WQL/class/path operations, output formatting, scripts, and DCOM, WinRM, or SSH transports.

## Usage

```text
xmic [global_options] <command|path <class>> [where "<clause>"] <verb> [arguments]
```

Running `xmic` without a command displays help. `xmic shell` starts the interactive command shell.

## Preset commands

`os`, `cpu`, `ram`, `disk`, `extdisk`, `net`, `nicconfig`, `gpu`, `bios`, `gswitch`, `vswitch`, `security`, `context`, `qfe`, `startup`, `share`, `baseboard`, `pagefile`, `port`, `sys`, `process`, `service`, `ntevent`, `apps`, `apps32`, `apps64`, `devices`, `threads`, `users`, and `sshcheck` report operating-system, hardware, network, process, service, application, security, or transport details. WQL-backed presets accept an optional `where "<clause>"` filter.

## WMI operations

```text
xmic path <WMI_Class> [where "<clause>"] get <PROP1,PROP2,...>
xmic path <WMI_Class> call <MethodName> [Param1=Val1 ...]
xmic path <WMI_Path> set <PropName>=<Value>
xmic path <WMI_Path> delete
xmic query "<WQL_QUERY>" <PROP1> [PROP2 ...]
```

## Common options

- `--node HOST`, `--user USER`, `--password PASS`, `--domain DOMAIN`: select a remote target and credentials. Prefer `--prompt-password` to avoid exposing passwords in process arguments.
- `--transport dcom|winrm|ssh`: select the remote transport; DCOM is the default.
- `--winrm-backend native|powershell`: select the WinRM backend.
- `--namespace NS`, `--locale LOCALE`, `--wmi-security LEVEL`: configure the WMI namespace, locale, and COM security level.
- `--ssh-port PORT`, `--ssh-key FILE`, `--ssh-prompt-key-passphrase`, `--ssh-strict-host-key MODE`, `--ssh-known-hosts FILE`, `--ssh-proxy-jump TARGETS`: configure SSH access.
- `--format table|csv|json`, `--output FILE`, `--no-header`, `--verbose`: control output.
- `--console-encoding utf16|utf8`: choose console encoding.
- `--timing`, `--benchmark-runs N`, `--benchmark-quiet`: report timing or repeat a command.
- `--script FILE`, `--script-stop-on-error`, `--script-no-echo`: execute command scripts.
- `help`, `shell`, `selftest`: show help, open the interactive shell, or run WSL detection checks.

## Examples

```powershell
xmic os
xmic process where "Name='explorer.exe'"
xmic --format json cpu
xmic query "SELECT Name, ProcessId FROM Win32_Process" Name ProcessId
xmic --transport ssh --node server.example.com --user admin sshcheck
xmic --benchmark-runs 5 --benchmark-quiet cpu
xmic selftest
```

Exit codes are `0` for success, `2` for invalid arguments, `3` for an unknown command, `4` for a WMI/runtime failure, and `5` when no data is returned. The executable requires the Windows WMI/MI development libraries to build.

## Build

From this directory, run `build.ps1` (PowerShell), `build.bat`, or `sh build.sh` (Git Bash). Builds install `xmic.exe` into `bin`; use `-NoInstall` or `--no-install` to keep the executable in this directory. Use `-Clean` or `clean` to remove local build artifacts.