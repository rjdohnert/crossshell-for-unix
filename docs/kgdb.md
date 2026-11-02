# kgdb

## Overview
`kgdb` is an authentic multi-target kernel debugging client and diagnostic inspection utility for Windows, FreeBSD, Microsoft Azure VMs, and WSL2.

It connects to live kernel probe contexts, remote GDB Remote Serial Protocol (RSP) stubs over TCP or Hyper-V named pipes, and analyzes Windows 64-bit kernel crash dumps (`MEMORY.DMP`).

---

## Command Line Usage
```
kgdb [OPTIONS] [DUMP_FILE]
```

### Options
- `--local`: Attach to the live local Windows kernel context (probes live CPU registers and module table).
- `--remote-win <HOST:PORT>`: Connect to a remote Windows kernel via KDNET or GDB RSP.
- `--azure <ENDPOINT>`: Connect to an Azure VM kernel debugging stub (e.g. `20.12.55.10:1234`).
- `--bsd [<TARGET> | <VMCORE>]`: Connect to a FreeBSD kernel GDB stub or parse a `vmcore` dump.
- `--wsl [<DISTRO>]`: Connect to a WSL2 microVM Linux kernel GDB stub.
- `--hyperv <PIPE_NAME>`: Connect to a Hyper-V virtual machine through a Win32 named pipe.
- `--remote <HOST:PORT>`: Connect to a generic remote GDB RSP stub over TCP.
- `-d, --dump <PATH>`: Load and decode an authentic Windows 64-bit crash dump (`MEMORY.DMP`).
- `-e, --exec <COMMANDS>`: Execute semicolon-separated debugger commands headlessly and exit.
- `--no-color`: Disable ANSI color escape codes.
- `-h, --help`: Display comprehensive command manual.
- `-v, -V, --version`: Display version information (`v4.6.0`).

---

## Interactive Debugger Commands
At the `(kgdb)` prompt, the following commands are available:

- `info registers` / `i r`: Display authentic 64-bit CPU context registers (`RAX`..`R15`, `RIP`, `RFLAGS`).
- `kldstat` / `lsmod`: List all active loaded kernel drivers and modules with base addresses and sizes.
- `bt` / `backtrace`: Display the kernel stack trace with real symbol and module offset resolution.
- `analyze` / `bugcheck`: Run authentic BSOD / Kernel Panic diagnostics, decoding Windows BugCheck codes and parameters.
- `x/<n> <addr>`: Examine memory at the specified address (when connected to a live GDB stub).
- `s` / `step`: Single-step target execution.
- `c` / `continue`: Resume target execution.
- `help [topic]`: Show help for commands or targets (`local`, `remote-win`, `azure`, `bsd`, `wsl`, `hyperv`, `examples`).
- `quit` / `q`: Exit the debugger.
