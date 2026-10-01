# vcc

## What it does
Provides a convenience wrapper around Microsoft Visual C++ (`cl.exe`) with GCC/POSIX-style flag translation.

It can auto-discover Visual Studio via `vswhere.exe`, initialize the compiler environment with `vcvars64.bat`, and then invoke `cl.exe`.

## Usage
- `vcc [FLAGS] [SOURCE_FILES] [NATIVE_MSVC_FLAGS]`

## Wrapper Options
- `-h`, `--help`, `/?`: show help
- `-v`, `--version`: show wrapper version

## Flag Translation (GCC/POSIX -> MSVC)
- `-o <file>` -> `/Fe:<file>`
- `-c` -> `/c`
- `-Wall`, `-Wextra` -> `/W4`
- `-O2`, `-O3` -> `/O2`
- `-O0` -> `/Od`
- `-g` -> `/Zi`
- `-I<dir>` / `-I <dir>` -> `/I<dir>`
- `-L<dir>` / `-L <dir>` -> `/LIBPATH:<dir>`
- `-std=c++17` -> `/std:c++17`
- `-std=c++20` -> `/std:c++20`
- `-std=c++latest` -> `/std:c++latest`

All other arguments are passed through to `cl.exe` unchanged.

## Environment Discovery
1. If `cl.exe` is already in `PATH`, `vcc` invokes it directly.
2. Otherwise, it queries `vswhere.exe` for a suitable Visual Studio install.
3. Then it launches `cmd.exe /C` and calls `vcvars64.bat` before invoking `cl.exe`.

## Examples
- `vcc -Wall -O2 -std=c++20 main.cpp -o app.exe`
- `vcc -c -Iinclude utils.cpp`
- `vcc -O2 main.cpp -o app.exe /link /SUBSYSTEM:CONSOLE`

## Exit Status
- Returns the underlying `cl.exe`/shell process exit code.
- Returns non-zero if Visual Studio cannot be located when `cl.exe` is not already available in `PATH`.

## UNIX origin
`vcc` is a CrossShellUX wrapper utility and not a traditional UNIX command.
